#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Start the two RP2350B PIO-USB host root ports.  Failure is non-fatal: the
 * emulator and the vintage controller ports continue to operate. */
bool usb_input_init(void);

/* Service TinyUSB without waiting.  Call once per emulated frame. */
void usb_input_task(void);

/* Return the normalized button bitmap from gamepad.h for player 0 or 1. */
uint32_t usb_input_buttons(unsigned player);

#ifdef __cplusplus
}
#endif
