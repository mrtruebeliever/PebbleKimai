#!/usr/bin/env python3
"""Generates the hero's two status icons (20x20, black on transparent).

  resources/images/status_running.png   clock face  -> a timer is running
  resources/images/status_last.png      history arc -> this is what ran last

Supersampled and downsampled for clean edges, like tools/gen_icon.py. Run from
the project root inside the icon venv:
    .art-venv/bin/python tools/gen_status_icons.py
"""
import math
from PIL import Image, ImageDraw

SS = 8
W = 20
INK = (0, 0, 0, 255)


def new_canvas():
    return Image.new("RGBA", (W * SS, W * SS), (0, 0, 0, 0))


def running():
    """A clock face with hands: time is being counted right now."""
    img = new_canvas()
    d = ImageDraw.Draw(img)
    S = W * SS
    lw = int(S * 0.10)
    r = S * 0.42
    cx = cy = S / 2
    d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=INK, width=lw)
    # Hands at 12 and 4 o'clock.
    d.line([(cx, cy), (cx, cy - r * 0.62)], fill=INK, width=lw)
    a = math.radians(-25)
    d.line([(cx, cy), (cx + math.cos(a) * r * 0.58, cy - math.sin(a) * r * 0.58)],
           fill=INK, width=lw)
    return img


def last():
    """A counter-clockwise arc with an arrow head: what ran before."""
    img = new_canvas()
    d = ImageDraw.Draw(img)
    S = W * SS
    lw = int(S * 0.10)
    r = S * 0.40
    cx = cy = S / 2
    # Arc open at the top-left, so the arrow head has room.
    d.arc([cx - r, cy - r, cx + r, cy + r], start=-60, end=200, fill=INK, width=lw)
    # Arrow head at the arc's start (top-left), pointing counter-clockwise.
    a = math.radians(200)
    hx, hy = cx + math.cos(a) * r, cy + math.sin(a) * r
    h = S * 0.17
    d.polygon([(hx, hy - h), (hx - h * 0.9, hy + h * 0.25), (hx + h * 0.9, hy + h * 0.25)],
              fill=INK)
    return img


for name, fn in (("status_running", running), ("status_last", last)):
    img = fn().resize((W, W), Image.LANCZOS)
    path = "resources/images/%s.png" % name
    img.save(path)
    print("wrote", path)
