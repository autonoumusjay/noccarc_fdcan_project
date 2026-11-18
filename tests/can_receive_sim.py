"""Exercise the actual application RX path with valid and malformed CAN FD frames."""
import argparse
from pathlib import Path
import re
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]
GROUP = struct.Struct("<5IIHBx")


def run(renode, output):
    output.mkdir(parents=True, exist_ok=True)
    elf = ROOT / "build/node2/node2.elf"
    listing = subprocess.check_output(["arm-none-eabi-nm", str(elf)], text=True)
    symbols = {fields[2]: int(fields[0], 16)
               for line in listing.splitlines() if len(fields := line.split()) == 3}
    owner = symbols["g_network_state"] + 2 * GROUP.size
    invalid = symbols["g_network_rx_invalid"]

    def frame(identifier, payload):
        return ("python \"from System import Array, Byte; "
                "from Antmicro.Renode.Core.CAN import CANMessageFrame; "
                "self.Machine['sysbus.fdcan1'].OnFrameReceived("
                f"CANMessageFrame({identifier}, "
                f"Array[Byte]({list(payload)}), False, False, True, True))\"")

    def snapshot(label):
        return [f'echo "GROUP {label}"', f"sysbus ReadBytes 0x{owner:x} {GROUP.size}",
                f'echo "INVALID {label}"', f"sysbus ReadDoubleWord 0x{invalid:x}"]

    legitimate = struct.pack("<BBH5I", 1, 3, 7, 11, 12, 13, 14, 15)
    changed = struct.pack("<BBH5I", 1, 3, 8, 91, 92, 93, 94, 95)
    duplicate = struct.pack("<BBH5I", 1, 3, 7, 81, 82, 83, 84, 85)
    script = [
        'mach create "receiver"',
        f"machine LoadPlatformDescription @{ROOT}/renode/assignment.repl",
        f"sysbus LoadELF @{elf}",
        "cpu PerformanceInMips 20",
        'emulation RunFor "0.2"',
        frame(0x103, struct.pack("<BBH", 1, 3, 1)),
        frame(0x113, legitimate),
        'emulation RunFor "0.02"',
        *snapshot("before"),
        frame(0x114, changed),             # Sender and identifier disagree.
        frame(0x113, changed[:16]),        # Legal CAN FD length, invalid application length.
        frame(0x113, duplicate),           # Same sequence with different values.
        'emulation RunFor "0.02"',
        *snapshot("after"),
        frame(0x113, changed),             # A subsequent valid update must still work.
        'emulation RunFor "0.02"',
        *snapshot("next"),
        "quit",
    ]
    scenario = output / "receive-test.resc"
    scenario.write_text("\n".join(script) + "\n")
    log = output / "renode.log"
    with log.open("w") as stream:
        subprocess.run([renode, "--disable-xwt", "--console", "-e", f"include @{scenario}"],
                       stdin=subprocess.DEVNULL, stdout=stream, stderr=subprocess.STDOUT,
                       check=True, timeout=90)
    transcript = log.read_text(errors="replace")
    # Renode can write a timestamped log line in the middle of a monitor marker.
    console = re.sub(r"\[\d{2}:\d{2}:\d{2}\.\d+\][^\n]*\n", "", transcript)
    groups = {label: bytes(int(x, 16) for x in re.findall(r"0x([0-9a-fA-F]{2})", data))
              for label, data in re.findall(r"GROUP (before|after|next)\s+\[([^]]+)\]", console)}
    counts = {label: int(value, 16) for label, value in
              re.findall(r"INVALID (before|after|next)\s+0x([0-9a-fA-F]+)", console)}
    assert groups.keys() == counts.keys() == {"before", "after", "next"}, log
    assert groups["before"] == groups["after"], "Rejected frames changed values, age, or sequence"
    assert counts == {"before": 0, "after": 2, "next": 2}, counts
    *first, first_tick, first_seq, valid = GROUP.unpack(groups["before"])
    *last, last_tick, last_seq, last_valid = GROUP.unpack(groups["next"])
    assert (first, first_seq, valid) == ([11, 12, 13, 14, 15], 7, 1)
    assert (last, last_seq, last_valid) == ([91, 92, 93, 94, 95], 8, 1)
    assert last_tick > first_tick
    print("PASS: malformed ID/length and duplicate left the owner group unchanged; "
          "a subsequent valid frame committed sequence 8.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--renode", default="renode")
    parser.add_argument("--output", type=Path, default=ROOT / "build/submission-verification/receive")
    args = parser.parse_args()
    run(args.renode, args.output.resolve())
