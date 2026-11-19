# The hardware

**Status: a design, not a built board.** The firmware runs on this pin assignment and is tested on a PC; no PCB has been laid out and nothing here has been measured on a cabinet. The points that must be checked on a real System 573 and cabinet before a PCB is ordered are listed [at the end](#to-check-on-a-real-573-and-cabinet).

Pinouts of the System 573's connectors and the registers behind them are from psx-spx's [Konami System 573](https://psx-spx.consoledev.net/konamisystem573/) page. The JAMMA edge is the JAMMA standard's, with the 573's button 4 and 5 pins from that page.

## What the board replaces

The System 573 unit is taken out of the cabinet. Everything that was plugged into it goes to the board or to the PC:

| On the 573 | Now |
|---|---|
| JAMMA edge: switches, coin counters | The board's JAMMA edge |
| I/O board light outputs, banks A–D (through the plugs on the 573's front plate) | The board's bank headers |
| EXT-OUT (CN4), EXT-IN (CN5), ANALOG (CN3) | The board's headers of the same pinout |
| JAMMA edge: video; the RGB DB15 | The PC's video output (not through the board) |
| RCA line out; JAMMA speaker pins | The PC's audio output (not through the board, [see below](#sound)) |
| Security cartridge, CD-ROM drive, PCMCIA cards, JVS memory card reader, e-AMUSEMENT units | Emulated in HIRAKU: nothing to connect |

## Block diagram

```
                      ┌────────────────────────────── USB_2_573 ──────────────────────────────┐
                      │                                                                       │
 PC ═══ USB ══════════╪═ Raspberry Pi Pico ──SPI─┬─► 5 × 74HCT595 ─┬─► 28 optocouplers ───────╪─► banks A, B, C, D
      (5 V, data)     │   (RP2040)               │   (40 outputs)  ├─► 8 × 5 V logic ─────────╪─► EXT-OUT
                      │      │                   │                 └─► 2 × ULN2003 channel ───╪─► JAMMA 8, J (coin counters)
                      │      │                   │                                            │
                      │      │                   └─◄ 4 × 74HCT165 ◄── 4.7 kΩ pull-ups, RC ◄───╪── JAMMA switches, EXT-IN,
                      │      │                       (32 inputs)                              │   TEST button, option switches
                      │      └─ ADC ◄── buffer, 5 V → 3.3 V ◄─────────────────────────────────╪── ANALOG CH0–CH3
                      └───────────────────────────────────────────────────────────────────────┘
```

A Raspberry Pi Pico (or any board with its pinout: Pico 2, a USB-C clone) sits on the board as a module. It keeps the PCB two-layer and hand-solderable: nothing finer than SOIC.

## The RP2040's pins

| Pin | Use |
|---|---|
| GP0, GP1 | UART0, debug messages (115200 baud) |
| GP2 | SPI0 SCK: the 595s' SRCLK and the 165s' CLK |
| GP3 | SPI0 TX: the first 595's SER |
| GP4 | SPI0 RX: the last 165's QH, through a 1 kΩ / 2 kΩ divider (5 V → 3.3 V) |
| GP5 | The 595s' RCLK (latch) |
| GP6 | The 165s' SH/LD (load) |
| GP7 | The 595s' /OE, with a 10 kΩ pull-up to 3.3 V |
| GP25 | The Pico's LED: lit while a host drives the board, slow blink on USB without one, fast blink without USB |
| GP26, GP27, GP28 | ADC0–2: ANALOG CH0–CH2 |
| GP29 | ADC3: ANALOG CH3, on a bare RP2040 only (a Pico uses it for itself; build with `-DU573_ANALOG_CHANNELS=4`) |

The /OE pull-up is the hardware half of the power-on rule: until the firmware has shifted zeroes into the 595s and pulled /OE low, every output driver is off. The firmware's watchdog restarts a firmware that hangs, which puts /OE high again.

All the logic is 74HCT at 5 V from USB, so its inputs take the RP2040's 3.3 V levels directly and only one signal (GP4) needs dropping to 3.3 V.

## Inputs: 4 × 74HCT165

Each switch input:

```
 +5 V (USB) ──[4.7 kΩ]──┬── connector pin (switch to ground)
                        └──[10 kΩ]──┬── 74HCT165 input
                                    └──[10 nF]── GND
```

The pull-up is what the 573 has. The series resistor and capacitor take the edge off contact noise (0.1 ms) and protect the input.

The chain is read from IN4 outwards; within a chip, input A is the lowest bit. Bit numbers are [the protocol's](PROTOCOL.md#switches).

| Chip | A | B | C | D | E | F | G | H |
|---|---|---|---|---|---|---|---|---|
| IN1 (bits 0–7) | P2 left (X) | P2 right (Y) | P2 up (V) | P2 down (W) | P2 button 1 (Z) | P2 button 2 (a) | P2 button 3 (b) | P2 start (U) |
| IN2 (bits 8–15) | P1 left (20) | P1 right (21) | P1 up (18) | P1 down (19) | P1 button 1 (22) | P1 button 2 (23) | P1 button 3 (24) | P1 start (17) |
| IN3 (bits 16–23) | P1 button 4 (25) | P1 button 5 (26) | P1 button 6 (EXT-IN 5) | P2 button 4 (c) | P2 button 5 (d) | P2 button 6 (EXT-IN 9) | Coin 1 (16) | Coin 2 (T) |
| IN4 (bits 24–31) | Service (R) | Test (15) + TEST button | Option 1 | Option 2 | Option 3 | Option 4 | pulled up | pulled up |

IN1's SER input is tied to ground. IN4's QH goes to the divider and GP4.

## Outputs: 5 × 74HCT595

The chain starts at OUT1; within a chip, QA is the lowest bit.

| Chip | QA | QB | QC | QD | QE | QF | QG | QH |
|---|---|---|---|---|---|---|---|---|
| OUT1 | A0 | A1 | A2 | A3 | A4 | A5 | A6 | A7 |
| OUT2 | B0 | B1 | B2 | B3 | B4 | B5 | B6 | B7 |
| OUT3 | C0 | C1 | C2 | C3 | C4 | C5 | C6 | C7 |
| OUT4 | D0 | D1 | D2 | D3 | Coin counter 1 | Coin counter 2 | spare | spare |
| OUT5 | OUT0 | OUT1 | OUT2 | OUT3 | OUT4 | OUT5 | OUT6 | OUT7 |

**Banks A–D (28 outputs).** As on Konami's I/O boards, each is an optocoupler's transistor, collector to the output pin and emitter to the bank's common:

```
 595 output ──[680 Ω]──►|── GND            output pin ──┐
                    (LED, 5.6 mA)                       ├─ phototransistor (TLP293-4, CTR ≥ 100 %)
                                           bank common ─┘
```

Seven quad optocouplers. The commons are separate nets, brought to the connectors as the original boards have them, with a jumper per bank to the board's ground for the bench.

**EXT-OUT (8 outputs).** OUT5's pins through 100 Ω each: 5 V logic levels, not isolated, as on the 573.

**Coin counters.** Two channels of a ULN2003A sink JAMMA pins 8 and J; its COM pin goes to the JAMMA +12 V for the coils' flyback.

## Connectors

### J1: JAMMA edge

Fingers on the PCB itself (2 × 28, 3.96 mm pitch, 1.6 mm board), so the cabinet's harness plugs on as it did on the 573. Pins the board uses are in bold.

| Parts side | | Solder side | |
|---|---|---|---|
| 1 | **GND** | A | **GND** |
| 2 | **GND** | B | **GND** |
| 3 | **+5 V** (to the accessory headers only) | C | **+5 V** |
| 4 | **+5 V** | D | **+5 V** |
| 5 | −5 V | E | −5 V |
| 6 | **+12 V** (coin counters' flyback) | F | **+12 V** |
| 7 | key | H | key |
| 8 | **Coin counter 1** | J | **Coin counter 2** |
| 9 | Coin lockout 1 | K | Coin lockout 2 |
| 10 | Speaker | L | Speaker |
| 11 | | M | |
| 12 | Video red | N | Video green |
| 13 | Video blue | P | Video sync |
| 14 | Video ground | R | **Service** |
| 15 | **Test** | S | Tilt |
| 16 | **Coin switch 1** | T | **Coin switch 2** |
| 17 | **P1 start** | U | **P2 start** |
| 18 | **P1 up** | V | **P2 up** |
| 19 | **P1 down** | W | **P2 down** |
| 20 | **P1 left** | X | **P2 left** |
| 21 | **P1 right** | Y | **P2 right** |
| 22 | **P1 button 1** | Z | **P2 button 1** |
| 23 | **P1 button 2** | a | **P2 button 2** |
| 24 | **P1 button 3** | b | **P2 button 3** |
| 25 | **P1 button 4** | c | **P2 button 4** |
| 26 | **P1 button 5** | d | **P2 button 5** |
| 27 | **GND** | e | **GND** |
| 28 | **GND** | f | **GND** |

The video and speaker fingers are brought to a pin header (RGB, sync, ground; speaker pair) and nothing else, for whoever wants to feed them.

**Power.** The board's logic runs from USB. The cabinet's +5 V from the JAMMA edge is not joined to USB's: through a 500 mA polyfuse it feeds only the 5 V pins of the EXT-IN, EXT-OUT and ANALOG headers, which power what hangs on them, as the 573's did. The grounds are common.

### J2: EXT-IN (the 573's CN5), 10 pins

| Pin | | Pin | |
|---|---|---|---|
| 1 | 5 V | 6 | GND |
| 2 | 5 V | 7 | P2 button 4 |
| 3 | P1 button 4 | 8 | P2 button 5 |
| 4 | P1 button 5 | 9 | P2 button 6 |
| 5 | P1 button 6 | 10 | GND |

Buttons 4 and 5 are the same nets as the JAMMA edge's.

### J3: EXT-OUT (CN4), 12 pins

| Pin | | Pin | |
|---|---|---|---|
| 1 | 5 V | 7 | OUT3 |
| 2 | 5 V | 8 | OUT2 |
| 3 | OUT7 | 9 | OUT1 |
| 4 | OUT6 | 10 | OUT0 |
| 5 | OUT5 | 11 | GND |
| 6 | OUT4 | 12 | GND |

### J4: ANALOG (CN3), 10 pins

| Pin | | Pin | |
|---|---|---|---|
| 1 | 5 V | 6 | GND |
| 2 | 5 V | 7 | CH1 |
| 3 | | 8 | CH2 |
| 4 | | 9 | CH3 |
| 5 | CH0 | 10 | GND |

Each channel: 100 kΩ to ground, 1 kΩ and 100 nF, a rail-to-rail buffer (MCP6004), a 10 kΩ / 20 kΩ divider to the ADC pin (5 V reads 255). None of the games HIRAKU runs on the 573 reads these; they are there for the board to be a whole 573's worth.

### J5–J8: light banks A, B, C, D

The pinouts of the **analog I/O board's** headers (GX700-PWB(F): CN33, CN34, CN35, CN36):

| Pin | J5 bank A | J6 bank B | J7 bank C | | Pin | J8 bank D |
|---|---|---|---|---|---|---|
| 1 | ACOM | BCOM | CCOM | | 1 | DCOM |
| 2 | A0 | B0 | C0 | | 2 | D0 |
| 3 | A1 | B1 | C1 | | 3 | D1 |
| 4 | A2 | B2 | C2 | | 4 | D2 |
| 5 | A3 | B3 | C3 | | 5 | D3 |
| 6 | A4 | B4 | C4 | | 6 | frame ground |
| 7 | A5 | B5 | C5 | | | |
| 8 | A6 | B6 | C6 | | | |
| 9 | A7 | B7 | C7 | | | |
| 10 | ACOM | BCOM | frame ground | | | |

The **digital I/O board's** headers (GX894-PWB(B)A: CN12, CN11, CN10, CN13) carry the same outputs in another order: pin 1 the common of outputs 0–3, pins 2–5 outputs 0–3, pin 6 the common of outputs 4–7, pins 7–10 outputs 4–7 (bank D: common, D0–D3). A cabinet whose 573 had that board needs that order, so the PCB has both footprints per bank, or the bank headers sit on a small plug-in board per I/O board type. Which one depends on what the cabinet's own plugs are: [to check](#to-check-on-a-real-573-and-cabinet).

## Sound

The board carries no sound. In the cabinets HIRAKU's 573 games come from, the 573's RCA line outputs go to the cabinet's own amplifier: plug the PC's line output into those RCA leads (a 3.5 mm to two RCA lead). HIRAKU's audio settings (ALSA, WASAPI exclusive, ASIO) then set the latency.

- Hum with the PC connected: the PC and the cabinet's supply should share an earth; a ground-loop isolator on the RCA lead cures the rest.
- GuitarFreaks cabinets take the 573's sound and the digital I/O board's MP3 sound on separate RCA pairs. HIRAKU mixes both into one output: feed either pair.
- A cabinet that used the 573's own 15 W amplifier on the JAMMA speaker pins needs a small stereo amplifier between the PC and the speaker header.

A later revision could put a USB hub, a USB audio DAC and RCA jacks on the board, for one cable to the PC. It is left out of the first one on purpose: the PC's own output works today and adds nothing to get wrong.

## Picture

Not through the board. A 573 cabinet's monitor takes 15 kHz RGB with composite sync (some take 31 kHz too), from the JAMMA edge or the DB15. A PC needs a video card and driver that make such a mode (15 kHz modelines on Linux, CRT EmuDriver on Windows), or a converter; or the cabinet gets a flat panel. HIRAKU's side of it is in its `docs/PLAY.md`.

## Parts

| Part | Count | Notes |
|---|---|---|
| Raspberry Pi Pico (or pin-compatible) | 1 | On two 20-pin sockets |
| 74HCT165 (SOIC-16) | 4 | HCT, not HC: 3.3 V inputs at 5 V supply |
| 74HCT595 (SOIC-16) | 5 | |
| TLP293-4 (or LTV-247, PC847: quad phototransistor optocoupler) | 7 | CTR rank ≥ 100 % |
| ULN2003A | 1 | Coin counters |
| MCP6004 | 1 | ANALOG buffers |
| Resistor arrays 4.7 kΩ, 10 kΩ, 680 Ω; 10 nF capacitors | | 26 inputs, 28 optocoupler LEDs |
| Polyfuse 500 mA, Schottky diode | 1 each | Cabinet 5 V to the accessory headers |
| Tactile switch, 4-way DIP switch | 1 each | TEST, options |
| Headers J2–J8 | | Types: [to check](#to-check-on-a-real-573-and-cabinet) |

## To check on a real 573 and cabinet

1. **The plugs.** Which I/O board the cabinet's 573 has (analog GX700-PWB(F) or digital GX894-PWB(B)A), and the make and pin order of the plugs the cabinet's harness puts on the 573's front plate (a DDR cabinet: 10-pin white, orange and red, and a 6-pin white, 7 and 2 pins of them used). The front plate's short harness to the I/O board can be reused on J5–J8 if the header family matches.
2. **The lamps' current.** What one light output has to sink into the cabinet's relay board (GN845-PWB(A)) or lamp driver. The design assumes a few milliamps, as an optocoupler gives; more needs a Darlington optocoupler or a transistor stage.
3. **A DDR stage's board** (GN845-PWB(B)): that it gives its panels after the game's dialogue replayed through the optocouplers at 0.5–2 ms an edge with nobody reading its answers. This is the design's one real unknown; a Pico on a breadboard with two optocouplers per stage is enough to find out.
4. **The switches' levels.** That everything on the JAMMA and EXT-IN pins pulls to ground against a 4.7 kΩ pull-up to 5 V (a stage board's outputs, DrumMania's pad interface).
5. **The sound's route** in the cabinet: RCA leads to an amplifier box, or the JAMMA speaker pins.
6. **The monitor:** 15 kHz only, or 31 kHz as well.
