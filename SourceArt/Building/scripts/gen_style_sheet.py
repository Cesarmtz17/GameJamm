"""Hoja de estilo de GameJamm: una lámina 16:9 con assets reales, la paleta y las reglas de ART.md.

Uso:
    blender --background --factory-startup --python gen_style_sheet.py -- <salida.png>

Sirve como referencia visual del equipo y como imagen de estilo para Meshy/Tripo/Rodin (PROMPTS-ASSETS.md §2).
"""
import math
import os
import sys

import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_assets as ga  # noqa: E402
import gen_parts as gp  # noqa: E402

W, H = 19.2, 10.8   # unidades de la lámina (X horizontal, Z vertical; la cámara mira hacia +Y)
INK = (0.12, 0.11, 0.10, 1)
PAPER = (0.96, 0.94, 0.89)


def mat(name, rgba):
    m = bpy.data.materials.new(name)
    m.diffuse_color = rgba
    return m


def text(body, x, z, size, color=INK, bold=False, align="LEFT"):
    cu = bpy.data.curves.new("T", "FONT")
    cu.body = body
    cu.size = size
    cu.align_x = align
    font = "C:/Windows/Fonts/segoeuib.ttf" if bold else "C:/Windows/Fonts/segoeui.ttf"
    if os.path.exists(font):
        cu.font = bpy.data.fonts.load(font, check_existing=True)
    o = bpy.data.objects.new("T", cu)
    o.location = (x, -0.5, z)
    o.rotation_euler = (math.radians(90), 0, 0)
    o.data.materials.append(mat("Mt", color))
    bpy.context.scene.collection.objects.link(o)
    return o


def rect(x, z, w, h, rgba, y=0.2):
    bpy.ops.mesh.primitive_plane_add(size=1, location=(x + w / 2, y, z + h / 2), rotation=(math.radians(90), 0, 0))
    o = bpy.context.active_object
    o.scale = (w, h, 1)
    o.data.materials.append(mat("Mr", rgba))
    return o


def place(obj, x, z, fit, rot_z=-35, tilt=18):
    """Pone un asset (centrado en su caja) en (x, z) de la lámina, escalado para caber en `fit`."""
    k = fit / max(obj.dimensions)
    obj.scale = (k, k, k)
    obj.rotation_euler = (math.radians(tilt), 0, math.radians(rot_z))
    obj.location = (x, 2.0, z)


def build_asset(builder, sm, size, palette_mat):
    p = gp.Part()
    builder(p)
    o = p.to_object(sm, size)
    o.data.materials.append(palette_mat)
    return o


def hero_cart(palette_mat):
    """Artefacto armado con piezas reales: tabla + 4 ruedas + asiento + ventilador + cañón + batería."""
    parts = []
    for t, loc in (("Plank", (0, 0, 0)), ("Seat", (-20, 0, 20)), ("Fan", (-107, 0, 30)), ("Cannon", (70, 0, 15)),
                   ("Battery", (40, 18, 15)), ("Wheel", (70, 40, -10)), ("Wheel", (70, -40, -10)),
                   ("BigWheel", (-70, 45, -5)), ("BigWheel", (-70, -45, -5))):
        o = gp.build(t).to_object(t, SIZES[t])
        o.data.materials.append(palette_mat)
        o.location = gp.ue(loc)
        parts.append(o)
    bpy.ops.object.select_all(action="DESELECT")
    for o in parts:
        o.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    bpy.ops.object.join()
    cart = bpy.context.active_object
    bpy.ops.object.origin_set(type="ORIGIN_GEOMETRY", center="BOUNDS")
    return cart


SIZES = {t: s for t, _, s, *_ in gp.PARTS}


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    out = os.path.abspath(argv[0] if argv else "style_sheet.png")
    gp.clear_scene()
    import tempfile
    palette_mat = gp.make_palette_material(tempfile.mkdtemp())

    # fondo y encabezado
    rect(0, 0, W, H, (*PAPER, 1), y=3.0)
    text("GAMEJAMM  ·  HOJA DE ESTILO", 0.6, H - 1.05, 0.62, bold=True)
    text("Low-poly estilizado  ·  islas flotantes y campo abierto  ·  artefactos de madera, hierro y cuerda",
         0.62, H - 1.55, 0.26)
    rect(0.6, H - 1.8, W - 1.2, 0.03, INK, y=2.9)

    # héroe: artefacto armado sobre una isla
    props = []
    cart = hero_cart(palette_mat)
    place(cart, 4.3, 6.3, 4.6, rot_z=-30, tilt=22)
    isl = build_asset(ga.island_small, "isle", (1500, 1500, 1000), palette_mat)
    place(isl, 4.3, 4.5, 3.8, rot_z=0, tilt=22)
    props += [cart, isl]
    text("Artefacto armado con piezas reales del juego", 4.3, 2.15, 0.22, align="CENTER")

    # paleta (16 colores)
    text("PALETA", 9.3, H - 2.45, 0.32, bold=True)
    names = {"wood": "Madera", "wood_dark": "Madera oscura", "iron": "Hierro", "red": "Equipo rojo / acento",
             "yellow": "Loot", "leather": "Cuero / roca", "black": "Negro", "cyan": "Hielo / cristal",
             "stone": "Piedra", "orange": "Fuego", "spark": "Rayo", "white": "Hoja / nube", "blue": "Equipo azul",
             "grass": "Pasto", "copper": "Cobre", "cloth": "Lona"}
    for i, (name, hx) in enumerate(gp.PALETTE):
        col, row = i % 4, i // 4
        x, z = 9.3 + col * 2.45, H - 3.35 - row * 0.95
        r, g, b = gp.hex_to_linear(hx)
        rect(x, z, 0.62, 0.62, (r, g, b, 1), y=2.5)
        text(names.get(name, name), x + 0.75, z + 0.33, 0.2, bold=True)
        text(hx.upper(), x + 0.75, z + 0.05, 0.19)

    # reglas
    text("REGLAS", 9.3, 2.85, 0.32, bold=True)
    rules = [
        "1. Formas gruesas y biseladas, proporciones de juguete; detalle fino en textura, no en geometría.",
        "2. Hecho con lo que hay: madera, hierro, cuerda, lona y piedra. Nada de metal pulido ni sci-fi.",
        "3. Color de equipo solo en bloques grandes (banderas, techos, ropa). La madera sigue siendo madera.",
        "4. Cada pieza dice su función de lejos: aspas enormes, boca de cañón exagerada, rayo en la batería.",
        "5. ≤ 5k triángulos, un material (M_Part_Master) con la paleta, X = adelante, pivote al centro.",
    ]
    for i, r_ in enumerate(rules):
        text(r_, 9.3, 2.3 - i * 0.4, 0.2)

    # tira de props abajo a la izquierda
    strip = [(ga.sword, "SM_WPN_SWORD", (100, 22, 7)), (ga.fuse_fire, "f1", (25, 25, 25)),
             (ga.fuse_ice, "f2", (25, 25, 25)), (ga.chest, "c", (122, 72, 72)),
             (ga.watchtower, "w", (300, 300, 800)), (ga.tree, "t", (300, 300, 700))]
    for i, (fn, sm, size) in enumerate(strip):
        o = build_asset(fn, sm, size, palette_mat)
        place(o, 1.1 + i * 1.35, 1.25, 1.1)
        props.append(o)
    text("Armas · fusión · mundo", 0.6, 0.2, 0.2)

    # render
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    sh = scene.display.shading
    sh.light = "STUDIO"
    sh.color_type = "TEXTURE"
    sh.show_cavity = True
    sh.cavity_type = "BOTH"
    sh.show_object_outline = False
    sh.background_type = "VIEWPORT"
    sh.background_color = PAPER
    cd = bpy.data.cameras.new("Cam")
    cd.type = "ORTHO"
    cd.ortho_scale = W
    cam = bpy.data.objects.new("Cam", cd)
    scene.collection.objects.link(cam)
    cam.location = (W / 2, -20, H / 2)
    cam.rotation_euler = (math.radians(90), 0, 0)
    scene.camera = cam
    scene.render.resolution_x, scene.render.resolution_y = 1920, 1080
    scene.view_settings.view_transform = "Standard"  # colores de la paleta sin el filtro AgX

    # pasada 1: lámina (texto, paleta, fondo) con color plano para que los colores salgan exactos
    layout_png, props_png = out + ".layout.png", out + ".props.png"
    for o in props:
        o.hide_render = True
    sh.light = "FLAT"
    sh.show_cavity = False
    scene.render.filepath = layout_png
    bpy.ops.render.render(write_still=True)
    # pasada 2: solo los modelos, con luz de estudio y fondo transparente
    for o in scene.objects:
        o.hide_render = o.type in {"MESH", "FONT"} and o not in props
    for o in props:
        o.hide_render = False
    sh.light = "STUDIO"
    sh.show_cavity = True
    scene.render.film_transparent = True
    scene.render.image_settings.color_mode = "RGBA"
    scene.render.filepath = props_png
    bpy.ops.render.render(write_still=True)

    # composición: modelos encima de la lámina (alpha over)
    import numpy as np
    a = bpy.data.images.load(layout_png)
    b = bpy.data.images.load(props_png)
    pa = np.empty(len(a.pixels), dtype=np.float32)
    pb = np.empty(len(b.pixels), dtype=np.float32)
    a.pixels.foreach_get(pa)
    b.pixels.foreach_get(pb)
    pa, pb = pa.reshape(-1, 4), pb.reshape(-1, 4)
    al = pb[:, 3:4]
    pa[:, :3] = pb[:, :3] * al + pa[:, :3] * (1 - al)
    pa[:, 3] = 1.0
    a.pixels.foreach_set(pa.ravel())
    a.filepath_raw = out
    a.file_format = "PNG"
    a.save()
    for f in (layout_png, props_png):
        os.remove(f)
    print("STYLE_SHEET_OK", out)


if __name__ == "__main__":
    main()
