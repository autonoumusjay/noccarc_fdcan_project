"""Verify application watchdog servicing, expiry on a stalled tick, and reboot."""
import argparse
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def run(renode, output):
    output.mkdir(parents=True, exist_ok=True)
    elf = ROOT / "build/node2/node2.elf"
    symbols = subprocess.check_output(["arm-none-eabi-nm", str(elf)], text=True)
    feeds = next(line.split()[0] for line in symbols.splitlines()
                 if line.endswith(" g_network_watchdog_feeds"))
    scenario = output / "watchdog.resc"
    scenario.write_text(f'''mach create "watchdog-test"
machine LoadPlatformDescription @{ROOT}/renode/assignment.repl
cpu PerformanceInMips 20
macro reset
"""
    cpu VectorTableOffset 0x08000000
    sysbus LoadELF @{elf}
"""
runMacro $reset
emulation RunFor "1.4"
echo "FEEDS BEFORE STALL"
sysbus ReadDoubleWord 0x{feeds}
echo "STALL TIMER"
sysbus WriteDoubleWord 0x40000000 0
emulation RunFor "0.7"
echo "PRE-EXPIRY"
emulation RunFor "0.9"
echo "FEEDS AFTER REBOOT"
sysbus ReadDoubleWord 0x{feeds}
quit
''')
    logfile = output / "renode.log"
    with logfile.open("w") as log:
        subprocess.run([renode, "--disable-xwt", "--console", "-e", f"include @{scenario}"],
                       stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT,
                       timeout=90, check=True)
    transcript = logfile.read_text(errors="replace")
    before = re.search(r"FEEDS BEFORE STALL\s+0x([0-9a-f]+)", transcript, re.I)
    after = re.search(r"FEEDS AFTER REBOOT\s+0x([0-9a-f]+)", transcript, re.I)
    reset = "Watchdog reset triggered!"
    assert before and int(before[1], 16) >= 4, logfile
    assert after and int(after[1], 16) > 0, logfile
    assert transcript.count(reset) == 1, logfile
    assert transcript.index("PRE-EXPIRY") < transcript.index(reset)
    print(f"Watchdog passed: {int(before[1], 16)} feeds without peers, one reset after "
          f"TIM2 stalled, {int(after[1], 16)} feeds after reboot.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--renode", default="renode")
    parser.add_argument("--output", type=Path, default=ROOT / "build/final-verification/watchdog")
    args = parser.parse_args()
    run(args.renode, args.output.resolve())
