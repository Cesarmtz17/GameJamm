"""Genera las piezas de construcción low-poly de GameJamm y las exporta a FBX para Unreal.

Uso (Blender 5.x, sin ventana):
    blender --background --factory-startup --python gen_parts.py -- <carpeta_salida> [carpeta_previews]

Convenciones (docs/assets/CATALOGO-ELEMENTOS.md y PROMPTS-ASSETS.md):
- Se modela en el marco de Unreal: X = adelante, Y = derecha, Z = arriba. Blender es diestro y UE no,
  así que Y_blender = -Y_ue; los helpers reciben coordenadas UE y hacen la conversión.
- 1 unidad de Blender = 1 m; el importador FBX de UE lo convierte a cm.
- El bounding box de cada pieza es exactamente su `Size` y el pivote está en el centro de la caja.
- Colisión `UCX_<SM>_00` convexa; sockets como Empties `SOCKET_Attach_<cara>` / `SOCKET_Fx_Out`.
- Un solo material (`M_Part_Master`) con textura de paleta: cada sub-pieza manda sus UVs a un color.
"""
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

# ---------------------------------------------------------------- paleta (ART.md §3)
PALETTE = [
    ("wood", "#C9A06B"),
    ("wood_dark", "#8A6A43"),
    ("iron", "#7D828A"),
    ("red", "#C94248"),
    ("yellow", "#F2C230"),
    ("leather", "#6B4A35"),
    ("black", "#2B2B2B"),
    ("cyan", "#8FE3FF"),
]
SWATCH = 16
COLOR_INDEX = {name: i for i, (name, _) in enumerate(PALETTE)}


def hex_to_linear(h):
    h = h.lstrip("#")
    srgb = [int(h[i:i + 2], 16) / 255 for i in (0, 2, 4)]
    return [c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4 for c in srgb]


def swatch_uv(color):
    i = COLOR_INDEX[color]
    return ((i + 0.5) / len(PALETTE), 0.5)


def ue(v):
    """Coordenada UE (cm) -> Blender (m)."""
    return Vector((v[0] / 100.0, -v[1] / 100.0, v[2] / 100.0))


# ---------------------------------------------------------------- geometría
class Part:
    """Acumula sub-piezas en un solo bmesh, cada una con su color de paleta."""

    def __init__(self):
        self.bm = bmesh.new()
        self.uv = self.bm.loops.layers.uv.new("UVMap")

    def _paint(self, faces, color):
        u, v = swatch_uv(color)
        for f in faces:
            for loop in f.loops:
                loop[self.uv].uv = (u, v)

    def box(self, center, size, color, bevel=0.0):
        """center/size en cm, marco UE."""
        tmp = bmesh.new()
        bmesh.ops.create_cube(tmp, size=1.0)
        s = Vector((size[0], size[1], size[2])) / 100.0
        bmesh.ops.scale(tmp, vec=s, verts=tmp.verts)
        if bevel > 0:
            bmesh.ops.bevel(tmp, geom=list(tmp.edges), offset=bevel / 100.0, segments=1, affect="EDGES",
                            profile=0.5, clamp_overlap=True)
        bmesh.ops.translate(tmp, vec=ue(center), verts=tmp.verts)
        self._merge(tmp, color)

    def cyl(self, center, radius, length, axis, color, segments=12, radius2=None, cap=True):
        """Cilindro (o cono si radius2) centrado; axis = 'x' | 'y' | 'z' en marco UE."""
        tmp = bmesh.new()
        bmesh.ops.create_cone(tmp, cap_ends=cap, cap_tris=False, segments=segments,
                              radius1=radius / 100.0, radius2=(radius if radius2 is None else radius2) / 100.0,
                              depth=length / 100.0)
        rot = {"x": Matrix.Rotation(math.radians(90), 3, "Y"),
               "y": Matrix.Rotation(math.radians(90), 3, "X"),
               "z": Matrix.Identity(3)}[axis]
        bmesh.ops.rotate(tmp, cent=(0, 0, 0), matrix=rot, verts=tmp.verts)
        bmesh.ops.translate(tmp, vec=ue(center), verts=tmp.verts)
        self._merge(tmp, color)

    def sphere(self, center, radius, color, subdiv=2):
        tmp = bmesh.new()
        bmesh.ops.create_icosphere(tmp, subdivisions=subdiv, radius=radius / 100.0)
        bmesh.ops.translate(tmp, vec=ue(center), verts=tmp.verts)
        self._merge(tmp, color)

    def ring(self, center, r_out, r_in, length, axis, color, segments=16):
        """Aro (cilindro hueco) para llantas y marcos."""
        tmp = bmesh.new()
        outer = bmesh.ops.create_circle(tmp, cap_ends=False, segments=segments, radius=r_out / 100.0)["verts"]
        inner = bmesh.ops.create_circle(tmp, cap_ends=False, segments=segments, radius=r_in / 100.0)["verts"]
        faces = []
        for i in range(segments):
            j = (i + 1) % segments
            faces.append(tmp.faces.new((outer[i], outer[j], inner[j], inner[i])))
        ext = bmesh.ops.extrude_face_region(tmp, geom=faces)
        moved = [g for g in ext["geom"] if isinstance(g, bmesh.types.BMVert)]
        bmesh.ops.translate(tmp, vec=(0, 0, length / 100.0), verts=moved)
        bmesh.ops.translate(tmp, vec=(0, 0, -length / 200.0), verts=tmp.verts)
        bmesh.ops.recalc_face_normals(tmp, faces=tmp.faces)
        rot = {"x": Matrix.Rotation(math.radians(90), 3, "Y"),
               "y": Matrix.Rotation(math.radians(90), 3, "X"),
               "z": Matrix.Identity(3)}[axis]
        bmesh.ops.rotate(tmp, cent=(0, 0, 0), matrix=rot, verts=tmp.verts)
        bmesh.ops.translate(tmp, vec=ue(center), verts=tmp.verts)
        self._merge(tmp, color)

    def _merge(self, tmp, color):
        tmp.normal_update()
        me = bpy.data.meshes.new("tmp")
        tmp.to_mesh(me)
        tmp.free()
        start = len(self.bm.faces)
        self.bm.from_mesh(me)
        bpy.data.meshes.remove(me)
        self.bm.faces.ensure_lookup_table()
        self._paint(self.bm.faces[start:], color)

    def to_object(self, name, size):
        me = bpy.data.meshes.new(name)
        self.bm.to_mesh(me)
        self.bm.free()
        obj = bpy.data.objects.new(name, me)
        bpy.context.scene.collection.objects.link(obj)
        fit_to_box(obj, size)
        return obj


def fit_to_box(obj, size):
    """Centra la malla en el origen y la escala para que su bounding box sea exactamente `size` (cm)."""
    me = obj.data
    xs = [v.co.x for v in me.vertices]
    ys = [v.co.y for v in me.vertices]
    zs = [v.co.z for v in me.vertices]
    mn = Vector((min(xs), min(ys), min(zs)))
    mx = Vector((max(xs), max(ys), max(zs)))
    c = (mn + mx) / 2
    d = mx - mn
    target = Vector((size[0], size[1], size[2])) / 100.0
    k = Vector(tuple(t / dd if dd > 1e-6 else 1.0 for t, dd in zip(target, d)))
    for v in me.vertices:
        v.co = Vector(((v.co.x - c.x) * k.x, (v.co.y - c.y) * k.y, (v.co.z - c.z) * k.z))


# ---------------------------------------------------------------- colisión y sockets
def add_ucx(mesh_obj, size, kind="box", axis="y", segments=12):
    name = f"UCX_{mesh_obj.name}_00"
    bm = bmesh.new()
    sx, sy, sz = (s / 100.0 for s in size)
    if kind == "box":
        bmesh.ops.create_cube(bm, size=1.0)
        bmesh.ops.scale(bm, vec=(sx, sy, sz), verts=bm.verts)
    else:  # cilindro convexo para ruedas (eje Y en UE)
        r = sx / 2
        bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=segments, radius1=r, radius2=r, depth=sy)
        bmesh.ops.rotate(bm, cent=(0, 0, 0), matrix=Matrix.Rotation(math.radians(90), 3, "X"), verts=bm.verts)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    obj = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(obj)
    obj.parent = mesh_obj
    obj.display_type = "WIRE"
    obj.hide_render = True
    return obj


FACES = {
    "Front": Vector((1, 0, 0)), "Back": Vector((-1, 0, 0)),
    "Right": Vector((0, 1, 0)), "Left": Vector((0, -1, 0)),
    "Top": Vector((0, 0, 1)), "Bottom": Vector((0, 0, -1)),
}


def add_socket(mesh_obj, name, ue_dir, size):
    """Empty en el centro de la cara, con su eje X saliendo de la superficie."""
    half = Vector((size[0], size[1], size[2])) / 2
    pos_ue = Vector((ue_dir.x * half.x, ue_dir.y * half.y, ue_dir.z * half.z))
    d = ue(ue_dir * 100.0).normalized()
    rot = Vector((1, 0, 0)).rotation_difference(d)
    emp = bpy.data.objects.new(f"SOCKET_{name}", None)
    emp.empty_display_type = "ARROWS"
    emp.empty_display_size = 0.08
    emp.location = ue(pos_ue)
    emp.rotation_mode = "QUATERNION"
    emp.rotation_quaternion = rot
    bpy.context.scene.collection.objects.link(emp)
    emp.parent = mesh_obj
    return emp


# ---------------------------------------------------------------- piezas (Size en cm, marco UE)
def wheel(part, size, treads):
    d, w = size[0], size[1]
    r = d / 2
    part.ring((0, 0, 0), r, r * 0.78, w, "y", "iron", segments=16)                  # aro de hierro
    part.cyl((0, 0, 0), r * 0.8, w * 0.7, "y", "wood", segments=16)                 # disco de madera
    part.cyl((0, 0, 0), r * 0.28, w, "y", "wood_dark", segments=8)                  # maza
    for k in range(4):                                                              # pernos
        a = math.radians(45 + 90 * k)
        part.cyl((math.cos(a) * r * 0.45, w * 0.36, math.sin(a) * r * 0.45), r * 0.06, w * 0.12, "y", "iron", 6)
        part.cyl((math.cos(a) * r * 0.45, -w * 0.36, math.sin(a) * r * 0.45), r * 0.06, w * 0.12, "y", "iron", 6)
    if treads:
        for k in range(12):                                                         # tacos
            a = math.radians(30 * k)
            part.box((math.cos(a) * r * 0.97, 0, math.sin(a) * r * 0.97), (r * 0.12, w, r * 0.12), "black")


def build(type_):
    p = Part()
    if type_ == "Plank":
        p.box((0, 0, 0), (200, 60, 10), "wood", bevel=1.5)
        for x in (-80, 80):
            p.box((x, 0, -6), (12, 60, 3), "wood_dark")                               # travesaños debajo
            for y in (-18, 18):
                p.cyl((x, y, 5.5), 2.2, 1.5, "z", "iron", 6)                         # pernos
    elif type_ == "Block":
        p.box((0, 0, 0), (56, 56, 56), "wood", bevel=2)
        for sx in (-1, 1):
            for sy in (-1, 1):
                for sz in (-1, 1):
                    p.box((sx * 25, sy * 25, sz * 25), (10, 10, 10), "iron")         # esquineros
    elif type_ == "Cylinder":
        p.cyl((0, 0, 0), 24, 150, "x", "wood", 12)
        for x in (-55, 55):
            p.cyl((x, 0, 0), 25, 10, "x", "iron", 12)                                 # flejes
    elif type_ == "Wheel":
        wheel(p, (50, 20, 50), treads=False)
    elif type_ == "BigWheel":
        wheel(p, (80, 30, 80), treads=True)
    elif type_ == "Fan":
        p.ring((0, 0, 0), 25, 22, 15, "x", "iron", 16)                                # marco
        p.cyl((0, 0, 0), 5, 15, "x", "wood_dark", 8)                                  # maza
        for k in range(3):                                                           # aspas grandes
            a = math.radians(90 + 120 * k)
            p.box((0, math.cos(a) * 12, math.sin(a) * 12), (3, 7, 22), "wood")
            # orienta cada aspa radialmente rotando el bloque recién agregado
            verts = list(p.bm.verts)[-8:]
            bmesh.ops.rotate(p.bm, cent=ue((0, math.cos(a) * 12, math.sin(a) * 12)),
                             matrix=Matrix.Rotation(-(a - math.radians(90)), 3, "X"), verts=list(verts))
    elif type_ == "Seat":
        p.box((0, 0, -9), (40, 40, 12), "wood", bevel=1.5)                            # base
        p.box((2, 0, -1), (34, 36, 5), "leather", bevel=1.5)                          # cojín
        p.box((-17, 0, 6), (6, 38, 18), "wood", bevel=1.0)                            # respaldo (atrás = -X)
        for y in (-17, 17):
            p.box((-17, y, 3), (7, 4, 12), "iron")                                    # herrajes
    elif type_ == "Battery":
        p.box((0, 0, -2), (20, 20, 16), "iron", bevel=1)
        p.box((0, 0, -2), (20.4, 14, 6), "yellow")                                    # franja con el rayo
        for y in (-5, 5):
            p.cyl((0, y, 7.5), 2, 5, "z", "black" if y < 0 else "red", 8)            # terminales
    elif type_ == "Cannon":
        p.box((-12, 0, -6), (36, 20, 8), "wood_dark", bevel=1)                        # base
        p.cyl((6, 0, 3), 6, 58, "x", "iron", 12, radius2=5)                           # cañón
        p.cyl((33, 0, 3), 8, 6, "x", "black", 12)                                     # boca exagerada
        p.cyl((-25, 0, 3), 6.5, 6, "x", "iron", 12)                                   # culata
    elif type_ == "Flamethrower":
        p.cyl((-6, 0, -2), 8, 26, "x", "red", 12)                                     # tanque de combustible
        p.cyl((-6, 0, 7), 2, 20, "x", "leather", 6)                                   # correa
        p.cyl((14, 0, -2), 4, 14, "x", "iron", 8, radius2=6)                          # boquilla
        p.cyl((20, 0, -2), 6.5, 2, "x", "black", 8)
    elif type_ == "Rocket":
        p.cyl((4, 0, 0), 7, 36, "x", "iron", 10)                                      # cuerpo
        p.cyl((28, 0, 0), 7, 12, "x", "red", 10, radius2=0.5)                         # punta (+X)
        for x in (-4, 12):
            p.cyl((x, 0, 0), 7.3, 4, "x", "red", 10)                                  # franjas
        p.cyl((-20, 0, 0), 4, 8, "x", "black", 8, radius2=6)                          # tobera (-X)
        for k in range(3):                                                           # aletas
            a = math.radians(90 + 120 * k)
            p.box((-12, math.cos(a) * 8, math.sin(a) * 8), (10, 2, 6), "red")
            verts = list(p.bm.verts)[-8:]
            bmesh.ops.rotate(p.bm, cent=ue((-12, math.cos(a) * 8, math.sin(a) * 8)),
                             matrix=Matrix.Rotation(-(a - math.radians(90)), 3, "X"), verts=list(verts))
    elif type_ == "Balloon":
        p.sphere((0, 0, 10), 35, "red")                                               # globo de lona
        p.cyl((0, 0, 4), 35.6, 8, "z", "yellow", 16)                                  # banda
        p.box((0, 0, -38), (20, 20, 14), "wood", bevel=1)                             # canasta
        for sx in (-1, 1):
            for sy in (-1, 1):
                p.box((sx * 9, sy * 9, -21), (1.5, 1.5, 22), "leather")              # cuerdas
    else:
        raise ValueError(type_)
    return p


# Receta -> (asset, Size cm, colisión, sockets, sockets de efecto) — tabla de JamBuildPart.cpp
SIX = ["Top", "Bottom", "Front", "Back", "Left", "Right"]
PARTS = [
    ("Plank", "SM_PART_PLANK", (200, 60, 10), "box", SIX, []),
    ("Block", "SM_PART_BLOCK", (60, 60, 60), "box", SIX, []),
    ("Cylinder", "SM_PART_CYLINDER", (150, 50, 50), "box", SIX, []),
    ("Wheel", "SM_DEV_WHEEL_S", (50, 20, 50), "wheel", ["Left", "Right"], []),
    ("BigWheel", "SM_DEV_WHEEL_L", (80, 30, 80), "wheel", ["Left", "Right"], []),
    ("Fan", "SM_DEV_FAN", (15, 50, 50), "box", SIX, [("Fx_Out", "Front")]),
    ("Seat", "SM_CTRL_SEAT", (40, 40, 30), "box", SIX, []),
    ("Battery", "SM_PWR_BATTERY_S", (20, 20, 20), "box", SIX, []),
    ("Cannon", "SM_DEV_CANNON", (70, 20, 20), "box", SIX, [("Fx_Out", "Front")]),
    ("Flamethrower", "SM_DEV_FLAME", (40, 20, 20), "box", SIX, [("Fx_Out", "Front")]),
    ("Rocket", "SM_DEV_ROCKET", (60, 20, 20), "box", SIX, [("Fx_Out", "Back")]),
    ("Balloon", "SM_DEV_BALLOON", (90, 90, 90), "box", SIX, []),
]


# ---------------------------------------------------------------- material y escena
def make_palette_material(out_dir):
    w = SWATCH * len(PALETTE)
    img = bpy.data.images.new("T_Part_Palette", width=w, height=SWATCH, alpha=False)
    px = []
    for _y in range(SWATCH):
        for _name, hx in PALETTE:
            r, g, b = hex_to_linear(hx)
            px += [r, g, b, 1.0] * SWATCH
    img.pixels = px
    img.filepath_raw = os.path.join(out_dir, "T_Part_Palette.png")
    img.file_format = "PNG"
    img.save()
    mat = bpy.data.materials.new("M_Part_Master")
    mat.use_nodes = True
    nt = mat.node_tree
    bsdf = nt.nodes.get("Principled BSDF")
    tex = nt.nodes.new("ShaderNodeTexImage")
    tex.image = img
    tex.interpolation = "Closest"
    nt.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.8
    return mat


def clear_scene():
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)


def export_fbx(obj, path):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    for c in obj.children:
        c.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"MESH", "EMPTY"},
                             mesh_smooth_type="FACE", add_leaf_bones=False, bake_anim=False,
                             path_mode="COPY", embed_textures=False)


def render_previews(objs, preview_dir):
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    sh = scene.display.shading
    sh.light = "STUDIO"
    sh.color_type = "TEXTURE"
    sh.show_cavity = True
    sh.cavity_type = "BOTH"
    sh.show_object_outline = True
    sh.background_type = "VIEWPORT"
    sh.background_color = (0.55, 0.72, 0.86)
    scene.render.resolution_x = 1600
    scene.render.resolution_y = 1200
    scene.render.film_transparent = False
    # copia de cada pieza en una cuadrícula 4x3, normalizada a ~1.3 m para que se vean todas
    cols, gap = 4, 2.4
    for i, o in enumerate(objs):
        c = o.copy()
        c.data = o.data
        c.parent = None
        bpy.context.scene.collection.objects.link(c)
        dims = o.dimensions
        k = 1.3 / max(dims)
        c.scale = (k, k, k)
        c.location = ((i % cols) * gap, -(i // cols) * gap, 0)
        c.rotation_euler = (0, 0, math.radians(-35))
    for o in objs:
        o.hide_render = True
        for ch in o.children:
            ch.hide_render = True
    cam_data = bpy.data.cameras.new("Cam")
    cam_data.type = "ORTHO"
    cam_data.ortho_scale = 10.5
    cam = bpy.data.objects.new("Cam", cam_data)
    scene.collection.objects.link(cam)
    target = Vector(((cols - 1) * gap / 2, -gap, 0))
    cam.location = target + Vector((7, -9, 7))
    cam.rotation_euler = (target - cam.location).to_track_quat("-Z", "Y").to_euler()
    scene.camera = cam
    scene.render.filepath = os.path.join(preview_dir, "parts_preview.png")
    bpy.ops.render.render(write_still=True)


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    out_dir = os.path.abspath(argv[0] if argv else os.path.join(os.path.dirname(__file__), ".."))
    preview_dir = os.path.abspath(argv[1]) if len(argv) > 1 else out_dir
    fbx_dir = os.path.join(out_dir, "FBX")
    blend_dir = os.path.join(out_dir, "Blend")
    os.makedirs(fbx_dir, exist_ok=True)
    os.makedirs(blend_dir, exist_ok=True)
    os.makedirs(preview_dir, exist_ok=True)

    clear_scene()
    bpy.context.scene.unit_settings.system = "METRIC"
    bpy.context.scene.unit_settings.scale_length = 1.0
    mat = make_palette_material(out_dir)

    report = []
    objs = []
    for recipe, sm, size, coll, sockets, fx in PARTS:
        obj = build(recipe).to_object(sm, size)
        obj.data.materials.append(mat)
        add_ucx(obj, size, "wheel" if coll == "wheel" else "box")
        for face in sockets:
            add_socket(obj, f"Attach_{face}", FACES[face], size)
        for name, face in fx:
            add_socket(obj, name, FACES[face], size)
        export_fbx(obj, os.path.join(fbx_dir, f"{sm}.fbx"))
        # Los nombres de Blender son únicos por archivo: cada pieza va en su propio .blend y sus sockets
        # se borran antes de la siguiente para que la próxima tenga los nombres exactos (sin .001).
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
        tris = sum(len(p.vertices) - 2 for p in obj.data.polygons)
        d = obj.dimensions
        report.append(f"{recipe:13s} {sm:18s} size_cm={d.x*100:.0f}x{d.y*100:.0f}x{d.z*100:.0f} tris={tris}")
        objs.append(obj)

    render_previews(objs, preview_dir)
    print("PARTS_REPORT_BEGIN")
    print("\n".join(report))
    print("PARTS_REPORT_END")


main()
