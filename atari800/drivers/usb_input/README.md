# USB input layer

This directory contains a small TinyUSB host input layer derived from
`fhoedemakers/pico_shared`. See `LICENSE.pico_shared` for its GPL notice.

The Fruit Jam build uses the board definition supplied by the pinned TinyUSB
checkout. HID and XInput reports are normalized into two emulator gamepad
slots.
