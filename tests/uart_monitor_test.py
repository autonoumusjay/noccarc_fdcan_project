"""Check the dashboard's wall-clock UART loss policy independently of CAN health."""
from pathlib import Path
import sys
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "dashboard"))
from monitor import Monitor

monitor = Monitor("unused", False)
snapshot = {"nodes": [{"online": True}, {"online": True},
                      {"online": False}, {"online": True}]}
with patch("monitor.time.monotonic") as clock:
    clock.return_value = 100.0
    assert monitor.status()["snapshot"] is None
    assert not monitor.status()["live"]

    monitor.latest = (100.0, snapshot)
    clock.return_value = 102.5
    assert monitor.status()["live"], "Slow simulation must not look like UART loss"
    assert not monitor.status()["snapshot"]["nodes"][2]["online"]

    clock.return_value = 105.0
    status = monitor.status()
    assert not status["live"] and status["snapshot"] == snapshot

    monitor.latest = (105.0, snapshot)
    assert monitor.status()["live"], "A fresh valid snapshot must restore the link"

    hardware = Monitor("unused", False, link_timeout=1.0)
    hardware.latest = (104.0, snapshot)
    assert not hardware.status()["live"]
