/* SPDX-License-Identifier: GPL-3.0-or-later */
/* USB_2_573 on an RP2040: the hardware around firmware/core. One loop, no interrupts of its
 * own: it scans the shift registers (the switches in, the outputs out), reads the analog
 * channels and moves the USB reports. */
#include <string.h>

#include "u573_board.h"
#include "bsp/board_api.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "hardware/watchdog.h"
#include "pico/bootrom.h"
#include "pico/stdlib.h"
#include "pico/unique_id.h"
#include "tusb.h"
#include "u573_core.h"

static u573_t g_state;
static uint64_t g_drive;   /* what the 595s hold */
static bool g_cart_power;  /* ... and whether the cartridge slot has its 5 V */
static bool g_mounted;

/* One pass over the chains: the switches as they are now come back, `outputs` goes onto the pins */
static uint32_t scan(uint64_t outputs) {
    uint8_t tx[CHAIN_BYTES], rx[CHAIN_BYTES];
    chain_from_outputs(outputs, g_cart_power, tx);
    gpio_put(PIN_IN_LOAD, 0);
    busy_wait_us_32(1);
    gpio_put(PIN_IN_LOAD, 1);
    spi_write_read_blocking(spi0, tx, rx, CHAIN_BYTES);
    gpio_put(PIN_OUT_LATCH, 1);
    busy_wait_us_32(1);
    gpio_put(PIN_OUT_LATCH, 0);
    return switches_from_chain(rx);
}

static void hardware_init(void) {
    /* the outputs stay off (the 595s' /OE is pulled up) until a first scan has cleared them */
    gpio_init(PIN_OUT_ENABLE);
    gpio_put(PIN_OUT_ENABLE, 1);
    gpio_set_dir(PIN_OUT_ENABLE, GPIO_OUT);
    gpio_init(PIN_OUT_LATCH);
    gpio_put(PIN_OUT_LATCH, 0);
    gpio_set_dir(PIN_OUT_LATCH, GPIO_OUT);
    gpio_init(PIN_IN_LOAD);
    gpio_put(PIN_IN_LOAD, 1);
    gpio_set_dir(PIN_IN_LOAD, GPIO_OUT);
    gpio_init(PIN_LED);
    gpio_set_dir(PIN_LED, GPIO_OUT);

    spi_init(spi0, SPI_HZ);
    spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);

    adc_init();
    for (int k = 0; k < U573_ANALOG_CHANNELS; k++) adc_gpio_init((uint)(PIN_ADC0 + k));

    scan(0);
    gpio_put(PIN_OUT_ENABLE, 0);
}

/* One channel each millisecond, smoothed a little: 0-255 for 0-5 V (the board scales 5 V to 3.3 V) */
static void analog_task(uint64_t now) {
    static uint64_t next;
    static int channel;
    static uint16_t smooth[4];
    if (!U573_ANALOG_CHANNELS || now < next) return;
    next = now + 1000;
    adc_select_input((uint)channel);
    smooth[channel] = (uint16_t)((smooth[channel] * 3 + adc_read() + 2) / 4);
    channel = (channel + 1) % U573_ANALOG_CHANNELS;
    uint8_t ch[4];
    for (int k = 0; k < 4; k++) ch[k] = (uint8_t)(smooth[k] >> 4);
    u573_set_analog(&g_state, ch);
}

static void usb_task(uint64_t now) {
    tud_task();
    if (!tud_mounted() || tud_suspended()) {
        if (g_mounted) u573_bus_idle(&g_state);
        g_mounted = false;
        return;
    }
    g_mounted = true;
    if (tud_hid_n_ready(0)) {
        uint8_t r[U573_REPORT_SIZE];
        u573_raw_in(&g_state, now, r);
        tud_hid_n_report(0, 0, r, sizeof r);
    }
    if (tud_hid_n_ready(1)) {
        /* the gamepad's report goes when it changed, and now and then */
        static uint8_t sent[U573_PAD_IN_SIZE];
        static uint64_t sent_at;
        uint8_t r[U573_PAD_IN_SIZE];
        u573_pad_in(&g_state, r);
        if (memcmp(r, sent, sizeof r) || now - sent_at > 100000) {
            memcpy(sent, r, sizeof r);
            sent_at = now;
            tud_hid_n_report(1, 0, r, sizeof r);
        }
    }
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t type, uint8_t *buffer, uint16_t reqlen) {
    (void)report_id;
    if (instance == 0 && type == HID_REPORT_TYPE_FEATURE) {
        uint8_t info[U573_REPORT_SIZE];
        u573_info(&g_state, info);
        if (reqlen > sizeof info) reqlen = sizeof info;
        memcpy(buffer, info, reqlen);
        return reqlen;
    }
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t type, const uint8_t *buffer, uint16_t size) {
    (void)report_id;
    if (type == HID_REPORT_TYPE_FEATURE) return;
    const uint64_t now = time_us_64();
    if (instance == 0) u573_raw_out(&g_state, now, buffer, size);
    else u573_pad_out(&g_state, now, buffer, size);
}

/* Lit: a host is talking to the raw interface. Slow blink: on USB, no such host. Fast: no USB. */
static void led_task(uint64_t now) {
    const bool on = u573_host_active(&g_state, now) ? true : g_mounted ? (now / 500000) & 1 : (now / 100000) & 1;
    gpio_put(PIN_LED, on);
}

int main(void) {
    board_init();
    hardware_init();
    u573_init(&g_state, time_us_64());
    g_state.hw_rev = U573_HW_REV;
    g_state.analog_channels = U573_ANALOG_CHANNELS;
    pico_unique_board_id_t id;
    pico_get_unique_board_id(&id);
    memcpy(g_state.uid, id.id, sizeof g_state.uid < sizeof id.id ? sizeof g_state.uid : sizeof id.id);

    tusb_rhport_init_t usb = {.role = TUSB_ROLE_DEVICE, .speed = TUSB_SPEED_AUTO};
    tusb_init(0, &usb);
    board_init_after_tusb();
    watchdog_enable(500, true);   /* a firmware that hangs restarts, and a restart turns the outputs off */

    uint64_t next_scan = 0;
    for (;;) {
        uint64_t now = time_us_64();
        watchdog_update();
        usb_task(now);
        /* each output state the host queued goes to the pins in turn, when it is due */
        bool changed = u573_task(&g_state, now, &g_drive);
        const bool power = u573_host_active(&g_state, now);
        if (power != g_cart_power) g_cart_power = power, changed = true;
        if (changed || now >= next_scan) {
            u573_sample(&g_state, now, scan(g_drive));
            next_scan = now + SCAN_PERIOD_US;
        }
        analog_task(now);
        led_task(now);
        if (g_state.bootloader) {
            scan(0);
            gpio_put(PIN_OUT_ENABLE, 1);
            sleep_ms(20);   /* (the report's acknowledgement gets out) */
            reset_usb_boot(0, 0);
        }
    }
}
