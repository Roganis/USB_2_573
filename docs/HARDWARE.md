# The hardware

**Status: a design, not a built board.** The firmware runs on this pin assignment and is tested on a PC; no PCB has been laid out and nothing here has been measured on a cabinet. The points that must be checked on a real System 573 and cabinet before a PCB is ordered are listed [at the end](#to-check-on-a-real-573-and-cabinet).

Pinouts of the System 573's connectors and the registers behind them are from psx-spx's [Konami System 573](https://psx-spx.consoledev.net/konamisystem573/) page. The JAMMA edge is the JAMMA standard's, with the 573's button 4 and 5 pins from that page. The connectors' part numbers, the optocouplers Konami used and the notes on the relay and stage boards are from NaokiS28's [Konami-System-573-Documentation](https://github.com/NaokiS28/Konami-System-573-Documentation) (its KiCad schematics and readmes), which says of itself that it is incomplete and may be wrong in places.

## What the board replaces

The System 573 unit is taken out of the cabinet. Everything that was plugged into it goes to the board or to the PC:

| On the 573 | Now |
|---|---|
| JAMMA edge: switches, coin counters | The board's JAMMA edge |
| I/O board light outputs, banks A–D (through the plugs on the 573's front plate) | The board's bank headers |
| EXT-OUT (CN4), EXT-IN (CN5), ANALOG (CN3) | The board's headers of the same pinout |
| JAMMA edge: video; the RGB DB15 | The PC's video output (not through the board) |
| RCA line out; JAMMA speaker pins | The PC's audio output (not through the board, [see below](#sound)) |
| Security cartridge: what hangs on it besides its security chip (Step Champ's lamp drivers, the amplifier control of DrumMania's) | The cartridge itself, in the board's cartridge slot |
| The security chip, CD-ROM drive, PCMCIA cards, JVS memory card reader, e-AMUSEMENT units | Emulated in HIRAKU: nothing to connect |

## Block diagram

```
                      ┌────────────────────────────── USB_2_573 ──────────────────────────────┐
                      │                                                                       │
 PC ═══ USB ══════════╪═ Raspberry Pi Pico ──SPI─┬─► 6 × 74HCT595 ─┬─► 28 optocouplers ───────╪─► banks A, B, C, D
      (5 V, data)     │   (RP2040)               │   (48 outputs)  ├─► 8 × 5 V logic ─────────╪─► EXT-OUT
                      │      │                   │                 ├─► 4 × 5 V logic, 5 V ────╪─► cartridge slot (D4–D7)
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

## Outputs: 6 × 74HCT595

The chain starts at OUT1; within a chip, QA is the lowest bit.

| Chip | QA | QB | QC | QD | QE | QF | QG | QH |
|---|---|---|---|---|---|---|---|---|
| OUT1 | A0 | A1 | A2 | A3 | A4 | A5 | A6 | A7 |
| OUT2 | B0 | B1 | B2 | B3 | B4 | B5 | B6 | B7 |
| OUT3 | C0 | C1 | C2 | C3 | C4 | C5 | C6 | C7 |
| OUT4 | D0 | D1 | D2 | D3 | Coin counter 1 | Coin counter 2 | spare | spare |
| OUT5 | OUT0 | OUT1 | OUT2 | OUT3 | OUT4 | OUT5 | OUT6 | OUT7 |
| OUT6 | Cartridge D4 | Cartridge D5 | Cartridge D6 | Cartridge D7 | Cartridge slot 5 V on | spare | spare | spare |

**Banks A–D (28 outputs).** As on Konami's I/O boards, each is an optocoupler's transistor, collector to the output pin and emitter to the bank's common:

```
 595 output ──[680 Ω]──►|── GND            output pin ──┐
                    (LED, 5.6 mA)                       ├─ phototransistor (TLP293-4, CTR ≥ 100 %)
                                           bank common ─┘
```

Seven quad optocouplers, or 28 single PC817s: that is the part Konami's I/O boards use, each with a 10 nF capacitor and an EMI filter on its output. What the outputs feed in a cabinet is the relay board (GN845-PWB(A): DDR, GuitarFreaks, DrumMania, PunchMania), whose inputs are logic inverters that switch the 12 V lamps themselves, so an output sinks a logic input's current and no more. The commons are separate nets, brought to the connectors as the original boards have them, with a jumper per bank to the board's ground for the bench.

**EXT-OUT (8 outputs).** OUT5's pins through 100 Ω each: 5 V logic levels, not isolated, as on the 573.

**Coin counters.** Two channels of a ULN2003A sink JAMMA pins 8 and J; its COM pin goes to the JAMMA +12 V for the coils' flyback.

**Cartridge slot (4 outputs and its power).** OUT6's QA–QD through 100 Ω each to the slot's D4–D7. QE turns on a P-channel MOSFET that feeds the slot's 5 V pins: the firmware sets it while a host drives the board.

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

The 573's own headers for these are JST NH series, 2.5 mm pitch (B10P-SHF-1AA for 10 pins, B12P-SHF-1AA for 12), as are the I/O boards' light headers below; its power header (CN17, 12 V / ground / 5 V) is a 6-pin JST VH. Fitting the same parts lets the cabinet's and the 573's own leads plug straight in.

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

The pinouts of the **analog I/O board's** headers (GX700-PWB(F): CN33, CN34, CN35 are B10P-SHF-1AA, CN36 is B6P-SHF-1AA):

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

The **digital I/O board's** headers (GX894-PWB(B)A: CN12 bank A, 13 pins; CN11 bank B, 12 pins; CN10 bank C, 10 pins; CN13 bank D, 5 pins; JST NH too) carry the same outputs in another order: pin 1 the common of outputs 0–3, pins 2–5 outputs 0–3, pin 6 the common of outputs 4–7, pins 7–10 outputs 4–7 (bank D: common, D0–D3). A cabinet whose 573 had that board needs that order, so the PCB has both footprints per bank, or the bank headers sit on a small plug-in board per I/O board type. Which one depends on what the cabinet's own plugs are: [to check](#to-check-on-a-real-573-and-cabinet).

### J9: security cartridge slot (the 573's CN14)

A socket for the cabinet's own security cartridge, with the 573's pin numbers and only these pins connected:

| Pin | | Pin | |
|---|---|---|---|
| 1, 2, 8, 9, 23, 24, 26 | GND | 21, 22, 41, 42 | 5 V, switched (on while a host drives the board) |
| 16 | D4 | 14 | D6 |
| 15 | D5 | 13 | D7 |
| 3 | /DSR: to ground on the cartridge, not used | 10, 32 | key |

Everything else is left open, D0–D3 and IO0 above all: those are the cartridge's security chip (its EEPROM or ZS01, and the serial number chip on D4 of some), which HIRAKU emulates from the game's dump. The board never drives them, so nothing a game does can write to a real cartridge.

What it is for: a cartridge that carries more than its chip keeps doing that job, driven by the game's own writes to D4–D7.
- **Step Champ** (and the other Champ games' cartridges of that kind): two 4094 shift registers and open-drain lamp drivers, clocked on D5 with D6 (reset) and D7 (data). The cabinet's 16-pin lamp plug and the cartridge's 12 V lead stay where they are, on the cartridge.
- **DrumMania, GuitarFreaks, DDR from 3rdMIX**: the cartridge's 6-pin "control" or "amp box" port, three logic signals that some cabinets use for the amplifier or lights.

A Step Champ cabinet without its cartridge has the same lamp word, decoded by HIRAKU, on bank A (A0–A7): that needs drivers for 12 V lamps on those outputs, which this board doesn't have.

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
| 74HCT595 (SOIC-16) | 6 | |
| TLP293-4 (or LTV-247, PC847: quad phototransistor optocoupler), or 28 × PC817 | 7 | CTR rank ≥ 100 % |
| P-channel MOSFET (logic level, SOT-23), 44-position cartridge socket | 1 each | The cartridge slot and its 5 V |
| ULN2003A | 1 | Coin counters |
| MCP6004 | 1 | ANALOG buffers |
| Resistor arrays 4.7 kΩ, 10 kΩ, 680 Ω; 10 nF capacitors | | 26 inputs, 28 optocoupler LEDs |
| Polyfuse 500 mA, Schottky diode | 1 each | Cabinet 5 V to the accessory headers |
| Tactile switch, 4-way DIP switch | 1 each | TEST, options |
| JST NH headers: B10P-SHF-1AA, B12P-SHF-1AA, B6P-SHF-1AA (and B13P, B5P for the digital board's order) | | J2–J8 |

## To check on a real 573 and cabinet

Answered since the first draft, by NaokiS28's documentation: the headers are JST NH; Konami's outputs are PC817 optocouplers into the relay board's logic inputs, so an optocoupler output is enough.

Still open:

1. **The plugs on the 573's front plate.** The cabinet's harness plugs into the 573's case, and a short harness inside goes on to the I/O board's headers. Whether the cabinet's plugs fit JST NH headers directly, or the 573's own short harness has to move over to the board with its front plate. And which I/O board the cabinet's 573 has: DrumMania 1st has the analog one (lights on banks C and D), 2nd Mix and later the digital one (banks A and B, another pin order).
2. **The cartridge slot's connector:** its type and pitch, from a 573. And on a DrumMania cabinet, whether anything is plugged into the cartridge's 6-pin control port at all.
3. **Step Champ's cartridge:** which board it is (the same kind as Salary Man Champ's, PWB0000088954, by MAME's description of its lamps), and that its lamps follow D5–D7 driven from the slot with nothing on D0–D3.
4. **The switches' levels.** That everything on the JAMMA and EXT-IN pins pulls to ground against a 4.7 kΩ pull-up to 5 V (DrumMania's pad interface, Step Champ's panels).
5. **The sound's route** in the cabinet: RCA leads to an amplifier box, or the JAMMA speaker pins (a Step Champ cabinet with a plain 573 may well use the 573's own amplifier).
6. **The monitor:** 15 kHz only, or 31 kHz as well.
7. **A DDR stage's board** (GN845-PWB(B)), for whoever has one: that it gives its panels after the game's dialogue replayed through the optocouplers. The 573 takes some 200 ms over a message and goes on clocking the bus during play; the board replays the game's edges at the game's own pace. NaokiS28's notes also say that grounding one of the stage board's pins gives the panels without any dialogue.
