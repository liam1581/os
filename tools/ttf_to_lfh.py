#!/usr/bin/env python3
"""Convert a TrueType font into this project's LFH v1 bitmap font format."""

from __future__ import annotations

import argparse
import struct
from pathlib import Path

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError as exc:
    raise SystemExit("Pillow is required: python3 -m pip install Pillow") from exc

DEFAULT_WIDTH = 15
DEFAULT_HEIGHT = 21
# This order exactly matches fbprint.c:get_glyph().
GLYPH_BYTES = (
    [0x20]
    + list(range(ord("A"), ord("Z") + 1))
    + list(range(ord("0"), ord("9") + 1))
    + [ord(c) for c in [":", ".", ",", "!", "-", "/", "_", "?", "\\", "\"", "#", "$", "%", "&", "'", "(", ")", "*", "+", ";", "<", "=", ">", "@", "[", "]", "^", "`", "{", "|", "}", "~"]]
    + list(range(ord("a"), ord("z") + 1))
    + [0x80, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
       0x8A, 0x8B, 0x8C, 0x8E, 0x91, 0x92, 0x93, 0x94, 0x95,
       0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9E, 0x9F]
    + list(range(0xA0, 0x100))
)
assert len(GLYPH_BYTES) == 218


def cp1252_character(code: int) -> str:
    return bytes([code]).decode("cp1252")


def render_glyph(font: ImageFont.FreeTypeFont, char: str, width: int, height: int,
                 threshold: int) -> bytes:
    image = Image.new("L", (width, height), 0)
    draw = ImageDraw.Draw(image)
    left, top, right, bottom = font.getbbox(char)
    glyph_width = right - left
    glyph_height = bottom - top
    draw.text(((width - glyph_width) // 2 - left, (height - glyph_height) // 2 - top),
              char, font=font, fill=255)

    row_bytes = (width + 7) // 8
    packed = bytearray(height * row_bytes)
    for y in range(height):
        for x in range(width):
            if image.getpixel((x, y)) >= threshold:
                packed[y * row_bytes + x // 8] |= 1 << (7 - (x % 8))
    return bytes(packed)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("font", type=Path, help="source .ttf font")
    parser.add_argument("output", type=Path, help="destination .lfh file")
    parser.add_argument("--width", type=int, default=DEFAULT_WIDTH)
    parser.add_argument("--height", type=int, default=DEFAULT_HEIGHT)
    parser.add_argument("--font-px", type=int, default=19,
                        help="TrueType rasterization size in pixels (default: 19)")
    parser.add_argument("--threshold", type=int, default=128,
                        help="grayscale value (0-255) considered an on pixel")
    args = parser.parse_args()

    if not (1 <= args.width <= 255 and 1 <= args.height <= 255):
        parser.error("width and height must fit in one LFH header byte")
    if not (0 <= args.threshold <= 255):
        parser.error("threshold must be between 0 and 255")

    font = ImageFont.truetype(str(args.font), args.font_px)
    glyph_data = b"".join(
        render_glyph(font, cp1252_character(code), args.width, args.height, args.threshold)
        for code in GLYPH_BYTES
    )
    # LFH v1: start, LFH\\0 signature, width, height, character count,
    # extended-ASCII flag, end. Glyph rows are packed MSB first.
    header = struct.pack("<B4sBBBBB", 0xFF, b"LFH\x00", args.width, args.height,
                         len(GLYPH_BYTES), 1, 0xFF)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(header + glyph_data)
    print(f"Wrote {args.output} ({len(GLYPH_BYTES)} glyphs, {args.width}x{args.height}, "
          f"{(args.width + 7) // 8} bytes per row).")


if __name__ == "__main__":
    main()