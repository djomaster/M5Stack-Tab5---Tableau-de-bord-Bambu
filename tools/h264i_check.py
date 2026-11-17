#!/usr/bin/env python3
"""Verifie le decodeur H.264 intra (src/h264i.cpp) contre ffmpeg, octet pour octet.

Prerequis sur le PC de developpement : ffmpeg (avec libx264), g++, python3 + numpy.
Usage : python tools/h264i_check.py
Le filtre de deblocage n'est pas implemente : ffmpeg est donc lance avec -skip_loop_filter all.
"""
import os, subprocess, sys, tempfile
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "..", "src")

CASES = [  # nom, source lavfi, options x264, largeur, hauteur
    ("I16 seul 64x64", "testsrc2=size=64x64:rate=30", "cabac=1:8x8dct=0:partitions=none", 64, 64),
    ("I4x4 64x64", "testsrc2=size=64x64:rate=30", "cabac=1:8x8dct=0:partitions=i4x4", 64, 64),
    ("I8x8 64x64", "testsrc2=size=64x64:rate=30", "cabac=1:8x8dct=1:partitions=i8x8,i4x4", 64, 64),
    ("High 1680x1080", "testsrc2=size=1680x1080:rate=30", "cabac=1:8x8dct=1", 1680, 1080),
    ("mandelbrot crf18", "mandelbrot=size=1680x1080:rate=30", "cabac=1:8x8dct=1:crf=18", 1680, 1080),
    ("1920x1080 recadre qp10", "mandelbrot=size=1920x1080:rate=30", "cabac=1:8x8dct=1:qp=10", 1920, 1080),
    ("4 tranches qp40", "mandelbrot=size=1680x1080:rate=30", "cabac=1:8x8dct=1:qp=40:slices=4", 1680, 1080),
    ("8 tranches 1760x1080", "testsrc2=size=1760x1080:rate=30", "cabac=1:8x8dct=1:crf=30:slices=8", 1760, 1080),
    ("bruit qp20 (niveaux d'echappement)", "nullsrc=size=1680x1080:rate=30,geq=random(1)*255:128:128", "cabac=1:8x8dct=1:qp=20", 1680, 1080),
]


def run(cmd, **kw):
    return subprocess.run(cmd, check=True, capture_output=True, **kw)


def main():
    tmp = tempfile.mkdtemp(prefix="h264i_")
    exe = os.path.join(tmp, "h264i_test")
    run(["g++", "-O2", "-Wall", "-o", exe, os.path.join(HERE, "h264i_test.cpp"), os.path.join(SRC, "h264i.cpp"), "-I" + SRC])
    st = os.path.join(tmp, "cabac_selftest")
    run(["g++", "-O1", "-w", "-o", st, os.path.join(HERE, "cabac_selftest.cpp"), "-I" + SRC])
    print(run([st]).stdout.decode().strip())
    bad = 0
    for name, src, x264, w, h in CASES:
        es, yuv, ref = (os.path.join(tmp, n) for n in ("a.h264", "a.yuv", "r.yuv"))
        run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-f", "lavfi", "-i", src, "-t", "1", "-c:v", "libx264",
             "-profile:v", "high", "-g", "30", "-bf", "0", "-x264-params", x264 + ":keyint=30", "-pix_fmt", "yuv420p", es])
        run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-skip_loop_filter", "all", "-i", es, "-frames:v", "1",
             "-pix_fmt", "yuv420p", ref])
        p = subprocess.run([exe, es, yuv, "0"], capture_output=True)
        if p.returncode != 0:
            print("ECHEC ", name, p.stderr.decode().strip())
            bad += 1
            continue
        a, b = np.fromfile(yuv, np.uint8), np.fromfile(ref, np.uint8)
        n = int((a != b).sum()) if a.size == b.size else -1
        print("%-6s %s  (%s)" % ("OK" if n == 0 else "ECART", name, p.stderr.decode().strip()))
        bad += n != 0
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
