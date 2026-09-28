#include "usb_input.h"

#include <cstdio>
#include <hardware/dma.h>
#include <pio_usb.h>
#include <tusb.h>

#include "gamepad.h"

namespace {
bool usb_ready;
bool last_connected[2];

int find_usb_dma_channel()
{
    /* HSTX owns channels 0 and 1.  The USB library claims the selected
     * channel during tusb_init(), so only identify it here. */
    for (int channel = 2; channel < NUM_DMA_CHANNELS; ++channel) {
        if (!dma_channel_is_claimed(channel))
            return channel;
    }
    return -1;
}
}

extern "C" bool usb_input_init(void)
{
    if (usb_ready)
        return true;

#if PICO_DEFAULT_PIO_USB_DP_PIN >= 0
    const int dma_channel = find_usb_dma_channel();
    if (dma_channel < 0) {
        std::printf("USB host disabled: no free DMA channel\n");
        return false;
    }
    pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
    pio_cfg.pin_dp = PICO_DEFAULT_PIO_USB_DP_PIN;
    pio_cfg.pio_tx_num = PIO_USB_USE_PIO;
    pio_cfg.pio_rx_num = PIO_USB_USE_PIO;
    pio_cfg.tx_ch = static_cast<uint8_t>(dma_channel);
    if (!tuh_configure(BOARD_TUH_RHPORT,
                       TUH_CFGID_RPI_PIO_USB_CONFIGURATION,
                       &pio_cfg)) {
        std::printf("USB host disabled: PIO configuration rejected\n");
        return false;
    }
#endif

    tusb_rhport_init_t host_init{};
    host_init.role = TUSB_ROLE_HOST;
    host_init.speed = TUSB_SPEED_AUTO;
    if (!tusb_init(BOARD_TUH_RHPORT, &host_init)) {
        std::printf("USB host disabled: TinyUSB init failed\n");
        return false;
    }

    std::printf("USB host ready: rhport=%d\n", BOARD_TUH_RHPORT);
    usb_ready = true;
    return true;
}

extern "C" void usb_input_task(void)
{
    if (!usb_ready)
        return;

    tuh_task();
    for (unsigned player = 0; player < 2; ++player) {
        const io::GamePadState &pad = io::getCurrentGamePadState(player);
        const bool connected = pad.isConnected();
        if (connected != last_connected[player]) {
            std::printf("USB pad %u: connected=%u\n",
                        player + 1,
                        connected ? 1u : 0u);
            last_connected[player] = connected;
        }
    }
}

extern "C" uint32_t usb_input_buttons(unsigned player)
{
    if (!usb_ready || player >= 2)
        return 0;
    const io::GamePadState &pad = io::getCurrentGamePadState(player);
    return pad.isConnected() ? pad.buttons : 0;
}
