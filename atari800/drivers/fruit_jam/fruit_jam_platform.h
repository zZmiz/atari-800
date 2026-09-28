#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void fruit_jam_present_screen(void);
void fruit_jam_hold_screen(void);
void fruit_jam_poll_controller(void);
void fruit_jam_get_present_metrics(uint32_t *wait_us, uint32_t *copy_us);
void fruit_jam_request_launcher(void);

#ifdef __cplusplus
}
#endif
