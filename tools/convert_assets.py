#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Genera source/assets.h: converte i PNG di art/ in array C RGB555 (RGB8).

- carte c01..c30  -> 44x44 (adattate, pixel art NEAREST, letterbox sul colore pannello)
- retro.png       -> 44x44
- icon_r1..r4     -> 16x16
- icon_e1..e12    -> 24x24
- logo.png        -> larghezza <= 96, altezza <= 48 (proporzioni mantenute)
- sfondo.png      -> 256x192 (sfondo titolo)
- icon.bmp        -> 32x32, max 16 colori, per GAME_ICON di ndstool

Formato colore identico alla macro RGB8 di libnds: r>>3 | (g>>3)<<5 | (b>>3)<<10.
Genera anche tools/preview_assets.png per controllo visivo.
"""
import os
import sys
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
ART = os.path.join(REPO, "art")
OUT = os.path.join(REPO, "source", "assets.h")
PREVIEW = os.path.join(HERE, "preview_assets.png")
ICON_BMP = os.path.join(REPO, "icon.bmp")

CARD_W, CARD_H = 44, 44
ICON_R_W, ICON_R_H = 16, 16
ICON_E_W, ICON_E_H = 24, 24
LOGO_MAX_W, LOGO_MAX_H = 96, 48
BG_W, BG_H = 256, 192

# Colori di appoggio (RGB 8 bit)
PANEL = (20, 34, 44)     # pannello carta
BG_COL = (11, 36, 50)    # sfondo scuro (oceanico, come #0b2432)


def rgb555(r, g, b):
    # bit 15 = opacita': senza, i pixel della bitmap Bmp16 sono trasparenti
    return 0x8000 | (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10)


def fit(img, tw, th, bg, cover=False):
    """Adatta l'immagine a twxth.
    cover=True: ritaglia al centro per riempire tutto (niente distorsione).
    cover=False: scala in base al lato minore, centra e letterbox sul colore bg.
    In riduzione usa LANCZOS, in ingrandimento NEAREST (pixel art fedele)."""
    w, h = img.size
    if cover:
        scale = max(tw / w, th / h)
        nw, nh = max(1, int(w * scale)), max(1, int(h * scale))
        img = img.resize((nw, nh), Image.LANCZOS)
        x, y = (nw - tw) // 2, (nh - th) // 2
        return img.crop((x, y, x + tw, y + th))
    scale = min(tw / w, th / h)
    nw, nh = max(1, int(w * scale)), max(1, int(h * scale))
    method = Image.LANCZOS if nw < w else Image.NEAREST
    img = img.resize((nw, nh), method)
    canvas = Image.new("RGB", (tw, th), bg)
    canvas.paste(img, ((tw - nw) // 2, (th - nh) // 2))
    return canvas


def focus(img, frac=0.78):
    """Ritaglia al centro un quadrato (lato = frac*lato minore): isola il
    soggetto (emblema della carta) e scarta il bordo scuro del PNG originale."""
    w, h = img.size
    side = min(w, h) * frac
    x = int((w - side) / 2)
    y = int((h - side) / 2)
    return img.crop((x, y, x + side, y + side))


def to_565(img, gain=1.0):
    w, h = img.size
    px = img.load()
    out = []
    for y in range(h):
        for x in range(w):
            r, g, b = px[x, y][:3]
            r = min(255, int(r * gain))
            g = min(255, int(g * gain))
            b = min(255, int(b * gain))
            out.append(rgb555(r, g, b))
    return out


def emit(name, img, lines, gain=1.0):
    w, h = img.size
    data = to_565(img, gain)
    lines.append("static const unsigned short %s[%d] = {" % (name, len(data)))
    for i in range(0, len(data), 16):
        chunk = data[i:i + 16]
        lines.append("  " + ", ".join("0x%04X" % v for v in chunk) + ",")
    lines.append("};")
    lines.append("#define %s_W %d" % (name, w))
    lines.append("#define %s_H %d" % (name, h))
    return name


def load(name):
    p = os.path.join(ART, name + ".png")
    if not os.path.exists(p):
        raise SystemExit("Manca il PNG: %s" % p)
    return Image.open(p).convert("RGBA")


def main():
    lines = []
    lines.append("/* Generato da tools/convert_assets.py - NON MODIFICARE A MANO */")
    lines.append("#ifndef ASSETS_H")
    lines.append("#define ASSETS_H")
    lines.append("")

    entries = []

    def add(tag, img, gain=1.0):
        name = "assets_%s" % tag
        emit(name, img, lines, gain)
        entries.append((name, img.size))
        lines.append("")

    # Carte: soggetto centrale + un filo di luce in piu' (chiarezza a 44px)
    for i in range(1, 31):
        img = load("c%02d" % i)
        add("c%02d" % i, fit(focus(img), CARD_W, CARD_H, PANEL), 1.15)

    add("retro", fit(focus(load("retro")), CARD_W, CARD_H, PANEL), 1.15)

    for i in range(1, 5):
        add("icon_r%d" % i, fit(focus(load("icon_r%d" % i)), ICON_R_W, ICON_R_H, BG_COL), 1.1)

    for i in range(1, 13):
        add("icon_e%d" % i, fit(focus(load("icon_e%d" % i)), ICON_E_W, ICON_E_H, BG_COL), 1.1)

    add("logo", fit(load("logo"), LOGO_MAX_W, LOGO_MAX_H, BG_COL))
    add("sfondo", fit(load("sfondo"), BG_W, BG_H, BG_COL, cover=True))

    # Indice per il blit: enum ASSET_* (la carta id -> ASSET_CARD_1 + (id-1))
    enum = []
    for i in range(1, 31):
        enum.append("    ASSET_CARD_%d = %d," % (i, i - 1))
    enum.append("    ASSET_RETRO = 30,")
    for i in range(1, 5):
        enum.append("    ASSET_ICON_R%d = %d," % (i, 30 + i))
    for i in range(1, 13):
        enum.append("    ASSET_ICON_E%d = %d," % (i, 34 + i))
    enum.append("    ASSET_LOGO = 47,")
    enum.append("    ASSET_SFONDO = 48,")
    enum.append("    ASSET_COUNT = 49")

    lines.append("enum {")
    lines.extend(enum)
    lines.append("};")
    lines.append("")
    lines.append("static const struct { const unsigned short *px; int w, h; } asset[ASSET_COUNT] = {")
    for name, (w, h) in entries:
        lines.append("  { %s, %d, %d }," % (name, w, h))
    lines.append("};")
    lines.append("")
    lines.append("#endif")

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="ascii", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    print("Scritto: %s" % OUT)

    # Anteprima visiva
    cols = 10
    cell = (CARD_W + 4, CARD_H + 4)
    sheet = Image.new("RGB", (cols * cell[0], 3 * cell[1]), (0, 0, 0))
    previews = [fit(load("c%02d" % i), CARD_W, CARD_H, PANEL) for i in range(1, 31)]
    for i, img in enumerate(previews):
        sheet.paste(img, ((i % cols) * cell[0], (i // cols) * cell[1]))
    ico = [fit(load("icon_e%d" % i), ICON_E_W, ICON_E_H, BG_COL) for i in range(1, 13)]
    for i, img in enumerate(ico):
        sheet.paste(img, ((i % cols) * cell[0], 2 * cell[1] + (i // cols) * 28))
    sheet.save(PREVIEW)
    print("Anteprima asset: %s" % PREVIEW)

    # Icona 32x32 per ndstool (GAME_ICON): max 16 colori
    icon = load("logo")
    icon = icon.resize((32, 32), Image.NEAREST).convert("RGB")
    icon = icon.quantize(colors=16)
    icon.save(ICON_BMP, "BMP")
    print("Icona ndstool: %s" % ICON_BMP)


if __name__ == "__main__":
    main()
