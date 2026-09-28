# Atari800 for Adafruit Fruit Jam

An Atari 8-bit computer emulator port for the Adafruit Fruit Jam (RP2350), based on [Atari800](https://github.com/atari800/atari800). The firmware provides HSTX HDMI video and audio, SD-card image browsing, USB gamepad input, and Fruit Jam codec audio.

This repository contains emulator and platform source only. It does not contain commercial games, cartridge images, or Atari system ROMs. The bundled open-source AltirraOS replacement is inherited from Atari800. Supply only software you are legally entitled to use.

## Hardware target

- Adafruit Fruit Jam
- `HW_CONFIG=8`
- HSTX HDMI output
- On-board microSD card
- USB host gamepad input
- TLV320 codec output
- UART0 diagnostics on GPIO 44/45 at 115200 baud

## Build

Requirements: Git, CMake, Ninja, and an Arm GNU toolchain compatible with the Raspberry Pi Pico SDK.

```sh
git clone <repository-url>
cd <repository-directory>
./build.sh
```

The UF2 is written to `build/picoAtari800.uf2`. The build script initializes the pinned top-level submodules.

The build is pinned to Pico SDK 2.3.1 and fixed revisions of TinyUSB, Pico-PIO-USB, and `fhoedemakers/pico_shared`. CMake rejects hardware configurations other than Fruit Jam.

## Use

1. Copy the UF2 to the Fruit Jam BOOTSEL drive.
2. Format a microSD card as FAT32.
3. Create `/roms/A800` and add supported Atari program or disk images.
4. Insert the card and power on the board.
5. Choose an image in the browser. Press Select+Start during emulation to open settings.

`Reboot to BOOTSEL` leaves the emulator and exposes the board's UF2 drive. No game content is included.

## Controls

- D-pad: Atari joystick or menu navigation
- A/B: fire or menu accept/back
- Select+Start: emulator settings

## Upstream and licensing

- Port source upstream: `https://github.com/DnCraptor/atari800.git`
- Port baseline: `2c819e7b96dc0a32984b8e9cc554059128a056ac`
- Atari800 and this derived port: GNU GPL v2 or later; see [LICENSE](LICENSE)
- SD-card driver: BSD-3-Clause; see `atari800/drivers/sdcard/LICENSE`
- USB input code derived from `fhoedemakers/pico_shared`: see `atari800/drivers/usb_input/LICENSE.pico_shared`
- Other pinned dependencies retain their licenses in their submodules

## Verification status

The Fruit Jam build and the following hardware behavior have been validated:

- HDMI video and audio
- TLV320 speaker and headphone output, including volume and sustained playback
- SD-card access and ROM browsing
- USB gamepad input, including Select+Start
- Reboot to BOOTSEL
- Sustained frame pacing and audio synchronization

The validated build uses 493,528 of 524,288 bytes of RAM (94.13%) and all 4 KiB of Scratch Y. These margins should be rechecked when adding features or updating dependencies.
