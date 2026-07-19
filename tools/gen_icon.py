#!/usr/bin/env python3
"""Generates PebbleKimai's icons: a stopwatch (time tracking).

Outputs:
  resources/images/menu_icon.png   25x25  dark ink on transparent (launcher)
  store/icon-80.png                80x80   white on a rounded Kimai-green tile
  store/icon-144.png               144x144 white on a rounded Kimai-green tile

Everything is rendered at 4x and downsampled for anti-aliasing. Run from the
project root inside the icon venv:
    source .art-venv/bin/activate && python3 tools/gen_icon.py
"""
import math
import os
from PIL import Image, ImageDraw

SS = 4
ACCENT = (0x55, 0xAA, 0x00, 255)   # Kimai-ish green (matches theme.h ACCENT_HEX)
INK_DARK = (40, 40, 40, 255)       # near-black, visible on the white launcher


def draw_stopwatch(size, ink, bg=None, tile=False):
    """Renders a stopwatch `size`x`size`. `ink` draws the watch; `bg` fills a
    rounded tile behind it when `tile` is True, else the background is clear."""
    W = size * SS
    img = Image.new("RGBA", (W, W), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)

    def s(v):
        return int(round(v * W))

    if tile and bg:
        d.rounded_rectangle([0, 0, W - 1, W - 1], radius=s(0.22), fill=bg)

    lw = max(1, s(0.055))            # stroke width

    # Watch body.
    cx, cy, r = s(0.5), s(0.58), s(0.34)
    d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=ink, width=lw)

    # Top button + stem.
    bw = s(0.14)
    d.rounded_rectangle([cx - bw, s(0.06), cx + bw, s(0.15)], radius=s(0.03), fill=ink)
    d.line([cx, s(0.15), cx, cy - r], fill=ink, width=lw)

    # Side start button (top-right diagonal nub).
    nx, ny = cx + int(r * 0.72), cy - int(r * 0.72)
    d.line([nx, ny, nx + s(0.09), ny - s(0.09)], fill=ink, width=lw)

    # Tick marks at 12 / 3 / 6 / 9.
    for ang in (90, 0, 270, 180):
        a = math.radians(ang)
        x1 = cx + int(math.cos(a) * r * 0.82)
        y1 = cy - int(math.sin(a) * r * 0.82)
        x2 = cx + int(math.cos(a) * r * 0.66)
        y2 = cy - int(math.sin(a) * r * 0.66)
        d.line([x1, y1, x2, y2], fill=ink, width=lw)

    # Hand pointing to ~1-2 o'clock + centre hub.
    ha = math.radians(52)
    hx = cx + int(math.cos(ha) * r * 0.62)
    hy = cy - int(math.sin(ha) * r * 0.62)
    d.line([cx, cy, hx, hy], fill=ink, width=max(1, s(0.05)))
    hub = s(0.045)
    d.ellipse([cx - hub, cy - hub, cx + hub, cy + hub], fill=ink)

    return img.resize((size, size), Image.LANCZOS)


def main():
    os.makedirs("resources/images", exist_ok=True)
    os.makedirs("store", exist_ok=True)

    draw_stopwatch(25, INK_DARK).save("resources/images/menu_icon.png")
    print("wrote resources/images/menu_icon.png (25x25)")

    for sz in (80, 144):
        draw_stopwatch(sz, (255, 255, 255, 255), bg=ACCENT, tile=True).save(
            "store/icon-%d.png" % sz)
        print("wrote store/icon-%d.png (%dx%d)" % (sz, sz, sz))


if __name__ == "__main__":
    main()
