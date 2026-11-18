"""Check stream resynchronization and corruption detection."""
from pathlib import Path
import struct
import sys
import zlib

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "dashboard"))
from protocol import Decoder, HEADER, NODE

frame = HEADER + struct.pack("<IIIB", 100, 500000, 2000000, 1)
for i in range(4):
    frame += NODE.pack(31, 20, 2, 1, 10, *range(i * 5 + 1, i * 5 + 6))
frame += struct.pack("<I", zlib.crc32(frame))
broken = bytearray(frame)
broken[25] ^= 1
decoder = Decoder()
assert not list(decoder.feed(b"noise" + frame[:3]))
snapshots = list(decoder.feed(frame[3:] + broken + frame))
assert len(snapshots) == 2 and decoder.errors == 1
assert snapshots[0]["nodes"][3]["values"] == [16, 17, 18, 19, 20]
