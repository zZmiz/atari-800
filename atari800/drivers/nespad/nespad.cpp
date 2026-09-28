#include "hardware/pio.h"
#include "nespad.h"

/* Fruit Jam v2.1 uses independent latch/clock/data pins for its two
 * controller ports. Clock 16 bits so both NES and SNES protocol-compatible
 * pads are sampled completely. */
static const uint16_t nespad_program_instructions[] = {
    0xd020, /* irq wait 0          side 1 */
    0xf101, /* set pins, 1         side 1 [1] */
    0xf02f, /* set x, 15           side 1 */
    0xf000, /* set pins, 0         side 1 */
    0xf400, /* set pins, 0         side 1 [4] */
    0xe100, /* set pins, 0         side 0 [1] */
    0x5301, /* in pins, 1          side 1 [3] */
    0x1044, /* jmp x--, 4          side 1 */
};

#define NESPAD_PROGRAM_LENGTH \
    (sizeof(nespad_program_instructions) / sizeof(nespad_program_instructions[0]))

static const struct pio_program nespad_program = {
    .instructions = nespad_program_instructions,
    .length = NESPAD_PROGRAM_LENGTH,
    .origin = -1,
};

static inline pio_sm_config nespad_program_get_default_config(uint offset)
{
    pio_sm_config config = pio_get_default_sm_config();
    sm_config_set_wrap(&config, offset, offset + NESPAD_PROGRAM_LENGTH - 1);
    sm_config_set_sideset(&config, 1, false, false);
    return config;
}

#if defined(PICO_RP2350)
#define NESPAD_PIO_COUNT 3
#else
#define NESPAD_PIO_COUNT 2
#endif

static PIO pad_pio[2];
static int8_t pad_sm[2] = {-1, -1};
static bool second_pad;
static int program_offset[NESPAD_PIO_COUNT];
static bool program_added[NESPAD_PIO_COUNT];

uint8_t nespad_states[2];
uint16_t nespad_states_ext[2];
uint16_t nespad_raw_ext[2];
uint8_t nespad_padtype[2];

bool nespad_begin(uint8_t padnum, uint32_t cpu_khz, uint8_t clk_pin,
                  uint8_t data_pin, uint8_t latch_pin, PIO pio)
{
    if (padnum > 1)
        return false;

    pad_pio[padnum] = pio;
    const int pio_index = pio_get_index(pio);
    pad_sm[padnum] = pio_claim_unused_sm(pio, true);
    if (pad_sm[padnum] < 0)
        return false;

    if (!program_added[pio_index]) {
        if (!pio_can_add_program(pio, &nespad_program))
            return false;
        program_offset[pio_index] = pio_add_program(pio, &nespad_program);
        program_added[pio_index] = true;
    }

    const uint offset = (uint)program_offset[pio_index];
    pio_sm_config config = nespad_program_get_default_config(offset);
    sm_config_set_sideset_pins(&config, clk_pin);
    sm_config_set_in_pins(&config, data_pin);
    sm_config_set_set_pins(&config, latch_pin, 1);
    sm_config_set_in_shift(&config, true, true, 16);
    sm_config_set_clkdiv_int_frac(&config, cpu_khz / 1000, 0);

    pio_gpio_init(pio, clk_pin);
    pio_gpio_init(pio, data_pin);
    pio_gpio_init(pio, latch_pin);
    gpio_set_pulls(data_pin, true, false);
    pio_sm_set_pindirs_with_mask(
        pio, pad_sm[padnum],
        (1u << clk_pin) | (1u << latch_pin),
        (1u << clk_pin) | (1u << data_pin) | (1u << latch_pin));

    pio_set_irq0_source_enabled(
        pio, (pio_interrupt_source)(pis_interrupt0 + pad_sm[padnum]), false);
    pio_sm_clear_fifos(pio, pad_sm[padnum]);
    pio_sm_init(pio, pad_sm[padnum], offset, &config);
    pio_sm_set_enabled(pio, pad_sm[padnum], true);
    second_pad |= padnum == 1;
    return true;
}

void nespad_read_start(void)
{
    if (pad_sm[0] >= 0) {
        while (!pio_sm_is_rx_fifo_empty(pad_pio[0], pad_sm[0]))
            (void)pio_sm_get(pad_pio[0], pad_sm[0]);
        pio_interrupt_clear(pad_pio[0], 0);
    }
    if (second_pad && pad_sm[1] >= 0) {
        while (!pio_sm_is_rx_fifo_empty(pad_pio[1], pad_sm[1]))
            (void)pio_sm_get(pad_pio[1], pad_sm[1]);
        if (pad_pio[1] != pad_pio[0])
            pio_interrupt_clear(pad_pio[1], 0);
    }
}

static bool nespad_try_decode(unsigned padnum, uint16_t *decoded)
{
    if (pad_sm[padnum] < 0 ||
        pio_sm_is_rx_fifo_empty(pad_pio[padnum], pad_sm[padnum]))
        return false;

    uint32_t raw = (pio_sm_get(pad_pio[padnum], pad_sm[padnum]) >> 16) ^ 0xffffu;
    nespad_raw_ext[padnum] = (uint16_t)raw;

    /* NES pads shift zero after their eight buttons, which becomes an all-one
     * ID nibble after inversion. SNES pads provide another four buttons. */
    if ((raw & 0xf000u) == 0xf000u) {
        nespad_padtype[padnum] = NESPAD_TYPE_NES;
        raw &= 0x00ffu;
    }
    else if (raw & 0x0f00u) {
        nespad_padtype[padnum] = NESPAD_TYPE_SNES;
    }
    else if (nespad_padtype[padnum] == NESPAD_TYPE_NES) {
        nespad_padtype[padnum] = NESPAD_TYPE_UNKNOWN;
    }
    *decoded = (uint16_t)(raw & 0x0fffu);
    return true;
}

void nespad_read_finish(void)
{
    uint16_t decoded = 0;
    nespad_states_ext[0] = nespad_try_decode(0, &decoded) ? decoded : 0;
    decoded = 0;
    nespad_states_ext[1] = second_pad && nespad_try_decode(1, &decoded)
                           ? decoded : 0;
    nespad_states[0] = (uint8_t)nespad_states_ext[0];
    nespad_states[1] = (uint8_t)nespad_states_ext[1];
}
