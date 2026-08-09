#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Genera source/font_8x8.h: font bitmap 8x8 monospazio da un TTF di sistema.
Solo glifi verificati e usati dal gioco: ASCII 0x20-0x7E + accenti latini
(0xE0 a, 0xE8 e, 0xE9 e, 0xEC i, 0xF2 o, 0xF9 u).
Genera anche un'anteprima PNG (tools/preview_font.png) per controllare la resa.
"""
import os
import sys
from PIL import Image, ImageFont, ImageDraw

OUT = os.path.join(os.path.dirname(__file__), "..", "source", "font_8x8.h")
PREVIEW = os.path.join(os.path.dirname(__file__), "preview_font.png")

W, H = 8, 8
CHARS = list(range(0x20, 0x7F)) + [0xE0, 0xE8, 0xE9, 0xEC, 0xF2, 0xF9]

FONTS = [
    "consola.ttf",
    "cour.ttf",
    "arial.ttf",
]
FONT_DIR = os.path.join(os.environ.get("WINDIR", r"C:\Windows"), "Fonts")


def find_font():
    for name in FONTS:
        p = os.path.join(FONT_DIR, name)
        if os.path.exists(p):
            return p
    raise SystemExit("Nessun TTF trovato tra: %s" % ", ".join(FONTS))


def rasterize(font, c):
    """Rende il glifo a 32px, lo scala a 8x8 conservando l'aspetto e lo
    centra nel reticolo. 1 bit, soglia 128."""
    cell = 32
    img = Image.new("L", (cell, cell), 0)
    d = ImageDraw.Draw(img)
    d.text((0, 0), chr(c), font=font, fill=255)
    bbox = img.getbbox()
    if bbox is None:
        return Image.new("L", (W, H), 0)
    crop = img.crop(bbox)
    scale = min(W / crop.width, H / crop.height)
    nw = max(1, int(round(crop.width * scale)))
    nh = max(1, int(round(crop.height * scale)))
    crop = crop.resize((nw, nh), Image.LANCZOS)
    out = Image.new("L", (W, H), 0)
    out.paste(crop, ((W - nw) // 2, (H - nh) // 2))
    px = out.load()
    for y in range(H):
        for x in range(W):
            px[x, y] = 255 if px[x, y] >= 128 else 0
    return out


def load_glyphs(path):
    font = ImageFont.truetype(path, 32)
    return {c: rasterize(font, c) for c in CHARS}


def build_preview(glyphs):
    n = len(glyphs)
    cols = 16
    rows = (n + cols - 1) // cols
    sheet = Image.new("L", (cols * (W + 2) + 2, rows * (H + 2) + 2), 0)
    d = ImageDraw.Draw(sheet)
    for i, c in enumerate(CHARS):
        x = 2 + (i % cols) * (W + 2)
        y = 2 + (i // cols) * (H + 2)
        sheet.paste(glyphs[c], (x, y))
    sheet = sheet.resize((sheet.width * 4, sheet.height * 4), Image.NEAREST)
    sheet.save(PREVIEW)
    print("Anteprima font: %s" % PREVIEW)


def main():
    font_path = find_font()
    print("Font usato: %s" % font_path)
    glyphs = load_glyphs(font_path)
    build_preview(glyphs)

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    lines = []
    lines.append("/* Generato da tools/make_font.py - NON MODIFICARE A MANO */")
    lines.append("#ifndef FONT_8X8_H")
    lines.append("#define FONT_8X8_H")
    lines.append("/* Font 8x8 1-bit: bit 7 = pixel piu a sinistra della riga. */")
    lines.append("static const unsigned char font_8x8[256][8] = {")
    for c in range(256):
        if c in glyphs:
            img = glyphs[c]
            px = img.load()
            row = []
            for y in range(H):
                v = 0
                for x in range(W):
                    if px[x, y]:
                        v |= 0x80 >> x
                row.append(v)
            lines.append("  {%s}, /* %d '%s' */" % (
                ", ".join("0x%02X" % b for b in row), c,
                chr(c) if 32 <= c <= 126 else "\\x%02X" % c))
        else:
            lines.append("  {0,0,0,0,0,0,0,0}, /* %d */" % c)
    lines.append("};")
    lines.append("#endif")
    with open(OUT, "w", encoding="ascii", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    print("Scritto: %s" % OUT)


if __name__ == "__main__":
    main()
