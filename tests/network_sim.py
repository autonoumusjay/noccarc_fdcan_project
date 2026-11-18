"""Run the four-node Renode fault/recovery test against captures from MCU1."""
import argparse
import json
import re
import struct
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "dashboard"))
from protocol import Decoder  # noqa: E402


def run(renode, build, output, reconnect_only=False):
    output.mkdir(parents=True, exist_ok=True)
    script = ['emulation SetGlobalSerialExecution True',
              'emulation SetGlobalQuantum "0.0001"', 'emulation CreateCANHub "canHub"']
    capture = output / "uart.bin"
    databases = {}
    ticks = {}
    for i in range(1, 5):
        elf = build / f"node{i}" / f"node{i}.elf"
        symbols = subprocess.check_output(["arm-none-eabi-nm", str(elf)], text=True)
        databases[i] = int(next(line.split()[0] for line in symbols.splitlines()
                                if line.endswith(" g_network_state")), 16)
        ticks[i] = int(next(line.split()[0] for line in symbols.splitlines()
                            if line.endswith(" g_system_tick_ms")), 16)
        script.extend((f'mach create "node{i}"',
                       f'machine LoadPlatformDescription @{ROOT}/renode/assignment.repl',
                       f'sysbus LoadELF @{elf}',
                       'cpu PerformanceInMips 20',
                       'connector Connect sysbus.fdcan1 canHub'))
        if i == 1:
            script.append(f'sysbus.usart3 CreateFileBackend @{capture}')
            dma_count = next(line.split()[0] for line in symbols.splitlines()
                             if line.endswith(" g_telemetry_dma_completed"))
    group = struct.Struct("<5IIHBx")
    stages = {"normal": (1, 2, 3, 4), "offline": (1, 2, 4), "recovered": (1, 2, 3, 4)}

    def check_databases(stage):
        for node in stages[stage]:
            script.extend((f'mach set "node{node}"', f'echo "TICK {stage} {node}"',
                           f'sysbus ReadDoubleWord 0x{ticks[node]:08x}',
                           f'echo "DATABASE {stage} {node}"',
                           f'sysbus ReadBytes 0x{databases[node]:08x} {4 * group.size}'))

    script.append('emulation RunFor "1.0"')
    check_databases("normal")
    script.extend(('mach set "node3"', 'connector Disconnect sysbus.fdcan1 canHub',
                   'emulation RunFor "1.5"'))
    check_databases("offline")
    script.append('mach set "node3"')
    if not reconnect_only:
        script.extend(('machine Reset', f'sysbus LoadELF @{build}/node3/node3.elf'))
    script.extend(('connector Connect sysbus.fdcan1 canHub', 'emulation RunFor "1.0"'))
    check_databases("recovered")
    script.extend(('mach set "node1"', 'echo "UART_DMA_COMPLETED"',
                   f'sysbus ReadDoubleWord 0x{dma_count}', 'quit'))
    scenario = output / "fault-test.resc"
    scenario.write_text("\n".join(script) + "\n")
    if capture.exists():
        capture.unlink()
    with (output / "renode.log").open("w") as log:
        subprocess.run([renode, "--disable-xwt", "--console", "-e", f"include @{scenario}"],
                       stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT,
                       timeout=120, check=True)

    transcript = (output / "renode.log").read_text(errors="replace")
    assert "Watchdog reset triggered!" not in transcript, "Unexpected watchdog reset"
    # Renode can write a timestamped log line in the middle of a monitor marker.
    console = re.sub(r"\[\d{2}:\d{2}:\d{2}\.\d+\][^\n]*\n", "", transcript)
    db_rows = re.findall(r"DATABASE (normal|offline|recovered) ([1-4])\s+\[([^]]+)\]",
                         console)
    tick_rows = re.findall(r"TICK (normal|offline|recovered) ([1-4])\s+0x([0-9a-fA-F]+)",
                           console)
    db = {(stage, int(node)): bytes(int(v, 16) for v in re.findall(r"0x([0-9a-fA-F]{2})", raw))
          for stage, node, raw in db_rows}
    tick = {(stage, int(node)): int(raw, 16) for stage, node, raw in tick_rows}
    expected = {(stage, node) for stage, nodes in stages.items() for node in nodes}
    assert db.keys() == tick.keys() == expected, (db.keys(), tick.keys())
    for stage, nodes in stages.items():
        payloads = {}
        matches = 0
        for node in nodes:
            assert len(db[stage, node]) == 4 * group.size
            for owner, (*values, updated, sequence, valid) in enumerate(
                group.iter_unpack(db[stage, node]), start=1
            ):
                should_be_valid = not (stage == "offline" and owner == 3)
                assert bool(valid) == should_be_valid, (stage, node, owner, valid)
                if not should_be_valid:
                    continue
                age = (tick[stage, node] - updated) & 0xffffffff
                assert age < 350, (stage, node, owner, age)
                offsets = {value - 1000 * ((owner - 1) * 5 + i)
                           for i, value in enumerate(values, start=1)}
                assert len(offsets) == 1 and 0 <= next(iter(offsets)) < 100, (
                    stage, node, owner, values
                )
                key = (owner, sequence)
                if key in payloads:
                    assert payloads[key] == values, (stage, node, owner, sequence)
                    matches += 1
                payloads[key] = values
        assert matches >= len(nodes), (stage, matches)
        print(f"{stage}: {len(nodes)} databases checked for freshness, validity, and "
              f"ownership; {matches} matching-sequence payload comparisons.")
    recovery_mode = "reconnect without reset" if reconnect_only else "reset and reconnect"
    count = re.search(r"UART_DMA_COMPLETED\s+0x([0-9a-fA-F]+)", console)
    assert count and int(count[1], 16) >= 3, "MCU1 DMA completion counter did not advance"
    print(f"MCU1 completed {int(count[1], 16)} USART3 TX DMA transfers.")
    decoder = Decoder()
    snapshots = list(decoder.feed(capture.read_bytes()))
    assert not decoder.errors, f"{decoder.errors} UART CRC errors"
    assert all(s["nominal"] == 500000 and s["data_rate"] == 2000000 and s["brs"]
               for s in snapshots)
    normal = offline = recovered = None
    for s in snapshots:
        nodes = s["nodes"]
        healthy = [n["online"] and n["valid"] and n["fresh"] for n in nodes]
        if normal is None and all(healthy):
            normal = s
        if normal and offline is None and healthy == [True, True, False, True] and not nodes[2]["online"]:
            assert nodes[2]["missed"] > 0 and not nodes[2]["valid"]
            assert all(nodes[i]["sequence"] > normal["nodes"][i]["sequence"] for i in (0, 1, 3))
            offline = s
        if offline and all(healthy) and nodes[2]["recoveries"] > 0:
            recovered = s
            break
    assert normal and offline and recovered, (
        f"Missing transition: normal={bool(normal)}, offline={bool(offline)}, "
        f"recovered={bool(recovered)}, snapshots={len(snapshots)}")
    # The first healthy snapshot after rejoin must reflect a new owner update.
    last_valid_before_outage = next(s["nodes"][2] for s in reversed(snapshots)
                                    if s["tick"] < offline["tick"] and s["nodes"][2]["fresh"])
    recovered_group = recovered["nodes"][2]
    if reconnect_only:
        assert recovered_group["sequence"] > last_valid_before_outage["sequence"]
        assert recovered_group["values"][0] > last_valid_before_outage["values"][0]
    else:
        assert recovered_group["sequence"] < last_valid_before_outage["sequence"]
        assert recovered_group["values"][0] < last_valid_before_outage["values"][0]
    print(f"MCU3 sequence {last_valid_before_outage['sequence']} before outage, "
          f"{recovered_group['sequence']} after {recovery_mode}.")
    evidence = (ROOT / "dashboard" / "index.html").read_text()
    status = json.dumps([dict(snapshot=s, replay=True, live=False, errors=0)
                         for s in snapshots])
    evidence_script = (
        f"const recorded = {status}; let step = 0;"
        'function replayCapture() { render(recorded[step++ % recorded.length]); }'
        'replayCapture(); setInterval(replayCapture, 750);'
    )
    (output / "dashboard-replay.html").write_text(evidence.replace("\npoll();",
                                                                  "\n" + evidence_script))
    for label, snapshot in (("normal", normal), ("MCU3 offline", offline), ("MCU3 recovered", recovered)):
        nodes = snapshot["nodes"]
        print(f"{snapshot['tick']:5d} ms {label}: "
              f"{sum(n['online'] for n in nodes)}/4 online, "
              f"{5 * sum(n['fresh'] for n in nodes)}/20 fresh, "
              f"MCU3 missed={nodes[2]['missed']} recoveries={nodes[2]['recoveries']}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--renode", default="renode")
    parser.add_argument("--build", type=Path, default=ROOT / "build")
    parser.add_argument("--output", type=Path, default=ROOT / "build" / "simulation")
    parser.add_argument("--reconnect-only", action="store_true")
    args = parser.parse_args()
    run(args.renode, args.build.resolve(), args.output.resolve(), args.reconnect_only)
