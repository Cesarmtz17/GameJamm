"""Iconos de inventario 256x256 (PNG con transparencia) para todas las piezas, armas, materiales y pickups.

Uso:
    blender --background --factory-startup --python gen_icons.py -- <carpeta_salida>

Nombre: T_Icon_<id>.png, donde <id> es el asset sin el prefijo SM_ (p. ej. T_Icon_PART_PLANK.png), como pide
PROMPTS-ASSETS.md §7. Misma cámara 3/4, luz y paleta para todos, así el inventario se ve parejo.
"""
import math
import os
import sys
import tempfile

import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_assets as ga  # noqa: E402
import gen_parts as gp  # noqa: E402

# solo lo que el jugador tiene en el inventario o recoge (no escenario)
ICON_GROUPS = {"Building", "Weapons", "Fusion", "Pickups"}


def items():
    for recipe, sm, size, *_ in gp.PARTS:
        yield sm, size, (lambda r=recipe: gp.build(r))
    seen = {sm for _, sm, *_ in gp.PARTS}
    for lst in (ga.ASSETS, ga.LOT3, ga.LOT4, ga.LOT5):
        for group, sm, size, builder, *_ in lst:
            if group not in ICON_GROUPS or sm in seen:
                continue
            if lst in (ga.ASSETS, ga.LOT3) and group == "Building":
                continue  # reemplazadas por LOT4
            seen.add(sm)

            def make(b=builder):
                p = gp.Part()
                b(p)
                return p
            yield sm, size, make


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    out = os.path.abspath(argv[0] if argv else "Icons")
    os.makedirs(out, exist_ok=True)
    gp.clear_scene()
    mat = gp.make_palette_material(tempfile.mkdtemp())

    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.view_settings.view_transform = "Standard"
    sh = scene.display.shading
    sh.light = "STUDIO"
    sh.color_type = "TEXTURE"
    sh.show_cavity = True
    sh.cavity_type = "BOTH"
    sh.show_object_outline = True
    sh.object_outline_color = (0.08, 0.07, 0.06)
    scene.render.film_transparent = True
    scene.render.image_settings.color_mode = "RGBA"
    scene.render.resolution_x = scene.render.resolution_y = 256
    cd = bpy.data.cameras.new("Cam")
    cd.type = "ORTHO"
    cam = bpy.data.objects.new("Cam", cd)
    scene.collection.objects.link(cam)
    scene.camera = cam
    d = Vector((1.0, -1.25, 0.9)).normalized()  # 3/4 desde adelante-derecha-arriba (marco Blender)

    n = 0
    for sm, size, make in items():
        obj = make().to_object(sm, size)
        obj.data.materials.append(mat)
        r = obj.dimensions.length * 0.5  # radio de la esfera que envuelve la caja: así no se corta en diagonal
        cd.ortho_scale = r * 2.1
        cam.location = d * (r * 6 + 2)
        cam.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
        cd.clip_end = r * 20 + 10
        scene.render.filepath = os.path.join(out, f"T_Icon_{sm[3:]}.png")
        bpy.ops.render.render(write_still=True)
        bpy.data.objects.remove(obj, do_unlink=True)
        n += 1
    print("ICONS_OK", n)


if __name__ == "__main__":
    main()
