/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef U573_TUSB_CONFIG_H
#define U573_TUSB_CONFIG_H

#define CFG_TUSB_RHPORT0_MODE OPT_MODE_DEVICE
#define CFG_TUD_ENDPOINT0_SIZE 64

#define CFG_TUD_HID 2 /* 0: the raw interface, 1: the gamepad */
#define CFG_TUD_HID_EP_BUFSIZE 64
#define CFG_TUD_CDC 0
#define CFG_TUD_MSC 0
#define CFG_TUD_MIDI 0
#define CFG_TUD_VENDOR 0

#endif
