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


# ---------------------------------------------------------------- lote 5: piezas para mecánicas (cofre que abre, FX, spawn)
def chest_base(p):    # 122 x 72 x 50, la tapa va aparte para animarla
    p.box((0, 0, 0), (120, 70, 50), "wood", bevel=2)
    for x in (-45, 45):
        p.box((x, 0, 0), (8, 72, 50), "iron")
    p.box((0, 0, 8), (122, 72, 8), "yellow")
    p.box((0, -36, 16), (12, 3, 10), "iron")


def chest_lid(p):     # 122 x 72 x 36, pivote en la bisagra trasera (Y+) abajo
    p.cyl((0, 0, 0), 35, 120, "x", "wood_dark", 8)
    for x in (-45, 45):
        p.cyl((x, 0, 0), 36, 8, "x", "iron", 8)
    p.box((0, 0, -17), (122, 72, 4), "wood")


def proj_bolt(p):     # 60 x 14 x 14, proyectil de cristal de la torre (+X)
    p.cyl((5, 0, 0), 6, 30, "x", "cyan", 6)
    p.cyl((25, 0, 0), 6, 12, "x", "cyan", 6, radius2=0.5)
    p.cyl((-18, 0, 0), 2, 16, "x", "white", 6, radius2=6)


def shard_wood(p):    # astilla de madera para destrucción
    p.box((0, 0, 0), (40, 8, 3), "wood")
    rot_last(p, 8, (0, 0, 0), "Z", 12)
    p.box((14, 3, 0), (14, 5, 3), "wood_dark")
    rot_last(p, 8, (14, 3, 0), "Z", -25)


def shard_stone(p):
    p.sphere((0, 0, 0), 10, "stone", subdiv=1)


def spawn_pad(p):     # 600 x 600 x 40, plataforma de reaparición (franjas en color de equipo)
    p.cyl((0, 0, 0), 300, 40, "z", "stone", 12)
    p.cyl((0, 0, 21), 240, 3, "z", "red", 12)
    p.cyl((0, 0, 23), 200, 3, "z", "stone", 12)
    for k in range(4):
        a = math.radians(45 + 90 * k)
        p.cyl((math.cos(a) * 270, math.sin(a) * 270, 80), 18, 120, "z", "wood_dark", 6)
        p.sphere((math.cos(a) * 270, math.sin(a) * 270, 150), 22, "cyan", subdiv=1)


def waterfall(p):     # 400 x 60 x 1500, cascada estilizada para orillas de isla
    p.box((0, 0, 0), (400, 20, 1500), "cyan")
    for i, x in enumerate((-150, -50, 60, 140)):
        p.box((x, -12, 300 - i * 180), (40, 6, 500), "white")
    p.box((0, -20, -720), (480, 60, 60), "white")                    # espuma


def floating_rock(p):  # roca flotante decorativa con punta abajo
    p.cyl((0, 0, 80), 150, 60, "z", "grass", 8)
    p.cyl((0, 0, -40), 145, 180, "z", "leather", 8, radius2=10)


LOT5 = [
    ("World/Props", "SM_PROP_CHEST_BASE", (122, 72, 50), chest_base, "box", "bottom", [("Lid_Hinge", (0, 36, 25))]),
    ("World/Props", "SM_PROP_CHEST_LID", (122, 72, 36), chest_lid, "box", "hinge_back", []),
    ("World/Props", "SM_PROP_SPAWN_PAD", (600, 600, 160), spawn_pad, "box", "bottom", [("Spawn", (0, 0, 30))]),
    ("World/Props", "SM_PROP_WATERFALL", (480, 60, 1500), waterfall, "none", "top", []),
    ("World/Props", "SM_PROP_FLOATING_ROCK", (300, 300, 300), floating_rock, "box", "top", []),
    ("Pickups", "SM_PROJ_BOLT", (60, 14, 14), proj_bolt, "box", "center", []),
    ("FX", "SM_FX_SHARD_WOOD", (50, 12, 3), shard_wood, "box", "center", []),
    ("FX", "SM_FX_SHARD_STONE", (20, 18, 16), shard_stone, "box", "center", []),
]


# ---------------------------------------------------------------- lote 6: kit para vestir el mapa
def tree_round(p):
    p.cyl((0, 0, -200), 35, 300, "z", "wood_dark", 6)
    p.sphere((0, 0, 100), 190, "grass", subdiv=1)
    p.sphere((90, 60, 40), 110, "grass", subdiv=1)


def tree_tall(p):
    p.cyl((0, 0, -350), 30, 200, "z", "wood_dark", 6)
    for r, z in ((180, -150), (140, 50), (100, 230), (60, 380)):
        p.cyl((0, 0, z), r, 220, "z", "grass", 7, radius2=r * 0.25)


def tree_dead(p):
    p.cyl((0, 0, -100), 35, 400, "z", "wood_dark", 6, radius2=18)
    for ang, z in ((35, 40), (-50, 120), (160, 60)):
        p.box((0, 0, z), (12, 12, 160), "wood_dark")
        rot_last(p, 8, (0, 0, z - 60), "Y", ang)


def rock_flat(p):
    p.cyl((0, 0, 0), 150, 60, "z", "stone", 7, radius2=120)
    p.cyl((60, 40, 40), 60, 40, "z", "stone", 6, radius2=40)


def rock_spire(p):
    p.cyl((0, 0, 0), 80, 500, "z", "stone", 6, radius2=10)
    p.cyl((60, 30, -150), 50, 200, "z", "stone", 5, radius2=8)


def grass_tuft(p):
    for k in range(5):
        a = math.radians(72 * k)
        p.cyl((math.cos(a) * 12, math.sin(a) * 12, 0), 6, 50, "z", "grass", 3, radius2=0.5)
        rot_last(p, 6, (math.cos(a) * 12, math.sin(a) * 12, -25), "X", 15 * (1 if k % 2 else -1))


def flowers(p):
    for k, col in enumerate(("red", "yellow", "white", "red", "cyan")):
        a = math.radians(72 * k)
        x, y = math.cos(a) * 20, math.sin(a) * 20
        p.cyl((x, y, -5), 1.5, 30, "z", "grass", 4)
        p.sphere((x, y, 12), 5, col, subdiv=1)


def mushroom(p):
    p.cyl((0, 0, -15), 8, 30, "z", "cloth", 8)
    p.cyl((0, 0, 8), 25, 16, "z", "red", 10, radius2=6)
    for x, y in ((10, 5), (-8, 10), (2, -12)):
        p.sphere((x, y, 13), 3, "white", subdiv=1)


def ruin_pillar(p):
    p.box((0, 0, -180), (90, 90, 40), "stone", bevel=4)
    p.cyl((0, 0, 0), 35, 340, "z", "stone", 8)
    p.box((0, 0, 160), (80, 80, 30), "stone", bevel=3)
    p.box((25, 25, -150), (30, 30, 20), "grass")


def ruin_wall(p):
    for i, h in enumerate((260, 200, 300, 140)):
        p.box((-150 + i * 100, 0, -150 + h / 2), (96, 60, h), "stone", bevel=3)
    p.box((0, 0, -140), (400, 64, 20), "grass")


def ruin_stairs(p):
    for i in range(5):
        p.box((-120 + i * 60, 0, -70 + i * 35), (60, 300, 35), "stone", bevel=2)


def platform(p):
    p.box((0, 0, 90), (400, 400, 20), "wood", bevel=2)
    for x in range(-150, 151, 100):
        p.box((x, 0, 101), (6, 400, 2), "wood_dark")
    for sx in (-1, 1):
        for sy in (-1, 1):
            p.box((sx * 180, sy * 180, -10), (25, 25, 200), "wood_dark")


def stairs_wood(p):
    for i in range(6):
        p.box((-125 + i * 50, 0, -85 + i * 35), (50, 160, 10), "wood")
    for y in (-80, 80):
        p.box((0, y, 0), (330, 10, 20), "wood_dark")
        rot_last(p, 8, (0, y, 0), "Y", -35)


def ladder(p):
    for y in (-25, 25):
        p.box((0, y, 0), (8, 8, 400), "wood_dark")
    for z in range(-170, 180, 45):
        p.cyl((0, 0, z), 3, 50, "y", "wood", 6)


def scaffold(p):
    for sx in (-1, 1):
        for sy in (-1, 1):
            p.box((sx * 140, sy * 90, 0), (12, 12, 500), "wood_dark")
    for z in (-100, 120):
        p.box((0, 0, z), (300, 200, 12), "wood")
    p.box((0, 95, 0), (300, 6, 10), "wood_dark")
    rot_last(p, 8, (0, 95, 0), "Y", 50)


def tent(p):
    p.box((0, -70, 0), (300, 10, 220), "cloth")
    rot_last(p, 8, (0, -70, 0), "X", 32)
    p.box((0, 70, 0), (300, 10, 220), "cloth")
    rot_last(p, 8, (0, 70, 0), "X", -32)
    p.box((0, 0, 95), (310, 12, 12), "wood_dark")
    p.box((0, 0, -100), (320, 280, 6), "leather")


def campfire(p):
    for k in range(6):
        a = math.radians(60 * k)
        p.sphere((math.cos(a) * 45, math.sin(a) * 45, -15), 14, "stone", subdiv=1)
    for k in range(3):
        p.box((0, 0, -10), (80, 10, 10), "wood_dark")
        rot_last(p, 8, (0, 0, -10), "Z", 60 * k)
    p.cyl((0, 0, 15), 22, 45, "z", "orange", 6, radius2=2)
    p.cyl((0, 0, 10), 12, 30, "z", "spark", 5, radius2=1)


def workbench(p):     # mesa de construcción del jugador
    p.box((0, 0, 10), (200, 90, 12), "wood", bevel=1.5)
    for sx in (-1, 1):
        for sy in (-1, 1):
            p.box((sx * 85, sy * 35, -40), (12, 12, 90), "wood_dark")
    p.box((-60, 0, 25), (40, 30, 18), "iron", bevel=1)               # tornillo de banco
    p.cyl((40, 10, 22), 12, 6, "z", "iron", 10)                       # engrane
    p.box((40, -25, 18), (50, 6, 4), "leather")                       # herramienta
    p.box((0, 40, 55), (200, 8, 80), "wood_dark")                     # tablero de herramientas


def weapon_rack(p):
    p.box((0, 0, -60), (160, 50, 20), "wood_dark")
    p.box((0, 15, 20), (160, 10, 160), "wood")
    for x in (-50, 0, 50):
        p.box((x, -5, 30), (10, 4, 120), "white")                      # espadas
        p.box((x, -5, -25), (26, 6, 6), "iron")


def sacks(p):
    for x, y, z in ((-30, 0, -10), (30, 5, -12), (0, -5, 25)):
        p.sphere((x, y, z), 32, "cloth", subdiv=1)


def barricade(p):     # barricada del carril (franja de equipo)
    for x in (-110, 0, 110):
        p.box((x, 0, 0), (14, 14, 160), "wood_dark")
        rot_last(p, 8, (x, 0, 0), "X", 20)
    p.box((0, -10, 20), (280, 10, 40), "wood")
    p.box((0, -16, 20), (280, 2, 12), "red")


def lantern_post(p):
    p.box((0, 0, -50), (14, 14, 350), "wood_dark")
    p.box((40, 0, 110), (90, 10, 10), "wood_dark")
    p.box((75, 0, 75), (24, 24, 30), "iron")
    p.box((75, 0, 75), (18, 18, 22), "spark")


def island_large(p):
    p.cyl((0, 0, 1700), 3000, 150, "z", "grass", 20)
    p.cyl((0, 0, 1000), 2960, 1300, "z", "leather", 18, radius2=1900)
    p.cyl((0, 0, -700), 1900, 2100, "z", "leather", 14, radius2=120)


def cliff_edge(p):
    p.box((0, 0, 100), (600, 300, 60), "grass")
    p.cyl((0, 0, -60), 340, 260, "y", "leather", 6)


LOT6 = [
    ("World/Props", "SM_PROP_TREE_ROUND", (420, 380, 600), tree_round, "trunk", "bottom", []),
    ("World/Props", "SM_PROP_TREE_TALL", (360, 360, 900), tree_tall, "trunk", "bottom", []),
    ("World/Props", "SM_PROP_TREE_DEAD", (300, 200, 500), tree_dead, "trunk", "bottom", []),
    ("World/Props", "SM_PROP_ROCK_FLAT", (300, 300, 100), rock_flat, "box", "bottom", []),
    ("World/Props", "SM_PROP_ROCK_SPIRE", (220, 200, 500), rock_spire, "box", "bottom", []),
    ("World/Props", "SM_PROP_GRASS_TUFT", (60, 60, 50), grass_tuft, "none", "bottom", []),
    ("World/Props", "SM_PROP_FLOWERS", (60, 60, 35), flowers, "none", "bottom", []),
    ("World/Props", "SM_PROP_MUSHROOM", (50, 50, 45), mushroom, "none", "bottom", []),
    ("World/Props", "SM_PROP_RUIN_PILLAR", (90, 90, 400), ruin_pillar, "box", "bottom", []),
    ("World/Props", "SM_PROP_RUIN_WALL", (400, 64, 300), ruin_wall, "box", "bottom", []),
    ("World/Props", "SM_PROP_RUIN_STAIRS", (300, 300, 175), ruin_stairs, "none", "bottom", []),
    ("World/Props", "SM_PROP_PLATFORM", (410, 410, 200), platform, "none", "bottom", []),
    ("World/Props", "SM_PROP_STAIRS_WOOD", (300, 170, 220), stairs_wood, "none", "bottom", []),
    ("World/Props", "SM_PROP_LADDER", (10, 60, 400), ladder, "box", "bottom", []),
    ("World/Props", "SM_PROP_SCAFFOLD", (300, 200, 500), scaffold, "none", "bottom", []),
    ("World/Props", "SM_PROP_TENT", (320, 300, 220), tent, "box", "bottom", []),
    ("World/Props", "SM_PROP_CAMPFIRE", (120, 120, 60), campfire, "none", "bottom", [("Fx_Fire", (0, 0, 10))]),
    ("World/Props", "SM_PROP_WORKBENCH", (210, 100, 140), workbench, "box", "bottom", []),
    ("World/Props", "SM_PROP_WEAPON_RACK", (170, 60, 180), weapon_rack, "box", "bottom", []),
    ("World/Props", "SM_PROP_SACKS", (130, 70, 90), sacks, "box", "bottom", []),
    ("World/Props", "SM_PROP_BARRICADE", (300, 80, 160), barricade, "box", "bottom", []),
    ("World/Props", "SM_PROP_LANTERN_POST", (100, 30, 360), lantern_post, "trunk", "bottom", [("Light", (75, 0, 120))]),
    ("World/Props", "SM_ISLAND_LARGE", (6000, 6000, 3000), island_large, "none", "top", []),
    ("World/Props", "SM_ISLAND_CLIFF_EDGE", (600, 340, 340), cliff_edge, "none", "top", []),
]


def apply_pivot(obj, size, pivot):
    """fit_to_box deja el centro en el origen; mueve la malla para poner el pivote donde corresponde."""
    dz = {"center": 0.0, "bottom": size[2] / 2, "top": -size[2] / 2, "top_deck": -size[2] / 2 + 0.1,
          "hinge_back": size[2] / 2}[pivot] / 100.0
    # hinge_back: pivote en la arista trasera de abajo (UE +Y), para girar la tapa del cofre sobre su bisagra
    dy = size[1] / 2 / 100.0 if pivot == "hinge_back" else 0.0
    for v in obj.data.vertices:
        v.co.z += dz
        v.co.y += dy
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
    assets = {"lot3": LOT3, "lot4": LOT4, "lot5": LOT5, "lot6": LOT6}.get(lot, ASSETS)
    if lot in ("lot2", "lot3"):  # las piezas de Building de los lotes 2 y 3 quedaron reemplazadas por LOT4 (medidas del código)
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
