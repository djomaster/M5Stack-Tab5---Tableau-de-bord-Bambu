#!/usr/bin/env python3
"""Génère les polices embarquées (src/assets/font_*.h) à partir de Liberation Sans.

Sous-ensemble : latin de base, latin-1 (accents français), ponctuation courante.
Usage : python3 tools/make_font_headers.py
Licence de la police : SIL Open Font License 1.1 (Liberation Fonts).
"""
import io
import os
import sys
from fontTools import subset

SRC = "/usr/share/fonts/truetype/liberation2"
OUT = os.path.join(os.path.dirname(__file__), "..", "src", "assets")
FONTS = {"font_regular": "LiberationSans-Regular.ttf", "font_bold": "LiberationSans-Bold.ttf"}
UNICODES = (
    list(range(0x20, 0x7F))
    + list(range(0xA0, 0x100))
    + [0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2026, 0x2039, 0x203A, 0x20AC, 0x0152, 0x0153]
)

def build(name, file):
    opts = subset.Options()
    opts.layout_features = ["kern"]
    opts.hinting = False
    opts.notdef_outline = True
    font = subset.load_font(os.path.join(SRC, file), opts)
    sub = subset.Subsetter(opts)
    sub.populate(unicodes=UNICODES)
    sub.subset(font)
    buf = io.BytesIO()
    subset.save_font(font, buf, opts)
    data = buf.getvalue()
    lines = []
    for i in range(0, len(data), 24):
        lines.append("  " + ",".join("0x%02x" % b for b in data[i:i + 24]) + ",")
    path = os.path.join(OUT, name + ".h")
    with open(path, "w", newline="\n") as f:
        f.write("// Généré par tools/make_font_headers.py : ne pas modifier à la main.\n")
        f.write("// Source : %s (SIL Open Font License 1.1)\n#pragma once\n#include <stdint.h>\n#include <stddef.h>\n\n" % file)
        f.write("static const uint8_t %s_ttf[] __attribute__((aligned(4))) = {\n" % name)
        f.write("\n".join(lines))
        f.write("\n};\nstatic const size_t %s_ttf_len = sizeof(%s_ttf);\n" % (name, name))
    print(name, len(data), "octets")

for n, f in FONTS.items():
    build(n, f)
