#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Watch a USB_2_573 board's switches and work its outputs from a PC (docs/BUILDING.md).

    u573.py info | watch | set OUTPUT... | chase | bootloader | selftest

Needs the `hid` module (pip install hid) for everything but selftest. The reports are
docs/PROTOCOL.md's.
"""
import struct
import sys
import time

VID, PID, USAGE_PAGE, PRODUCT = 0x1209, 0x0001, 0xFF73, "USB_2_573"
REPORT = 64
CMD_KEEPALIVE, CMD_OUTPUTS, CMD_CONFIG, CMD_RESET, CMD_BOOTLOADER = 0x00, 0x01, 0x02, 0x03, 0x7E

SWITCHES = (
    ["P2 left", "P2 right", "P2 up", "P2 down", "P2 button 1", "P2 button 2", "P2 button 3", "P2 start"]
    + ["P1 left", "P1 right", "P1 up", "P1 down", "P1 button 1", "P1 button 2", "P1 button 3", "P1 start"]
    + ["P1 button 4", "P1 button 5", "P1 button 6", "P2 button 4", "P2 button 5", "P2 button 6"]
    + ["Coin 1", "Coin 2", "Service", "Test", "Option 1", "Option 2", "Option 3", "Option 4"]
)

# An output's name -> its bit in the 48-bit state
OUTPUTS = {}
for _bank, _first, _count in (("A", 0, 8), ("B", 8, 8), ("C", 16, 8), ("D", 24, 4), ("OUT", 32, 8)):
    for _n in range(_count):
        OUTPUTS[f"{_bank}{_n}"] = _first + _n
OUTPUTS["COUNTER1"], OUTPUTS["COUNTER2"] = 40, 41
for _n in range(4, 8):
    OUTPUTS[f"CART{_n}"] = 28 + _n - 4   # the security cartridge slot's D4-D7
CHASE = [n for n in OUTPUTS if not n.startswith(("COUNTER", "CART"))]


def command(cmd, data=b"", count=0, flags=0):
    r = bytes([0x53, cmd, count, flags, 0, 0, 0, 0]) + data
    return r + bytes(REPORT - len(r))


def outputs_report(entries, flush=False):
    """entries: (delay in microseconds, state) pairs, at most 7"""
    data = b"".join(struct.pack("<H", min((d + 9) // 10, 0xFFFF)) + s.to_bytes(8, "little")[:6] for d, s in entries)
    return command(CMD_OUTPUTS, data, len(entries), 1 if flush else 0)


def state_of(names):
    state = 0
    for n in names:
        if n.upper() not in OUTPUTS:
            sys.exit(f"{n}: not an output (A0-A7, B0-B7, C0-C7, D0-D3, OUT0-OUT7, COUNTER1, COUNTER2, CART4-CART7)")
        state |= 1 << OUTPUTS[n.upper()]
    return state


def parse_in(r):
    if len(r) < 26 or r[0] != 0x73:
        return None
    return {
        "version": r[1],
        "status": r[2],
        "switches": struct.unpack_from("<I", bytes(r), 4)[0],
        "analog": list(r[8:12]),
        "applied": struct.unpack_from("<H", bytes(r), 12)[0],
        "queued": r[14],
        "capacity": r[15],
        "outputs": int.from_bytes(bytes(r[20:26]), "little"),
    }


def switch_names(bits):
    return [SWITCHES[k] for k in range(len(SWITCHES)) if bits >> k & 1]


def open_board():
    try:
        import hid
    except ImportError:
        sys.exit("this needs the hid module: pip install hid")
    found = None
    for d in hid.enumerate(VID, PID):
        if not (d.get("product_string") or "").startswith(PRODUCT):
            continue
        page = d.get("usage_page") or 0
        if page == USAGE_PAGE or (not page and d.get("interface_number") == 0):
            found = d
            break
    if not found:
        sys.exit("no USB_2_573 board found")
    try:
        if hasattr(hid, "Device"):
            return hid.Device(path=found["path"])
        dev = hid.device()
        dev.open_path(found["path"])
        return dev
    except Exception as e:  # the hid module's own error types differ between its two makes
        sys.exit(f"the board can't be opened ({e}): on Linux see docs/BUILDING.md for the udev rule")


def write(dev, report):
    dev.write(b"\x00" + report)  # the report ID first: none


def read(dev, timeout_ms=100):
    r = dev.read(REPORT, timeout_ms)
    return parse_in(r) if r else None


def hold(dev, state, seconds=None):
    """Drive `state`, saying so often enough that the board's failsafe stays away"""
    write(dev, outputs_report([(0, state)], flush=True))
    until = None if seconds is None else time.monotonic() + seconds
    while until is None or time.monotonic() < until:
        time.sleep(0.2)
        write(dev, command(CMD_KEEPALIVE))


def main(argv):
    what = argv[1] if len(argv) > 1 else ""
    if what == "selftest":
        return selftest()
    if what not in ("info", "watch", "set", "chase", "bootloader"):
        sys.exit(__doc__)
    dev = open_board()
    try:
        if what == "info":
            f = bytes(dev.get_feature_report(0, REPORT + 1))[1:]
            if f[:4] != b"U573":
                sys.exit("the board's description isn't a USB_2_573's")
            print(f"protocol {f[4]}, firmware {f[5]}.{f[6]}.{f[7]}, hardware revision {f[8]}")
            print(f"output queue {f[9]} entries, {f[10]} analog channels")
            debounce, failsafe, counter = struct.unpack_from("<HHH", f, 12)
            print(f"debounce {debounce} us, failsafe {failsafe} ms, coin counter limit {counter} ms")
            print("board number", f[20:28].hex())
        elif what == "watch":
            last = None
            print("the switches as they change (Ctrl-C ends)")
            while True:
                i = read(dev)
                if i and (i["switches"], i["analog"]) != last:
                    last = (i["switches"], i["analog"])
                    print(", ".join(switch_names(i["switches"])) or "(none)", " analog", i["analog"])
        elif what == "set":
            state = state_of(argv[2:])
            print("on:", " ".join(argv[2:]) or "(nothing)", "(Ctrl-C ends, everything off)")
            hold(dev, state)
        elif what == "chase":
            print("each output in turn (Ctrl-C ends, everything off)")
            while True:
                for n in CHASE:
                    print(n, end=" ", flush=True)
                    hold(dev, 1 << OUTPUTS[n], 0.5)
                print()
        elif what == "bootloader":
            write(dev, command(CMD_BOOTLOADER, b"BOOT"))
            print("the board restarts as a drive that takes a .uf2 file")
    except KeyboardInterrupt:
        print()
    finally:
        if what in ("set", "chase"):
            try:
                write(dev, command(CMD_RESET))
            except Exception:
                pass
        dev.close()
    return 0


def selftest():
    """The reports' bytes, against docs/PROTOCOL.md (no board needed)"""
    r = outputs_report([(0, state_of(["A0", "D3"])), (505, state_of(["out7", "COUNTER1"]))], flush=True)
    assert len(r) == REPORT and r[:8] == bytes([0x53, 1, 2, 1, 0, 0, 0, 0])
    assert r[8:16] == bytes([0, 0, 0x01, 0, 0, 0x08, 0, 0])
    assert r[16:24] == bytes([51, 0, 0, 0, 0, 0, 0x80, 0x01]) and not any(r[24:])
    assert command(CMD_BOOTLOADER, b"BOOT")[:12] == bytes([0x53, 0x7E, 0, 0, 0, 0, 0, 0]) + b"BOOT"
    assert len(OUTPUTS) == 42 and len(CHASE) == 36 and len(SWITCHES) == 30
    assert state_of(["cart4", "CART7"]) == 0x90000000
    i = parse_in(bytes([0x73, 1, 1, 9]) + struct.pack("<I", 1 << 25 | 1 << 8) + bytes([1, 2, 3, 4, 0x34, 0x12, 5, 128]) + bytes(48))
    assert i["switches"] == 1 << 25 | 1 << 8 and i["applied"] == 0x1234 and i["queued"] == 5 and i["capacity"] == 128
    assert switch_names(i["switches"]) == ["P1 left", "Test"] and i["analog"] == [1, 2, 3, 4]
    assert parse_in(bytes(64)) is None
    print("u573.py: its reports are the protocol's")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
