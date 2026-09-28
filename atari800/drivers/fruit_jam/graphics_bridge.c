
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "graphics.h"
#include "hstx.h"
#include "pico/time.h"

#define FRUIT_JAM_WIDTH 320
#define FRUIT_JAM_HEIGHT 240
#define ATARI_CROP_X 32

static uint8_t *source_buffer;
static uint16_t source_width;
static uint16_t source_height;
static uint16_t palette_rgb555[256];
static bool display_started;
static bool presentation_started;
static uint32_t last_wait_us;
static uint32_t last_copy_us;

uint8_t *text_buffer;

void graphics_init(void)
{
    if (!display_started) {
        hstx_init(false);
        display_started = true;
    }
}

void graphics_set_mode(enum graphics_mode_t mode)
{
    (void)mode;
}

void graphics_set_buffer(uint8_t *buffer, uint16_t width, uint16_t height)
{
    source_buffer = buffer;
    source_width = width;
    source_height = height;
}

void graphics_set_offset(int x, int y)
{
    (void)x;
    (void)y;
}

void graphics_set_palette(uint8_t index, uint32_t color)
{
    const uint16_t r = (color >> 19) & 0x1f;
    const uint16_t g = (color >> 11) & 0x1f;
    const uint16_t b = (color >> 3) & 0x1f;
    palette_rgb555[index] = (uint16_t)((r << 10) | (g << 5) | b);
}

void graphics_set_textbuffer(uint8_t *buffer)
{
    text_buffer = buffer;
}

void graphics_set_bgcolor(uint32_t color888)
{
    (void)color888;
}

void graphics_set_flashmode(bool flash_line, bool flash_frame)
{
    (void)flash_line;
    (void)flash_frame;
}

void clrScr(uint8_t color)
{
    if (source_buffer)
        memset(source_buffer, color, (size_t)source_width * source_height);
}

void fruit_jam_present_screen(void)
{
    if (!display_started || !source_buffer || source_height < FRUIT_JAM_HEIGHT ||
        source_width < ATARI_CROP_X + FRUIT_JAM_WIDTH)
        return;

    const uint32_t wait_start = time_us_32();

    hstx_paceFrame(!presentation_started);
    presentation_started = true;
    const uint32_t copy_start = time_us_32();
    last_wait_us = copy_start - wait_start;
    uint32_t *dst = (uint32_t *)hstx_getframebuffer();
    for (unsigned y = 0; y < FRUIT_JAM_HEIGHT; ++y) {
        const uint8_t *src = source_buffer + (size_t)y * source_width + ATARI_CROP_X;
        for (unsigned x = 0; x < FRUIT_JAM_WIDTH; x += 2) {
            const uint16_t left = palette_rgb555[src[x]];
            const uint16_t right = palette_rgb555[src[x + 1]];
            *dst++ = (uint32_t)left | ((uint32_t)right << 16);
        }
    }
    last_copy_us = time_us_32() - copy_start;
}

void fruit_jam_hold_screen(void)
{
    if (!display_started)
        return;

    const uint32_t wait_start = time_us_32();
    hstx_paceFrame(!presentation_started);
    presentation_started = true;
    last_wait_us = time_us_32() - wait_start;
    last_copy_us = 0;
}

void fruit_jam_get_present_metrics(uint32_t *wait_us, uint32_t *copy_us)
{
    if (wait_us)
        *wait_us = last_wait_us;
    if (copy_us)
        *copy_us = last_copy_us;
}
