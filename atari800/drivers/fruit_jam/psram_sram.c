
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "psram_compat.h"

#define SHADOW_RAM_SIZE (16u * 1024u + 8u * 1024u + 8u * 1024u + 2u * 1024u)

static uint8_t shadow_ram[SHADOW_RAM_SIZE];
bool PSRAM_AVAILABLE = false;

void init_psram(void)
{
    memset(shadow_ram, 0, sizeof(shadow_ram));
    PSRAM_AVAILABLE = true;
}

void psram_cleanup(void)
{
    memset(shadow_ram, 0, sizeof(shadow_ram));
}

void write8psram(uint32_t address, uint8_t value)
{
    if (address < sizeof(shadow_ram))
        shadow_ram[address] = value;
}

void write16psram(uint32_t address, uint16_t value)
{
    write8psram(address, (uint8_t)value);
    write8psram(address + 1, (uint8_t)(value >> 8));
}

uint8_t read8psram(uint32_t address)
{
    return address < sizeof(shadow_ram) ? shadow_ram[address] : 0xff;
}

uint16_t read16psram(uint32_t address)
{
    return (uint16_t)(read8psram(address) | ((uint16_t)read8psram(address + 1) << 8));
}
