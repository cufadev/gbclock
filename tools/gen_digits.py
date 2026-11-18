#!/usr/bin/env python3
"""Genera src/digits.c con los 10 digitos como tarjetas de 32x64 px (estilo Fliqlo).

Formato: por cada digito, 64 filas x 8 bytes. Cada fila son 4 tiles de 8 px,
y cada tile-fila ocupa 2 bytes (formato 2bpp de Game Boy: plano bajo, plano alto).
Colores de la tarjeta (paleta DMG por defecto): 0 = blanco (digito), 2 = gris oscuro
(tarjeta), 3 = negro (fondo y hendija del medio).
"""
import sys
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

FONT = "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf"
W, H = 32, 64
GLYPH_H = 44          # alto del digito dentro de la tarjeta
INSET = 2             # px negros a cada lado: la tarjeta visible mide W - 2*INSET
VW = W - 2 * INSET
BG, CARD, INK = 3, 2, 0


def glyph_mask(ch):
    size = 80
    f = ImageFont.truetype(FONT, size)
    img = Image.new("L", (200, 200), 0)
    ImageDraw.Draw(img).text((20, 20), ch, font=f, fill=255)
    box = img.getbbox()
    img = img.crop(box)
    # escalar para que el alto sea GLYPH_H y que entre en el ancho (con margen)
    scale = GLYPH_H / img.height
    nw = max(1, round(img.width * scale))
    if nw > VW - 6:
        scale = (VW - 6) / img.width
        nw = VW - 6
    nh = round(img.height * scale)
    img = img.resize((nw, nh), Image.LANCZOS)
    return img.point(lambda v: 255 if v >= 128 else 0)


def card(ch):
    px = [[CARD if INSET <= x < W - INSET else BG for x in range(W)] for _ in range(H)]
    # esquinas redondeadas (3 px) de cada mitad
    corner = [(0, 0), (1, 0), (2, 0), (0, 1), (0, 2), (1, 1)]
    for (dx, dy) in corner:
        for (x, y) in ((INSET + dx, dy), (W - 1 - INSET - dx, dy), (INSET + dx, H - 1 - dy), (W - 1 - INSET - dx, H - 1 - dy)):
            px[y][x] = BG
    # hendija central (la bisagra del flip)
    for x in range(W):
        px[H // 2 - 1][x] = BG
    # digito centrado
    m = glyph_mask(ch)
    ox = (W - m.width) // 2
    oy = (H - m.height) // 2
    for y in range(m.height):
        for x in range(m.width):
            if m.getpixel((x, y)):
                px[oy + y][ox + x] = INK
    # la hendija corta el digito
    for x in range(W):
        px[H // 2 - 1][x] = BG
    return px


def encode(px):
    out = []
    for y in range(H):
        for tx in range(W // 8):
            lo = hi = 0
            for b in range(8):
                c = px[y][tx * 8 + b]
                lo = (lo << 1) | (c & 1)
                hi = (hi << 1) | ((c >> 1) & 1)
            out += [lo, hi]
    return out


def preview(px, name):
    sym = {0: "#", 1: "+", 2: ".", 3: " "}
    print(name)
    for row in px:
        print("".join(sym[c] for c in row))


def main():
    data = []
    for d in "0123456789":
        px = card(d)
        if len(sys.argv) > 1 and sys.argv[1] == d:
            preview(px, d)
        data.append(encode(px))
    lines = ["/* Generado por tools/gen_digits.py - no editar a mano */",
             "#include <stdint.h>", "",
             "const uint8_t digit_cards[10][512] = {"]
    for rows in data:
        lines.append("  {")
        for i in range(0, 512, 16):
            lines.append("    " + ",".join(f"0x{b:02X}" for b in rows[i:i + 16]) + ",")
        lines.append("  },")
    lines.append("};")
    Path(__file__).resolve().parent.parent.joinpath("src", "digits.c").write_text("\n".join(lines) + "\n")
    print("src/digits.c generado")


main()
