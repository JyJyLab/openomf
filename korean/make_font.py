#!/usr/bin/env python3
"""Build KOREAN.FNT (Hangul bitmap glyphs for OpenOMF) from the Galmuri pixel fonts (SIL OFL 1.1).

  FONT_SMALL (OMF 6px font)  <- Galmuri7 rendered at 8px  -> 7x7 glyphs
  FONT_BIG   (OMF 8px font)  <- Galmuri9 rendered at 10px -> 9x9 glyphs

Latin text keeps the original OMF fonts; only codepoints listed here are drawn from this file.

File format (little endian):
  "OMFKOFNT" (8 bytes), u16 table count
  per table: u8 font_size (1 = FONT_BIG, 2 = FONT_SMALL), u8 width, u8 height, u8 bytes_per_row, u32 glyph count,
             u32 codepoints[count] (ascending), bitmaps[count][height][bytes_per_row] (MSB = leftmost pixel)

usage: make_font.py <Galmuri7.ttf> <Galmuri9.ttf> <out KOREAN.FNT>
"""
import struct
import sys

from PIL import Image, ImageDraw, ImageFont

CODEPOINTS = sorted(list(range(0xAC00, 0xD7A4))      # Hangul syllables
              + list(range(0x3131, 0x318F))    # compatibility jamo
              + [0x00B7, 0x2026, 0x2018, 0x2019, 0x201C, 0x201D, 0x300C, 0x300D, 0x2192, 0x2190, 0x00D7, 0x203B])  # sorted: the engine looks glyphs up by binary search


def table(ttf, px, size_id, w, h, dy=-1):
    font = ImageFont.truetype(ttf, px)
    if dy is None:
        # outline fonts: fit the tallest syllables into the cell
        bbs = [font.getbbox(c) for c in '힘뷁갉똥를젊았']
        top, bot = min(b[1] for b in bbs), max(b[3] for b in bbs)
        dy = -top - max(0, (bot - top) - h) // 2
    bpr = (w + 7) // 8
    cps, bits = [], bytearray()
    for cp in CODEPOINTS:
        im = Image.new('1', (w + 4, h + 4), 0)
        d = ImageDraw.Draw(im)
        d.fontmode = '1'
        d.text((0, dy), chr(cp), font=font, fill=1)  # Galmuri leaves one empty row on top (dy -1)
        px_ = im.load()
        if im.getbbox() is None:
            continue
        cps.append(cp)
        for y in range(h):
            row = 0
            for x in range(w):
                if px_[x, y]:
                    row |= 1 << (bpr * 8 - 1 - x)
            bits += row.to_bytes(bpr, 'big')
    head = struct.pack('<BBBBI', size_id, w, h, bpr, len(cps))
    return head + struct.pack('<%dI' % len(cps), *cps) + bytes(bits), len(cps)


def main():
    g7, g9, out = sys.argv[1:4]
    big, nb = table(g9, 10, 1, 9, 9)
    small, ns = table(g7, 8, 2, 7, 7)
    with open(out, 'wb') as f:
        f.write(b'OMFKOFNT' + struct.pack('<H', 2) + big + small)
    print('KOREAN.FNT: big %d glyphs, small %d glyphs -> %s' % (nb, ns, out))


if __name__ == '__main__':
    main()
