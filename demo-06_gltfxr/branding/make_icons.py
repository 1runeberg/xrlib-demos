#!/usr/bin/env python3
"""Generate the gltfxr app icons for Android and visionOS from one set of shapes"""
import json
import math
import subprocess
import sys
from pathlib import Path

root = Path(__file__).resolve().parent.parent
branding = root / 'branding'
android = root / 'android/resources'
assets = root / 'visionos/Assets.xcassets'
SIZE = 1024

# Shapes use a 1024 unit canvas. Android vector drawables reuse it as their viewport,
# so foreground content stays inside the adaptive icon safe zone (radius ~313 from centre)

def linear(x1, y1, x2, y2, *stops):
    return dict(kind='linear', x1=x1, y1=y1, x2=x2, y2=y2, stops=stops)

def ellipse(cx, cy, rx, ry):
    return f'M{cx - rx},{cy} A{rx},{ry} 0 1,0 {cx + rx},{cy} A{rx},{ry} 0 1,0 {cx - rx},{cy} Z'

def polygon(*points):
    return 'M' + ' L'.join(f'{x:.1f},{y:.1f}' for x, y in points) + ' Z'

horizon = 590
vanish = ( 512, horizon )

def back():
    shapes = []

    # Night sky fading to warm charcoal at the horizon, black floor below
    shapes.append(dict(d=polygon((0, 0), (SIZE, 0), (SIZE, horizon), (0, horizon)), fill=linear(0, 0, 0, horizon, (0, '#FF050506'), (.7, '#FF14110D'), (1, '#FF2A2418'))))
    shapes.append(dict(d=polygon((0, horizon), (SIZE, horizon), (SIZE, SIZE), (0, SIZE)), fill='#FF060504'))

    # Stars, kept clear of the cube
    for x, y, r, a in [(170, 150, 7, 'E6'), (300, 260, 4, '99'), (215, 395, 5, 'B3'), (840, 185, 6, 'D9'), (735, 110, 4, '8C'), (880, 360, 5, 'A6'),
                       (95, 300, 3, '80'), (610, 70, 3, '80'), (395, 115, 4, '99'), (935, 470, 3, '73'), (120, 505, 3, '66')]:
        shapes.append(dict(d=ellipse(x, y, r, r), fill=f'#{a}F2E8CC'))

    # Perspective floor grid in the panel accent gold, fading into the horizon
    lines = []
    for i in range(-7, 8):
        x = 512 + i * 170
        dx, dy = x - vanish[0], SIZE - vanish[1]
        lines.append(f'M{vanish[0] + dx * .04:.1f},{vanish[1] + dy * .04:.1f} L{x},{SIZE}')
    depth = SIZE - horizon
    for z in (1, 1.35, 1.8, 2.5, 3.5, 5, 7.5, 12):
        y = horizon + depth / z
        lines.append(f'M0,{y:.1f} L{SIZE},{y:.1f}')
    shapes.append(dict(d=' '.join(lines), stroke=linear(0, horizon, 0, SIZE, (0, '#00D9B84C'), (.35, '#66D9B84C'), (1, '#E6E8C35A')), width=5))

    # Warm glow along the horizon
    shapes.append(dict(d=polygon((0, horizon - 70), (SIZE, horizon - 70), (SIZE, horizon + 40), (0, horizon + 40)),
                       fill=linear(0, horizon - 70, 0, horizon + 40, (0, '#00D6AE45'), (.64, '#40D6AE45'), (1, '#00D6AE45'))))
    return shapes

# Cube geometry, an isometric model floating above its floor marker
cube_centre = ( 512, 450 )
cube_side = 235

def cube_points():
    cx, cy = cube_centre
    s = cube_side
    c = s * math.cos(math.radians(30))
    top = (cx, cy - s)
    upper_left, upper_right = (cx - c, cy - s / 2), (cx + c, cy - s / 2)
    lower_left, lower_right = (cx - c, cy + s / 2), (cx + c, cy + s / 2)
    bottom = (cx, cy + s)
    return top, upper_left, upper_right, lower_left, lower_right, bottom, (cx, cy)

def middle():
    shapes = []

    # Floor marker, a soft glowing ring like the night grid user glow
    cx, cy = 512, 770
    for rx, ry, width, alpha in [(205, 44, 34, '14'), (205, 44, 20, '26'), (205, 44, 9, '59'), (205, 44, 4, 'CC')]:
        shapes.append(dict(d=ellipse(cx, cy, rx, ry), stroke=f'#{alpha}FFE07F', width=width))
    shapes.append(dict(d=ellipse(cx, cy, 150, 30), fill='#40FFD86A'))

    # Inside of the glass cube, the three hidden faces
    top, upper_left, upper_right, lower_left, lower_right, bottom, centre = cube_points()
    shapes.append(dict(d=polygon(centre, lower_left, bottom, lower_right), fill='#F2100C07'))
    shapes.append(dict(d=polygon(centre, lower_left, upper_left, top), fill='#F21A150D'))
    shapes.append(dict(d=polygon(centre, top, upper_right, lower_right), fill='#F20C0906'))
    shapes.append(dict(d=f'M{centre[0]:.1f},{centre[1]:.1f} L{lower_left[0]:.1f},{lower_left[1]:.1f} M{centre[0]:.1f},{centre[1]:.1f} L{lower_right[0]:.1f},{lower_right[1]:.1f} M{centre[0]:.1f},{centre[1]:.1f} L{top[0]:.1f},{top[1]:.1f}',
                       stroke='#59E8C35A', width=4))
    return shapes

def mark():
    # Rune Berg mark (anvil, hammer and Southern Cross) floating inside the cube, without its sky
    svg = (branding / 'runeberg_mark.svg').read_text()
    body = svg[svg.index('>') + 1:svg.rindex('</svg>')].replace('<rect width="1024" height="1024" fill="url(#sky)"/>', '')
    body = body.replace('id="', 'id="rb').replace('url(#', 'url(#rb')
    scale = .46
    return f'<g transform="translate({cube_centre[0] - 512 * scale:.1f} {cube_centre[1] - 500 * scale:.1f}) scale({scale})">{body}</g>'

def front():
    top, upper_left, upper_right, lower_left, lower_right, bottom, centre = cube_points()
    shapes = []

    # Gold tinted glass so the mark inside stays readable
    shapes.append(dict(d=polygon(top, upper_right, centre, upper_left), fill=linear(0, top[1], 0, centre[1], (0, '#4DFFF1C8'), (1, '#1FE8C35A'))))
    shapes.append(dict(d=polygon(upper_left, centre, bottom, lower_left), fill=linear(upper_left[0], 0, centre[0], 0, (0, '#26E0B546'), (1, '#0DE0B546'))))
    shapes.append(dict(d=polygon(centre, upper_right, lower_right, bottom), fill=linear(centre[0], 0, upper_right[0], 0, (0, '#1A0A0806'), (1, '#330A0806'))))

    # Front edges stay faint where they cross the mark
    shapes.append(dict(d=f'M{upper_left[0]:.1f},{upper_left[1]:.1f} L{centre[0]:.1f},{centre[1]:.1f} L{upper_right[0]:.1f},{upper_right[1]:.1f} M{centre[0]:.1f},{centre[1]:.1f} L{bottom[0]:.1f},{bottom[1]:.1f}',
                       stroke='#40FFF6DC', width=4))
    shapes.append(dict(d=polygon(top, upper_right, lower_right, bottom, lower_left, upper_left), stroke='#A6FFF1C8', width=5))
    return shapes

def svg_colour(value):
    return f'#{value[3:]}', int(value[1:3], 16) / 255

def svg(shapes, name):
    defs, body = [], []
    for index, shape in enumerate(shapes):
        if isinstance(shape, str):
            body.append(shape)
            continue
        attrs = []
        for key, prop in (('fill', 'fill'), ('stroke', 'stroke')):
            value = shape.get(key)
            if value is None:
                attrs.append(f'{prop}="none"' if key == 'fill' else '')
            elif isinstance(value, dict):
                gradient = f'{name}{index}{key}'
                stops = ''.join(f'<stop offset="{o}" stop-color="{svg_colour(c)[0]}" stop-opacity="{svg_colour(c)[1]:.3f}"/>' for o, c in value['stops'])
                defs.append(f'<linearGradient id="{gradient}" gradientUnits="userSpaceOnUse" x1="{value["x1"]:.1f}" y1="{value["y1"]:.1f}" x2="{value["x2"]:.1f}" y2="{value["y2"]:.1f}">{stops}</linearGradient>')
                attrs.append(f'{prop}="url(#{gradient})"')
            else:
                colour, alpha = svg_colour(value)
                attrs.append(f'{prop}="{colour}" {prop}-opacity="{alpha:.3f}"')
        if 'width' in shape:
            attrs.append(f'stroke-width="{shape["width"]}" stroke-linecap="round" stroke-linejoin="round"')
        body.append(f'<path d="{shape["d"]}" {" ".join(a for a in attrs if a)}/>')
    return defs, body

def write_svg(path, *layers):
    defs, body = [], []
    for index, shapes in enumerate(layers):
        d, b = svg(shapes, f'l{index}g')
        defs += d
        body += b
    path.write_text(f'<svg xmlns="http://www.w3.org/2000/svg" width="{SIZE}" height="{SIZE}" viewBox="0 0 {SIZE} {SIZE}">\n<defs>{"".join(defs)}</defs>\n' + '\n'.join(body) + '\n</svg>\n')

LICENSE = '''    <!--
      Copyright 2024-26 Rune Berg (http://runeberg.io | https://github.com/1runeberg)
      Licensed under Apache 2.0 (https://www.apache.org/licenses/LICENSE-2.0)
      SPDX-License-Identifier: Apache-2.0
    -->
'''

def android_colour(key, value):
    if not isinstance(value, dict):
        return f'\n            android:{key}="{value}"', ''
    items = ''.join(f'\n                    <item android:offset="{o}" android:color="{c}" />' for o, c in value['stops'])
    gradient = f'''
            <aapt:attr name="android:{key}">
                <gradient
                    android:type="linear"
                    android:startX="{value["x1"]:.1f}"
                    android:startY="{value["y1"]:.1f}"
                    android:endX="{value["x2"]:.1f}"
                    android:endY="{value["y2"]:.1f}">{items}
                </gradient>
            </aapt:attr>'''
    return '', gradient

def write_vector(path, *layers):
    paths = []
    for shapes in layers:
        for shape in shapes:
            attrs, children = f'\n            android:pathData="{shape["d"]}"', ''
            for key, prop in (('fill', 'fillColor'), ('stroke', 'strokeColor')):
                if key in shape:
                    a, c = android_colour(prop, shape[key])
                    attrs += a
                    children += c
            if 'width' in shape:
                attrs += f'\n            android:strokeWidth="{shape["width"]}"\n            android:strokeLineCap="round"\n            android:strokeLineJoin="round"'
            paths.append(f'        <path{attrs}>{children}\n        </path>' if children else f'        <path{attrs} />')
    path.write_text(f'''<vector xmlns:android="http://schemas.android.com/apk/res/android" xmlns:aapt="http://schemas.android.com/aapt"
    android:width="108dp"
    android:height="108dp"
    android:viewportWidth="{SIZE}"
    android:viewportHeight="{SIZE}">

{LICENSE}
    <group>
''' + '\n'.join(paths) + '\n    </group>\n</vector>\n')

def rasterise(source, target, size):
    subprocess.run(['swift', branding / 'rasterise.swift', source, target, str(size)], check=True)

def main():
    build = root / 'build/branding'
    build.mkdir(parents=True, exist_ok=True)
    layers = dict(back=back(), middle=middle() + front(), front=[mark()])

    # Master artwork, kept alongside the generator
    write_svg(branding / 'gltfxr_icon.svg', *layers.values())

    # Android adaptive icon layers plus legacy launcher PNGs
    write_vector(android / 'drawable/ic_openxr_app_background.xml', layers['back'])
    write_svg(build / 'foreground.svg', layers['middle'], layers['front'])
    for folder, size in (('mdpi', 48), ('hdpi', 72), ('xhdpi', 96), ('xxhdpi', 144), ('xxxhdpi', 192)):
        rasterise(branding / 'gltfxr_icon.svg', android / f'mipmap-{folder}/ic_openxr_app.png', size)
        rasterise(build / 'foreground.svg', android / f'mipmap-{folder}/ic_openxr_app_foreground.png', size * 9 // 4)

    # visionOS layered app icon: sky on the opaque back layer, glass cube in the middle, mark in front so it lifts out when gazed at
    info = {'info': {'author': 'xcode', 'version': 1}}
    stack = assets / 'AppIcon.solidimagestack'
    stack.mkdir(parents=True, exist_ok=True)
    (assets / 'Contents.json').write_text(json.dumps(info, indent=2) + '\n')
    (stack / 'Contents.json').write_text(json.dumps({**info, 'layers': [{'filename': 'Front.solidimagestacklayer'}, {'filename': 'Middle.solidimagestacklayer'}, {'filename': 'Back.solidimagestacklayer'}]}, indent=2) + '\n')
    for name, shapes in layers.items():
        layer = stack / f'{name.title()}.solidimagestacklayer'
        imageset = layer / 'Content.imageset'
        imageset.mkdir(parents=True, exist_ok=True)
        (layer / 'Contents.json').write_text(json.dumps(info, indent=2) + '\n')
        (imageset / 'Contents.json').write_text(json.dumps({'images': [{'filename': f'{name}.png', 'idiom': 'vision', 'scale': '2x'}], **info}, indent=2) + '\n')
        write_svg(build / f'{name}.svg', shapes)
        rasterise(build / f'{name}.svg', imageset / f'{name}.png', SIZE)

    rasterise(branding / 'gltfxr_icon.svg', branding / 'gltfxr_icon.png', SIZE)

if __name__ == '__main__':
    sys.exit(main())
