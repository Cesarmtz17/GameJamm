"""Mapa vestido: el mismo layout de gen_map.py, pero con los modelos reales del juego en vez de figuras.

Uso:
    blender --background --factory-startup --python gen_map_dressed.py -- <carpeta World> <carpeta_previews>

Salida: FBX/SM_MAP_Dressed.fbx, Blend/MAP_Dressed.blend, map_layout.json (mismas posiciones que el blockout) y
previews map_dressed_hero.png / map_dressed_top.png.
"""
import math
import os
import random
import sys
import tempfile

import bpy
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "..", "Building", "scripts"))
import gen_assets as ga  # noqa: E402
import gen_map as gm  # noqa: E402
import gen_parts as gp  # noqa: E402

PALETTE = None
rnd = random.Random(11)


def recolor(obj, src, dst):
    """Cambia un color de la paleta por otro remapeando UVs (p. ej. banderas rojas -> azules)."""
    u_src, u_dst = gp.swatch_uv(src)[0], gp.swatch_uv(dst)[0]
    uv = obj.data.uv_layers.active.data
    for loop in uv:
        if abs(loop.uv[0] - u_src) < 1e-4:
            loop.uv[0] = u_dst


def real(builder, size, pivot, x, y, z=0.0, rot=0.0, scale=1.0, team=None, name="P"):
    p = gp.Part()
    builder(p)
    o = p.to_object(name, size)
    ga.apply_pivot(o, size, pivot)
    o.data.materials.append(PALETTE)
    if team == "Blue":
        recolor(o, "red", "blue")
    o.location = gm.bl(x, y, z)
    o.rotation_euler.z = math.radians(rot)
    o.scale = (scale, scale, scale)
    bpy.context.scene.collection.objects.link(o) if o.name not in bpy.context.scene.collection.objects else None
    gm.OBJS.append(o)
    return o


# ---- reemplazos de las figuras del blockout (mismas firmas que gen_map)
def tower(name, team, x, y, z=0.0):
    real(ga.watchtower, (300, 300, 800), "bottom", x, y, z, rot=0 if team == "Red" else 180, team=team, name=name)
    real(ga.barricade, (300, 80, 160), "bottom", x + (6 if team == "Red" else -6), y + 4.5, z,
         rot=90, team=team, name=f"{name}_Barricade")
    gm.mark("Tower", team, x, y, z, "ATeamStructure: dispara a enemigos cercanos")


def keep(name, team, x, y, z=0.0):
    real(ga.keep, (530, 530, 800), "bottom", x, y, z, rot=0 if team == "Red" else 180, scale=1.6, team=team, name=name)
    s = 1 if team == "Red" else -1
    for dy in (-12, 12):
        real(ga.flag_pole, (180, 60, 520), "bottom", x + 12 * s, y + dy, z, rot=0 if team == "Red" else 180,
             team=team, name=f"{name}_Flag")
    real(ga.spawn_pad, (600, 600, 160), "bottom", x + 16 * s, y, z, team=team, name=f"{name}_Spawn")
    gm.mark("Base", team, x, y, z, "ATeamStructure base: si cae, pierde el equipo")


def loot_camp(name, x, y, z):
    real(ga.ruin_arch, (150, 540, 500), "bottom", x - 3, y + 1, z, rot=20, name=f"{name}_Arch")
    real(ga.ruin_pillar, (90, 90, 400), "bottom", x + 4, y - 4, z, name=f"{name}_Pillar")
    real(ga.chest_base, (122, 72, 50), "bottom", x, y, z, rot=rnd.uniform(-30, 30), name=f"{name}_Chest")
    real(ga.chest_lid, (122, 72, 36), "hinge_back", x, y + 0.36, z + 0.5, name=f"{name}_ChestLid")
    real(ga.scrap_pile, (180, 140, 80), "bottom", x + 3.5, y + 3, z, rot=40, name=f"{name}_Scrap")
    real(ga.crate, (104, 104, 104), "bottom", x - 4, y - 3, z, rot=15, name=f"{name}_Crate")
    real(ga.barrel, (84, 84, 100), "bottom", x - 2.5, y - 4.2, z, name=f"{name}_Barrel")
    real(ga.campfire, (120, 120, 60), "bottom", x + 2, y - 1.5, z, name=f"{name}_Fire")
    gm.mark("LootCamp", "Neutral", x, y, z, "campamento: enemigos + cofre")
    gm.mark("Chest", "Neutral", x, y, z, "ALootChest")


TREES = [(ga.tree, (300, 300, 700)), (ga.tree_round, (420, 380, 600)), (ga.tree_tall, (360, 360, 900))]


def tree(name, x, y, z):
    fn, size = rnd.choice(TREES)
    real(fn, size, "bottom", x, y, z, rot=rnd.uniform(0, 360), scale=rnd.uniform(0.8, 1.2), name=name)


def dress_meadow():
    """Vegetación, rocas y faroles sobre la pradera central, lejos del carril y del río."""
    def free(x, y):
        on_lane = abs(y) < 7
        on_river = abs(x - y * 20 / 68) < 6
        inside = (x / 54) ** 2 + (y / 34) ** 2 < 0.85
        return inside and not on_lane and not on_river
    placed = 0
    while placed < 70:
        x, y = rnd.uniform(-54, 54), rnd.uniform(-34, 34)
        if not free(x, y):
            continue
        kind = rnd.random()
        if kind < 0.45:
            real(ga.grass_tuft, (60, 60, 50), "bottom", x, y, 0, rot=rnd.uniform(0, 360), scale=rnd.uniform(1, 2))
        elif kind < 0.65:
            real(ga.flowers, (60, 60, 35), "bottom", x, y, 0, rot=rnd.uniform(0, 360), scale=1.5)
        elif kind < 0.8:
            real(ga.bush, (210, 160, 120), "bottom", x, y, 0, rot=rnd.uniform(0, 360))
        elif kind < 0.92:
            real(ga.rock_flat, (300, 300, 100), "bottom", x, y, 0, rot=rnd.uniform(0, 360), scale=rnd.uniform(0.6, 1))
        else:
            real(ga.mushroom, (50, 50, 45), "bottom", x, y, 0, scale=2)
        placed += 1
    for x in (-50, -30, -10, 10, 30, 50):  # faroles a los lados del camino
        real(ga.lantern_post, (100, 30, 360), "bottom", x, 4.5, 0, rot=90)
    real(ga.rock_spire, (220, 200, 500), "bottom", -36, 24, 0, rot=30)
    real(ga.rock_spire, (220, 200, 500), "bottom", 38, -22, 0, rot=200)
    real(ga.workbench, (210, 100, 140), "bottom", -112, 10, 0, rot=90)       # mesa de construcción en cada base
    real(ga.workbench, (210, 100, 140), "bottom", 112, -10, 0, rot=-90)
    real(ga.tent, (320, 300, 220), "bottom", -108, -12, 0, rot=10)
    real(ga.tent, (320, 300, 220), "bottom", 108, 12, 0, rot=190, team="Blue")


def main():
    global PALETTE
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    out = os.path.abspath(argv[0] if argv else os.path.join(HERE, ".."))
    prev = os.path.abspath(argv[1] if len(argv) > 1 else out)
    for d in (out, prev, os.path.join(out, "FBX"), os.path.join(out, "Blend")):
        os.makedirs(d, exist_ok=True)
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    PALETTE = gp.make_palette_material(tempfile.mkdtemp())

    gm.tower, gm.keep, gm.loot_camp, gm.tree = tower, keep, loot_camp, tree
    gm.build()
    dress_meadow()

    # los meshes del blockout (islas, puentes, río) llevan su color como material plano
    mats = {}
    for o in gm.OBJS:
        if o.type == "MESH" and not o.data.materials:
            key = tuple(round(c, 3) for c in o.color)
            if key not in mats:
                m = bpy.data.materials.new(f"M_Map_{len(mats):02d}")
                m.diffuse_color = o.color
                mats[key] = m
            o.data.materials.append(mats[key])

    import json
    with open(os.path.join(out, "map_layout.json"), "w", encoding="utf-8") as f:
        json.dump(gm.LAYOUT, f, indent=2, ensure_ascii=False)
    bpy.ops.object.select_all(action="DESELECT")
    for o in gm.OBJS:
        o.select_set(True)
    bpy.ops.export_scene.fbx(filepath=os.path.join(out, "FBX", "SM_MAP_Dressed.fbx"), use_selection=True,
                             object_types={"MESH"}, mesh_smooth_type="FACE", add_leaf_bones=False, bake_anim=False)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, "Blend", "MAP_Dressed.blend"), copy=True)

    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.view_settings.view_transform = "Standard"
    sh = scene.display.shading
    sh.light = "STUDIO"
    sh.color_type = "TEXTURE"
    sh.show_cavity = True
    sh.show_shadows = True
    sh.show_object_outline = True
    sh.background_type = "VIEWPORT"
    sh.background_color = (0.45, 0.70, 0.90)
    gm.render(os.path.join(prev, "map_dressed_hero.png"), gm.bl(-150, -140, 95), gm.bl(10, 5, -5), 0,
              ortho=False, w=1800, h=1000)
    gm.render(os.path.join(prev, "map_dressed_close.png"), gm.bl(-75, -38, 22), gm.bl(-30, 0, 2), 0,
              ortho=False, w=1800, h=1000)
    gm.render(os.path.join(prev, "map_dressed_top.png"), Vector((0, 0, 300)), Vector((0, 0, 0)), 290,
              ortho=True, w=1800, h=1100)
    print("DRESSED_OK", len(gm.OBJS), "objetos")


if __name__ == "__main__":
    main()
