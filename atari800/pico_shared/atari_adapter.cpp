
#include "FrensHelpers.h"
#include "settings.h"
#include "menu.h"
#include "menu_settings.h"
#include "recentgames.h"
extern "C" {
#include "afile.h"
#ifdef ATARI_MENU_HOST_TEST
#define TRUE 1
#define FALSE 0
extern int UI_is_active;
void Atari800_Coldstart(void);
void CARTRIDGE_Remove(void);
void SIO_Dismount(int);
#else
#include "atari.h"
#include "cartridge.h"
#include "sio.h"
#include "ui.h"
#include "sound.h"
#include "fruit_jam_platform.h"
#endif
void fruit_jam_return_to_launcher_now(void);
}

struct settings settings;
bool scaleMode8_7_ = false;
uintptr_t ROM_FILE_ADDR = 0;
int maxRomSize = 0;
char ErrorMessage[40] = {};
static int8_t visibility[MOPT_COUNT];
static const uint8_t screenModes[] = {1,1,1,1};
static constexpr const char *settingsPath = "/atari800-menu.cfg";
static char currentImagePath[FF_MAX_LFN] = {};
extern "C" { unsigned fruit_jam_menu_epoch = 0; }

namespace Frens {
uint GetAvailableMemory() { return 0; }
bool applyScreenMode(ScreenMode mode) {
    hstx_setAspectRatio87(mode == ScreenMode::SCANLINE_8_7 || mode == ScreenMode::NOSCANLINE_8_7);
    hstx_setScanLines(mode == ScreenMode::SCANLINE_8_7 || mode == ScreenMode::SCANLINE_1_1);
    return mode == ScreenMode::SCANLINE_8_7 || mode == ScreenMode::NOSCANLINE_8_7;
}
void restoreScanlines() { applyScreenMode(settings.screenMode); hstx_setScanLineType(settings.scanlineType); }
[[noreturn]] void rebootToBootloader() { fruit_jam_return_to_launcher_now(); __builtin_unreachable(); }
bool fileExists(const char *path) {
    FILINFO *info = static_cast<FILINFO *>(malloc(sizeof(FILINFO)));
    if (!info) return false;
    bool ok = f_stat(path, info) == FR_OK;
    free(info); return ok;
}
void getextensionfromfilename(const char *name, char *out, size_t size) {
    const char *dot = strrchr(name, '.');
    snprintf(out, size, "%s", dot ? dot : "");
}
}
namespace FrensSettings {
void setEmulatorType(const char *) {}
emulators getEmulatorType() { return ATARI800; }
emulators getEmulatorTypeForSettings() { return ATARI800; }
const char *getEmulatorTypeString(bool) { return "A800"; }
void resetsettings(struct settings *dst) {
    if (!dst) dst = &settings;
    memset(dst, 0, sizeof(*dst));
    dst->version = SETTINGS_VERSION;
    dst->screenMode = ScreenMode::NOSCANLINE_1_1;
    dst->fgcolor = CBLACK; dst->bgcolor = CWHITE;
    dst->flags.audioEnabled = 1;
    dst->selectedRow = STARTROW;
    strcpy(dst->currentDir, "/roms/A800");
}
void savesettings() {
    FIL *f = static_cast<FIL *>(malloc(sizeof(FIL)));
    if (!f) return;
    if (f_open(f, settingsPath, FA_WRITE | FA_CREATE_ALWAYS) == FR_OK) {
        UINT n = 0;
        FRESULT rc = f_write(f, &settings, sizeof(settings), &n);
        f_close(f);
        if (rc != FR_OK || n != sizeof(settings)) printf("[menu] settings write failed\n");
    }
    free(f);
    video_output_set_dvi_mode(settings.flags.useDVIModeForHDMI);
    Frens::restoreScanlines();
}
void loadsettings() {
    resetsettings();
    FIL *f = static_cast<FIL *>(malloc(sizeof(FIL)));
    struct settings *loaded = static_cast<struct settings *>(malloc(sizeof(settings)));
    if (f && loaded && f_open(f, settingsPath, FA_READ) == FR_OK) {
        UINT n = 0;
        if (f_size(f) == sizeof(settings) && f_read(f, loaded, sizeof(settings), &n) == FR_OK &&
            n == sizeof(settings) && loaded->version == SETTINGS_VERSION &&
            loaded->fgcolor < 64 && loaded->bgcolor < 64 && unsigned(loaded->screenMode) < 4 &&
            loaded->scanlineType < 2) {
            settings = *loaded;
            settings.currentDir[FF_MAX_LFN-1] = 0;

            strcpy(settings.currentDir, "/roms/A800");
            settings.firstVisibleRowINDEX = 0; settings.selectedRow = STARTROW;
        }
        f_close(f);
    }
    free(loaded); free(f);
}
}

extern "C" bool fruit_jam_menu_audio_enabled(void) { return settings.flags.audioEnabled; }
extern "C" void fruit_jam_shared_menu_init(void) {
    std::fill(visibility, visibility + MOPT_COUNT, int8_t(-1));
    visibility[MOPT_EXIT_GAME] = visibility[MOPT_RESET_GAME] = 0;
    for (int opt : {MOPT_REBOOT_TO_LOADER, MOPT_SCREENMODE, MOPT_SCANLINE_TYPE,
                    MOPT_AUDIO_ENABLE, MOPT_DISPLAY_MODE, MOPT_FONT_COLOR,
                    MOPT_FONT_BACK_COLOR, MOPT_ENTER_BOOTSEL_MODE, MOPT_CONTROLLER_TEST,
                    MOPT_RECENT_GAMES}) visibility[opt] = 1;
    g_settings_visibility = visibility; g_available_screen_modes = screenModes;
    FrensSettings::loadsettings();
    video_output_set_dvi_mode(settings.flags.useDVIModeForHDMI);
}

static bool loadAtariImage(const char *path, bool remember)
{
    if (!path || !path[0]) return false;

    CARTRIDGE_Remove();
    for (int drive = 1; drive <= 8; ++drive) SIO_Dismount(drive);
    const int type = AFILE_OpenFile(path, TRUE, 1, FALSE);
    if (type == AFILE_ERROR) return false;
    if ((type & 255) == AFILE_ROM && (type >> 8)) {
        CARTRIDGE_Remove();
        snprintf(ErrorMessage, sizeof(ErrorMessage), "Use a typed .car cartridge image");
        return false;
    }
    if (remember) {
        strncpy(currentImagePath, path, sizeof(currentImagePath) - 1);
        currentImagePath[sizeof(currentImagePath) - 1] = 0;
        Frens::Recent::add(path, "A800", 0, 0);
    }
    printf("[menu] loaded %s%s\n", path, remember ? "" : " (reset)");
    return true;
}

extern "C" void fruit_jam_shared_browser(void) {
    char path[FF_MAX_LFN] = {};
    for (;;) {
        menu("Atari 800", ErrorMessage, false, false, ".xex .atr .xfd .car .rom .bin", path);
        ErrorMessage[0] = 0;
        if (loadAtariImage(path, true)) return;
        if (ErrorMessage[0]) continue;
        snprintf(ErrorMessage, sizeof(ErrorMessage), "Cannot load selected Atari image");
    }
}
extern "C" void fruit_jam_shared_settings(void) {
    UI_is_active = TRUE;
    int result = showSettingsMenu(true);
    if (result == 3) fruit_jam_shared_browser();
    if (result == 5 && !loadAtariImage(currentImagePath, false)) {
        snprintf(ErrorMessage, sizeof(ErrorMessage), "Cannot restart current Atari image");
        fruit_jam_shared_browser();
    }
    UI_is_active = FALSE;
    Frens::restoreScanlines();
    ++fruit_jam_menu_epoch;
}
