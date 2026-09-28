#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern bool PSRAM_AVAILABLE;
void init_psram(void);
void psram_cleanup(void);
void write8psram(uint32_t address, uint8_t value);
void write16psram(uint32_t address, uint16_t value);
uint8_t read8psram(uint32_t address);
uint16_t read16psram(uint32_t address);

#ifdef __cplusplus
}
#endif
