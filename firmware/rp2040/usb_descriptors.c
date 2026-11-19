/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The board on USB: one device, two HID interfaces. 0 is the board's own protocol (64-byte
 * vendor reports, docs/PROTOCOL.md); 1 is a plain gamepad with 38 lamps, for anything else. */
#include <string.h>

#include "pico/unique_id.h"
#include "tusb.h"
#include "u573_proto.h"

#define EP_RAW_OUT 0x01
#define EP_RAW_IN 0x81
#define EP_PAD_IN 0x82

static const tusb_desc_device_t device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0,
    .bDeviceSubClass = 0,
    .bDeviceProtocol = 0,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = U573_USB_VID,
    .idProduct = U573_USB_PID,
    .bcdDevice = 0x0100,
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = 3,
    .bNumConfigurations = 1,
};

const uint8_t *tud_descriptor_device_cb(void) { return (const uint8_t *)&device; }

static const uint8_t raw_report[] = {
    0x06, U573_HID_USAGE_PAGE & 0xff, U573_HID_USAGE_PAGE >> 8, /* Usage Page (vendor) */
    0x09, 0x01,                                                 /* Usage (1) */
    0xa1, 0x01,                                                 /* Collection (Application) */
    0x15, 0x00, 0x26, 0xff, 0x00,                               /*   Logical 0..255 */
    0x75, 0x08, 0x95, U573_REPORT_SIZE,                         /*   64 bytes */
    0x09, 0x02, 0x81, 0x02,                                     /*   Input */
    0x09, 0x03, 0x91, 0x02,                                     /*   Output */
    0x09, 0x04, 0xb1, 0x02,                                     /*   Feature */
    0xc0,
};

static const uint8_t pad_report[] = {
    0x05, 0x01, 0x09, 0x05,             /* Usage Page (Generic Desktop), Usage (Gamepad) */
    0xa1, 0x01,                         /* Collection (Application) */
    0x05, 0x09, 0x19, 0x01, 0x29, 26,   /*   Buttons 1-26 */
    0x15, 0x00, 0x25, 0x01,             /*   Logical 0..1 */
    0x75, 0x01, 0x95, 26, 0x81, 0x02,   /*   26 bits, Input */
    0x75, 0x06, 0x95, 0x01, 0x81, 0x03, /*   6 bits of padding */
    0x05, 0x01,                         /*   Usage Page (Generic Desktop) */
    0x09, 0x30, 0x09, 0x31, 0x09, 0x32, 0x09, 0x33, /* X, Y, Z, Rx */
    0x15, 0x00, 0x26, 0xff, 0x00,       /*   Logical 0..255 */
    0x75, 0x08, 0x95, 0x04, 0x81, 0x02, /*   4 bytes, Input */
    0x05, 0x08, 0x09, 0x4b,             /*   Usage Page (LED), Usage (Generic Indicator) */
    0x15, 0x00, 0x25, 0x01,             /*   Logical 0..1 */
    0x75, 0x01, 0x95, U573_PAD_LAMPS, 0x91, 0x02, /* 38 bits, Output */
    0x75, 0x02, 0x95, 0x01, 0x91, 0x03, /*   2 bits of padding */
    0xc0,
};

const uint8_t *tud_hid_descriptor_report_cb(uint8_t instance) { return instance == 0 ? raw_report : pad_report; }

#define CONFIG_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_INOUT_DESC_LEN + TUD_HID_DESC_LEN)
static const uint8_t configuration[] = {
    /* bus powered, 100 mA: the Pico and the logic; the cabinet's lamps have their own supply */
    TUD_CONFIG_DESCRIPTOR(1, 2, 0, CONFIG_LEN, 0, 100),
    TUD_HID_INOUT_DESCRIPTOR(0, 4, HID_ITF_PROTOCOL_NONE, sizeof raw_report, EP_RAW_OUT, EP_RAW_IN, U573_REPORT_SIZE, 1),
    TUD_HID_DESCRIPTOR(1, 5, HID_ITF_PROTOCOL_NONE, sizeof pad_report, EP_PAD_IN, 16, 1),
};

const uint8_t *tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return configuration;
}

const uint16_t *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    static uint16_t out[1 + 48];
    char serial[2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1];
    const char *s;
    switch (index) {
    case 0:
        out[0] = (uint16_t)(TUSB_DESC_STRING << 8 | 4);
        out[1] = 0x0409;
        return out;
    case 1: s = U573_USB_MANUFACTURER; break;
    case 2: s = U573_USB_PRODUCT; break;
    case 3:
        pico_get_unique_board_id_string(serial, sizeof serial);
        s = serial;
        break;
    case 4: s = "USB_2_573 raw"; break;
    case 5: s = "USB_2_573 gamepad"; break;
    default: return NULL;
    }
    size_t n = strlen(s);
    if (n > 48) n = 48;
    for (size_t k = 0; k < n; k++) out[1 + k] = (uint8_t)s[k];
    out[0] = (uint16_t)(TUSB_DESC_STRING << 8 | (2 * n + 2));
    return out;
}
