#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""Writes Cromarc's app icon sources: the Icon Composer package (macOS 26+)
and the SVG the bitmaps for older macOS are rendered from.

Usage: make_icon.py <out dir>  ->  <out>/AppIcon.icon, <out>/flat.svg
The icon is a white "C" arc with a pink dot in its opening, on indigo.
"""
import json
import math
import os
import sys

C = 512
R_OUT, R_IN = 330, 210
GAP = 40  # degrees each side of the opening, which faces right
MID = (R_OUT + R_IN) / 2
CAP = (R_OUT - R_IN) / 2
DOT_R = 58


def point(radius, degrees):
    a = math.radians(degrees)
    return C + radius * math.cos(a), C + radius * math.sin(a)


def arc_path():
    ox0, oy0 = point(R_OUT, GAP)
    ox1, oy1 = point(R_OUT, 360 - GAP)
    ix1, iy1 = point(R_IN, 360 - GAP)
    ix0, iy0 = point(R_IN, GAP)
    return (f'M{ox0:.2f} {oy0:.2f}A{R_OUT} {R_OUT} 0 1 1 {ox1:.2f} {oy1:.2f}'
            f'L{ix1:.2f} {iy1:.2f}A{R_IN} {R_IN} 0 1 0 {ix0:.2f} {iy0:.2f}Z')


def arc_svg():
    caps = ''.join(
        f'<circle cx="{x:.2f}" cy="{y:.2f}" r="{CAP}" fill="#FFFFFF"/>'
        for x, y in (point(MID, GAP), point(MID, 360 - GAP)))
    return ('<svg width="1024" height="1024" viewBox="0 0 1024 1024" '
            'fill="none" xmlns="http://www.w3.org/2000/svg">'
            f'<path d="{arc_path()}" fill="#FFFFFF"/>{caps}</svg>\n')


def dot_svg(color='#FF5C8A'):
    x, y = point(MID, 0)
    return ('<svg width="1024" height="1024" viewBox="0 0 1024 1024" '
            'fill="none" xmlns="http://www.w3.org/2000/svg">'
            f'<circle cx="{x:.2f}" cy="{y:.2f}" r="{DOT_R}" fill="{color}"/>'
            '</svg>\n')


def icon_json():
    return {
        'fill': {
            'linear-gradient': [
                'extended-srgb:0.42000,0.25000,0.85000,1.00000',
                'extended-srgb:0.12000,0.09000,0.36000,1.00000',
            ],
        },
        'groups': [{
            'layers': [
                {'image-name': 'dot.svg', 'name': 'dot', 'glass': True},
                {'image-name': 'arc.svg', 'name': 'arc', 'glass': True},
            ],
            'shadow': {'kind': 'neutral', 'opacity': 0.5},
            'translucency': {'enabled': True, 'value': 0.2},
        }],
        'supported-platforms': {'squares': 'shared'},
    }


def flat_svg():
    # The pre-macOS 26 shape: a 824 pt rounded square on the 1024 grid.
    body = (
        '<defs><linearGradient id="g" x1="0" y1="0" x2="0" y2="1">'
        '<stop offset="0" stop-color="#6B40D9"/>'
        '<stop offset="1" stop-color="#1F175C"/></linearGradient></defs>'
        '<rect x="100" y="100" width="824" height="824" rx="185" '
        'fill="url(#g)"/>')
    inner = arc_svg().split('>', 1)[1].rsplit('</svg>', 1)[0]
    dot = dot_svg().split('>', 1)[1].rsplit('</svg>', 1)[0]
    return ('<svg width="1024" height="1024" viewBox="0 0 1024 1024" '
            'xmlns="http://www.w3.org/2000/svg">'
            f'{body}<g transform="translate(512 512) scale(0.8) '
            f'translate(-512 -512)">{inner}{dot}</g></svg>\n')


def main():
    out = sys.argv[1]
    assets = os.path.join(out, 'AppIcon.icon', 'Assets')
    os.makedirs(assets, exist_ok=True)
    with open(os.path.join(assets, 'arc.svg'), 'w') as f:
        f.write(arc_svg())
    with open(os.path.join(assets, 'dot.svg'), 'w') as f:
        f.write(dot_svg())
    with open(os.path.join(out, 'AppIcon.icon', 'icon.json'), 'w') as f:
        json.dump(icon_json(), f, indent=2)
        f.write('\n')
    with open(os.path.join(out, 'flat.svg'), 'w') as f:
        f.write(flat_svg())


if __name__ == '__main__':
    main()
