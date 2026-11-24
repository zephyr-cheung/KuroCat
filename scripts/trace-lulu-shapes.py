#!/usr/bin/env python3
"""Trace the Lulu (水豚噜噜) idle reference frame into flat vector polygons.

Source artwork: https://github.com/czy666chen/lulu (MIT, (c) 2026 czy666chen).
The generated header contains polygon point lists only; no bitmap is shipped or
loaded at runtime. Regenerate with:

    python scripts/trace-lulu-shapes.py path/to/spritesheet.webp

(Only needed when the reference art or the trace settings change.)
"""
import sys
from pathlib import Path

import cv2
import numpy as np

CELL_W, CELL_H = 192, 208
SCALE = 4  # trace at 4x for smoother curves, then divide the points back
# Idle frame 0 of the 8x11 sprite sheet.
FRAME_ROW, FRAME_COL = 0, 0

HEAD_ORIGIN = (95.0, 80.0)     # cell-space head anchor
BODY_ORIGIN = (95.0, 157.0)    # cell-space body anchor

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "windows" / "src" / "LuluShapes.hpp"


def simplify(contour, eps=0.9):
    approx = cv2.approxPolyDP(contour, eps, True)
    return [(float(p[0][0]), float(p[0][1])) for p in approx]


def smooth(pts, passes=3, window=1):
    """Circular moving average: removes pixel-scale fringe without losing the silhouette."""
    for _ in range(passes):
        n = len(pts)
        if n < 5:
            return pts
        out = []
        for i in range(n):
            xs = [pts[(i + k) % n][0] for k in range(-window, window + 1)]
            ys = [pts[(i + k) % n][1] for k in range(-window, window + 1)]
            out.append((sum(xs) / len(xs), sum(ys) / len(ys)))
        pts = out
    return pts


def largest(contours, min_area=80):
    contours = [c for c in contours if cv2.contourArea(c) >= min_area]
    if not contours:
        return None
    return max(contours, key=cv2.contourArea)


def main():
    if len(sys.argv) < 2:
        raise SystemExit("usage: trace-lulu-shapes.py <spritesheet.webp>")
    img = cv2.imread(sys.argv[1], cv2.IMREAD_UNCHANGED)
    if img is None:
        raise SystemExit("cannot read sprite sheet")
    cell = img[FRAME_ROW * CELL_H:(FRAME_ROW + 1) * CELL_H,
               FRAME_COL * CELL_W:(FRAME_COL + 1) * CELL_W]
    cell = cv2.resize(cell, (CELL_W * SCALE, CELL_H * SCALE), interpolation=cv2.INTER_CUBIC)
    b, g, r, a = cv2.split(cell)
    rgb = cv2.merge([r, g, b])
    hsv = cv2.cvtColor(rgb, cv2.COLOR_RGB2HSV)
    solid = (a > 200).astype(np.uint8) * 255
    solid = cv2.morphologyEx(solid, cv2.MORPH_CLOSE, np.ones((5, 5), np.uint8))

    yellow = cv2.bitwise_and(cv2.inRange(hsv, (20, 90, 150), (40, 255, 255)), solid)
    orange = cv2.bitwise_and(cv2.inRange(hsv, (5, 150, 150), (20, 255, 255)), solid)
    green = cv2.bitwise_and(cv2.inRange(hsv, (35, 80, 60), (90, 255, 255)), solid)

    yarn = cv2.bitwise_or(yellow, cv2.bitwise_or(orange, green))
    yarn = cv2.morphologyEx(yarn, cv2.MORPH_CLOSE, np.ones((7, 7), np.uint8))
    yy = np.arange(CELL_H * SCALE)[:, None] * np.ones((1, CELL_W * SCALE), np.int32)
    def rows(lo, hi):
        return ((yy >= lo * SCALE) & (yy < hi * SCALE)).astype(np.uint8) * 255

    def region(mask, lo, hi):
        return cv2.bitwise_and(mask, rows(lo, hi))

    def poly(mask, lo, hi, eps=2.2, min_area=80):
        m = region(mask, lo, hi)
        m = cv2.morphologyEx(m, cv2.MORPH_CLOSE, np.ones((7, 7), np.uint8))
        cnts, _ = cv2.findContours(m, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        c = largest(cnts, min_area * SCALE * SCALE)
        if c is None:
            return []
        pts = smooth(simplify(c, eps), passes=3, window=1)
        arr = np.array(pts, dtype=np.float32).reshape(-1, 1, 2)
        pts = simplify(arr.astype(np.int32), eps * 0.5)
        return [(x / SCALE, y / SCALE) for x, y in pts]

    # Head silhouette (crown cut just below the hat brim), muzzle, hat.
    # Trace the full outer silhouette, including the orange muzzle edge, from
    # alpha. The muzzle is drawn again as its own layer; this underlay prevents
    # small gaps when head and muzzle are rotated independently.
    head = poly(solid, 36, 130, eps=2.2)
    muzzle = poly(orange, 45, 133, eps=2.0)
    hat = poly(orange, 0, 33, eps=2.0, min_area=200)
    # The tiny leaf is easier and cleaner to draw procedurally in the renderer.
    leaf = []

    # Torso: erase the side arms so procedural arms can animate on the keyboard.
    torso_mask = region(yellow, 120, 178)
    arm_free = torso_mask.copy()
    arm_free[:, :54 * SCALE] = 0
    arm_free[:, 138 * SCALE:] = 0
    torso = poly(arm_free, 120, 178, eps=2.2, min_area=300)
    shorts = poly(orange, 150, 208, eps=2.2, min_area=300)
    leg_l = poly(yellow, 178, 208, eps=0.7, min_area=40)
    # Split the leg contour by x if both legs merged into one blob.
    legs = region(yellow, 178, 208)
    cnts, _ = cv2.findContours(legs, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    cnts = sorted([c for c in cnts if cv2.contourArea(c) > 40 * SCALE * SCALE],
                  key=lambda c: cv2.boundingRect(c)[0])
    def scaled(c):
        return [(x / SCALE, y / SCALE) for x, y in simplify(c, 2.2)]
    leg_l = scaled(cnts[0]) if len(cnts) >= 1 else []
    leg_r = scaled(cnts[1]) if len(cnts) >= 2 else []

    def cpp(points, origin):
        pts = ", ".join("{%.2ff, %.2ff}" % (x - origin[0], y - origin[1]) for x, y in points)
        return "    { " + pts + " }"

    def block(name, points, origin):
        return f"inline const std::vector<LuluPoint>& {name}() {{\n    static const std::vector<LuluPoint> p = " + cpp(points, origin) + ";\n    return p;\n}\n"

    header = """// Generated by scripts/trace-lulu-shapes.py — do not edit by hand.
// Flat vector outlines traced from the MIT-licensed Lulu reference art
// (https://github.com/czy666chen/lulu, (c) 2026 czy666chen). No bitmaps are shipped.
#pragma once
#include <vector>

struct LuluPoint { float x, y; };

"""
    for name, pts, org in [
        ("luluHeadShape", head, HEAD_ORIGIN),
        ("luluMuzzleShape", muzzle, HEAD_ORIGIN),
        ("luluHatShape", hat, HEAD_ORIGIN),
        ("luluLeafShape", leaf, HEAD_ORIGIN),
        ("luluTorsoShape", torso, BODY_ORIGIN),
        ("luluShortsShape", shorts, BODY_ORIGIN),
        ("luluLegLeftShape", leg_l, BODY_ORIGIN),
        ("luluLegRightShape", leg_r, BODY_ORIGIN),
    ]:
        header += block(name, pts, org) + "\n"

    OUT.write_text(header, encoding="utf-8")
    print("wrote", OUT)
    for name, pts in [("head", head), ("muzzle", muzzle), ("hat", hat), ("torso", torso),
                      ("shorts", shorts), ("legL", leg_l), ("legR", leg_r), ("leaf", leaf)]:
        print(f"  {name}: {len(pts)} points")


if __name__ == "__main__":
    main()
