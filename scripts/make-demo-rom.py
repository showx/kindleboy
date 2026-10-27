"""Generate an original DMG test ROM, with no commercial game or boot ROM data."""
from pathlib import Path
import argparse
import re
import struct


class Program:
    def __init__(self):
        self.code = bytearray()
        self.labels = {}
        self.fixups = []

    def emit(self, *values):
        self.code.extend(values)

    def label(self, name):
        self.labels[name] = 0x150 + len(self.code)

    def jump(self, name, opcode=0xC3):
        self.emit(opcode, 0, 0)
        self.fixups.append((len(self.code) - 2, name))

    def finish(self):
        for offset, name in self.fixups:
            struct.pack_into('<H', self.code, offset, self.labels[name])
        return self.code


def make_rom():
    rom = bytearray(32768)
    rom[0x100:0x104] = bytes((0, 0xC3, 0x50, 0x01))
    rom[0x134:0x143] = b'KINDLEBOY DEMO'.ljust(15, b'\0')
    # MBC1 + 8 KiB battery RAM also exercises the save path.
    rom[0x147:0x14A] = bytes((0x03, 0, 0x02))
    rom[0x14A] = 1
    rom[0x14B] = 0x33
    checksum = 0
    for value in rom[0x134:0x14D]:
        checksum = (checksum - value - 1) & 255
    rom[0x14D] = checksum

    tiles = bytearray()
    for shade in range(4):
        tiles.extend(bytes((255 if shade & 1 else 0, 255 if shade & 2 else 0)) * 8)
    font_path = Path(__file__).resolve().parents[1] / 'vendor/font8x8_basic.h'
    font = []
    for line in font_path.read_text(encoding='utf-8').splitlines():
        values = re.findall(r'0x[0-9A-Fa-f]{2}', line.split('/*')[0])
        if len(values) == 8:
            font.append([int(value, 16) for value in values])
    if len(font) != 128:
        raise ValueError('Expected the vendored 128-character font8x8 table')
    for glyph in font:
        for row in glyph:
            bits = int(f'{row:08b}'[::-1], 2)
            tiles.extend((bits, bits))
    rom[0x2000:0x2000 + len(tiles)] = tiles
    background = bytearray(1024)
    for y in range(32):
        for x in range(32):
            background[y * 32 + x] = (x // 4 + y // 4) % 4
    def text(x, y, message):
        background[y * 32 + x:y * 32 + x + len(message)] = bytes(ord(c) + 4 for c in message)
    text(2, 3, 'KINDLEBOY')
    text(2, 5, 'DMG TEST')
    text(2, 9, 'A INVERT')
    text(2, 11, 'B NORMAL')
    rom[0x3000:0x3400] = background

    p = Program()
    p.emit(0xF3, 0x31, 0xFE, 0xDF)  # DI; SP=$DFFE
    p.label('initial_vblank')
    p.emit(0xF0, 0x44, 0xFE, 144)
    p.jump('initial_vblank', 0xDA)
    p.emit(0xAF, 0xE0, 0x40, 0xE0, 0x42, 0xE0, 0x43, 0xE0, 0x80)
    p.emit(0x3E, 0x0A, 0xEA, 0, 0x00)  # enable MBC1 RAM
    def copy(label, source, dest, length):
        p.emit(0x21, source & 255, source >> 8, 0x11, dest & 255, dest >> 8,
               0x01, length & 255, length >> 8)
        p.label(label)
        p.emit(0x2A, 0x12, 0x13, 0x0B, 0x78, 0xB1)
        p.jump(label, 0xC2)
    copy('copy_tiles', 0x2000, 0x8000, len(tiles))
    copy('copy_map', 0x3000, 0x9800, len(background))
    p.emit(0x3E, 0xE4, 0xE0, 0x47, 0x3E, 0x91, 0xE0, 0x40)
    p.label('vblank')
    p.emit(0xF0, 0x44, 0xFE, 144)
    p.jump('vblank', 0xDA)
    # Animate one stripe tile while keeping the on-screen labels stationary.
    p.emit(0xF0, 0x80, 0x3C, 0xE0, 0x80, 0xEA, 0, 0xA0,
           0xCB, 0x3F, 0xCB, 0x3F, 0xEA, 0x30, 0x80)
    p.emit(0x3E, 0x10, 0xE0, 0x00, 0xF0, 0x00, 0xF0, 0x00, 0xCB, 0x47)
    p.jump('not_a', 0xC2)
    p.emit(0x3E, 0x1B, 0xE0, 0x47)
    p.label('not_a')
    p.emit(0xF0, 0x00, 0xCB, 0x4F)
    p.jump('not_b', 0xC2)
    p.emit(0x3E, 0xE4, 0xE0, 0x47)
    p.label('not_b')
    p.label('visible')
    p.emit(0xF0, 0x44, 0xFE, 144)
    p.jump('visible', 0xD2)
    p.jump('vblank')
    code = p.finish()
    if 0x150 + len(code) > 0x2000:
        raise ValueError('Program overlaps tile data')
    rom[0x150:0x150 + len(code)] = code
    rom[0x14E:0x150] = struct.pack('>H', sum(rom) & 65535)
    return rom


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', nargs='?', default='build/demo/KindleBoy-Demo.gb')
    path = Path(parser.parse_args().output)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(make_rom())
    print(f'Created {path} (32768 bytes)')
