#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "hardware/pio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DPAD_LEFT   0x0040u
#define DPAD_RIGHT  0x0080u
#define DPAD_DOWN   0x0020u
#define DPAD_UP     0x0010u
#define DPAD_START  0x0008u
#define DPAD_SELECT 0x0004u
#define DPAD_B      0x0002u
#define DPAD_A      0x0001u
#define DPAD_SNES_A 0x0100u
#define DPAD_SNES_X 0x0200u

enum {
    NESPAD_TYPE_UNKNOWN = 0,
    NESPAD_TYPE_NES,
    NESPAD_TYPE_SNES,
};

extern uint8_t nespad_states[2];
extern uint16_t nespad_states_ext[2];
extern uint16_t nespad_raw_ext[2];
extern uint8_t nespad_padtype[2];

extern bool nespad_begin(uint8_t padnum, uint32_t cpu_khz, uint8_t clk_pin,
                         uint8_t data_pin, uint8_t latch_pin, PIO pio);
extern void nespad_read_start(void);
extern void nespad_read_finish(void);

#ifdef __cplusplus
}
#endif
