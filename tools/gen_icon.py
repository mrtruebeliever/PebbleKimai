#!/usr/bin/env python3
"""Generates PebbleKimai's icons: a stopwatch (time tracking), Pebble-style.

Flat design in the mid-2010s Pebble appstore look: a bold white glyph on a
rounded Kimai-green tile with a 45-degree flat long shadow; the launcher menu
icon is a bold dark silhouette on transparent.

Outputs:
  resources/images/menu_icon.png   25x25   dark silhouette on transparent
  store/icon-80.png                80x80    white glyph on a green tile
  store/icon-144.png               144x144  white glyph on a green tile

Everything is rendered supersampled and downsampled for clean edges. Run from
the project root inside the icon venv:
    source .art-venv/bin/activate && python3 tools/gen_icon.py
"""
import math
import os
from PIL import Image, ImageChops, ImageDraw

SS = 6
ACCENT = (0x55, 0xAA, 0x00, 255)   # Kimai-ish green (matches theme.h ACCENT_HEX)
INK_DARK = (34, 34, 34, 255)       # near-black launcher silhouette
WHITE = (255, 255, 255, 255)


def draw_stopwatch(W, ink, ticks=True, bold=1.0):
    """Draws a stopwatch filling a WxW transparent RGBA image in colour `ink`."""
    img = Image.new("RGBA", (W, W), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)

    def s(v):
        return v * W

    lw = int(round(s(0.075 * bold)))          # main stroke
    cx, cy, r = s(0.5), s(0.585), s(0.335)     # watch body

    # Top button (crown) + stem.
    bw, bt, bb = s(0.135), s(0.05), s(0.145)
    d.rounded_rectangle([cx - bw, bt, cx + bw, bb], radius=s(0.035), fill=ink)
    d.line([(cx, bb), (cx, cy - r - lw * 0.3)], fill=ink, width=lw)

    # Start button nub at ~1-2 o'clock.
    na = math.radians(52)
    nx, ny = cx + math.cos(na) * r, cy - math.sin(na) * r
    d.line([(nx, ny), (nx + s(0.085), ny - s(0.085))], fill=ink, width=lw)

    # Watch body ring.
    d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=ink, width=lw)

    # Tick marks at 12 / 3 / 6 / 9.
    if ticks:
        tw = max(1, int(round(lw * 0.85)))
        for ang in (90, 0, 270, 180):
            a = math.radians(ang)
            x1, y1 = cx + math.cos(a) * r * 0.80, cy - math.sin(a) * r * 0.80
            x2, y2 = cx + math.cos(a) * r * 0.62, cy - math.sin(a) * r * 0.62
            d.line([(x1, y1), (x2, y2)], fill=ink, width=tw)

    # Hand to ~1 o'clock + centre hub.
    ha = math.radians(58)
    hx, hy = cx + math.cos(ha) * r * 0.60, cy - math.sin(ha) * r * 0.60
    d.line([(cx, cy), (hx, hy)], fill=ink, width=lw)
    hub = s(0.05)
    d.ellipse([cx - hub, cy - hub, cx + hub, cy + hub], fill=ink)

    return img


def long_shadow(glyph, tile_mask, length, alpha=48):
    """A flat 45-degree long shadow from `glyph`'s silhouette, clipped to the
    tile, as a semi-transparent black RGBA layer."""
    W = glyph.width
    src = glyph.split()[-1]
    acc = Image.new("L", (W, W), 0)
    step = max(1, W // 260)
    for i in range(step, length + 1, step):
        shifted = Image.new("L", (W, W), 0)
        shifted.paste(src, (i, i))
        acc = ImageChops.lighter(acc, shifted)
    acc = ImageChops.subtract(acc, src)          # shadow sits behind the glyph
    acc = ImageChops.multiply(acc, tile_mask)    # keep it on the tile
    acc = acc.point(lambda v: int(v * alpha / 255))
    shadow = Image.new("RGBA", (W, W), (0, 0, 0, 0))
    shadow.putalpha(acc)
    return shadow


def store_icon(size):
    W = size * SS
    tile = Image.new("RGBA", (W, W), (0, 0, 0, 0))
    d = ImageDraw.Draw(tile)
    d.rounded_rectangle([0, 0, W - 1, W - 1], radius=int(W * 0.22), fill=ACCENT)
    tile_mask = tile.split()[-1]

    glyph = draw_stopwatch(W, WHITE, ticks=True, bold=1.0)

    out = Image.new("RGBA", (W, W), (0, 0, 0, 0))
    out.alpha_composite(tile)
    out.alpha_composite(long_shadow(glyph, tile_mask, length=int(W * 0.9)))
    out.alpha_composite(glyph)
    return out.resize((size, size), Image.LANCZOS)


def menu_icon(size):
    W = size * SS
    glyph = draw_stopwatch(W, INK_DARK, ticks=False, bold=1.25)
    return glyph.resize((size, size), Image.LANCZOS)


def main():
    os.makedirs("resources/images", exist_ok=True)
    os.makedirs("store", exist_ok=True)

    menu_icon(25).save("resources/images/menu_icon.png")
    print("wrote resources/images/menu_icon.png (25x25)")

    for sz in (80, 144):
        store_icon(sz).save("store/icon-%d.png" % sz)
        print("wrote store/icon-%d.png (%dx%d)" % (sz, sz, sz))


if __name__ == "__main__":
    main()
