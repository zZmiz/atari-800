
#include <cstdlib>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <hardware/watchdog.h>
#include <hardware/clocks.h>
#include <hardware/flash.h>
#include <hardware/uart.h>
#include <hardware/vreg.h>
#include <hardware/structs/watchdog.h>
#include <pico/bootrom.h>
#include <pico/time.h>
#include <pico/stdlib.h>
#include "graphics.h"
#include "psram_compat.h"
#include "fruit_jam_platform.h"
#include "audio.h"
#include "hstx.h"
#include "gamepad.h"
#include "usb_input.h"
#include "tlv320dac3100.h"

extern "C" {
void fruit_jam_shared_menu_init(void);
void fruit_jam_shared_browser(void);
bool fruit_jam_menu_audio_enabled(void);
extern unsigned fruit_jam_menu_epoch;
#include "ui.h"
#include "libatari800/libatari800.h"
#include "libatari800/input.h"
#include "nespad.h"
#include "sound.h"
#include "akey.h"
#include "memory.h"
#include "ff.h"
#include "debug.h"
#include "screen.h"
#include "sound.h"
#include "util.h"
#include "input.h"
#include "statesav.h"
}

extern "C" UBYTE __aligned(4) __screen[Screen_HEIGHT * Screen_WIDTH];
extern "C" int vprintf(const char *format, va_list args);
extern "C" int vsnprintf(char *buffer, size_t size, const char *format,
                           va_list args);

static void telemetry_printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

static char telemetry_pending[640];
static size_t telemetry_pending_length;
static size_t telemetry_pending_offset;

static void telemetry_queue_printf(const char *format, ...)
{

    if (telemetry_pending_offset < telemetry_pending_length)
        return;

    va_list args;
    va_start(args, format);
    int length = vsnprintf(telemetry_pending, sizeof(telemetry_pending),
                           format, args);
    va_end(args);
    if (length < 0) {
        telemetry_pending_length = 0;
        telemetry_pending_offset = 0;
        return;
    }
    telemetry_pending_length = (size_t)length < sizeof(telemetry_pending)
        ? (size_t)length : sizeof(telemetry_pending) - 1u;
    telemetry_pending_offset = 0;
}

static void telemetry_drain(void)
{

    while (telemetry_pending_offset < telemetry_pending_length &&
           uart_is_writable(uart0)) {
        uart_putc_raw(uart0, telemetry_pending[telemetry_pending_offset++]);
    }
}

static input_template_t input_map;
static unsigned int sp = 0;
static bool qPressed = false;
static bool wPressed = false;
static bool ePressed = false;
static bool aPressed = false;
static bool dPressed = false;
static bool zPressed = false;
static bool xPressed = false;
static bool cPressed = false;
static bool _7Pressed = false;
static bool _8Pressed = false;
static bool _9Pressed = false;
static bool _4Pressed = false;
static bool _6Pressed = false;
static bool _1Pressed = false;
static bool _2Pressed = false;
static bool _3Pressed = false;
static bool f1Pressed = false;
static bool f2Pressed = false;
static bool f3Pressed = false;
static bool f4Pressed = false;
static bool delPressed = false;

extern "C" {
bool __time_critical_func(handleScancode)(const uint32_t ps2scancode) {
    if (!ps2scancode) {
        input_map.keychar = 0;
        input_map.select = 0;
        input_map.option = 0;
        input_map.start = 0;
    } else {
        if ((ps2scancode & 0xFF00) == 0xE000) {
            switch ((uint8_t)ps2scancode & 0xFF) {
                case 0x1d:
                    input_map.control |= 2;
                    input_map.trig1 = 1;
                    return true;
                case 0x9d:
                    input_map.control &= ~2;
                    input_map.trig1 = 0;
                    return true;
                case 0x38:
                    input_map.alt |= 2;
                    return true;
                case 0xb8:
                    input_map.alt &= ~2;
                    return true;
            }
        }

        if (((uint8_t)ps2scancode & 0xFF) > 0x80) {
            switch((uint8_t)ps2scancode & 0xFF) {
                case 0xb6: sp &= ~1; input_map.shift = sp; break;
                case 0xaa: sp &= ~2; input_map.shift = sp; break;
                case 0xd3: {
                    input_map.keychar = 0;
                    delPressed = false;
                    break;
                }
                case 0x9d: {
                    input_map.control &= ~1;
                    input_map.trig0 = 0;
                    break;
                }
                case 0xb8:
                    input_map.alt &= ~1;
                    break;
                case 0xbc: {
                    input_map.option = 0;
                    f2Pressed = false;
                    break;
                }
                case 0xbd: {
                    input_map.select = 0;
                    f3Pressed = false;
                    break;
                }
                case 0xbe: {
                    input_map.start = 0;
                    f4Pressed = false;
                    break;
                }
                case 0xc8: {
                    input_map.keychar = 0;
                    input_map.joy1 &= INPUT_STICK_FORWARD;
                    _8Pressed = false;
                    break;
                }
                case 0xcb: {
                    input_map.keychar = 0;
                    input_map.joy1 &= INPUT_STICK_LEFT;
                    _4Pressed = false;
                    break;
                }
                case 0xd0: {
                    input_map.keychar = 0;
                    input_map.joy1 &= INPUT_STICK_BACK;
                    _2Pressed = false;
                    break;
                }
                case 0xcd: {
                    input_map.keychar = 0;
                    input_map.joy1 &= INPUT_STICK_RIGHT;
                    _6Pressed = false;
                    break;
                }
                case 0xc7: {
                    input_map.keychar = 0;
                    input_map.joy1 &= INPUT_STICK_UL;
                    _7Pressed = false;
                    break;
                }
                case 0xc9: {
                    input_map.keychar = 0;
                    input_map.joy1 &= INPUT_STICK_UR;
                    _9Pressed = false;
                    break;
                }
                case 0xcf: {
                    input_map.keychar = 0;
                    input_map.joy1 &= INPUT_STICK_LL;
                    _1Pressed = false;
                    break;
                }
                case 0xd1: {
                    input_map.keychar = 0;
                    input_map.joy1 &= INPUT_STICK_LR;
                    _3Pressed = false;
                    break;
                }
                case 0x90: {
                    input_map.keychar = 0;
                    input_map.joy0 &= INPUT_STICK_UL;
                    qPressed = false;
                    break;
                }
                case 0x91: {
                    input_map.keychar = 0;
                    input_map.joy0 &= INPUT_STICK_FORWARD;
                    wPressed = false;
                    break;
                }
                case 0x92: {
                    input_map.keychar = 0;
                    input_map.joy0 &= INPUT_STICK_UR;
                    ePressed = false;
                    break;
                }
                case 0x9e: {
                    input_map.keychar = 0;
                    input_map.joy0 &= INPUT_STICK_LEFT;
                    aPressed = false;
                    break;
                }
                case 0xa0: {
                    input_map.keychar = 0;
                    input_map.joy0 &= INPUT_STICK_RIGHT;
                    dPressed = false;
                    break;
                }
                case 0xac: {
                    input_map.keychar = 0;
                    input_map.joy0 &= INPUT_STICK_LL;
                    zPressed = false;
                    break;
                }
                case 0xad: {
                    input_map.keychar = 0;
                    input_map.joy0 &= INPUT_STICK_BACK;
                    xPressed = false;
                    break;
                }
                case 0xae: {
                    input_map.keychar = 0;
                    input_map.joy0 &= INPUT_STICK_LR;
                    cPressed = false;
                    break;
                }
                default: input_map.keychar = 0; break;
            }
            return true;
        }
        switch((uint8_t)ps2scancode & 0xFF) {
            case 0x53: {
                input_map.keychar = 127;
                delPressed = true;
                if (input_map.alt && input_map.control) {
                    f_unlink("/.firmware");
                    watchdog_enable(1, true);
                    while (true);

                }
                break;
            }
            case 0x1c: input_map.keychar = '\n'; break;
            case 0x39: input_map.keychar = ' '; break;
            case 0x0b: input_map.keychar = sp ? ')' : '0'; break;
            case 0x02: input_map.keychar = sp ? '!' : '1'; break;
            case 0x03: input_map.keychar = sp ? '@' : '2'; break;
            case 0x04: input_map.keychar = sp ? '#' : '3'; break;
            case 0x05: input_map.keychar = sp ? '$' : '4'; break;
            case 0x06: input_map.keychar = sp ? '%' : '5'; break;
            case 0x07: input_map.keychar = sp ? '^' : '6'; break;
            case 0x08: input_map.keychar = sp ? '&' : '7'; break;
            case 0x09: input_map.keychar = sp ? '*' : '8'; break;
            case 0x0a: input_map.keychar = sp ? '(' : '9'; break;
            case 0x34: input_map.keychar = sp ? '>' : '.'; break;
            case 0x33: input_map.keychar = sp ? '<' : ','; break;
            case 0x27: input_map.keychar = sp ? ':' : ';'; break;
            case 0x35: input_map.keychar = sp ? '?' : '/'; break;
            case 0x2b: input_map.keychar = sp ? '|' : '\\'; break;
            case 0x29: input_map.keychar = sp ? '`' : '~'; break;
            case 0x0c: input_map.keychar = sp ? '_' : '-'; break;
            case 0x0d: input_map.keychar = sp ? '+' : '='; break;
            case 0x1a: input_map.keychar = sp ? '{' : '['; break;
            case 0x1b: input_map.keychar = sp ? '}' : ']'; break;
            case 0x28: input_map.keychar = sp ? '\'' : '"'; break;
            case 0x0f: input_map.keychar = '\t'; break;
            case 0x3a: if(sp & 4) sp &= ~4; else sp |= 4; input_map.shift = sp; break;
            case 0x1d: {
                input_map.control |= 1;
                input_map.trig0 = 1;
                break;
            }
            case 0x38: {
                input_map.alt |= 1;
                break;
            }
            case 0x1e: {
                input_map.keychar = sp ? 'A' : 'a';
                input_map.joy0 |= ~INPUT_STICK_LEFT;
                aPressed = true;
                break;
            }
            case 0x30: input_map.keychar = sp ? 'B' : 'b'; break;
            case 0x2e: {
	            if (input_map.alt) {
                    input_map.keychar = 255;
			        UI_alt_function = UI_MENU_CARTRIDGE;
                    return true;
                }
                input_map.keychar = sp ? 'C' : 'c';
                input_map.joy0 |= ~INPUT_STICK_LR;
                cPressed = true;
                break;
            }
            case 0x20: {
	            if (input_map.alt) {
                    input_map.keychar = 255;
			        UI_alt_function = UI_MENU_DISK;
                    return true;
                }
                input_map.keychar = sp ? 'D' : 'd';
                input_map.joy0 |= ~INPUT_STICK_RIGHT;
                dPressed = true;
                break;
            }
            case 0x12: {
                input_map.keychar = sp ? 'E' : 'e';
                input_map.joy0 |= ~INPUT_STICK_UR;
                ePressed = true;
                break;
            }
            case 0x21: input_map.keychar = sp ? 'F' : 'f'; break;
            case 0x22: input_map.keychar = sp ? 'G' : 'g'; break;
            case 0x23: input_map.keychar = sp ? 'H' : 'h'; break;
            case 0x17: input_map.keychar = sp ? 'I' : 'i'; break;
            case 0x24: input_map.keychar = sp ? 'J' : 'j'; break;
            case 0x25: input_map.keychar = sp ? 'K' : 'k'; break;
            case 0x26:
	            if (input_map.alt) {
                    input_map.keychar = 255;
			        UI_alt_function = UI_MENU_LOADSTATE;
                    return true;
                }
                input_map.keychar = sp ? 'L' : 'l'; break;
            case 0x32: input_map.keychar = sp ? 'M' : 'm'; break;
            case 0x31: input_map.keychar = sp ? 'N' : 'n'; break;
            case 0x18:
	            if (input_map.alt) {
                    input_map.keychar = 255;
			        UI_alt_function = UI_MENU_SOUND;
                    return true;
                }
                input_map.keychar = sp ? 'O' : 'o'; break;
            case 0x19: input_map.keychar = sp ? 'P' : 'p'; break;
            case 0x10: {
	            if (input_map.alt) {
                    input_map.keychar = 255;
			        UI_alt_function = UI_MENU_RESETW;
                    return true;
                }
                input_map.keychar = sp ? 'Q' : 'q';
                input_map.joy0 |= ~INPUT_STICK_UL;
                qPressed = true;
                break;
            }
            case 0x13:
	            if (input_map.alt) {
                    input_map.keychar = 255;
			        UI_alt_function = UI_MENU_RUN;
                    return true;
                }
                input_map.keychar = sp ? 'R' : 'r';
                break;
            case 0x1f:
	            if (input_map.alt) {
                    input_map.keychar = 255;
			        UI_alt_function = UI_MENU_SAVESTATE;
                    return true;
                }
                input_map.keychar = sp ? 'S' : 's'; break;
            case 0x14:
	            if (input_map.alt) {
                    input_map.keychar = 255;
			        UI_alt_function = UI_MENU_CASSETTE;
                    return true;
                }
                input_map.keychar = sp ? 'T' : 't'; break;
            case 0x16: input_map.keychar = sp ? 'U' : 'u'; break;
            case 0x2f: input_map.keychar = sp ? 'V' : 'v'; break;
            case 0x11: {
                input_map.keychar = sp ? 'W' : 'w';
                input_map.joy0 |= ~INPUT_STICK_FORWARD;
                wPressed = true;
                break;
            }
            case 0x2d: {
                input_map.keychar = sp ? 'X' : 'x';
                input_map.joy0 |= ~INPUT_STICK_BACK;
                xPressed = true;
                break;
            }
            case 0x15:
	            if (input_map.alt) {
                    input_map.keychar = 255;
			        UI_alt_function = UI_MENU_SETTINGS;
                    return true;
                }
                input_map.keychar = sp ? 'Y' : 'y'; break;
            case 0x2c: {
                input_map.keychar = sp ? 'Z' : 'z';
                input_map.joy0 |= ~INPUT_STICK_LL;
                zPressed = true;
                break;
            }
            case 0x36: sp |= 1; input_map.shift = sp; break;
            case 0x2a: sp |= 2; input_map.shift = sp; break;
            case 0x01: input_map.keychar = 27; break;
            case 0x0e: input_map.keychar = '\b'; break;
            case 0x3b: {
                if (input_map.control) {
                    StateSav_SaveAtariState("\\atari800\\~f1.sav", FA_CREATE_ALWAYS | FA_WRITE | FA_READ, TRUE);
                    return true;
                }
                if (input_map.shift) {
                    StateSav_ReadAtariState("\\atari800\\~f1.sav", FA_READ);
                    return true;
                }
                input_map.keychar = 255;
                f1Pressed = true;
                break;
            }
            case 0x3c: {
                if (input_map.control) {
                    StateSav_SaveAtariState("\\atari800\\~f2.sav", FA_CREATE_ALWAYS | FA_WRITE | FA_READ, TRUE);
                    return true;
                }
                if (input_map.shift) {
                    StateSav_ReadAtariState("\\atari800\\~f2.sav", FA_READ);
                    return true;
                }
                input_map.option = 1;
                f2Pressed = true;
                break;
            }
            case 0x3d: {
                if (input_map.control) {
                    StateSav_SaveAtariState("\\atari800\\~f3.sav", FA_CREATE_ALWAYS | FA_WRITE | FA_READ, TRUE);
                    return true;
                }
                if (input_map.shift) {
                    StateSav_ReadAtariState("\\atari800\\~f3.sav", FA_READ);
                    return true;
                }
                input_map.select = 1;
                f3Pressed = true;
                break;
            }
            case 0x3e: {
                if (input_map.control) {
                    StateSav_SaveAtariState("\\atari800\\~f4.sav", FA_CREATE_ALWAYS | FA_WRITE | FA_READ, TRUE);
                    return true;
                }
                if (input_map.shift) {
                    StateSav_ReadAtariState("\\atari800\\~f4.sav", FA_READ);
                    return true;
                }
                input_map.start = 1;
                f4Pressed = true;
                break;
            }
            case 0x3f:
                if (input_map.control) {
                    StateSav_SaveAtariState("\\atari800\\~f5.sav", FA_CREATE_ALWAYS | FA_WRITE | FA_READ, TRUE);
                    return true;
                }
                if (input_map.shift) {
                    StateSav_ReadAtariState("\\atari800\\~f5.sav", FA_READ);
                    return true;
                }
                input_map.keychar = 250;
                break;
            case 0x40:
                if (input_map.control) {
                    StateSav_SaveAtariState("\\atari800\\~f6.sav", FA_CREATE_ALWAYS | FA_WRITE | FA_READ, TRUE);
                    return true;
                }
                if (input_map.shift) {
                    StateSav_ReadAtariState("\\atari800\\~f6.sav", FA_READ);
                    return true;
                }
                break;
            case 0x41:
                if (input_map.control) {
                    StateSav_SaveAtariState("\\atari800\\~f7.sav", FA_CREATE_ALWAYS | FA_WRITE | FA_READ, TRUE);
                    return true;
                }
                if (input_map.shift) {
                    StateSav_ReadAtariState("\\atari800\\~f7.sav", FA_READ);
                    return true;
                }
                break;
            case 0x42:
                if (input_map.control) {
                    StateSav_SaveAtariState("\\atari800\\~f8.sav", FA_CREATE_ALWAYS | FA_WRITE | FA_READ, TRUE);
                    return true;
                }
                if (input_map.shift) {
                    StateSav_ReadAtariState("\\atari800\\~f8.sav", FA_READ);
                    return true;
                }
                break;
            case 0x43:
                if (input_map.control) {
                    StateSav_SaveAtariState("\\atari800\\~f9.sav", FA_CREATE_ALWAYS | FA_WRITE | FA_READ, TRUE);
                    return true;
                }
                if (input_map.shift) {
                    StateSav_ReadAtariState("\\atari800\\~f9.sav", FA_READ);
                    return true;
                }
                break;
            case 0x44:
                if (input_map.control) {
                    StateSav_SaveAtariState("\\atari800\\~f10.sav", FA_CREATE_ALWAYS | FA_WRITE | FA_READ, TRUE);
                    return true;
                }
                if (input_map.shift) {
                    StateSav_ReadAtariState("\\atari800\\~f10.sav", FA_READ);
                    return true;
                }
                break;
            case 0x57:
                if (input_map.control) {
                    StateSav_SaveAtariState("\\atari800\\~f11.sav", FA_CREATE_ALWAYS | FA_WRITE | FA_READ, TRUE);
                    return true;
                }
                if (input_map.shift) {
                    StateSav_ReadAtariState("\\atari800\\~f11.sav", FA_READ);
                    return true;
                }
                break;
            case 0x58:
                if (input_map.control) {
                    StateSav_SaveAtariState("\\atari800\\~f12.sav", FA_CREATE_ALWAYS | FA_WRITE | FA_READ, TRUE);
                    return true;
                }
                if (input_map.shift) {
                    StateSav_ReadAtariState("\\atari800\\~f12.sav", FA_READ);
                    return true;
                }
                break;
            case 0x48: {
                input_map.keychar = 254;
                input_map.joy1 |= ~INPUT_STICK_FORWARD;
                _8Pressed = true;
                break;
            }
            case 0x4b: {
                input_map.keychar = 253;
                input_map.joy1 |= ~INPUT_STICK_LEFT;
                _4Pressed = true;
                break;
            }
            case 0x50: {
                input_map.keychar = 252;
                input_map.joy1 |= ~INPUT_STICK_BACK;
                _2Pressed = true;
                break;
            }
            case 0x4d: {
                input_map.keychar = 251;
                input_map.joy1 |= ~INPUT_STICK_RIGHT;
                _6Pressed = true;
                break;
            }
            case 0x47: {
                input_map.keychar = '7';
                input_map.joy1 |= ~INPUT_STICK_UL;
                _7Pressed = true;
                break;
            }
            case 0x49: {
                input_map.keychar = '9';
                input_map.joy1 |= ~INPUT_STICK_UR;
                _9Pressed = true;
                break;
            }
            case 0x4f: {
                input_map.keychar = '1';
                input_map.joy1 |= ~INPUT_STICK_LL;
                _1Pressed = true;
                break;
            }
            case 0x51: {
                input_map.keychar = '3';
                input_map.joy1 |= ~INPUT_STICK_LR;
                _3Pressed = true;
                break;
            }
            default:
                return true;
        }
        if(input_map.alt) {
            if(input_map.keychar >= 'a' && input_map.keychar <= 'z')
                input_map.keychar -= 'a' - 1;
            else if(input_map.keychar >= 'A' && input_map.keychar <= 'Z')
                input_map.keychar -= 'A' - 1;
        }
    }
    return true;
}
}

static bool controller_ui_chord;
static bool controller_key_active;
static volatile bool launcher_requested;

extern "C" void fruit_jam_request_launcher(void) {
    launcher_requested = true;
}

static uint16_t usb_to_controller(uint32_t buttons) {
    uint16_t pad = 0;
    if (buttons & io::GamePadState::Button::LEFT) pad |= DPAD_LEFT;
    if (buttons & io::GamePadState::Button::RIGHT) pad |= DPAD_RIGHT;
    if (buttons & io::GamePadState::Button::UP) pad |= DPAD_UP;
    if (buttons & io::GamePadState::Button::DOWN) pad |= DPAD_DOWN;
    if (buttons & io::GamePadState::Button::START) pad |= DPAD_START;
    if (buttons & io::GamePadState::Button::SELECT) pad |= DPAD_SELECT;
    if (buttons & io::GamePadState::Button::A) pad |= DPAD_A;
    if (buttons & io::GamePadState::Button::B) pad |= DPAD_B;
    if (buttons & io::GamePadState::Button::X) pad |= DPAD_SNES_X;
    if (buttons & io::GamePadState::Button::Y) pad |= DPAD_SNES_A;
    return pad;
}

static uint8_t controller_to_atari_stick(uint16_t pad) {
    uint8_t stick = 0;
    if (pad & DPAD_LEFT) stick |= (uint8_t)~INPUT_STICK_LEFT;
    if (pad & DPAD_RIGHT) stick |= (uint8_t)~INPUT_STICK_RIGHT;
    if (pad & DPAD_UP) stick |= (uint8_t)~INPUT_STICK_FORWARD;
    if (pad & DPAD_DOWN) stick |= (uint8_t)~INPUT_STICK_BACK;
    return stick;
}

static bool controller_fire_pressed(uint16_t pad) {

    return (pad & (DPAD_A | DPAD_B | DPAD_SNES_A | DPAD_SNES_X)) != 0;
}

extern "C" void fruit_jam_poll_controller(void) {
    usb_input_task();
    nespad_read_start();
    busy_wait_us_32(250);
    nespad_read_finish();
    const uint16_t pad = nespad_states_ext[0] |
                         usb_to_controller(usb_input_buttons(0));
    const uint16_t pad2 = nespad_states_ext[1] |
                          usb_to_controller(usb_input_buttons(1));
    const bool ui_chord = (pad & (DPAD_START | DPAD_SELECT)) ==
                          (DPAD_START | DPAD_SELECT);

    input_map.joy0 = controller_to_atari_stick(pad);
    input_map.trig0 = controller_fire_pressed(pad);
    input_map.joy1 = controller_to_atari_stick(pad2);
    input_map.trig1 = controller_fire_pressed(pad2);
    input_map.start = 0;
    input_map.select = 0;

    if (UI_is_active) {
        uint8_t key = 0;
        if (pad & DPAD_UP) key = 254;
        else if (pad & DPAD_DOWN) key = 252;
        else if (pad & DPAD_LEFT) key = 253;
        else if (pad & DPAD_RIGHT) key = 251;
        else if (pad & DPAD_A) key = '\n';
        else if (pad & DPAD_B) key = 27;
        input_map.keychar = key;
        controller_key_active = key != 0;
    }
    else {
        if (controller_key_active)
            input_map.keychar = 0;
        controller_key_active = false;
        input_map.start = (pad & DPAD_START) != 0;
        input_map.select = (pad & DPAD_SELECT) != 0;
        if (ui_chord && !controller_ui_chord) {
            input_map.keychar = 255;
            controller_key_active = true;
        }
    }
    controller_ui_chord = ui_chord;
}

static i2s_config_t i2s_output;
static bool i2s_ready;
static int16_t pcm_frames[2048] __attribute__((aligned(4)));
static constexpr uint32_t AUDIO_OUTPUT_HZ = 44100;
static constexpr uint32_t VIDEO_OUTPUT_HZ = 60;
static constexpr uint32_t AUDIO_FRAMES_PER_VIDEO_FRAME =
    AUDIO_OUTPUT_HZ / VIDEO_OUTPUT_HZ;
static int32_t audio_frame_balance;
static uint32_t audio_duplicate_frames;
static int16_t audio_previous_left;
static int16_t audio_previous_right;
static bool audio_previous_valid;
static uint16_t audio_gain_q8 = 3u * 256u;
static uint16_t audio_window_input_peak;
static uint16_t audio_window_peak;
static uint32_t audio_active_blocks;
extern "C" uint32_t fruit_jam_synth_us;
static uint32_t audio_output_us;

static constexpr const char *ATARI_AUDIO_SOURCE_HZ = "22021";

extern "C" UBYTE *LIBATARI800_Sound_array = 0;
extern "C" unsigned int sound_array_fill;

extern "C" void PLATFORM_SoundWrite(UBYTE const *buffer, unsigned int size) {
    const uint32_t output_start = time_us_32();
    const unsigned channels = (unsigned)libatari800_get_num_sound_channels();
    const unsigned sample_bytes = (unsigned)libatari800_get_sound_sample_size();
    unsigned frames = channels && sample_bytes
        ? size / (channels * sample_bytes) : 0;
    if (frames > 512)
        frames = 512;

    if (sample_bytes != sizeof(int16_t))
        panic("Atari sound is not 16-bit PCM");
    const int16_t *source = reinterpret_cast<const int16_t *>(buffer);

    if (LIBATARI800_Sound_array && LIBATARI800_Sound_array != buffer)
        memcpy(LIBATARI800_Sound_array, buffer, size);
    sound_array_fill = size;

    unsigned source_peak = 0;
    bool block_has_audio = false;
    unsigned output_frames = 0;
    for (unsigned i = 0; i < frames; ++i) {
        const int left = source[i * channels];
        const int right = channels > 1
            ? source[i * channels + 1]
            : left;
        const unsigned left_peak = left < 0 ? (unsigned)-left : (unsigned)left;
        const unsigned right_peak = right < 0 ? (unsigned)-right : (unsigned)right;
        const unsigned peak = left_peak > right_peak ? left_peak : right_peak;
        if (peak > source_peak)
            source_peak = peak;
        if (peak > 512u)
            block_has_audio = true;
    }
    if (source_peak > audio_window_input_peak)
        audio_window_input_peak = (uint16_t)source_peak;
    unsigned safe_gain_q8 = source_peak
        ? (32000u * 256u) / source_peak : 3u * 256u;
    if (safe_gain_q8 > 3u * 256u)
        safe_gain_q8 = 3u * 256u;
    if (audio_gain_q8 > safe_gain_q8)
        audio_gain_q8 = (uint16_t)safe_gain_q8;
    else if (audio_gain_q8 < safe_gain_q8) {
        unsigned released_gain = (unsigned)audio_gain_q8 + 16u;
        audio_gain_q8 = (uint16_t)(released_gain < safe_gain_q8
            ? released_gain : safe_gain_q8);
    }
    for (unsigned i = 0; i < frames; ++i) {
        const int left = source[i * channels];
        const int right = channels > 1
            ? source[i * channels + 1]
            : left;
        const int16_t output_left = (int16_t)(left * audio_gain_q8 / 256);
        const int16_t output_right = (int16_t)(right * audio_gain_q8 / 256);

        pcm_frames[output_frames * 2] = audio_previous_valid
            ? (int16_t)(((int)audio_previous_left + output_left) / 2)
            : output_left;
        pcm_frames[output_frames * 2 + 1] = audio_previous_valid
            ? (int16_t)(((int)audio_previous_right + output_right) / 2)
            : output_right;
        ++output_frames;
        pcm_frames[output_frames * 2] = output_left;
        pcm_frames[output_frames * 2 + 1] = output_right;
        ++output_frames;
        audio_previous_left = output_left;
        audio_previous_right = output_right;
        audio_previous_valid = true;
        const unsigned left_peak = output_left < 0
            ? (unsigned)-output_left : (unsigned)output_left;
        const unsigned right_peak = output_right < 0
            ? (unsigned)-output_right : (unsigned)output_right;
        const unsigned peak = left_peak > right_peak ? left_peak : right_peak;
        if (peak > audio_window_peak)
            audio_window_peak = (uint16_t)peak;
    }
    if (block_has_audio)
        ++audio_active_blocks;

    audio_frame_balance +=
        (int32_t)AUDIO_FRAMES_PER_VIDEO_FRAME - (int32_t)output_frames;
    while (audio_frame_balance > 0 && output_frames < 1024u && frames > 0) {
        pcm_frames[output_frames * 2] = pcm_frames[(output_frames - 1u) * 2];
        pcm_frames[output_frames * 2 + 1u] =
            pcm_frames[(output_frames - 1u) * 2 + 1u];
        output_frames++;
        audio_frame_balance--;
        audio_duplicate_frames++;
    }

    if (!fruit_jam_menu_audio_enabled())
        memset(pcm_frames, 0, output_frames * 2 * sizeof(int16_t));
    if (i2s_ready && output_frames)
        i2s_dma_write_count(&i2s_output, pcm_frames, output_frames);
    for (unsigned i = 0; i < output_frames; ++i)
        hstx_push_audio_sample(pcm_frames[i * 2], pcm_frames[i * 2 + 1]);
    audio_output_us += time_us_32() - output_start;
}

#include "f_util.h"
static FATFS fatfs;
bool SD_CARD_AVAILABLE = false;
static void init_fs() {
    FRESULT result = f_mount(&fatfs, "", 1);
    if (FR_OK != result) {
        telemetry_printf("Unable to mount SD-card: %s (%d)\n",
                         FRESULT_str(result), result);
    } else {
        SD_CARD_AVAILABLE = true;
    }
}

extern "C" {
extern char __end__;
extern char __HeapLimit;
void *_sbrk(ptrdiff_t increment);
}

static uint32_t runtime_heap_free_bytes() {
    const uintptr_t heap_end = reinterpret_cast<uintptr_t>(&__HeapLimit);
    uintptr_t heap_break = reinterpret_cast<uintptr_t>(_sbrk(0));
    const uintptr_t heap_start = reinterpret_cast<uintptr_t>(&__end__);
    if (heap_break < heap_start || heap_break > heap_end)
        heap_break = heap_start;
    return static_cast<uint32_t>(heap_end - heap_break);
}

[[noreturn]] static void return_to_launcher() {
    telemetry_printf("[system] rebooting to BOOTSEL\n");
    reset_usb_boot(0, 0);
    while (true)
        tight_loop_contents();
}

extern "C" void fruit_jam_return_to_launcher_now(void) { return_to_launcher(); }

int main() {
    const uint64_t chip_entry_us = time_us_64();
    vreg_set_voltage(VREG_VOLTAGE_1_20);
    sleep_ms(10);
    set_sys_clock_khz(252 * KHZ, true);
    const uint32_t sys_hz = clock_get_hz(clk_sys);
    const bool hstx_clock_ok = clock_configure(
        clk_hstx,
        0,
        CLOCKS_CLK_HSTX_CTRL_AUXSRC_VALUE_CLK_SYS,
        sys_hz,
        126 * MHZ);
    stdio_init_all();
    if (!hstx_clock_ok)
        panic("Unable to configure 126 MHz HSTX clock");
    telemetry_printf("[boot] app_entry_ms=%llu sys_mhz=%lu clean_resume=%u\n",
                     (unsigned long long)(chip_entry_us / 1000u),
                     (unsigned long)(clock_get_hz(clk_sys) / 1000000u),
                     watchdog_enable_caused_reboot() ? 1u : 0u);
    const uint32_t controller_clock_khz = clock_get_hz(clk_sys) / 1000;
#if NES_GPIO_CLK >= 0
    nespad_begin(0, controller_clock_khz,
                 NES_GPIO_CLK, NES_GPIO_DATA, NES_GPIO_LAT, pio1);
#endif
#if NES_GPIO_CLK_1 >= 0
    nespad_begin(1, controller_clock_khz,
                 NES_GPIO_CLK_1, NES_GPIO_DATA_1, NES_GPIO_LAT_1, pio1);
#endif

    graphics_set_buffer(__screen, Screen_WIDTH, Screen_HEIGHT);
    graphics_set_textbuffer(__screen);
    graphics_init();
    telemetry_printf("[boot] hdmi_ready_ms=%llu\n",
                     (unsigned long long)(time_us_64() / 1000u));

    usb_input_init();

    init_fs();
    init_psram();
    telemetry_printf("[boot] storage_ready_ms=%llu sd=%u\n",
                     (unsigned long long)(time_us_64() / 1000u),
                     SD_CARD_AVAILABLE ? 1u : 0u);

    char *atari_args[] = {
        "-atari",
        "-ntsc",
        "-800-rev",
        "altirra",
        "-dsprate",
        const_cast<char *>(ATARI_AUDIO_SOURCE_HZ),
        "-atari_files",
        "\\roms\\A800",
        NULL,
    };
    telemetry_printf("Fruit Jam Atari800: init AltirraOS and ROM browser\n");
    if (!libatari800_init(-1, atari_args)) {
        while (true)
            tight_loop_contents();
    }
    libatari800_clear_input_array(&input_map);
    const uint64_t core_ready_us = time_us_64();
    telemetry_printf("[boot] core_ready_ms=%llu app_init_ms=%llu sound=%dHz/%dch/%dbit\n",
                     (unsigned long long)(core_ready_us / 1000u),
                     (unsigned long long)((core_ready_us - chip_entry_us) / 1000u),
                     libatari800_get_sound_frequency(),
                     libatari800_get_num_sound_channels(),
                     libatari800_get_sound_sample_size() * 8);

#ifdef SOUND
    const bool codec_ready = tlv320_init();
    if (codec_ready)
        tlv320_set_volume(0);
    i2s_output = i2s_get_default_config();
    i2s_output.sample_freq = AUDIO_OUTPUT_HZ;
    i2s_output.channel_count = 2;
    i2s_output.clock_pin_base = PICO_AUDIO_I2S_CLOCK_PIN_BASE;
    i2s_output.data_pin = PICO_AUDIO_I2S_DATA_PIN;
    i2s_output.pio = pio1;
    i2s_output.dma_trans_count = 2048;
    i2s_ready = i2s_init(&i2s_output);
    telemetry_printf("[audio] codec=%s i2s=%s rate=%lu source=%d ring=%u dma=%u/%u sm=%u "
                     "pins=%u/%u/%u\n",
                     codec_ready ? "ready" : "disabled",
                     i2s_ready ? "ready" : "disabled",
                     (unsigned long)i2s_output.sample_freq,
                     libatari800_get_sound_frequency(),
                     (unsigned)i2s_output.dma_trans_count,
                     (unsigned)i2s_output.dma_channel,
                     (unsigned)i2s_output.dma_companion_channel,
                     (unsigned)i2s_output.sm,
                     (unsigned)i2s_output.clock_pin_base,
                     (unsigned)(i2s_output.clock_pin_base + 1u),
                     (unsigned)i2s_output.data_pin);
#endif

    hstx_audio_stats_t hdmi_audio_stats = {};
    hstx_get_audio_stats(&hdmi_audio_stats);
    telemetry_printf("[audio] hdmi=ready rate=%lu raw=%u di=%u\n",
                     (unsigned long)pico_hdmi_get_audio_sample_rate(),
                     (unsigned)HSTX_AUDIO_RAW_RING_SIZE,
                     (unsigned)DI_RING_BUFFER_SIZE);
    uint32_t last_hdmi_silence_packets = hdmi_audio_stats.silence_packets;

    telemetry_printf("[boot] rom_browser_ready_ms=%llu\n",
                     (unsigned long long)(time_us_64() / 1000u));

    LIBATARI800_Input_array = &input_map;
    fruit_jam_shared_menu_init();
    fruit_jam_shared_browser();
    libatari800_clear_input_array(&input_map);

    uint64_t window_start_us = time_us_64();
    uint64_t frame_total_us = 0;
    uint64_t active_total_us = 0;
    uint32_t frame_max_us = 0;
    uint32_t active_max_us = 0;
    uint32_t window_frames = 0;
    uint32_t window_error_frames = 0;
    uint32_t window_late_frames = 0;
    int window_last_error = 0;
    bool first_frame = true;

    uint64_t stage_total[5] = {};
    uint32_t stage_max[5] = {}, stage_worst[5] = {};
    while(true) {

        const uint32_t frame_start_us = time_us_32();
        const unsigned menu_epoch = fruit_jam_menu_epoch;
        fruit_jam_synth_us = audio_output_us = 0;
        libatari800_next_frame(&input_map);
        if (menu_epoch != fruit_jam_menu_epoch) {

            window_start_us = time_us_64();
            frame_total_us = active_total_us = 0;
            frame_max_us = active_max_us = window_frames = window_error_frames = window_late_frames = 0;
            window_last_error = 0;
            memset(stage_total, 0, sizeof(stage_total));
            memset(stage_max, 0, sizeof(stage_max));
            memset(stage_worst, 0, sizeof(stage_worst));
            continue;
        }
        if (libatari800_error_code) {
            ++window_error_frames;
            window_last_error = libatari800_error_code;
        }
        const uint32_t frame_us = time_us_32() - frame_start_us;
        if (frame_us > 16667u) ++window_late_frames;
        uint32_t wait_us = 0;
        uint32_t copy_us = 0;
        fruit_jam_get_present_metrics(&wait_us, &copy_us);
        const uint32_t presentation_us = wait_us + copy_us;
        const uint32_t active_us = frame_us > presentation_us
            ? frame_us - presentation_us : 0;
        const uint32_t accounted = fruit_jam_synth_us + audio_output_us + presentation_us;
        const uint32_t stage[5] = {fruit_jam_synth_us, audio_output_us,
            copy_us, wait_us, frame_us > accounted ? frame_us - accounted : 0};
        for (unsigned i = 0; i < 5; ++i) {
            stage_total[i] += stage[i];
            if (stage[i] > stage_max[i]) stage_max[i] = stage[i];
            if (frame_us > frame_max_us) stage_worst[i] = stage[i];
        }
        frame_total_us += frame_us;
        active_total_us += active_us;
        if (frame_us > frame_max_us)
            frame_max_us = frame_us;
        if (active_us > active_max_us)
            active_max_us = active_us;
        ++window_frames;

        if (first_frame) {
            telemetry_printf("[boot] first_frame_ms=%llu heap_free=%lu\n",
                             (unsigned long long)(time_us_64() / 1000u),
                             (unsigned long)runtime_heap_free_bytes());
            first_frame = false;
        }

        const uint64_t now_us = time_us_64();
        const uint64_t window_us = now_us - window_start_us;
        if (window_us >= 1000000u || window_frames >= 60u) {
            const uint32_t fps100 = (uint32_t)(
                (uint64_t)window_frames * 100000000u / window_us);
            const uint32_t load100 = (uint32_t)(
                active_total_us * 10000u / window_us);
            i2s_stats_t audio_stats = {};
            if (i2s_ready)
                i2s_get_stats(&i2s_output, &audio_stats);
            hstx_get_audio_stats(&hdmi_audio_stats);
            const uint32_t hdmi_silence_delta =
                hdmi_audio_stats.silence_packets - last_hdmi_silence_packets;
            last_hdmi_silence_packets = hdmi_audio_stats.silence_packets;
            telemetry_queue_printf(
                "[perf] f=%lu wall=%lluus late=%lu fps=%lu.%02lu frame=%llu/%luus active=%llu/%luus "
                "load=%lu.%02lu%% heap=%lu audio=%u c=%lu e=%lu b=%lu d=%lu u=%lu "
                "r=%lu g=%lu ix=%lu busy=%u ipk=%u pk=%u gn=%u ab=%lu hdmi=%u/%u/%lu/%lu/%lu err=%lu/%d "
                "sa=%lu/%lu/%lu/%lu/%lu sm=%lu/%lu/%lu/%lu/%lu sw=%lu/%lu/%lu/%lu/%lu\n",
                (unsigned long)window_frames,
                (unsigned long long)window_us,
                (unsigned long)window_late_frames,
                (unsigned long)(fps100 / 100u),
                (unsigned long)(fps100 % 100u),
                (unsigned long long)(frame_total_us / window_frames),
                (unsigned long)frame_max_us,
                (unsigned long long)(active_total_us / window_frames),
                (unsigned long)active_max_us,
                (unsigned long)(load100 / 100u),
                (unsigned long)(load100 % 100u),
                (unsigned long)runtime_heap_free_bytes(),
                (unsigned)audio_stats.fill_frames,
                (unsigned long)audio_duplicate_frames,
                (unsigned long)audio_stats.samples_enqueued,
                (unsigned long)audio_stats.dma_blocks,
                (unsigned long)audio_stats.samples_dropped,
                (unsigned long)audio_stats.underruns,
                (unsigned long)audio_stats.restarts,
                (unsigned long)audio_stats.clock_gaps,
                (unsigned long)audio_stats.max_irq_interval_us,
                audio_stats.dma_busy ? 1u : 0u,
                (unsigned)audio_window_input_peak,
                (unsigned)audio_window_peak,
                (unsigned)audio_gain_q8,
                (unsigned long)audio_active_blocks,
                (unsigned)hdmi_audio_stats.raw_fill,
                (unsigned)hdmi_audio_stats.di_level,
                (unsigned long)hdmi_audio_stats.samples_dropped,
                (unsigned long)hdmi_silence_delta,
                (unsigned long)hdmi_audio_stats.packets_queued,
                (unsigned long)window_error_frames,
                window_last_error,
                (unsigned long)(stage_total[0]/window_frames), (unsigned long)(stage_total[1]/window_frames),
                (unsigned long)(stage_total[2]/window_frames), (unsigned long)(stage_total[3]/window_frames),
                (unsigned long)(stage_total[4]/window_frames),
                (unsigned long)stage_max[0], (unsigned long)stage_max[1], (unsigned long)stage_max[2],
                (unsigned long)stage_max[3], (unsigned long)stage_max[4],
                (unsigned long)stage_worst[0], (unsigned long)stage_worst[1], (unsigned long)stage_worst[2],
                (unsigned long)stage_worst[3], (unsigned long)stage_worst[4]);
            window_start_us = now_us;
            frame_total_us = 0;
            active_total_us = 0;
            frame_max_us = 0;
            active_max_us = 0;
            window_frames = 0;
            window_error_frames = 0;
            window_late_frames = 0;
            window_last_error = 0;
            audio_window_input_peak = 0;
            audio_window_peak = 0;
            audio_active_blocks = 0;
            memset(stage_total, 0, sizeof(stage_total));
            memset(stage_max, 0, sizeof(stage_max));
            memset(stage_worst, 0, sizeof(stage_worst));
        }
        if (launcher_requested)
            return_to_launcher();
        telemetry_drain();
        tight_loop_contents();
    }

    __unreachable();
}
