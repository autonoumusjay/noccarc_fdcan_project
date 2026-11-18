"""Decode MCU1 UART snapshots, with byte-stream resynchronization."""
import struct
import zlib

HEADER = b"NC\x01\x01"
FRAME_SIZE = 169
NODE = struct.Struct("<B9I")


class Decoder:
    def __init__(self):
        self.buffer = bytearray()
        self.errors = 0

    def feed(self, data):
        self.buffer.extend(data)
        while True:
            start = self.buffer.find(HEADER)
            if start < 0:
                del self.buffer[:-3]
                return
            del self.buffer[:start]
            if len(self.buffer) < FRAME_SIZE:
                return
            frame = self.buffer[:FRAME_SIZE]
            expected, = struct.unpack_from("<I", frame, FRAME_SIZE - 4)
            if zlib.crc32(frame[:-4]) != expected:
                self.errors += 1
                del self.buffer[0]
                continue
            del self.buffer[:FRAME_SIZE]
            tick, nominal, data_rate, brs = struct.unpack_from("<IIIB", frame, 4)
            nodes = []
            for index in range(4):
                flags, age, missed, recoveries, sequence, *values = NODE.unpack_from(
                    frame, 17 + index * NODE.size
                )
                nodes.append(dict(online=bool(flags & 1), seen=bool(flags & 2),
                                  recovering=bool(flags & 4), valid=bool(flags & 8),
                                  fresh=bool(flags & 16), age=age, missed=missed,
                                  recoveries=recoveries, sequence=sequence, values=values))
            yield dict(tick=tick, nominal=nominal, data_rate=data_rate, brs=bool(brs),
                       nodes=nodes)
