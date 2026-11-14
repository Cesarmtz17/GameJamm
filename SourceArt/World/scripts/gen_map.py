"""Blockout del mapa de GameJamm: archipiélago de islas flotantes con un carril y campo abierto al centro.

Uso:
    blender --background --factory-startup --python gen_map.py -- <carpeta_salida> <carpeta_previews>

Salida:
- Blend/MAP_Blockout.blend          escena completa
- FBX/SM_MAP_Blockout.fbx           todas las islas, puentes y props en posición de mundo (pivote del mapa = origen)
- map_layout.json                   posiciones (cm, marco UE) de spawns, bases, torres, campamentos y cofres para Core
- previews: map_top.png (planta con etiquetas) y map_hero.png (vista 3/4)

Marco UE: X = del Rojo al Azul, Y = derecha, Z = arriba. Blender: Y_blender = -Y_ue. 1 unidad = 1 m.
Diseño (GDD, MVP): 1 carril; por equipo 1 base + 2 torres; 4 campamentos de loot; el centro es una pradera abierta.
"""
import json
import math
import os
import random
import sys

import bmesh
import bpy
from mathutils import Vector

random.seed(7)

COL = {
    "grass": (0.66, 0.80, 0.29, 1), "meadow": (0.47, 0.60, 0.29, 1), "rock": (0.47, 0.38, 0.29, 1),
    "stone": (0.85, 0.73, 0.54, 1), "water": (0.25, 0.56, 0.58, 1), "wood": (0.79, 0.63, 0.42, 1),
    "red": (0.79, 0.26, 0.28, 1), "blue": (0.21, 0.36, 0.78, 1), "loot": (0.95, 0.76, 0.19, 1),
    "cloud": (0.96, 0.96, 0.93, 1), "tree": (0.30, 0.50, 0.22, 1), "path": (0.73, 0.58, 0.39, 1),
    "crystal": (0.56, 0.89, 1.0, 1), "label": (0.1, 0.1, 0.1, 1),
}
OBJS = []
LAYOUT = {"units": "cm", "frame": "UE (X = Rojo->Azul, Y = derecha, Z = arriba)", "items": []}


def bl(x, y, z=0.0):
    """Metros en marco UE -> Blender."""
    return Vector((x, -y, z))


def link(obj, color):
    obj.color = COL[color]
    bpy.context.scene.collection.objects.link(obj)
    OBJS.append(obj)
    return obj


def mesh_from_bm(name, bm, color):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    return link(bpy.data.objects.new(name, me), color)


def mark(kind, team, x, y, z=0.0, note=""):
    LAYOUT["items"].append({"kind": kind, "team": team, "x": round(x * 100), "y": round(y * 100),
                            "z": round(z * 100), "note": note})


# ---------------------------------------------------------------- formas
def island_full(name, cx, cy, rx, ry, top=0.0, depth=None, sides=14, color="grass"):
    """Isla: tapa de pasto plana e irregular + roca de abajo que termina en punta."""
    depth = depth or max(rx, ry) * 0.9
    pts = []
    for i in range(sides):
        a = 2 * math.pi * i / sides
        j = random.uniform(0.86, 1.04)
        pts.append((cx + math.cos(a) * rx * j, cy + math.sin(a) * ry * j))
    # tapa de pasto (con un poco de grosor)
    bm = bmesh.new()
    up = [bm.verts.new(bl(x, y, top)) for x, y in pts]
    dn = [bm.verts.new(bl(x, y, top - 1.2)) for x, y in pts]
    bm.faces.new(up)
    for i in range(sides):
        j = (i + 1) % sides
        bm.faces.new((up[j], up[i], dn[i], dn[j]))
    mesh_from_bm(f"{name}_Top", bm, color)
    # roca de abajo
    bm = bmesh.new()
    a0 = [bm.verts.new(bl(x, y, top - 1.2)) for x, y in pts]
    a1 = [bm.verts.new(bl(cx + (x - cx) * random.uniform(0.55, 0.7), cy + (y - cy) * random.uniform(0.55, 0.7),
                          top - 1.2 - depth * random.uniform(0.3, 0.45))) for x, y in pts]
    tip = bm.verts.new(bl(cx, cy, top - 1.2 - depth))
    for i in range(sides):
        j = (i + 1) % sides
        bm.faces.new((a0[j], a0[i], a1[i], a1[j]))
        bm.faces.new((a1[j], a1[i], tip))
    bm.faces.new(list(reversed(a0)))
    mesh_from_bm(f"{name}_Rock", bm, "rock")


def box(name, x, y, z, sx, sy, sz, color):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.scale(bm, vec=(sx, sy, sz), verts=bm.verts)
    o = mesh_from_bm(name, bm, color)
    o.location = bl(x, y, z + sz / 2)
    return o


def cone(name, x, y, z, r1, r2, h, color, seg=8):
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=seg, radius1=r1, radius2=r2, depth=h)
    o = mesh_from_bm(name, bm, color)
    o.location = bl(x, y, z + h / 2)
    return o


def blob(name, x, y, z, r, sz, color, sub=1):
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=sub, radius=r)
    bmesh.ops.scale(bm, vec=(1, 1, sz), verts=bm.verts)
    o = mesh_from_bm(name, bm, color)
    o.location = bl(x, y, z)
    return o


def bridge(name, x0, y0, x1, y1, z=0.0, width=4.0):
    L = math.hypot(x1 - x0, y1 - y0)
    ang = math.atan2(y1 - y0, x1 - x0)
    n = max(2, int(L / 1.2))
    for i in range(n):
        t = (i + 0.5) / n
        x, y = x0 + (x1 - x0) * t, y0 + (y1 - y0) * t
        o = box(f"{name}_{i:02d}", x, y, z - 0.25, L / n * 0.85, width, 0.25, "wood")
        o.rotation_euler.z = -ang
    for side in (-1, 1):  # cuerdas
        mx, my = (x0 + x1) / 2 - math.sin(ang) * side * width / 2, (y0 + y1) / 2 + math.cos(ang) * side * width / 2
        o = box(f"{name}_Rope{side}", mx, my, z + 0.9, L, 0.15, 0.15, "path")
        o.rotation_euler.z = -ang


def tree(name, x, y, z):
    cone(f"{name}_Trunk", x, y, z, 0.35, 0.3, 2.0, "wood", 6)
    cone(f"{name}_Leaves", x, y, z + 1.6, 2.0, 0.0, 4.5, "tree", 7)


def tower(name, team, x, y, z=0.0):
    c = "red" if team == "Red" else "blue"
    for dx in (-1.2, 1.2):
        for dy in (-1.2, 1.2):
            box(f"{name}_Leg", x + dx, y + dy, z, 0.4, 0.4, 7.0, "wood")
    box(f"{name}_Deck", x, y, z + 7.0, 3.6, 3.6, 0.5, "wood")
    cone(f"{name}_Roof", x, y, z + 9.4, 2.6, 0.0, 2.0, c, 4).rotation_euler.z = math.radians(45)
    blob(f"{name}_Crystal", x, y, z + 8.4, 0.7, 1.5, "crystal", 1)
    box(f"{name}_Banner", x + 1.9, y, z + 4.0, 0.1, 1.4, 2.4, c)
    mark("Tower", team, x, y, z, "ATeamStructure: dispara a enemigos cercanos")


def keep(name, team, x, y, z=0.0):
    c = "red" if team == "Red" else "blue"
    box(f"{name}_Keep", x, y, z, 10, 10, 12, "stone")
    for dx in (-5, 5):
        for dy in (-5, 5):
            cone(f"{name}_Turret", x + dx, y + dy, z, 1.6, 1.6, 14, "stone", 8)
            cone(f"{name}_TurretRoof", x + dx, y + dy, z + 14, 2.0, 0.0, 3, c, 8)
    box(f"{name}_Banner", x + (5.1 if team == "Red" else -5.1), y, z + 3, 0.2, 4, 7, c)
    for dx in (-9, -4, 4, 9):  # muralla baja
        box(f"{name}_Wall", x + dx, y - 9, z, 4.5, 1.2, 3, "stone")
        box(f"{name}_Wall", x + dx, y + 9, z, 4.5, 1.2, 3, "stone")
    mark("Base", team, x, y, z, "ATeamStructure base: si cae, pierde el equipo")


def loot_camp(name, x, y, z):
    for i, (dx, dy) in enumerate(((-3, -2), (3, 2), (-2, 3))):  # ruinas
        box(f"{name}_Pillar{i}a", x + dx, y + dy, z, 1.2, 1.2, 4.5, "stone")
        box(f"{name}_Pillar{i}b", x + dx + 2.6, y + dy, z, 1.2, 1.2, 3.5, "stone")
    box(f"{name}_Arch", x - 1.7, y - 2, z + 4.5, 4.2, 1.2, 0.9, "stone")
    box(f"{name}_Chest", x, y, z, 1.2, 0.7, 0.7, "loot")
    box(f"{name}_Scrap", x + 2.5, y - 3, z, 1.5, 1.0, 0.8, "wood")
    box(f"{name}_Scrap2", x - 3.5, y + 0.5, z, 1.0, 1.4, 0.6, "path")
    mark("LootCamp", "Neutral", x, y, z, "campamento: enemigos + cofre")
    mark("Chest", "Neutral", x, y, z, "ALootChest")


# ---------------------------------------------------------------- mapa
def build():
    # bases (extremos) y torres
    island_full("ISL_BaseRed", -120, 0, 22, 20, 0, 26)
    island_full("ISL_BaseBlue", 120, 0, 22, 20, 0, 26)
    keep("RedBase", "Red", -124, 0)
    keep("BlueBase", "Blue", 124, 0)
    mark("PlayerStart", "Red", -108, 0, 0.5, "spawn del jugador / aliados")
    mark("PlayerStart", "Blue", 108, 0, 0.5, "spawn de bots enemigos")

    island_full("ISL_T2Red", -82, 0, 10, 9, 0, 14)
    island_full("ISL_T2Blue", 82, 0, 10, 9, 0, 14)
    tower("TowerRed2", "Red", -82, 0)
    tower("TowerBlue2", "Blue", 82, 0)

    # pradera central (campo abierto) con río, colinas y torres 1
    island_full("ISL_Meadow", 0, 0, 58, 38, 0, 40, sides=20, color="meadow")
    tower("TowerRed1", "Red", -44, 0)
    tower("TowerBlue1", "Blue", 44, 0)
    river_ang = math.atan2(68, 20)  # río que cruza la pradera en diagonal
    box("River", 0, 0, 0.02, 70, 6.0, 0.05, "water").rotation_euler.z = -river_ang
    box("Bridge_River", 0, 0, 0.05, 11, 5, 0.3, "wood")
    for i, (hx, hy, hr) in enumerate(((-22, 20, 9), (24, -18, 10), (-28, -22, 7), (30, 22, 7))):
        blob(f"Hill_{i}", hx, hy, -0.5, hr, 0.28, "meadow", 2)
    for i, (px, py) in enumerate(((-58, 0), (-30, 0), (-12, 2), (12, -2), (30, 0), (58, 0))):  # camino del carril
        box(f"Path_{i}", px, py, 0.03, 18, 4.5, 0.05, "path")
    for i in range(16):
        a = random.uniform(0, 2 * math.pi)
        r = random.uniform(0.55, 0.92)
        x, y = math.cos(a) * 54 * r, math.sin(a) * 34 * r
        if abs(y) > 6 and abs(x - (y * 0.3)) > 7:
            tree(f"Tree_{i}", x, y, 0)
    # molino
    cone("Windmill_Base", 18, 26, 0, 2.2, 1.6, 8, "stone", 8)
    box("Windmill_Sail1", 18, 23.6, 8, 0.3, 0.4, 9, "wood").rotation_euler.x = math.radians(35)
    box("Windmill_Sail2", 18, 23.6, 8, 0.3, 0.4, 9, "wood").rotation_euler.x = math.radians(-55)

    # puentes del carril
    bridge("Bridge_RedBase_T2", -98, 0, -92, 0)
    bridge("Bridge_T2_Meadow_R", -72, 0, -57, 0)
    bridge("Bridge_T2_Meadow_B", 57, 0, 72, 0)
    bridge("Bridge_BlueBase_T2", 92, 0, 98, 0)

    # campamentos de loot en islas chicas a los lados, unidos por puente a la pradera o a la isla de la T2
    def edge(cx, cy, rx, ry, dx, dy):
        a = math.atan2(dy, dx)
        r = 1.0 / math.sqrt((math.cos(a) / rx) ** 2 + (math.sin(a) / ry) ** 2)
        return cx + math.cos(a) * r * 0.85, cy + math.sin(a) * r * 0.85

    for name, x, y, src in (("CampNW", -40, 60, (0, 0, 58, 38)), ("CampSE", 40, -60, (0, 0, 58, 38)),
                            ("CampSW", -86, -36, (-82, 0, 10, 9)), ("CampNE", 86, 36, (82, 0, 10, 9))):
        island_full(f"ISL_{name}", x, y, 11, 10, 0, 14)
        loot_camp(name, x, y, 0)
        sx, sy, srx, sry = src
        x0, y0 = edge(sx, sy, srx, sry, x - sx, y - sy)
        x1, y1 = edge(x, y, 11, 10, sx - x, sy - y)
        bridge(f"Bridge_{name}", x0, y0, x1, y1)

    # islas decorativas y nubes
    for i, (x, y, z, r) in enumerate(((-60, 70, 18, 6), (70, -72, 14, 7), (0, 75, 25, 5), (-100, 60, 10, 5),
                                      (100, -55, 20, 6), (0, -78, 8, 4))):
        island_full(f"ISL_Deco{i}", x, y, r, r * 0.9, z, r * 1.3)
        if r >= 6:
            tree(f"DecoTree{i}", x, y, z)
    # las nubes van con el cielo volumetrico de UE (Volumetric Clouds), no como meshes
    for i, (x, y) in enumerate(((-120, 21), (58, -30), (-58, 33), (120, -21))):  # cascadas
        box(f"Waterfall_{i}", x, y, -22, 3.0, 0.4, 22, "water")


def label(text, x, y, size=6):
    cu = bpy.data.curves.new(f"L_{text}", "FONT")
    cu.body = text
    cu.size = size
    cu.align_x = "CENTER"
    o = bpy.data.objects.new(f"L_{text}", cu)
    o.location = bl(x, y, 30)
    o.color = COL["label"]
    bpy.context.scene.collection.objects.link(o)
    return o


def render(path, cam_loc, target, ortho_scale, ortho=True, w=1600, h=1000):
    scene = bpy.context.scene
    cd = bpy.data.cameras.new("Cam")
    cd.type = "ORTHO" if ortho else "PERSP"
    if ortho:
        cd.ortho_scale = ortho_scale
    else:
        cd.lens = 30
    cd.clip_end = 2000
    cam = bpy.data.objects.new("Cam", cd)
    scene.collection.objects.link(cam)
    cam.location = cam_loc
    cam.rotation_euler = (target - cam_loc).to_track_quat("-Z", "Y").to_euler()
    scene.camera = cam
    scene.render.resolution_x, scene.render.resolution_y = w, h
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(cam, do_unlink=True)


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    out = os.path.abspath(argv[0] if argv else os.path.join(os.path.dirname(__file__), ".."))
    prev = os.path.abspath(argv[1] if len(argv) > 1 else out)
    for d in (out, prev, os.path.join(out, "FBX"), os.path.join(out, "Blend")):
        os.makedirs(d, exist_ok=True)
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)

    build()

    with open(os.path.join(out, "map_layout.json"), "w", encoding="utf-8") as f:
        json.dump(LAYOUT, f, indent=2, ensure_ascii=False)

    bpy.ops.object.select_all(action="DESELECT")
    for o in OBJS:
        o.select_set(True)
    bpy.ops.export_scene.fbx(filepath=os.path.join(out, "FBX", "SM_MAP_Blockout.fbx"), use_selection=True,
                             object_types={"MESH"}, mesh_smooth_type="FACE", add_leaf_bones=False, bake_anim=False)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, "Blend", "MAP_Blockout.blend"), copy=True)

    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    sh = scene.display.shading
    sh.light = "STUDIO"
    sh.color_type = "OBJECT"
    sh.show_cavity = True
    sh.show_shadows = True
    sh.show_object_outline = True
    sh.background_type = "VIEWPORT"
    sh.background_color = (0.35, 0.62, 0.86)

    render(os.path.join(prev, "map_hero.png"), bl(-150, -140, 95), bl(10, 5, -5), 0, ortho=False, w=1800, h=1000)

    for t, x, y in (("BASE ROJA", -120, 26), ("BASE AZUL", 120, 26), ("T2", -82, 12), ("T2", 82, 12),
                    ("T1", -44, 9), ("T1", 44, 9), ("PRADERA (campo abierto)", 0, -44), ("LOOT", -40, 72),
                    ("LOOT", 40, -72), ("LOOT", -88, -52), ("LOOT", 88, 52), ("rio", 14, 14)):
        label(t, x, y, 5 if t not in ("rio",) else 4)
    render(os.path.join(prev, "map_top.png"), Vector((0, 0, 300)), Vector((0, 0, 0)), 290, ortho=True, w=1800, h=1100)
    print("MAP_OK", len(OBJS), "objetos;", len(LAYOUT["items"]), "marcadores")


if __name__ == "__main__":
    main()
