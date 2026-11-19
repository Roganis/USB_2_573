# The cabinets

What each cabinet has on the board's pins. With HIRAKU's `--usb573` none of this needs setting up: the game reads and drives the same pins it did on the 573. The tables are for wiring a cabinet, for checking it with `tools/u573.py`, and for using the board's gamepad interface with other programs.

Where the tables come from:
- **Inputs:** MAME's `ksys573.cpp` input ports, as HIRAKU's System 573 core has them (`runtime/sys573/board.cpp`), and psx-spx's DDR Solo table.
- **Lights:** psx-spx's light mapping tables (Dance Dance Revolution, DrumMania) and MAME's output names put through the I/O boards' register wiring. HIRAKU's test `usb573_board` checks that the two agree where both exist.

Nothing here has been checked on a cabinet yet.

"Lit" is given for the pin: banks A–D light a lamp when the output sinks (state bit 1); EXT-OUT lamps are lit when the pin is low (state bit 0), as the games write them.

## Dance Dance Revolution (two players)

Each stage has a board (GN845-PWB(B)) between its sensors and the JAMMA harness. It gives the four panels only after the game has been through a dialogue with it over that stage's data and clock outputs; before that it holds right and up. The 573 is slow about it (some 200 ms a message) and goes on clocking that bus while the game is played. HIRAKU relays the game's edges at the game's pace (its `docs/USB573.md`). The gamepad interface cannot: with other programs, the stages stay locked unless a System 573 game has run since the cabinet was turned on.

| Input | Pin | Input | Pin |
|---|---|---|---|
| P1 left panel | JAMMA 20 | P2 left panel | JAMMA X |
| P1 right panel | 21 | P2 right panel | Y |
| P1 up panel | 18 | P2 up panel | V |
| P1 down panel | 19 | P2 down panel | W |
| P1 start | 17 | P2 start | U |
| P1 panel buttons 1–3 | 22, 23, 24 | P2 panel buttons 1–3 | Z, a, b |
| Coin | 16 | Test, Service | 15, R |

| Output | Wired to | Output | Wired to |
|---|---|---|---|
| A0 | P1 up arrow | B0 | P2 up arrow |
| A1 | P1 down arrow | B1 | P2 down arrow |
| A2 | P1 left arrow | B2 | P2 left arrow |
| A3 | P1 right arrow | B3 | P2 right arrow |
| A4 | Data to stage 1's board | B4 | Data to stage 2's board |
| A5 | Clock to stage 1's board | B5 | Clock to stage 2's board |
| C2 | P1 buttons' lamps | C3 | P2 buttons' lamps |
| C4 | Marquee, bottom right | C5 | Marquee, top right |
| C6 | Marquee, bottom left | C7 | Marquee, top left |
| D0 | Speakers' neon | | |

The 573's front plate has a 10-pin plug per bank (white: 1P, bank A; orange: 2P, bank B; red: bank C) and a 6-pin white one for bank D.

## Dance Dance Revolution Solo

No stage board: each panel's two sensors are on JAMMA pins of their own.

| Input | Pin | Input | Pin |
|---|---|---|---|
| Left, sensor A | JAMMA 20 (P1 left) | Left, sensor B | 23 (P1 button 2) |
| Right, sensor A | 21 (P1 right) | Right, sensor B | a (P2 button 2) |
| Up, sensor A | 18 (P1 up) | Up, sensor B | Z (P2 button 1) |
| Down, sensor A | 19 (P1 down) | Down, sensor B | 24 (P1 button 3) |
| Up-left, sensor A | X (P2 left) | Up-left, sensor B | 22 (P1 button 1) |
| Up-right, sensor A | Y (P2 right) | Up-right, sensor B | b (P2 button 3) |
| Select left | 26 (P1 button 5) | Select right | d (P2 button 5) |
| Start | 17 | Coin, Test, Service | 16, 15, R |

| Output | Wired to (MAME's names) | Output | Wired to |
|---|---|---|---|
| B0 | Extra ball 4 | C0 | Speaker |
| B1 | Extra ball 3 | C4 | Start lamp |
| B2 | Extra ball 2 | C5 | Body left |
| B3 | Extra ball 1 | C6 | Body centre |
| | | C7 | Body right |

## DrumMania

The pads' interface in the cabinet gives a switch closure per hit. The lights go through the cabinet's relay board. DrumMania's security cartridge (GX896-PWB(A)A) also has a 6-pin control port for the amplifier: if the cabinet has a lead on it, the cartridge goes into the board's cartridge slot ([HARDWARE.md](HARDWARE.md#j9-security-cartridge-slot-the-573s-cn14)).

| Input | Pin | Input | Pin |
|---|---|---|---|
| Hi-hat | JAMMA 18 (P1 up) | Cymbal | 22 (P1 button 1) |
| Snare | 19 (P1 down) | Bass pedal | 24 (P1 button 3) |
| High tom | 20 (P1 left) | Select left | X (P2 left) |
| Low tom | 21 (P1 right) | Select right | Y (P2 right) |
| Start | 17 | Coin, Test, Service | 16, 15, R |

| Lamp | First cabinets (analog I/O board) | Later cabinets (digital I/O board) |
|---|---|---|
| Hi-hat | C0 | A0 |
| Snare | C1 | A1 |
| High tom | C2 | A2 |
| Low tom | C3 | A3 |
| Cymbal | C4 | B4 |
| Start | C6 | B6 |
| Select | C7 | B7 |
| Spotlight | D0 | B0 |
| Top neon | D1 | B2 |
| Bottom neon | D3 | B1 |

On the later cabinets the neons and the Start and Select lamps are lit while their output does *not* sink (MAME's `drmn_output_callback`).

## Step Champ

A plain System 573: no I/O board, no CD-ROM drive, the game in the 573's flash. Three players, each with three controls and a Start. MAME names the controls red, green and blue, after Hyper Bishi Bashi Champ's buttons whose input ports it shares.

| Input | Player 1 | Player 2 | Player 3 |
|---|---|---|---|
| Red | JAMMA 22 (P1 button 1) | Z (P2 button 1) | 20 (P1 left) |
| Green | 23 (P1 button 2) | a (P2 button 2) | 19 (P1 down) |
| Blue | 24 (P1 button 3) | b (P2 button 3) | 21 (P1 right) |
| Start | 17 | U | 18 (P1 up) |

Coin 16, Test 15, Service R.

The lamps are not on the 573 at all. The security cartridge carries two shift registers and lamp drivers, and the game clocks a word of eight bits into them over the cartridge slot's D5 (clock), D6 (reset) and D7 (data); the cabinet's lamp plug and a 12 V lead are on the cartridge. Put the cartridge in the board's cartridge slot and it goes on doing that.

| Bit of the word (the first clocked in is 7) | Lamp (MAME's names) | On bank A, decoded by HIRAKU |
|---|---|---|
| 7, 6, 5 | Start lamps of players 1, 2, 3 | A7, A6, A5 |
| 3, 2, 1 | Halogens 1, 2, 3 | A3, A2, A1 |

HIRAKU has Step Champ from MAME's description of it and has not run it; MAME itself has no sound for it.

## GuitarFreaks

| Input | Pin (guitar 1) | Pin (guitar 2) |
|---|---|---|
| Neck R, G, B | JAMMA 22, 23, 24 (P1 buttons 1–3) | Z, a, b |
| Pick | 18 (P1 up) | V |
| Wailing (tilt) | 19 (P1 down) | W |
| Effect knob, a 2-bit position | 20, 21 (P1 left, right) | X, Y |
| Start | 17 | U |

Coin 16, Test 15, Service R (MAME's GuitarFreaks ports read Service on player 1's button 4, pin 25).

| Output | Lamp (lit when low) |
|---|---|
| EXT-OUT OUT7 | Spotlight, left |
| OUT6 | Spotlight, right |
| OUT5 | P1 start |
| OUT4 | P2 start |

## Dance Maniax

| Input | Pin (player 1) | Pin (player 2) |
|---|---|---|
| Upper left, upper right sensor | JAMMA 22, 23 (P1 buttons 1, 2) | Z, a |
| Lower left sensor, beams D0 and D1 | 25 (P1 button 4), 20 (P1 left) | c, X |
| Lower right sensor, beams D0 and D1 | 26 (P1 button 5), 21 (P1 right) | d, Y |
| Select left, right | 18, 19 (P1 up, down) | V, W |
| Start | 17 | U |

| Output | Lamp |
|---|---|
| Sensor lamps 0–11, blue | C0, C2, C4, C6, B0, B2, A4, A6, A0, A2, B4, B6 |
| Sensor lamps 0–11, red | C1, C3, C5, C7, B1, B3, A5, A7, A1, A3, B5, B7 |
| Spotlights: yellow, pink, blue | D0, D1, D2 |
| EXT-OUT OUT1, OUT4 (lit when low) | P1 start, P2 start |
| EXT-OUT OUT0, OUT3 (lit when low) | Left 1, left 0 |
| EXT-OUT OUT2, OUT5 (lit when low) | Right 1, right 0 |

## Mambo a GoGo

Three congas of three pads, each pad read by two sensors.

| Pad | Sensors' pins | Pad | Sensors' pins | Pad | Sensors' pins |
|---|---|---|---|---|---|
| Left 1 | 23 (P1 button 2), U (P2 start) | Centre 1 | V (P2 up), 19 (P1 down) | Right 1 | a (P2 button 2), T (coin 2) |
| Left 2 | 26 (P1 button 5), 22 (P1 button 1) | Centre 2 | W (P2 down), X (P2 left) | Right 2 | d (P2 button 5), Z (P2 button 1) |
| Left 3 | 25 (P1 button 4), 24 (P1 button 3) | Centre 3 | 18 (P1 up), Y (P2 right) | Right 3 | c (P2 button 4), b (P2 button 3) |

Select left and right: 20, 21 (P1 left, right). Start 17, coin 16, Test 15, Service R.

| Output | Lamp |
|---|---|
| D0, D1, D2 | Conga left, centre, right |
| A4, A5 | Fire lamp left, right |
| A6, A7 | Fire fan left, right |
| EXT-OUT OUT3 (lit when low) | Start |
| EXT-OUT OUT5, OUT4 (lit when low) | Select left, select right |
