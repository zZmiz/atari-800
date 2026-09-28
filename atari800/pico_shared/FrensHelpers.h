#pragma once
// Atari hardware adapter for the vendored Fruit Jam menu. No other core's
// video, USB, clock, flash or audio initialisation is linked into Atari.
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cstdio>
#ifdef ATARI_MENU_HOST_TEST
#include "host_platform.h"
#else
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "hstx.h"
#endif
#include "ff.h"
#define HSTX 1
#define CBLACK 15
#define CWHITE 48
#define CRED 21
#define CGREEN 42
#define CBLUE 2
#define CLIGHTBLUE 17
#define DEFAULT_FGCOLOR CBLACK
#define DEFAULT_BGCOLOR CWHITE
#define SCREENWIDTH 320
#define SCREENHEIGHT 240
#define ROMINFOFILE "/atari-current-unused.txt"
#define FILEXTFORSEARCH ".555"
#define NES_PIN_CLK 6
#define NES_PIN_CLK_1 9
#define WII_PIN_SDA -1
#define WII_PIN_SCL -1
#define WIIPAD_IS_CONNECTED() false
#define USE_I2S_AUDIO 0
#define PICO_AUDIO_I2S_DRIVER_TLV320 99
#define USE_PICO_EXTRAS_I2S 1
#define EXT_AUDIO_DACERROR() false
#define EXT_AUDIO_IS_ENABLED false
#define EXT_AUDIO_SETVOLUME(x) ((void)0)
#define EXT_AUDIO_DISABLE() ((void)0)
#define HW_CONFIG 7
enum class ScreenMode { SCANLINE_8_7, NOSCANLINE_8_7, SCANLINE_1_1, NOSCANLINE_1_1, MAX };
enum class ScanlineType : uint8_t { SIMPLE, LCD, MAX };
extern bool scaleMode8_7_;
extern uintptr_t ROM_FILE_ADDR;
extern int maxRomSize;
extern char ErrorMessage[40];
extern int abSwapped;
namespace Frens {
inline void *f_malloc(size_t n) { return malloc(n); }
inline void f_free(void *p) { free(p); }
inline bool isPsramEnabled() { return false; }
inline bool isFrameBufferUsed() { return true; }
inline uint32_t time_ms() { return to_ms_since_boot(get_absolute_time()); }
inline void PaceFrames60fps(bool init, bool = false) { hstx_paceFrame(init); }
inline void blinkLed(bool) {}
inline void pollHeadPhoneJack() {}
inline void resetWifi() {}
inline bool romIsByteSwapped() { return false; }
inline uint32_t getCrcOfLoadedRom() { return 0; }
inline uint32_t getMaxFreqKHz() { return 252000; }
inline uint32_t getMinFreqKHz() { return 252000; }
inline vreg_voltage getMaxVoltage() { return VREG_VOLTAGE_1_20; }
inline vreg_voltage getMinVoltage() { return VREG_VOLTAGE_1_20; }
inline bool WriteMaxValuesToFlash() { return false; }
inline bool WriteMinValuesToFlash() { return false; }
bool applyScreenMode(ScreenMode mode);
void restoreScanlines();
uint GetAvailableMemory();
[[noreturn]] void rebootToBootloader();
bool fileExists(const char *path);
void getextensionfromfilename(const char *name, char *out, size_t size);
inline bool cstr_endswith(const char *s, const char *suffix) {
    return strlen(s) >= strlen(suffix) && !strcasecmp(s + strlen(s) - strlen(suffix), suffix);
}
}
