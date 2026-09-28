/**
 * MIT License
 *
 * Copyright (c) 2022 Vincent Mistler
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#define PWM_PIN0 (AUDIO_PWM_PIN&0xfe)
#define PWM_PIN1 (PWM_PIN0+1)

#include "audio.h"
#include "hardware/irq.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"

#define I2S_DMA_BLOCK_FRAMES 128u
#define I2S_START_THRESHOLD_FRAMES 1024u

static i2s_config_t *active_i2s;
static int companion_dma_channel = -1;
static uint32_t dma_stage[2][I2S_DMA_BLOCK_FRAMES] __attribute__((aligned(4)));
static volatile bool dma_running;
static volatile bool rebuffering;
static uint32_t last_dma_irq_us;
static uint32_t clock_divider_q8_floor;
static uint32_t clock_divider_remainder;
static uint32_t clock_divider_denominator;
static uint32_t clock_divider_accumulator;

/* The RP2350 PIO divider has eight fractional bits.  A single rounded value
 * is close to 44.1 kHz, but its sub-Hz error eventually drains a burst-fed
 * source ring.  Alternate the two adjacent Q8 divider values at DMA block
 * boundaries so their long-term average is the exact requested rate. */
static void __not_in_flash_func(i2s_advance_clock_divider)(
    const i2s_config_t *config)
{
#ifndef AUDIO_PWM_PIN
    uint32_t divider_q8 = clock_divider_q8_floor;
    clock_divider_accumulator += clock_divider_remainder;
    if (clock_divider_accumulator >= clock_divider_denominator) {
        clock_divider_accumulator -= clock_divider_denominator;
        divider_q8++;
    }
    pio_sm_set_clkdiv_int_frac(config->pio, config->sm,
                               divider_q8 >> 8u, divider_q8 & 0xffu);
#else
    (void)config;
#endif
}

static inline uint16_t i2s_ring_fill(const i2s_config_t *config)
{
    return (uint16_t)((config->write_index - config->read_index) &
                      (config->dma_trans_count - 1u));
}

static void __not_in_flash_func(i2s_copy_to_stage)(i2s_config_t *config,
                                                   uint32_t *dst,
                                                   uint16_t count)
{
    for (uint16_t i = 0; i < count; ++i) {
        dst[i] = config->dma_buf[config->read_index];
        config->read_index = (uint16_t)((config->read_index + 1u) &
                                        (config->dma_trans_count - 1u));
    }
}

static void __not_in_flash_func(i2s_refill_stage)(i2s_config_t *config,
                                                  unsigned stage)
{
    uint16_t available = i2s_ring_fill(config);
    if (rebuffering && available >= I2S_START_THRESHOLD_FRAMES)
        rebuffering = false;

    if (!rebuffering && available >= I2S_DMA_BLOCK_FRAMES) {
        i2s_copy_to_stage(config, dma_stage[stage], I2S_DMA_BLOCK_FRAMES);
        return;
    }

    if (!rebuffering) {
        i2s_copy_to_stage(config, dma_stage[stage], available);
        for (uint16_t i = available; i < I2S_DMA_BLOCK_FRAMES; ++i)
            dma_stage[stage][i] = 0;
        config->underruns++;
        rebuffering = true;
    }
    else {
        for (uint16_t i = 0; i < I2S_DMA_BLOCK_FRAMES; ++i)
            dma_stage[stage][i] = 0;
    }
}

static bool __not_in_flash_func(i2s_service_dma)(i2s_config_t *config,
                                                 int channel,
                                                 unsigned stage)
{
    const uint32_t mask = 1u << channel;
    if (!(dma_hw->ints1 & mask))
        return false;
    dma_hw->ints1 = mask;
    config->dma_blocks++;
    i2s_refill_stage(config, stage);
    dma_channel_set_read_addr(channel, dma_stage[stage], false);
    dma_channel_set_trans_count(channel, I2S_DMA_BLOCK_FRAMES, false);
    return true;
}

static void __isr __time_critical_func(i2s_dma_handler)(void)
{
    i2s_config_t *config = active_i2s;
    if (!config)
        return;

    const uint32_t now = time_us_32();
    if (last_dma_irq_us) {
        const uint32_t interval = now - last_dma_irq_us;
        if (interval > config->max_irq_interval_us)
            config->max_irq_interval_us = interval;
    }
    last_dma_irq_us = now;

    if (i2s_service_dma(config, config->dma_channel, 0))
        i2s_advance_clock_divider(config);
    if (i2s_service_dma(config, companion_dma_channel, 1))
        i2s_advance_clock_divider(config);
    if (dma_running && !dma_channel_is_busy(config->dma_channel) &&
        !dma_channel_is_busy(companion_dma_channel)) {
        /* This is a real transport gap: both chained channels stopped before
         * the completion IRQ could re-arm either one. Keep a direct counter
         * so a click cannot hide behind healthy source-ring statistics. */
        config->clock_gaps++;
        dma_channel_start(config->dma_channel);
    }
}

static int claim_companion_dma_channel(int primary)
{
    for (int channel = 4; channel < NUM_DMA_CHANNELS; ++channel) {
        if (channel != primary && !dma_channel_is_claimed(channel)) {
            dma_channel_claim(channel);
            return channel;
        }
    }
    return -1;
}

#ifdef AUDIO_PWM_PIN
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#endif

/**
 * return the default i2s context used to store information about the setup
 */
i2s_config_t i2s_get_default_config(void) {
    i2s_config_t i2s_config = {
		.sample_freq = 44100, 
		.channel_count = 2,
		.data_pin = 26,
		.clock_pin_base = 27,
		.pio = pio1,
		.sm = 0,
        .dma_channel = 0,
        .dma_companion_channel = 0,
        .dma_buf = NULL,
        .dma_trans_count = 0,
        .volume = 0,
		.read_index = 0,
		.write_index = 0,
		.samples_enqueued = 0,
		.samples_dropped = 0,
		.dma_blocks = 0,
		.underruns = 0,
		.restarts = 0,
		.clock_gaps = 0,
		.max_irq_interval_us = 0,
		.started = false,
	};

    return i2s_config;
}

/**
 * Initialize the I2S driver. Must be called before calling i2s_write or i2s_dma_write
 * i2s_config: I2S context obtained by i2s_get_default_config()
 */


bool i2s_init(i2s_config_t *i2s_config) {

    /* Ring indexing uses a mask and every DMA transfer is one aligned block. */
    if (i2s_config->dma_trans_count < I2S_START_THRESHOLD_FRAMES ||
        (i2s_config->dma_trans_count & (i2s_config->dma_trans_count - 1u)) != 0 ||
        (i2s_config->dma_trans_count % I2S_DMA_BLOCK_FRAMES) != 0)
        return false;

    i2s_config->dma_buf = calloc(i2s_config->dma_trans_count, sizeof(uint32_t));
    if (!i2s_config->dma_buf)
        return false;

    int dma_channel = -1;
    /* HSTX owns DMA IRQ0. Restrict audio to channels routed to DMA IRQ1 so
     * the two drivers cannot replace or starve one another's handler. */
    for (int channel = 4; channel < NUM_DMA_CHANNELS; ++channel) {
        if (!dma_channel_is_claimed(channel)) {
            dma_channel_claim(channel);
            dma_channel = channel;
            break;
        }
    }
    if (dma_channel < 0) {
        free(i2s_config->dma_buf);
        i2s_config->dma_buf = NULL;
        return false;
    }
    i2s_config->dma_channel = (uint8_t)dma_channel;
    companion_dma_channel = claim_companion_dma_channel(dma_channel);
    if (companion_dma_channel < 0) {
        dma_channel_unclaim(i2s_config->dma_channel);
        free(i2s_config->dma_buf);
        i2s_config->dma_buf = NULL;
        return false;
    }
    i2s_config->dma_companion_channel = (uint8_t)companion_dma_channel;


#ifndef AUDIO_PWM_PIN

    uint8_t func=GPIO_FUNC_PIO1;    // TODO: GPIO_FUNC_PIO0 for pio0 or GPIO_FUNC_PIO1 for pio1
    gpio_set_function(i2s_config->data_pin, func);
    gpio_set_function(i2s_config->clock_pin_base, func);
    gpio_set_function(i2s_config->clock_pin_base+1, func);
    
    int sm = pio_claim_unused_sm(i2s_config->pio, false);
    if (sm < 0) {
        dma_channel_unclaim(companion_dma_channel);
        dma_channel_unclaim(i2s_config->dma_channel);
        free(i2s_config->dma_buf);
        i2s_config->dma_buf = NULL;
        return false;
    }
    i2s_config->sm = (uint8_t)sm;

    /* Set PIO clock */
    uint32_t system_clock_frequency = clock_get_hz(clk_sys);
    const uint64_t divider_numerator =
        (uint64_t)system_clock_frequency * 4u;
    uint32_t divider =
        (uint32_t)(divider_numerator / i2s_config->sample_freq);
    clock_divider_q8_floor = divider;
    clock_divider_remainder =
        (uint32_t)(divider_numerator % i2s_config->sample_freq);
    clock_divider_denominator = i2s_config->sample_freq;
    clock_divider_accumulator = 0;

#ifdef I2S_CS4334
    uint offset = pio_add_program(i2s_config->pio, &audio_i2s_cs4334_program);
    audio_i2s_cs4334_program_init(i2s_config->pio, i2s_config->sm , offset, i2s_config->data_pin , i2s_config->clock_pin_base);
    divider >>= 3;
#else
    uint offset = pio_add_program(i2s_config->pio, &audio_i2s_program);
    audio_i2s_program_init(i2s_config->pio, i2s_config->sm , offset, i2s_config->data_pin , i2s_config->clock_pin_base);

#endif

    pio_sm_set_clkdiv_int_frac(i2s_config->pio, i2s_config->sm , divider >> 8u, divider & 0xffu);

    pio_sm_set_enabled(i2s_config->pio, i2s_config->sm, false);
#endif
    dma_channel_config dma_config = dma_channel_get_default_config(i2s_config->dma_channel);
    channel_config_set_read_increment(&dma_config, true);
    channel_config_set_write_increment(&dma_config, false);

    channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_32);

    uint32_t* addr_write_DMA=&(i2s_config->pio->txf[i2s_config->sm]);
#ifdef AUDIO_PWM_PIN
    gpio_set_function(PWM_PIN0, GPIO_FUNC_PWM);
    gpio_set_function(PWM_PIN1, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(PWM_PIN0);


   
    pwm_config c_pwm=pwm_get_default_config();
    pwm_config_set_clkdiv(&c_pwm,1.0);
    //pwm_config_set_wrap(&c_pwm,(1<<12)-1);//MAX PWM value
    pwm_config_set_wrap(&c_pwm,clock_get_hz(clk_sys)/(i2s_config->sample_freq));//MAX PWM value
    pwm_init(slice_num,&c_pwm,true);

    //Для синхронизации используем другой произвольный канал ШИМ


    channel_config_set_dreq(&dma_config, pwm_get_dreq(slice_num));     


    addr_write_DMA=(uint32_t*)&pwm_hw->slice[slice_num].cc;
#else
    channel_config_set_dreq(&dma_config, pio_get_dreq(i2s_config->pio, i2s_config->sm, true));
#endif
    
    dma_channel_config companion_config = dma_channel_get_default_config(companion_dma_channel);
    channel_config_set_read_increment(&companion_config, true);
    channel_config_set_write_increment(&companion_config, false);
    channel_config_set_transfer_data_size(&companion_config, DMA_SIZE_32);
    channel_config_set_dreq(&companion_config,
                            pio_get_dreq(i2s_config->pio, i2s_config->sm, true));
    channel_config_set_chain_to(&dma_config, companion_dma_channel);
    channel_config_set_chain_to(&companion_config, i2s_config->dma_channel);

    memset(dma_stage, 0, sizeof(dma_stage));
    dma_running = false;
    rebuffering = true;
    last_dma_irq_us = 0;
    dma_channel_configure(i2s_config->dma_channel, &dma_config,
                          addr_write_DMA, dma_stage[0],
                          I2S_DMA_BLOCK_FRAMES, false);
    dma_channel_configure(companion_dma_channel, &companion_config,
                          addr_write_DMA, dma_stage[1],
                          I2S_DMA_BLOCK_FRAMES, false);

    active_i2s = i2s_config;
    dma_channel_set_irq1_enabled(i2s_config->dma_channel, true);
    dma_channel_set_irq1_enabled(companion_dma_channel, true);
    irq_add_shared_handler(DMA_IRQ_1, i2s_dma_handler,
                           PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
    irq_set_enabled(DMA_IRQ_1, true);

    pio_sm_set_enabled(i2s_config->pio, i2s_config->sm , true);
    return true;
}

/**
 * Write samples to I2S directly and wait for completion (blocking)
 * i2s_config: I2S context obtained by i2s_get_default_config()
 *     sample: pointer to an array of len x 32 bits samples
 *             Each 32 bits sample contains 2x16 bits samples, 
 *             one for the left channel and one for the right channel
 *        len: length of sample in 32 bits words
 */
void i2s_write(const i2s_config_t *i2s_config,const int16_t *samples,const size_t len) {
    for(size_t i=0;i<len;i++) {
            pio_sm_put_blocking(i2s_config->pio, i2s_config->sm, (uint32_t)samples[i]);
    }
}

/**
 * Write samples to DMA buffer and initiate DMA transfer (non blocking)
 * i2s_config: I2S context obtained by i2s_get_default_config()
 *     sample: pointer to an array of dma_trans_count x 32 bits samples
 */
void i2s_dma_write(i2s_config_t *i2s_config,const int16_t *samples) {
    i2s_dma_write_count(i2s_config, samples, i2s_config->dma_trans_count);
}

void i2s_dma_write_count(i2s_config_t *i2s_config, const int16_t *samples,
                         size_t frame_count) {
    if (!i2s_config->dma_buf || !frame_count)
        return;

    /* A completed Atari frame supplies about 736 samples. Queue the complete
     * burst before starting DMA; this removes the old blocking wait and gives
     * the audio clock a useful amount of data immediately. */
    uint32_t irq_state = save_and_disable_interrupts();
    /* If DMA completed just before interrupts were masked, consume that
     * completion first. Otherwise the restart path could retransmit the old
     * block using a read index which its pending IRQ has not advanced yet. */
    if (dma_hw->ints1 & (1u << i2s_config->dma_channel))
        i2s_dma_handler();
    for (size_t i = 0; i < frame_count; ++i) {
        const uint16_t next = (uint16_t)((i2s_config->write_index + 1u) &
                                         (i2s_config->dma_trans_count - 1u));
        if (next == i2s_config->read_index) {
            i2s_config->samples_dropped += (uint32_t)(frame_count - i);
            break;
        }

        int16_t left = samples[i * 2u];
        int16_t right = samples[i * 2u + 1u];
        if (i2s_config->volume) {
            left >>= i2s_config->volume;
            right >>= i2s_config->volume;
        }
        i2s_config->dma_buf[i2s_config->write_index] =
            ((uint32_t)(uint16_t)left << 16) | (uint16_t)right;
        i2s_config->write_index = next;
        i2s_config->samples_enqueued++;
    }

    /* The block can also complete while the burst above is being copied with
     * interrupts masked. Consume that late completion before consulting BUSY;
     * otherwise the producer can restart the just-finished block using a stale
     * read index, then the deferred IRQ advances/reconfigures it a second time. */
    if (dma_hw->ints1 & (1u << i2s_config->dma_channel))
        i2s_dma_handler();

    if (!dma_running) {
        const uint16_t fill = i2s_ring_fill(i2s_config);
        if (fill >= I2S_START_THRESHOLD_FRAMES) {
            i2s_config->started = true;
            i2s_config->restarts++;
            dma_running = true;
            dma_channel_start(i2s_config->dma_channel);
        }
    }
    restore_interrupts(irq_state);
}

void i2s_get_stats(i2s_config_t *i2s_config, i2s_stats_t *stats)
{
    uint32_t irq_state = save_and_disable_interrupts();
    stats->samples_enqueued = i2s_config->samples_enqueued;
    stats->samples_dropped = i2s_config->samples_dropped;
    stats->dma_blocks = i2s_config->dma_blocks;
    stats->underruns = i2s_config->underruns;
    stats->restarts = i2s_config->restarts;
    stats->clock_gaps = i2s_config->clock_gaps;
    stats->max_irq_interval_us = i2s_config->max_irq_interval_us;
    stats->fill_frames = i2s_ring_fill(i2s_config);
    stats->dma_channel = i2s_config->dma_channel;
    stats->dma_companion_channel = i2s_config->dma_companion_channel;
    stats->state_machine = i2s_config->sm;
    stats->dma_busy = dma_channel_is_busy(i2s_config->dma_channel) ||
                      dma_channel_is_busy(companion_dma_channel);
    restore_interrupts(irq_state);
}

/**
 * Adjust the output volume
 * i2s_config: I2S context obtained by i2s_get_default_config()
 *     volume: desired volume between 0 (highest. volume) and 16 (lowest volume)
 */
void i2s_volume(i2s_config_t *i2s_config,uint8_t volume) {
    if(volume>16) volume=16;
    i2s_config->volume=volume;
}

/**
 * Increases the output volume
 */
void i2s_increase_volume(i2s_config_t *i2s_config) {
    if(i2s_config->volume>0) {
        i2s_config->volume--;
    }
}

/**
 * Decreases the output volume
 */
void i2s_decrease_volume(i2s_config_t *i2s_config) {
    if(i2s_config->volume<16) {
        i2s_config->volume++;
    }
}
