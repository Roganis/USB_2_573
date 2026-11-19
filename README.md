# USB_2_573

A board that takes a Konami System 573's place in its arcade cabinet and puts the cabinet on USB, so that a PC running [HIRAKU](https://github.com/Roganis/HIRAKU) plays the cabinet's games on the cabinet itself: Dance Dance Revolution, GuitarFreaks, DrumMania, Dance Maniax, Mambo a GoGo.

```
 cabinet                         USB_2_573                        PC
 ───────                         ─────────                        ──
 JAMMA harness: switches,   ──►  26 inputs    ┐
   coin counters            ◄──  2 drivers    │
 I/O board light plugs      ◄──  28 isolated  ├── USB (HID) ───►  HIRAKU --usb573
 EXT-OUT / EXT-IN / ANALOG  ◄─►  8 out, 4 in  ┘
 amplifier (RCA)            ◄───────────────────────────────────  PC line out
 monitor                    ◄───────────────────────────────────  PC video out
```

The cabinet's harness plugs onto the board as it did onto the 573. The PC needs one USB cable and no set-up: HIRAKU finds the board, gives the game the cabinet's switches on the pins they are wired to, and drives the lights, the coin counters and a DDR cabinet's stage boards with every change the game makes. Nothing is bound per game.

## Where it stands

| Part | State |
|---|---|
| Firmware (`firmware/`) | Written for an RP2040 (Raspberry Pi Pico). Builds to a `.uf2`. Its logic is tested on a PC; **it has not run on hardware yet** |
| HIRAKU's side | `--usb573` in HIRAKU (its `docs/USB573.md`), tested against this firmware's own core; not yet on hardware either |
| Hardware (`docs/HARDWARE.md`) | A design: block diagram, every pin, the parts. **No PCB yet**, and six things to check on a real 573 and cabinet first |
| Sound and picture | Not through the board: the PC's own outputs to the cabinet's amplifier and monitor (`docs/HARDWARE.md`) |

The one real unknown is a DDR cabinet's stage boards, which give their panels only after the game has spoken to them over two light outputs. The board replays the game's side of that dialogue; whether a real stage board accepts it this way has to be tried on one.

## What's here

| | |
|---|---|
| [docs/HARDWARE.md](docs/HARDWARE.md) | The board: what it replaces, block diagram, pins, connectors, parts, what to check first |
| [docs/CABINETS.md](docs/CABINETS.md) | Each cabinet's switches and lamps on the board's pins |
| [docs/PROTOCOL.md](docs/PROTOCOL.md) | The board on USB: both HID interfaces, the reports |
| [docs/BUILDING.md](docs/BUILDING.md) | Building and flashing the firmware, the tests, trying a board |
| `firmware/core/` | The firmware without its hardware: debounce, the timed output queue, the failsafe, the reports |
| `firmware/rp2040/` | The RP2040 around it: USB descriptors, the shift registers, the ADC |
| `firmware/tests/` | The core's tests, on a PC |
| `tools/u573.py` | Watch a board's switches and work its outputs from a PC |

## How it works, briefly

- **Two USB HID interfaces.** A raw one for HIRAKU (64-byte reports, 1000 a second each way) and a plain gamepad with 38 lamps for anything else.
- **Inputs** are read 4000 times a second. A press is reported at once and its bounce ignored; a tap shorter than a report is never lost.
- **Outputs** come as timed entries in a queue, so the order and spacing of edges survive USB: lamps go out at once, a DDR stage's clock and data edges one by one at the pace the host asks.
- **Nothing stays on by accident.** Outputs are off at power-on (in hardware), a second after the host stops talking, and when USB goes away; a coin counter's coil is let go after a second whatever the host says.

## Licence

GPL-3.0-or-later (`LICENSE`). The System 573's pinouts and registers are from psx-spx's Konami System 573 page; the games' input and lamp assignments from MAME's `ksys573.cpp` by way of HIRAKU.

Konami, System 573 and the games' names are trademarks of Konami. This project is not affiliated with or endorsed by Konami.
