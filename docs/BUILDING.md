# Building, flashing, testing

## The tests (any PC)

The firmware's logic runs on a PC with nothing but a C compiler and CMake:

```bash
cmake -S firmware/tests -B build/tests
cmake --build build/tests
ctest --test-dir build/tests --output-on-failure
```

HIRAKU has the other half: its test `usb573_board` runs HIRAKU's driver against a copy of `firmware/core/` (its `tests/usb573/firmware/`). After a change to the protocol, copy `firmware/core/` there again and run it.

## The firmware

You need CMake, the Arm GNU toolchain (`gcc-arm-none-eabi`, with `libnewlib-arm-none-eabi` and `libstdc++-arm-none-eabi-newlib` on Debian and Ubuntu) and the [Pico SDK](https://github.com/raspberrypi/pico-sdk) 2.x with its TinyUSB submodule:

```bash
git clone --depth 1 -b 2.2.0 https://github.com/raspberrypi/pico-sdk
git -C pico-sdk submodule update --init --depth 1 lib/tinyusb

cmake -S firmware/rp2040 -B build/rp2040 -DPICO_SDK_PATH=$PWD/pico-sdk
cmake --build build/rp2040
```

That makes `build/rp2040/usb_2_573.uf2` for a Raspberry Pi Pico.

| Option | For |
|---|---|
| `-DPICO_BOARD=pico2` (or another board's name) | A board other than the first Pico, with the same pins |
| `-DU573_ANALOG_CHANNELS=4` | A bare RP2040 whose GPIO29 is ANALOG CH3 (a Pico has three channels) |

## Flashing

Hold the Pico's BOOTSEL button while plugging it in: it shows up as a drive named `RPI-RP2`. Copy `usb_2_573.uf2` onto it. The Pico restarts as the board.

A board that already runs the firmware goes back to that drive without the button: `tools/u573.py bootloader`.

## Trying a board

`tools/u573.py` needs Python 3 and the `hid` module (`pip install hid`, with the system's hidapi library). On Linux the board's raw interface must be yours to open:

```bash
printf '%s\n' \
  'KERNEL=="hidraw*", ATTRS{idVendor}=="1209", ATTRS{idProduct}=="0001", ATTRS{product}=="USB_2_573*", TAG+="uaccess"' \
  | sudo tee /etc/udev/rules.d/70-usb-2-573.rules >/dev/null
sudo udevadm control --reload && sudo udevadm trigger --subsystem-match=hidraw
```

| Command | What it does |
|---|---|
| `tools/u573.py info` | The firmware's version, the queue's size, the settings |
| `tools/u573.py watch` | Prints the switches by name as they change, and the analog channels |
| `tools/u573.py set A0 C2 OUT7` | Those outputs on, the others off, until Ctrl-C |
| `tools/u573.py chase` | Each output on for half a second in turn: A0 to D3, OUT0 to OUT7 (not the coin counters, nor the cartridge slot's `CART4`–`CART7`) |
| `tools/u573.py bootloader` | Restarts the board as its USB bootloader |

A Pico alone, with nothing on its pins, already answers `info`, and `watch` shows every switch closed or open at random (its inputs float). On the bench, the first things to see are a switch to ground on a 74HCT165 input showing up in `watch`, and `chase` walking an LED along the 74HCT595 outputs.

With HIRAKU, `HK_USB573_LOG=1` prints every report it reads and writes.
