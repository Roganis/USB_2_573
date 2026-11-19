# The board on USB

One full-speed USB device with two HID interfaces. No driver is installed on Windows, Linux or macOS.

| Interface | What it is | For |
|---|---|---|
| 0, "USB_2_573 raw" | Vendor-defined HID (usage page `0xff73`), 64-byte input, output and feature reports, polled every millisecond | A host that knows the board: HIRAKU's `--usb573` |
| 1, "USB_2_573 gamepad" | A HID gamepad: 26 buttons, 4 axes, 38 lamps | Anything else: MAME, StepMania, a joystick tester |

The constants are in `firmware/core/u573_proto.h`, which a host can include as it is. Multi-byte fields are little-endian.

**IDs.** `1209:0001` is pid.codes' test ID, which any project may use during development. A host must also check the product string, which starts with `USB_2_573`, and take the interface whose usage page is `0xff73` (or interface 0). Before boards are handed out, the board needs a product ID of its own from [pid.codes](https://pid.codes).

## Switches

One bit each, 1 = closed (the pin is pulled to ground). Bits 0–15 are the System 573's register `0x1f400008` inverted.

| Bit | Switch | JAMMA pin | Bit | Switch | JAMMA pin |
|---|---|---|---|---|---|
| 0 | Player 2 left | X | 8 | Player 1 left | 20 |
| 1 | Player 2 right | Y | 9 | Player 1 right | 21 |
| 2 | Player 2 up | V | 10 | Player 1 up | 18 |
| 3 | Player 2 down | W | 11 | Player 1 down | 19 |
| 4 | Player 2 button 1 | Z | 12 | Player 1 button 1 | 22 |
| 5 | Player 2 button 2 | a | 13 | Player 1 button 2 | 23 |
| 6 | Player 2 button 3 | b | 14 | Player 1 button 3 | 24 |
| 7 | Player 2 start | U | 15 | Player 1 start | 17 |

| Bit | Switch | Pin |
|---|---|---|
| 16, 17, 18 | Player 1 buttons 4, 5, 6 | JAMMA 25, 26 (and EXT-IN 3, 4); EXT-IN 5 |
| 19, 20, 21 | Player 2 buttons 4, 5, 6 | JAMMA c, d (and EXT-IN 7, 8); EXT-IN 9 |
| 22, 23 | Coin switches 1, 2 | JAMMA 16, T |
| 24 | Service | JAMMA R |
| 25 | Test | JAMMA 15, and the board's own TEST button |
| 26–29 | The board's option switches 1–4 | (no meaning yet) |

**Debounce.** A change is reported at once; that switch is then left alone for the debounce time (2 ms unless the host changes it), so a press costs no time and the contacts' bounce after it isn't seen. The switches are read 4000 times a second.

**No lost taps.** A switch that closed and opened again between two reports is still reported closed, once, in each interface's next report.

## Outputs

A 48-bit state.

| Bits | Output | 1 means |
|---|---|---|
| 0–7 | A0–A7 | the output sinks to its bank's common: a lamp wired to it is lit |
| 8–15 | B0–B7 | the same |
| 16–23 | C0–C7 | the same |
| 24–27 | D0–D3 | the same |
| 32–39 | EXT-OUT OUT0–OUT7 | the pin is at 5 V |
| 40, 41 | Coin counters 1, 2 | the coil is energised |

All other bits are ignored.

Three things turn outputs off without the host:
- **Power-on and reset:** everything is off until a host says otherwise (the output drivers are disabled in hardware until the firmware has cleared them).
- **The failsafe:** once a host has spoken to the raw interface, a second without any raw output report turns everything off and empties the queue. A host sends a keepalive when it has nothing else to say.
- **A coin counter held for more than a second** is let go by the board, and stays off until the host has let go of it too.

When the USB bus is suspended or unplugged, everything is off.

## The raw interface

### Input report (board to host), every millisecond

| Byte | Field |
|---|---|
| 0 | `0x73` |
| 1 | Protocol version: 1 |
| 2 | Status: bit 0 a raw output report came within the failsafe time; bit 1 output entries were dropped because the queue was full (until a reset); bit 2 a coin counter was let go by the board (until a reset) |
| 3 | A counter, one more each report |
| 4–7 | The switches |
| 8–11 | ANALOG CH0–CH3, 0–255 for 0–5 V (as the 573's ADC0834 reads them) |
| 12–13 | Output entries applied since the last reset (wraps at 65536) |
| 14 | Output entries waiting in the queue |
| 15 | The queue's size (128) |
| 16–19 | The board's clock at the report, microseconds |
| 20–25 | The outputs as they are driven now |
| 26–63 | 0 |

### Output report (host to board)

| Byte | Field |
|---|---|
| 0 | `0x53` |
| 1 | Command |
| 2 | `OUTPUTS`: the number of entries, 0–7 |
| 3 | `OUTPUTS`: flags. Bit 0: drop what is still queued first |
| 4–7 | 0 |
| 8– | The command's data |

| Command | Data | What it does |
|---|---|---|
| `0x00` KEEPALIVE | none | Nothing, but the host is there (as every valid report says) |
| `0x01` OUTPUTS | entries of 8 bytes | Queues output states |
| `0x02` CONFIG | u16 debounce (µs), u16 failsafe (ms, at least 100), u16 coin counter limit (ms, at least 10) | Until power-off |
| `0x03` RESET | none | Queue emptied, outputs off, the applied count and the status flags cleared |
| `0x7e` BOOTLOADER | `BOOT` | Outputs off; the board restarts as its USB bootloader (a drive that takes a `.uf2` file) |

A report that doesn't start with `0x53`, or with a command not listed, is ignored.

**An output entry** is a u16 delay and the 48-bit state. The state goes onto the pins `delay` × 10 µs after the entry before it did, and never sooner than it arrives. With a delay of 0, every entry still reaches the pins, one after the other. The board never merges entries.

This is what a Dance Dance Revolution cabinet needs: the game talks to each stage's board by toggling two light outputs (data and clock), and the edges must arrive one by one, in order, not faster than that board follows. A host sends lamps with a delay of 0 and the stage's edges with a delay.

**Flow control.** The queue holds 128 entries; more are dropped (status bit 1). A host counts what it has sent against bytes 12–13 and keeps the difference under the queue's size.

### Feature report (GET_REPORT)

| Byte | Field |
|---|---|
| 0–3 | `U573` |
| 4 | Protocol version |
| 5–7 | Firmware version: major, minor, patch |
| 8 | Hardware revision |
| 9 | The queue's size |
| 10 | Analog channels fitted (3 on a Raspberry Pi Pico, 4 on a bare RP2040) |
| 12–13, 14–15, 16–17 | Debounce (µs), failsafe (ms), coin counter limit (ms) |
| 20–27 | The board's own number (the RP2040's flash ID) |

## The gamepad interface

**Input, 8 bytes:** buttons 1–26 are switch bits 0–25 (button 9 is player 1 left, 16 is player 1 start, 26 is Test), then 6 bits of padding and four 8-bit axes, X, Y, Z and Rx, for ANALOG CH0–CH3. The joystick directions are buttons, not a hat: on a 573 cabinet those pins are drum pads, guitar picks and floor sensors as often as a joystick.

**Output, 5 bytes:** 38 one-bit lamps (LED page, Generic Indicator) in the order A0–A7, B0–B7, C0–C7, D0–D3, OUT0–OUT7, coin counter 1, coin counter 2. It can be written through the control endpoint (SET_REPORT).

What the gamepad interface lit stays lit until it is written again: programs that drive HID lamps write only when a lamp changes. While a host talks to the raw interface the gamepad's outputs are ignored.

HIRAKU can use this interface without `--usb573`, like any controller with LEDs: bind its buttons in the launcher's Controls and its lamps in Lights.
