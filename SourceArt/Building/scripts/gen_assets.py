"""Segundo lote de assets de GameJamm: armas, materiales de fusión, piezas extra de vehículo y props del mundo.

Uso (Blender 5.x, sin ventana):
    blender --background --factory-startup --python gen_assets.py -- <raiz SourceArt> [carpeta_previews]

Reutiliza las herramientas de gen_parts.py (marco UE, paleta, UCX_, SOCKET_). Cada grupo va a su carpeta:
SourceArt/Weapons, SourceArt/Fusion, SourceArt/Building (piezas extra) y SourceArt/World/Props.
Pivote: centro de la caja (armas y piezas) o centro de la base (fusión y props), según PROMPTS-ASSETS.md.
"""
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_parts as gp  # noqa: E402

Part, ue = gp.Part, gp.ue


def rot_last(part, n, center, axis, deg):
    """Gira los últimos n vértices agregados (la última sub-pieza) alrededor de `center` (cm, marco UE)."""
    verts = list(part.bm.verts)[-n:]
    bmesh.ops.rotate(part.bm, cent=ue(center), matrix=Matrix.Rotation(math.radians(deg), 3, axis), verts=verts)


# ---------------------------------------------------------------- armas (eje largo en X, mango en -X)
def sword(p):
    p.box((-38, 0, 0), (22, 5, 5), "leather", bevel=0.8)              # mango
    p.box((-50, 0, 0), (5, 7, 7), "iron", bevel=1)                    # pomo
    p.box((-25, 0, 0), (5, 22, 6), "iron", bevel=1)                   # guarda
    p.box((10, 0, 0), (66, 9, 2.4), "white", bevel=0.6)               # hoja
    p.cyl((44, 0, 0), 4.5, 2.4, "z", "white", 3)                       # punta
    p.box((10, 0, 0), (60, 1.5, 2.6), "stone")                        # canal


def bow(p):
    # brazos: cadena de tablitas sobre un arco de círculo en XZ (ápice en +X, cuerda en -X)
    pts = []
    for i in range(9):
        a = math.radians(-65 + 130 * i / 8)
        pts.append((-40 + 52 * math.cos(a), 52 * math.sin(a)))
    for (x0, z0), (x1, z1) in zip(pts, pts[1:]):
        mx, mz = (x0 + x1) / 2, (z0 + z1) / 2
        seg = math.hypot(x1 - x0, z1 - z0)
        p.box((mx, 0, mz), (3.5, 4, seg + 1.5), "wood")
        rot_last(p, 8, (mx, 0, mz), "Y", math.degrees(math.atan2(x1 - x0, z1 - z0)))
    p.box((12, 0, 0), (4, 5, 16), "leather", bevel=0.5)                # empuñadura en el ápice
    p.box((pts[0][0], 0, 0), (0.6, 0.6, pts[-1][1] - pts[0][1]), "cloth")  # cuerda


def arrow(p):
    p.cyl((0, 0, 0), 0.9, 70, "x", "wood", 6)                          # astil
    p.cyl((37, 0, 0), 2.2, 6, "x", "iron", 6, radius2=0.2)             # punta
    for k in range(3):                                                # plumas
        a = 90 + 120 * k
        p.box((-30, math.cos(math.radians(a)) * 2.2, math.sin(math.radians(a)) * 2.2), (10, 0.4, 3.5), "red")
        rot_last(p, 8, (-30, 0, 0), "X", -(a - 90))


# ---------------------------------------------------------------- materiales de fusión (caja 25 cm, pivote abajo)
def fuse_fire(p):
    p.cyl((0, 0, 0), 8, 16, "z", "red", 10)
    p.cyl((0, 0, 9.5), 4, 3, "z", "iron", 8)
    p.box((0, -8.3, 1), (6, 1, 8), "orange")                          # llama pintada
    p.box((0, -8.3, 4), (3, 1, 5), "spark")
    p.cyl((0, 0, -6), 8.4, 2, "z", "leather", 10)                      # correa


def fuse_ice(p):
    p.box((0, 0, 0), (9, 9, 22), "cyan")
    rot_last(p, 8, (0, 0, 0), "Z", 45)
    p.box((5, 3, -3), (6, 6, 13), "cyan")
    rot_last(p, 8, (5, 3, -3), "Y", 25)
    p.box((-5, -3, -4), (6, 6, 11), "white")
    rot_last(p, 8, (-5, -3, -4), "X", -25)


def fuse_rock(p):
    p.sphere((0, 0, 0), 12, "stone", subdiv=1)
    p.sphere((5, 4, -4), 7, "stone", subdiv=1)


def fuse_shock(p):
    p.box((0, 0, -9), (16, 16, 6), "wood", bevel=0.8)
    for i in range(4):
        p.cyl((0, 0, -4 + i * 3.5), 5.5 - i * 0.6, 2, "z", "copper", 10)
    p.sphere((0, 0, 10), 4, "spark", subdiv=1)


def fuse_bomb(p):
    p.sphere((0, 0, -2), 11, "black", subdiv=2)
    p.cyl((0, 0, 9.5), 3, 3, "z", "iron", 8)
    p.cyl((2, 0, 12), 0.7, 5, "z", "cloth", 5)


# ---------------------------------------------------------------- piezas extra de vehículo (centro de la caja)
def panel(p):
    p.box((0, 0, 0), (100, 100, 8), "wood", bevel=1)
    for x in (-30, 30):
        p.box((x, 0, -5), (10, 100, 3), "wood_dark")
    for sx in (-1, 1):
        for sy in (-1, 1):
            p.box((sx * 45, sy * 45, 4.5), (10, 10, 1.5), "iron")


def wall(p):
    p.box((0, 0, 0), (200, 20, 150), "stone", bevel=2)
    for z in (-40, 40):
        p.box((0, 0, z), (204, 22, 10), "wood_dark")


def ball(p):
    p.sphere((0, 0, 0), 30, "wood", subdiv=2)
    p.cyl((0, 0, 0), 30.5, 6, "x", "iron", 12)
    p.cyl((0, 0, 0), 30.5, 6, "y", "iron", 12)


def wing(p):
    p.box((0, 0, 0), (60, 200, 5), "cloth", bevel=0.5)               # lona
    for y in (-95, -50, 0, 50, 95):
        p.box((0, y, 0), (62, 4, 7), "wood")                           # costillas
    p.box((25, 0, 0), (6, 200, 8), "wood_dark")                       # larguero


def hinge(p):
    p.box((0, 0, -9), (30, 30, 10), "iron", bevel=1)
    p.box((0, 0, 9), (30, 30, 10), "iron", bevel=1)
    p.cyl((0, 0, 0), 5, 30, "y", "black", 10)                          # eje
    for sx in (-1, 1):
        for sz in (-1, 1):
            p.cyl((sx * 10, 0, sz * 14.5), 2, 2, "z", "stone", 6)


def stake(p):
    p.cyl((0, 0, 10), 7, 60, "z", "wood", 8)
    p.cyl((0, 0, -30), 7, 20, "z", "iron", 8, radius2=0.5)             # punta
    p.cyl((0, 0, 38), 9, 4, "z", "iron", 8)                            # cabeza


def spring(p):
    p.box((0, 0, -17), (40, 40, 6), "wood", bevel=1)
    p.box((0, 0, 17), (40, 40, 6), "wood", bevel=1)
    for i in range(6):
        p.cyl((0, 0, -11 + i * 4.4), 14, 2, "z", "iron", 10)


# ---------------------------------------------------------------- props del mundo (pivote al centro de la base)
def chest(p):
    p.box((0, 0, -10), (120, 70, 50), "wood", bevel=2)
    p.cyl((0, 0, 15), 35, 120, "x", "wood_dark", 8)                    # tapa redonda
    for x in (-45, 45):
        p.box((x, 0, 0), (8, 72, 72), "iron")
    p.box((0, 0, -6), (122, 72, 8), "yellow")                          # franja de loot
    p.box((0, -36, 6), (12, 3, 14), "iron")                            # cerradura


def watchtower(p):
    for sx in (-1, 1):
        for sy in (-1, 1):
            p.box((sx * 120, sy * 120, -100), (30, 30, 600), "wood")
    for z in (-300, 0):
        p.box((0, 0, z), (280, 20, 20), "wood_dark")
        p.box((0, 0, z), (20, 280, 20), "wood_dark")
    p.box((0, 0, 220), (300, 300, 40), "wood", bevel=4)                # plataforma
    p.cyl((0, 0, 350), 60, 140, "z", "cyan", 6, radius2=0)             # cristal que dispara
    p.cyl((0, 0, 290), 60, 100, "z", "cyan", 6, radius2=60)
    p.box((150, 0, 0), (6, 120, 220), "red")                           # estandarte (MI cambia color de equipo)


def keep(p):
    p.box((0, 0, -100), (360, 360, 600), "stone", bevel=6)
    for sx in (-1, 1):
        for sy in (-1, 1):
            p.cyl((sx * 190, sy * 190, -50), 70, 700, "z", "stone", 10)
            p.cyl((sx * 190, sy * 190, 360), 85, 120, "z", "red", 10, radius2=0)
    for i in range(-2, 3):
        p.box((i * 75, 182, 220), (40, 20, 40), "stone")               # almenas
        p.box((i * 75, -182, 220), (40, 20, 40), "stone")
    p.box((182, 0, -150), (10, 160, 240), "wood_dark")                  # puerta
    p.box((182, 0, 80), (12, 140, 160), "red")                          # estandarte grande


def island_small(p):
    p.cyl((0, 0, 450), 750, 100, "z", "grass", 14)
    p.cyl((0, 0, 200), 740, 400, "z", "leather", 14, radius2=420)
    p.cyl((0, 0, -250), 420, 500, "z", "leather", 10, radius2=40)


def ruin_arch(p):
    for y in (-200, 200):
        p.box((0, y, -40), (140, 120, 420), "stone", bevel=6)
    p.box((0, 0, 200), (150, 540, 100), "stone", bevel=6)
    p.box((40, -260, -200), (80, 60, 100), "stone")                    # bloque caído
    p.box((0, 230, 60), (142, 60, 60), "grass")                         # musgo


def windmill(p):
    p.cyl((0, 0, -100), 180, 700, "z", "stone", 10, radius2=130)
    p.cyl((0, 0, 330), 150, 160, "z", "red", 10, radius2=10)
    p.cyl((-170, 0, 200), 25, 60, "x", "wood_dark", 8)                 # eje
    for k in range(4):
        p.box((-200, 0, 200), (10, 60, 400), "cloth")
        rot_last(p, 8, (-200, 0, 200), "X", 45 + 90 * k)


def bridge_seg(p):
    for i in range(3):
        p.box((-130 + i * 130, 0, -20), (115, 200, 12), "wood", bevel=1)
    for y in (-95, 95):
        for x in (-190, 190):
            p.box((x, y, 10), (12, 12, 100), "wood_dark")
        p.cyl((0, y, 50), 3, 400, "x", "cloth", 6)                     # cuerdas


def tree(p):
    p.cyl((0, 0, -250), 30, 200, "z", "wood_dark", 6)
    p.cyl((0, 0, 0), 150, 300, "z", "grass", 7, radius2=60)
    p.cyl((0, 0, 220), 100, 250, "z", "grass", 7, radius2=0)


def rock(p):
    p.sphere((0, 0, 0), 100, "stone", subdiv=1)
    p.sphere((70, 40, -30), 55, "stone", subdiv=1)


# grupo, asset, size (cm), builder, colisión, pivote, sockets [(nombre, cara | (x,y,z))]
SIX = [(f"Attach_{f}", f) for f in ("Top", "Bottom", "Front", "Back", "Left", "Right")]
ASSETS = [
    ("Weapons", "SM_WPN_SWORD", (100, 22, 7), sword, "box", "center", [("Grip", (-38, 0, 0)), ("Fuse_Slot", (36, 0, 0))]),
    ("Weapons", "SM_WPN_BOW", (32, 6, 120), bow, "box", "center", [("Grip", (14, 0, 0)), ("Arrow_Nock", (-15, 0, 0))]),
    ("Weapons", "SM_WPN_ARROW", (80, 6, 6), arrow, "box", "center", [("Fuse_Slot", (40, 0, 0))]),
    ("Fusion", "SM_FUSE_FIRE", (25, 25, 25), fuse_fire, "box", "bottom", [("Attach_Bottom", "Bottom")]),
    ("Fusion", "SM_FUSE_ICE", (25, 25, 25), fuse_ice, "box", "bottom", [("Attach_Bottom", "Bottom")]),
    ("Fusion", "SM_FUSE_ROCK", (25, 25, 25), fuse_rock, "box", "bottom", [("Attach_Bottom", "Bottom")]),
    ("Fusion", "SM_FUSE_SHOCK", (25, 25, 25), fuse_shock, "box", "bottom", [("Attach_Bottom", "Bottom")]),
    ("Fusion", "SM_FUSE_BOMB", (25, 25, 25), fuse_bomb, "box", "bottom", [("Attach_Bottom", "Bottom")]),
    ("Building", "SM_PART_PANEL", (100, 100, 10), panel, "box", "center", SIX),
    ("Building", "SM_PART_WALL", (200, 22, 150), wall, "box", "center", SIX),
    ("Building", "SM_PART_BALL", (60, 60, 60), ball, "box", "center", SIX),
    ("Building", "SM_DEV_WING", (62, 200, 8), wing, "box", "center", SIX),
    ("Building", "SM_JOINT_HINGE", (30, 30, 30), hinge, "box", "center", SIX),
    ("Building", "SM_ANCHOR_STAKE", (18, 18, 80), stake, "box", "center", [("Attach_Top", "Top")]),
    ("Building", "SM_DEV_SPRING", (40, 40, 40), spring, "box", "center", [("Attach_Top", "Top"), ("Attach_Bottom", "Bottom")]),
    ("World/Props", "SM_PROP_CHEST", (122, 72, 72), chest, "box", "bottom", []),
    ("World/Props", "SM_TOWER_WATCH", (300, 300, 800), watchtower, "box", "bottom", [("Shoot", (0, 0, 640))]),
    ("World/Props", "SM_BASE_KEEP", (530, 530, 800), keep, "box", "bottom", []),
    ("World/Props", "SM_ISLAND_SMALL", (1500, 1500, 1000), island_small, "none", "top", []),
    ("World/Props", "SM_PROP_RUIN_ARCH", (150, 540, 500), ruin_arch, "box", "bottom", []),
    ("World/Props", "SM_PROP_WINDMILL", (400, 360, 900), windmill, "box", "bottom", []),
    ("World/Props", "SM_PROP_BRIDGE", (400, 200, 100), bridge_seg, "box", "top_deck", []),
    ("World/Props", "SM_PROP_TREE", (300, 300, 700), tree, "trunk", "bottom", []),
    ("World/Props", "SM_PROP_ROCK", (330, 260, 200), rock, "box", "bottom", []),
]


# ---------------------------------------------------------------- lote 3: dispositivos, personaje, proyectiles, pickups, mundo
def dev_frost(p):
    p.cyl((-6, 0, 0), 8, 26, "x", "cyan", 10)
    p.cyl((14, 0, 0), 4, 14, "x", "iron", 8, radius2=6)
    p.cyl((20, 0, 0), 6.5, 2, "x", "white", 8)
    for x in (-12, 0):
        p.cyl((x, 0, 0), 8.4, 2, "x", "iron", 10)


def dev_shock(p):
    p.box((-8, 0, 0), (22, 18, 18), "wood_dark", bevel=1)
    for i in range(3):
        p.cyl((6 + i * 4, 0, 0), 6 - i * 1.2, 2.5, "x", "copper", 10)
    p.sphere((18, 0, 0), 3, "spark", subdiv=1)


def ctrl_head(p):
    p.cyl((0, 0, -10), 14, 10, "z", "iron", 10)                      # base giratoria
    p.cyl((0, 0, -2), 4, 8, "z", "iron", 8)                          # cuello
    p.box((0, 0, 8), (24, 22, 14), "wood", bevel=1.5)                  # cabeza
    p.cyl((12.5, 0, 9), 5, 3, "x", "cyan", 10)                         # lente (+X)


def battery_l(p):
    p.box((0, 0, -3), (40, 30, 30), "iron", bevel=1.5)
    p.box((0, 0, -3), (41, 22, 10), "yellow")
    for y in (-8, 8):
        p.cyl((0, y, 14), 3, 6, "z", "black" if y < 0 else "red", 8)
    p.box((0, 0, 14), (30, 3, 4), "leather")                           # asa


def timebomb(p):
    p.sphere((0, 0, -3), 15, "black", subdiv=2)
    p.cyl((0, -14.5, 0), 7, 2, "y", "white", 12)                       # reloj
    p.box((0, -15.8, 2), (1, 0.8, 5), "black")
    p.cyl((0, 0, 13), 3, 5, "z", "iron", 8)


def beanie(p):
    p.sphere((0, 0, 2), 11, "red", subdiv=2)
    p.cyl((0, 0, -6), 11.5, 5, "z", "cloth", 14)                       # doblez
    p.sphere((0, 0, 13), 3, "cloth", subdiv=1)                          # pompón


def goggle_helmet(p):
    p.sphere((0, 0, 0), 12, "leather", subdiv=2)
    p.cyl((0, 0, -4), 12.6, 3, "z", "iron", 14)
    for y in (-5, 5):
        p.cyl((11, y, 1), 3.5, 3, "x", "cyan", 10)


def junk_helmet(p):
    p.cyl((0, 0, 0), 12, 14, "z", "iron", 8, radius2=10)
    p.box((10, 0, 2), (4, 16, 3), "black")                             # visera
    p.box((0, 0, 8), (6, 2, 6), "yellow")                              # aleta de chatarra


def cannonball(p):
    p.sphere((0, 0, 0), 10, "black", subdiv=2)


def pickup_part(p):
    p.box((0, 0, 0), (40, 40, 40), "wood", bevel=2)
    p.box((0, 0, 0), (42, 10, 42), "yellow")
    p.box((0, 0, 0), (10, 42, 42), "yellow")


def pickup_material(p):
    p.sphere((0, 0, -2), 14, "leather", subdiv=2)                      # bolsa
    p.cyl((0, 0, 11), 6, 6, "z", "cloth", 8, radius2=3)
    p.sphere((0, 0, 17), 4, "spark", subdiv=1)


def island_medium(p):
    p.cyl((0, 0, 900), 1500, 100, "z", "grass", 16)
    p.cyl((0, 0, 500), 1480, 700, "z", "leather", 16, radius2=900)
    p.cyl((0, 0, -350), 900, 1000, "z", "leather", 12, radius2=80)


def fence(p):
    for x in (-90, 0, 90):
        p.box((x, 0, 0), (12, 12, 120), "wood_dark")
    for z in (-20, 30):
        p.box((0, 0, z), (200, 6, 14), "wood")


def barrel(p):
    p.cyl((0, 0, 0), 38, 100, "z", "wood", 12)
    p.cyl((0, 0, -15), 38, 100 * 0.7, "z", "wood", 12, radius2=42)
    for z in (-35, 35):
        p.cyl((0, 0, z), 40, 6, "z", "iron", 12)


def crate(p):
    p.box((0, 0, 0), (100, 100, 100), "wood", bevel=3)
    for sx in (-1, 1):
        p.box((sx * 50, 0, 0), (4, 104, 14), "wood_dark")
        p.box((sx * 50, 0, 0), (4, 14, 104), "wood_dark")


def scrap_pile(p):
    p.box((0, 0, -20), (160, 120, 40), "leather", bevel=4)
    p.cyl((-30, 10, 20), 30, 15, "y", "iron", 10)                      # rueda vieja
    p.box((30, -10, 10), (80, 20, 8), "wood")
    rot_last(p, 8, (30, -10, 10), "Y", 25)
    p.box((10, 25, 15), (40, 40, 30), "wood_dark", bevel=2)
    p.cyl((50, 30, 5), 8, 60, "x", "iron", 8)


def bush(p):
    p.sphere((0, 0, 0), 70, "grass", subdiv=1)
    p.sphere((50, 30, -15), 45, "grass", subdiv=1)
    p.sphere((-45, -20, -20), 40, "grass", subdiv=1)


def flag_pole(p):
    p.cyl((0, 0, 0), 6, 500, "z", "wood_dark", 8)
    p.box((60, 0, 180), (120, 4, 80), "red")                            # bandera (MI cambia color de equipo)
    p.cyl((0, 0, -240), 30, 20, "z", "stone", 8)


def ramp(p):
    p.box((0, 0, 0), (400, 300, 100), "grass")
    verts = list(p.bm.verts)[-8:]
    for v in verts:                                                    # cuña: baja la cara trasera (-X)
        if v.co.x < 0:
            v.co.z = -0.5
    p.box((0, 0, -60), (400, 300, 20), "leather")


def brazier(p):
    p.cyl((0, 0, -40), 8, 100, "z", "iron", 8)
    p.cyl((0, 0, 20), 35, 30, "z", "iron", 10, radius2=20)
    p.cyl((0, 0, 45), 18, 30, "z", "orange", 6, radius2=2)
    p.cyl((0, 0, -88), 30, 6, "z", "stone", 8)


def lane_sign(p):
    p.box((0, 0, -60), (10, 10, 160), "wood_dark")
    p.box((10, 0, 30), (120, 6, 40), "wood", bevel=1)
    p.cyl((75, 0, 30), 24, 6, "y", "wood", 3)                          # punta de flecha (+X)


LOT3 = [
    ("Building", "SM_DEV_FROST", (40, 20, 20), dev_frost, "box", "center", SIX + [("Fx_Out", "Front")]),
    ("Building", "SM_DEV_SHOCK", (40, 20, 20), dev_shock, "box", "center", SIX + [("Fx_Out", "Front")]),
    ("Building", "SM_CTRL_HEAD", (40, 30, 40), ctrl_head, "box", "center", SIX + [("Fx_Out", "Front")]),
    ("Building", "SM_PWR_BATTERY_L", (40, 30, 40), battery_l, "box", "center", SIX),
    ("Building", "SM_CTRL_TIMEBOMB", (30, 30, 36), timebomb, "box", "center", SIX),
    ("Characters", "SM_ACC_BEANIE", (24, 24, 20), beanie, "none", "bottom", [("Head", "Bottom")]),
    ("Characters", "SM_ACC_GOGGLE_HELMET", (26, 26, 24), goggle_helmet, "none", "bottom", [("Head", "Bottom")]),
    ("Characters", "SM_ACC_JUNK_HELMET", (26, 26, 20), junk_helmet, "none", "bottom", [("Head", "Bottom")]),
    ("Pickups", "SM_PROJ_CANNONBALL", (20, 20, 20), cannonball, "box", "center", []),
    ("Pickups", "SM_PICKUP_PART", (44, 44, 44), pickup_part, "box", "bottom", []),
    ("Pickups", "SM_PICKUP_MATERIAL", (30, 30, 40), pickup_material, "box", "bottom", []),
    ("World/Props", "SM_ISLAND_MEDIUM", (3000, 3000, 2000), island_medium, "none", "top", []),
    ("World/Props", "SM_PROP_FENCE", (200, 12, 120), fence, "box", "bottom", []),
    ("World/Props", "SM_PROP_BARREL", (84, 84, 100), barrel, "box", "bottom", []),
    ("World/Props", "SM_PROP_CRATE", (104, 104, 104), crate, "box", "bottom", []),
    ("World/Props", "SM_PROP_SCRAP_PILE", (180, 140, 80), scrap_pile, "box", "bottom", []),
    ("World/Props", "SM_PROP_BUSH", (210, 160, 120), bush, "none", "bottom", []),
    ("World/Props", "SM_PROP_FLAG", (180, 60, 520), flag_pole, "trunk", "bottom", []),
    ("World/Props", "SM_PROP_RAMP", (400, 300, 120), ramp, "none", "bottom", []),
    ("World/Props", "SM_PROP_BRAZIER", (70, 70, 180), brazier, "box", "bottom", []),
    ("World/Props", "SM_PROP_LANE_SIGN", (160, 50, 180), lane_sign, "trunk", "bottom", []),
]


# ---------------------------------------------------------------- lote 4: piezas alineadas con la tabla de JamBuildPart.cpp
def wall_v(p):        # 30 x 200 x 150 (delgado en X)
    p.box((0, 0, 0), (30, 200, 150), "stone", bevel=2)
    for z in (-40, 40):
        p.box((0, 0, z), (32, 204, 10), "wood_dark")


def panel_v(p):       # 10 x 100 x 100 (delgado en X)
    p.box((0, 0, 0), (8, 100, 100), "wood", bevel=1)
    for y in (-30, 30):
        p.box((-5, y, 0), (3, 10, 100), "wood_dark")
    for sy in (-1, 1):
        for sz in (-1, 1):
            p.box((4.5, sy * 45, sz * 45), (1.5, 10, 10), "iron")


def girder(p):        # 300 x 20 x 20
    p.box((0, 0, 8), (300, 20, 4), "iron")
    p.box((0, 0, -8), (300, 20, 4), "iron")
    p.box((0, 0, 0), (300, 4, 14), "iron")
    for x in range(-135, 140, 45):
        p.cyl((x, 0, 10.5), 2, 1.5, "z", "stone", 6)


def sled(p):          # 160 x 70 x 15
    p.box((0, 0, 4), (130, 70, 6), "wood", bevel=1)
    for y in (-30, 30):
        p.box((0, y, -4), (150, 6, 4), "iron")
        p.cyl((74, y, 0), 5, 6, "y", "iron", 8)                       # punta curva
        for x in (-40, 40):
            p.box((x, y, 0), (6, 6, 8), "wood_dark")


def hinge_v(p):       # 20 x 60 x 20 (eje en Y)
    p.box((-6, 0, 0), (8, 60, 20), "iron", bevel=1)
    p.box((6, 0, 0), (8, 60, 20), "iron", bevel=1)
    p.cyl((0, 0, 0), 4, 60, "y", "black", 10)


def turntable(p):     # 40 x 40 x 30
    p.cyl((0, 0, -10), 20, 10, "z", "iron", 12)
    p.cyl((0, 0, 0), 12, 10, "z", "black", 10)
    p.cyl((0, 0, 10), 19, 10, "z", "wood", 12)
    p.box((12, 0, 15), (10, 4, 1), "yellow")                            # marca de giro


def stake_s(p):       # 15 x 15 x 20
    p.cyl((0, 0, 4), 6, 10, "z", "wood", 8)
    p.cyl((0, 0, -6), 6, 8, "z", "iron", 8, radius2=0.5)
    p.cyl((0, 0, 9), 7.5, 2, "z", "iron", 8)


def spring_s(p):      # 30 x 30 x 18
    p.box((0, 0, -7), (30, 30, 4), "wood", bevel=0.8)
    p.box((0, 0, 7), (30, 30, 4), "wood", bevel=0.8)
    for i in range(3):
        p.cyl((0, 0, -3 + i * 3), 10, 1.5, "z", "iron", 10)


def jet(p):           # 60 x 40 x 40, tobera hacia -X
    p.cyl((5, 0, 0), 18, 40, "x", "iron", 12)
    p.cyl((-20, 0, 0), 14, 14, "x", "black", 12, radius2=18)
    p.cyl((26, 0, 0), 20, 6, "x", "copper", 12)
    p.cyl((24, 0, 0), 12, 3, "x", "black", 12)


def turbine(p):       # 25 x 90 x 90
    p.ring((0, 0, 0), 45, 40, 25, "x", "iron", 20)
    p.cyl((0, 0, 0), 8, 25, "x", "wood_dark", 10)
    for k in range(6):
        a = 90 + 60 * k
        p.box((0, math.cos(math.radians(a)) * 22, math.sin(math.radians(a)) * 22), (4, 9, 34), "wood")
        rot_last(p, 8, (0, math.cos(math.radians(a)) * 22, math.sin(math.radians(a)) * 22), "X", -(a - 90))


def hover_stone(p):   # 45 x 45 x 45
    p.sphere((0, 0, 0), 22, "stone", subdiv=1)
    p.cyl((0, 0, -16), 8, 8, "z", "cyan", 6, radius2=2)
    p.cyl((0, 0, 0), 22.8, 4, "z", "cyan", 10)                        # vena brillante


def wing_s(p):        # 80 x 160 x 6
    p.box((0, 0, 0), (80, 160, 4), "cloth", bevel=0.5)
    for y in (-75, -38, 0, 38, 75):
        p.box((0, y, 0), (82, 4, 6), "wood")
    p.box((30, 0, 0), (6, 160, 6), "wood_dark")


def sail(p):          # 10 x 120 x 100
    p.box((0, 0, 0), (3, 110, 96), "cloth")
    p.box((-3, 0, 48), (6, 120, 4), "wood_dark")
    p.box((-3, 0, -48), (6, 120, 4), "wood_dark")
    p.box((-3, 0, 0), (6, 5, 100), "wood")


def stabilizer(p):    # 30 x 30 x 20
    p.cyl((0, 0, -5), 15, 10, "z", "iron", 10)
    p.sphere((0, 0, 5), 9, "copper", subdiv=1)
    p.cyl((0, 0, 0), 15.3, 2, "z", "cyan", 10)


def head_c(p):        # 40 x 40 x 40
    p.cyl((0, 0, -15), 18, 10, "z", "iron", 10)
    p.cyl((0, 0, -6), 5, 8, "z", "iron", 8)
    p.box((0, 0, 8), (30, 28, 22), "wood", bevel=1.5)
    p.cyl((15.5, 0, 9), 7, 3, "x", "cyan", 10)


def battery_c(p):     # 35 x 35 x 35
    p.box((0, 0, -3), (35, 35, 28), "iron", bevel=1.5)
    p.box((0, 0, -3), (36, 25, 9), "yellow")
    for y in (-9, 9):
        p.cyl((0, y, 13), 3, 6, "z", "black" if y < 0 else "red", 8)


def emitter(color_tank, tip):
    def b(p):         # 40 x 25 x 25, boquilla hacia +X
        p.cyl((-6, 0, 0), 10, 26, "x", color_tank, 10)
        for x in (-12, 0):
            p.cyl((x, 0, 0), 10.5, 2, "x", "iron", 10)
        p.cyl((14, 0, 0), 5, 14, "x", "iron", 8, radius2=7)
        p.cyl((20, 0, 0), 7.5, 2, "x", tip, 8)
    return b


def shock_c(p):       # 40 x 25 x 25
    p.box((-8, 0, 0), (22, 24, 24), "wood_dark", bevel=1)
    for i in range(3):
        p.cyl((6 + i * 4, 0, 0), 8 - i * 1.6, 2.5, "x", "copper", 10)
    p.sphere((18, 0, 0), 3.5, "spark", subdiv=1)


def beam_c(p):        # 50 x 12 x 12
    p.cyl((-5, 0, 0), 5.5, 40, "x", "iron", 8)
    p.cyl((18, 0, 0), 6, 6, "x", "copper", 8)
    p.cyl((23, 0, 0), 4, 3, "x", "cyan", 8)


def bomb_c(p):        # 30 x 30 x 30
    p.sphere((0, 0, -2), 13, "black", subdiv=2)
    p.cyl((0, -12.5, 0), 6, 2, "y", "white", 12)
    p.cyl((0, 0, 12), 3, 5, "z", "iron", 8)


def hydrant(p):       # 40 x 30 x 30, boquilla hacia +X
    p.cyl((-5, 0, 0), 13, 28, "z", "red", 10)
    p.cyl((-5, 0, 15), 11, 4, "z", "iron", 10, radius2=4)
    p.cyl((12, 0, 2), 4, 14, "x", "iron", 8)
    p.cyl((19, 0, 2), 5.5, 2, "x", "cyan", 8)


def light_c(p):       # 25 x 25 x 25, luz hacia +X
    p.box((-3, 0, -4), (18, 22, 12), "iron", bevel=1)
    p.cyl((4, 0, 3), 9, 12, "x", "iron", 10, radius2=11)
    p.cyl((10.5, 0, 3), 10.5, 1.5, "x", "spark", 10)


FX = [("Fx_Out", "Front")]
LOT4 = [
    ("Building", "SM_PART_WALL", (30, 200, 150), wall_v, "box", "center", SIX),
    ("Building", "SM_PART_PANEL", (10, 100, 100), panel_v, "box", "center", SIX),
    ("Building", "SM_PART_GIRDER", (300, 20, 20), girder, "box", "center", SIX),
    ("Building", "SM_DEV_SLED", (160, 70, 15), sled, "box", "center", SIX),
    ("Building", "SM_JOINT_HINGE", (20, 60, 20), hinge_v, "box", "center", SIX),
    ("Building", "SM_JOINT_TURNTABLE", (40, 40, 30), turntable, "box", "center", SIX),
    ("Building", "SM_ANCHOR_STAKE", (15, 15, 20), stake_s, "box", "center", SIX),
    ("Building", "SM_DEV_SPRING", (30, 30, 18), spring_s, "box", "center", SIX),
    ("Building", "SM_DEV_JET", (60, 40, 40), jet, "box", "center", SIX + [("Fx_Out", "Back")]),
    ("Building", "SM_DEV_TURBINE", (25, 90, 90), turbine, "box", "center", SIX + FX),
    ("Building", "SM_DEV_HOVER", (45, 45, 45), hover_stone, "box", "center", SIX),
    ("Building", "SM_DEV_WING", (80, 160, 6), wing_s, "box", "center", SIX),
    ("Building", "SM_DEV_SAIL", (10, 120, 100), sail, "box", "center", SIX),
    ("Building", "SM_DEV_STABILIZER", (30, 30, 20), stabilizer, "box", "center", SIX),
    ("Building", "SM_CTRL_HEAD", (40, 40, 40), head_c, "box", "center", SIX + FX),
    ("Building", "SM_PWR_BATTERY_L", (35, 35, 35), battery_c, "box", "center", SIX),
    ("Building", "SM_DEV_FROST", (40, 25, 25), emitter("cyan", "white"), "box", "center", SIX + FX),
    ("Building", "SM_DEV_SHOCK", (40, 25, 25), shock_c, "box", "center", SIX + FX),
    ("Building", "SM_DEV_BEAM", (50, 12, 12), beam_c, "box", "center", SIX + FX),
    ("Building", "SM_CTRL_TIMEBOMB", (30, 30, 30), bomb_c, "box", "center", SIX),
    ("Building", "SM_DEV_HYDRANT", (40, 30, 30), hydrant, "box", "center", SIX + FX),
    ("Building", "SM_DEV_LIGHT", (25, 25, 25), light_c, "box", "center", SIX + FX),
]


def apply_pivot(obj, size, pivot):
    """fit_to_box deja el centro en el origen; mueve la malla para poner el pivote donde corresponde."""
    dz = {"center": 0.0, "bottom": size[2] / 2, "top": -size[2] / 2, "top_deck": -size[2] / 2 + 0.1}[pivot] / 100.0
    for v in obj.data.vertices:
        v.co.z += dz
    return dz


def socket_at(obj, name, where, size, dz):
    if isinstance(where, str):
        e = gp.add_socket(obj, name, gp.FACES[where], size)
        e.location.z += dz
    else:
        e = bpy.data.objects.new(f"SOCKET_{name}", None)
        e.empty_display_type = "ARROWS"
        e.empty_display_size = 0.08
        e.location = ue(where)
        e.location.z += dz
        bpy.context.scene.collection.objects.link(e)
        e.parent = obj
    return e


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    root = os.path.abspath(argv[0] if argv else os.path.join(os.path.dirname(__file__), "..", ".."))
    preview_dir = os.path.abspath(argv[1]) if len(argv) > 1 else root
    lot = argv[2] if len(argv) > 2 else "lot2"
    assets = {"lot3": LOT3, "lot4": LOT4}.get(lot, ASSETS)
    if lot != "lot4":  # las piezas de Building de los lotes 2 y 3 quedaron reemplazadas por LOT4 (medidas del código)
        assets = [a for a in assets if a[0] != "Building"]
    os.makedirs(preview_dir, exist_ok=True)

    gp.clear_scene()
    mat = gp.make_palette_material(os.path.join(root, "Building"))

    report, objs = [], []
    for group, sm, size, builder, coll, pivot, sockets in assets:
        fbx_dir = os.path.join(root, group, "FBX")
        blend_dir = os.path.join(root, group, "Blend")
        os.makedirs(fbx_dir, exist_ok=True)
        os.makedirs(blend_dir, exist_ok=True)
        p = Part()
        builder(p)
        obj = p.to_object(sm, size)
        dz = apply_pivot(obj, size, pivot)
        obj.data.materials.append(mat)
        if coll == "box":
            u = gp.add_ucx(obj, size, "box")
            for v in u.data.vertices:
                v.co.z += dz
        elif coll == "trunk":  # solo el tronco bloquea; la copa no
            u = gp.add_ucx(obj, (size[0] * 0.2, size[1] * 0.2, size[2]), "box")
            for v in u.data.vertices:
                v.co.z += dz
        for name, where in sockets:
            socket_at(obj, name, where, size, dz)
        gp.export_fbx(obj, os.path.join(fbx_dir, f"{sm}.fbx"))
        others = [o for o in bpy.context.scene.objects if o is not obj and o.parent is not obj]
        for o in others:
            bpy.context.scene.collection.objects.unlink(o)
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(blend_dir, f"{sm}.blend"), copy=True)
        for o in others:
            bpy.context.scene.collection.objects.link(o)
        bad = [c.name for c in obj.children if "." in c.name]
        if bad:
            raise RuntimeError(f"{sm}: nombres con sufijo {bad}")
        for c in list(obj.children):
            bpy.data.objects.remove(c, do_unlink=True)
        tris = sum(len(f.vertices) - 2 for f in obj.data.polygons)
        d = obj.dimensions
        report.append(f"{group:12s} {sm:18s} size_cm={d.x*100:.0f}x{d.y*100:.0f}x{d.z*100:.0f} tris={tris}")
        objs.append(obj)

    groups = {}
    for (group, *_), o in zip(assets, objs):
        groups.setdefault(group, []).append(o)
    for group, gobjs in groups.items():
        for o in objs:
            o.hide_render = o not in gobjs
        # borra las copias y la cámara de la vista previa anterior
        prev = [o for o in bpy.context.scene.objects if o.type == "MESH" and o not in objs]
        for o in prev:
            bpy.data.objects.remove(o, do_unlink=True)
        for c in [o for o in bpy.context.scene.objects if o.type == "CAMERA"]:
            bpy.data.objects.remove(c, do_unlink=True)
        gp.render_previews(gobjs, preview_dir)
        os.replace(os.path.join(preview_dir, "parts_preview.png"),
                   os.path.join(preview_dir, f"{lot}_{group.replace('/', '_')}.png"))
    print("ASSETS_REPORT_BEGIN")
    print("\n".join(report))
    print("ASSETS_REPORT_END")


if __name__ == "__main__":
    main()
