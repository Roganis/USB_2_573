/* SPDX-License-Identifier: GPL-3.0-or-later */
/* USB_2_573 on an RP2040 (a Raspberry Pi Pico on the carrier board): which pin does what, and
 * how the shift registers are chained. docs/HARDWARE.md has the schematic's side of this.
 *
 * One SPI bus scans everything: six 74HCT595 take the outputs while four 74HCT165 give the
 * switches, on the same clock.
 */
#ifndef U573_BOARD_H
#define U573_BOARD_H

#include <stdbool.h>
#include <stdint.h>

#define U573_HW_REV 1

#define PIN_UART_TX 0   /* debug messages, 115200 baud */
#define PIN_UART_RX 1
#define PIN_SCK 2       /* SPI0 SCK: the 595s' SRCLK and the 165s' CLK */
#define PIN_MOSI 3      /* SPI0 TX: the first 595's SER */
#define PIN_MISO 4      /* SPI0 RX: the last 165's QH (5 V, through a divider) */
#define PIN_OUT_LATCH 5 /* the 595s' RCLK: a rising edge puts the shifted bits on their pins */
#define PIN_IN_LOAD 6   /* the 165s' SH/LD: low takes the switches in, high shifts */
#define PIN_OUT_ENABLE 7 /* the 595s' /OE, pulled up on the board: the outputs are off until the firmware says */
#define PIN_LED 25      /* the Pico's own LED */
#define PIN_ADC0 26     /* ANALOG CH0-CH2 (5 V scaled to 3.3 V) */
/* A Pico keeps GPIO29 for itself (VSYS): three channels. A bare RP2040 has the fourth. */
#ifndef U573_ANALOG_CHANNELS
#define U573_ANALOG_CHANNELS 3
#endif

#define SPI_HZ 1000000
#define SCAN_PERIOD_US 250 /* the switches are read that often (4 kHz) */

/* The 595 chain, from the RP2040 outwards: OUT1 (A0-A7 on QA-QH), OUT2 (B0-B7), OUT3 (C0-C7),
 * OUT4 (D0-D3, coin counter 1, coin counter 2, two spare), OUT5 (EXT-OUT OUT0-OUT7), OUT6 (the
 * cartridge slot's D4-D7 on QA-QD, the slot's 5 V switch on QE, three spare).
 * The byte for the farthest chip goes first, each byte's top bit first (it ends on QH).
 * The cartridge slot has power while a host drives the board: a cartridge's own lamp drivers
 * (Step Champ's) go dark with the host, as they did with the 573. */
#define CHAIN_BYTES 6
static inline void chain_from_outputs(uint64_t state, bool cart_power, uint8_t tx[CHAIN_BYTES]) {
    tx[0] = (uint8_t)((state >> 28 & 0x0f) | (cart_power ? 0x10 : 0)); /* OUT6: cartridge D4-D7, its 5 V */
    tx[1] = (uint8_t)(state >> 32);                                    /* OUT5: EXT-OUT */
    tx[2] = (uint8_t)((state >> 24 & 0x0f) | (state >> 40 & 3) << 4);  /* OUT4: D0-D3, the counters */
    tx[3] = (uint8_t)(state >> 16);                                    /* OUT3: C */
    tx[4] = (uint8_t)(state >> 8);                                     /* OUT2: B */
    tx[5] = (uint8_t)state;                                            /* OUT1: A */
}

/* The 165 chain, read from the chip next to the RP2040: IN4 (switch bits 24-31 on A-H), IN3
 * (16-23), IN2 (8-15), IN1 (0-7). A closed switch pulls its pin low. */
static inline uint32_t switches_from_chain(const uint8_t rx[CHAIN_BYTES]) {
    return ~((uint32_t)rx[0] << 24 | (uint32_t)rx[1] << 16 | (uint32_t)rx[2] << 8 | rx[3]);   /* (the rest: nothing) */
}

#endif
