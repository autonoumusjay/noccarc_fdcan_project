"""Serve the MCU1 monitoring dashboard on localhost using Python's standard library."""
import argparse
from http.server import BaseHTTPRequestHandler, HTTPServer
import json
import math
import os
from pathlib import Path
import termios
import threading
import time
import tty

from protocol import Decoder

DEFAULT_LINK_TIMEOUT = 5.0  # Wall-clock seconds; Renode snapshots use simulated time.


class Monitor:
    def __init__(self, source, replay, link_timeout=DEFAULT_LINK_TIMEOUT):
        self.source = source
        self.replay = replay
        self.link_timeout = link_timeout
        self.decoder = Decoder()
        self.latest = None

    def read_forever(self):
        while True:
            try:
                with os.fdopen(os.open(self.source, os.O_RDONLY | os.O_NOCTTY |
                                       os.O_NONBLOCK), "rb", buffering=0) as stream:
                    if not self.replay:
                        tty.setraw(stream)
                        settings = termios.tcgetattr(stream)
                        settings[4] = settings[5] = termios.B115200
                        settings[2] &= ~(termios.PARENB | termios.CSTOPB | termios.CSIZE |
                                         getattr(termios, "CRTSCTS", 0))
                        settings[2] |= termios.CS8 | termios.CLOCAL | termios.CREAD
                        termios.tcsetattr(stream, termios.TCSANOW, settings)
                    while True:
                        data = stream.read(169 if self.replay else 4096)
                        if data == b"":
                            if self.replay:
                                return
                            break
                        if data:
                            for snapshot in self.decoder.feed(data):
                                self.latest = (time.monotonic(), snapshot)
                        time.sleep(0.25 if self.replay else 0.02)
            except (OSError, termios.error):
                pass
            self.decoder.buffer.clear()
            time.sleep(0.3)

    def status(self):
        latest = self.latest
        return dict(snapshot=latest[1] if latest else None,
                    live=bool(latest and time.monotonic() - latest[0] < self.link_timeout),
                    replay=self.replay, errors=self.decoder.errors)


def serve(monitor, port):
    page = Path(__file__).with_name("index.html").read_bytes()

    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            if self.path == "/":
                data, content_type = page, "text/html; charset=utf-8"
            elif self.path == "/api/status":
                data = json.dumps(monitor.status()).encode()
                content_type = "application/json"
            else:
                self.send_error(404)
                return
            self.send_response(200)
            self.send_header("Content-Type", content_type)
            self.send_header("Cache-Control", "no-store")
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)

        def log_message(self, _format, *_args):
            pass

    with HTTPServer(("127.0.0.1", port), Handler) as server:
        print(f"Dashboard at http://127.0.0.1:{server.server_port}", flush=True)
        server.serve_forever()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", help="UART device or Renode PTY path")
    parser.add_argument("--replay", action="store_true", help="Replay an MCU1 binary capture")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--link-timeout", type=float, default=DEFAULT_LINK_TIMEOUT,
                        help="Wall-clock seconds without a valid frame before declaring UART loss")
    args = parser.parse_args()
    if not math.isfinite(args.link_timeout) or args.link_timeout <= 0:
        parser.error("--link-timeout must be a finite positive number")
    monitor = Monitor(args.source, args.replay, args.link_timeout)
    threading.Thread(target=monitor.read_forever, daemon=True).start()
    serve(monitor, args.port)
