"""Normaliza un modelo exportado de Tripo Studio (GLB/FBX/OBJ) al contrato de piezas de GameJamm y lo exporta a FBX.

Uso (Blender 5.x, sin ventana):
    blender --background --factory-startup --python tripo_normalize.py -- \
        --in raw/wheel_v1.glb --name SM_DEV_WHEEL_S --size 50 20 50 --out FBX \
        [--auto-axis] [--rot z90] [--tris 5000] [--tex 1024] [--ucx none|box|wheel] \
        [--sockets six|lr|none] [--fx Fx_Out:Front] [--preview previews/SM_DEV_WHEEL_S.png] [--blend Blend/x.blend]

Contrato (Source/GameJamm/Building/JamBuildPart.cpp, InitFromDef + docs/assets/PROMPTS-ASSETS.md):
- La malla visual se dibuja con escala absoluta: su bounding box tiene que medir EXACTAMENTE `Size` (cm, X x Y x Z de UE).
- Pivote en el centro de la caja; X = adelante, Y = derecha, Z = arriba (marco UE). Ruedas: eje en Y.
- Un solo slot de material: el código solo hace brillar el slot 0 (parámetro escalar `Emissive`).
- <= 5k triángulos y texturas chicas (1K o 512): presupuesto de memoria de video para PCs modestas y zip final < 1 GB.

Marco: 1 unidad de Blender = 1 m (el importador FBX de UE convierte a cm) y Y_blender = -Y_ue, igual que los scripts de
SourceArt/Building. Las rotaciones de `--rot` son en ejes de Blender, en grados, aplicadas en orden (p. ej. `z90,x-90`).
"""
import argparse
import json
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

AXES = "xyz"
FACES = {
    "Front": Vector((1, 0, 0)), "Back": Vector((-1, 0, 0)),
    "Right": Vector((0, 1, 0)), "Left": Vector((0, -1, 0)),
    "Top": Vector((0, 0, 1)), "Bottom": Vector((0, 0, -1)),
}


def parse_args():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    p = argparse.ArgumentParser()
    p.add_argument("--in", dest="src", required=True)
    p.add_argument("--name", required=True, help="Nombre del asset en UE, p. ej. SM_DEV_WHEEL_S")
    p.add_argument("--size", type=float, nargs=3, required=True, help="Size en cm, X Y Z del marco UE")
    p.add_argument("--out", required=True, help="Carpeta del FBX")
    p.add_argument("--auto-axis", action="store_true",
                   help="Gira 90 grados para que el lado más largo/corto del modelo coincida con el de Size")
    p.add_argument("--rot", default="", help="Rotaciones extra en ejes de Blender: 'z90,x-90'")
    p.add_argument("--tris", type=int, default=5000)
    p.add_argument("--tex", type=int, default=1024)
    p.add_argument("--ucx", default="none", choices=["none", "box", "wheel"])
    p.add_argument("--sockets", default="six", choices=["six", "lr", "none"])
    p.add_argument("--fx", action="append", default=[], help="Socket de efecto 'Nombre:Cara', p. ej. Fx_Out:Front")
    p.add_argument("--center", type=float, nargs=3, default=None,
                   help="Centro de la caja en cm del mundo (UE). Para mallas del mapa que traen su posición integrada")
    p.add_argument("--fit", default="exact", choices=["exact", "height"],
                   help="exact = caja idéntica a Size (piezas); height = misma altura y proporciones propias, el ancho "
                        "solo se aprieta si se pasa >10%% de Size (props del mapa: árboles, rocas)")
    p.add_argument("--overflow", type=float, default=1.1,
                   help="Con --fit height: cuánto puede pasarse el ancho de Size antes de apretarlo (1.1 = 10%%)")
    p.add_argument("--zclip", type=float, nargs=2, default=None,
                   help="Conserva solo la rebanada entre dos fracciones de la altura (0-1), p. ej. cuerpo y tapa de un cofre")
    p.add_argument("--preview", default="")
    p.add_argument("--blend", default="")
    return p.parse_args(argv)


def ue(v):
    """Coordenada UE (cm) -> Blender (m)."""
    return Vector((v[0] / 100.0, -v[1] / 100.0, v[2] / 100.0))


# ---------------------------------------------------------------- importar y unir
def clear_scene():
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)


def import_model(path):
    ext = os.path.splitext(path)[1].lower()
    if ext in (".glb", ".gltf"):
        bpy.ops.import_scene.gltf(filepath=path)
    elif ext == ".fbx":
        bpy.ops.import_scene.fbx(filepath=path)
    elif ext == ".obj":
        bpy.ops.wm.obj_import(filepath=path)
    else:
        raise SystemExit(f"Formato no soportado: {ext}")


def join_meshes(name):
    """Hornea las transformaciones de la jerarquía en los vértices, une todas las mallas y borra lo demás."""
    for o in [o for o in bpy.data.objects if o.type == "MESH" and o.name.upper().startswith(("UCX_", "UBX_", "USP_"))]:
        bpy.data.objects.remove(o, do_unlink=True)
    meshes = [o for o in bpy.data.objects if o.type == "MESH"]
    if not meshes:
        raise SystemExit("El archivo no trae mallas")
    for o in meshes:
        mw = o.matrix_world.copy()
        o.parent = None
        o.data = o.data.copy() if o.data.users > 1 else o.data
        o.data.transform(mw)
        o.matrix_world = Matrix.Identity(4)
    for o in [o for o in bpy.data.objects if o.type != "MESH"]:
        bpy.data.objects.remove(o, do_unlink=True)
    bpy.ops.object.select_all(action="DESELECT")
    for o in meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
    obj.name = name
    obj.data.name = name
    return obj


def tri_count(obj):
    obj.data.calc_loop_triangles()
    return len(obj.data.loop_triangles)


def bbox(obj):
    xs = [v.co.x for v in obj.data.vertices]
    ys = [v.co.y for v in obj.data.vertices]
    zs = [v.co.z for v in obj.data.vertices]
    mn = Vector((min(xs), min(ys), min(zs)))
    mx = Vector((max(xs), max(ys), max(zs)))
    return mn, mx


# ---------------------------------------------------------------- orientar, reducir y encajar en la caja
def apply_rotations(obj, spec):
    for tok in [t.strip() for t in spec.split(",") if t.strip()]:
        axis, deg = tok[0].upper(), float(tok[1:])
        obj.data.transform(Matrix.Rotation(math.radians(deg), 4, axis))


def auto_axis(obj, size_ue):
    """Permuta ejes (rotaciones de 90 grados) para que el orden de largos del modelo sea el de Size.

    Solo considera ejes claramente distintos (razón > 1.25) para no girar piezas casi cúbicas por ruido.
    """
    import itertools
    mn, mx = bbox(obj)
    cur = [max(c, 1e-6) for c in (mx - mn)]
    tgt = [max(t, 1e-6) for t in size_ue]  # |Y| es igual en ambos marcos
    nc = [c / max(cur) for c in cur]
    nt = [t / max(tgt) for t in tgt]

    def err(perm):  # perm[destino] = eje actual que va a ese destino
        return sum(abs(math.log(nc[perm[d]] / nt[d])) for d in range(3))

    identity = (0, 1, 2)
    best = min(itertools.permutations(range(3)),
               key=lambda p: (round(err(p), 2), sum(p[i] != i for i in range(3))))
    if err(best) > err(identity) - 0.15:  # mejora marginal o empate: no girar
        return "sin cambio"
    m = Matrix(((0.0, 0.0, 0.0), (0.0, 0.0, 0.0), (0.0, 0.0, 0.0)))
    for d in range(3):
        m[d][best[d]] = 1.0
    if m.determinant() < 0:
        # Una permutación impar es un espejo: niega el eje más corto del destino para que sea una rotación pura.
        r = min(range(3), key=lambda d: tgt[d])
        for c in range(3):
            m[r][c] = -m[r][c]
    obj.data.transform(m.to_4x4())
    return "ejes " + ", ".join(f"{AXES[best[d]]}->{AXES[d]}" for d in range(3) if best[d] != d)


def zclip(obj, lo, hi):
    """Corta la malla con planos horizontales y deja solo lo que queda entre lo y hi (fracciones de la altura)."""
    mn, mx = bbox(obj)
    h = mx.z - mn.z
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    for frac, keep_above in ((lo, True), (hi, False)):
        if (keep_above and frac <= 0.0) or (not keep_above and frac >= 1.0):
            continue
        geom = bm.verts[:] + bm.edges[:] + bm.faces[:]
        bmesh.ops.bisect_plane(bm, geom=geom, plane_co=(0, 0, mn.z + h * frac), plane_no=(0, 0, 1),
                               clear_inner=keep_above, clear_outer=not keep_above)
    bm.to_mesh(obj.data)
    bm.free()
    obj.data.update()


def decimate(obj, budget):
    before = tri_count(obj)
    if before <= budget:
        return before, before
    bpy.context.view_layer.objects.active = obj
    mod = obj.modifiers.new("Decimate", "DECIMATE")
    mod.decimate_type = "COLLAPSE"
    mod.ratio = max(0.01, budget / before * 0.97)
    mod.use_collapse_triangulate = True
    bpy.ops.object.modifier_apply(modifier=mod.name)
    return before, tri_count(obj)


def fit_to_box(obj, size_ue):
    """Centra y escala por eje para que la caja sea exactamente Size. Devuelve el factor por eje (distorsión)."""
    mn, mx = bbox(obj)
    c = (mn + mx) / 2
    d = mx - mn
    target = Vector((size_ue[0], size_ue[1], size_ue[2])) / 100.0
    k = Vector(tuple(t / dd if dd > 1e-6 else 1.0 for t, dd in zip(target, d)))
    for v in obj.data.vertices:
        v.co = Vector(((v.co.x - c.x) * k.x, (v.co.y - c.y) * k.y, (v.co.z - c.z) * k.z))
    obj.data.update()
    kmin = min(k)
    return [round(x / kmin, 3) for x in k]


def fit_height(obj, size_ue, overflow=1.1):
    """Escala uniforme para que la altura sea Size.z; X/Y conservan su proporción salvo que se pasen de Size*overflow.

    Queda centrado en X/Y y con la base en el piso de la caja (z = -Size.z/2), así el centro del mundo sigue valiendo.
    """
    target = Vector((size_ue[0], size_ue[1], size_ue[2])) / 100.0

    def squeeze(d):  # cuánto habría que apretar X/Y (1 = nada)
        kz = target.z / d.z if d.z > 1e-6 else 1.0
        return max(1.0, *(d[i] * kz / (target[i] * overflow) for i in (0, 1)))

    mn, mx = bbox(obj)
    d = mx - mn
    if squeeze(Vector((d.y, d.x, d.z))) < squeeze(d) - 0.02:  # girar 90° deforma menos: se gira
        obj.data.transform(Matrix.Rotation(math.radians(90), 4, "Z"))
        mn, mx = bbox(obj)
        d = mx - mn
    c = (mn + mx) / 2
    kz = target.z / d.z if d.z > 1e-6 else 1.0
    k = [kz, kz, kz]
    for i in (0, 1):
        if d[i] * kz > target[i] * overflow:
            k[i] = target[i] * overflow / d[i]
    for v in obj.data.vertices:
        v.co = Vector(((v.co.x - c.x) * k[0], (v.co.y - c.y) * k[1], (v.co.z - c.z) * k[2]))
    obj.data.update()
    kmin = min(k)
    return [round(x / kmin, 3) for x in k]


# ---------------------------------------------------------------- material y texturas
def image_role(node_tree, img_node):
    """Rol de una textura según a qué entrada del Principled BSDF llega."""
    seen = set()
    stack = [img_node]
    while stack:
        n = stack.pop()
        if n in seen:
            continue
        seen.add(n)
        for out in n.outputs:
            for link in out.links:
                to = link.to_node
                if to.type == "BSDF_PRINCIPLED":
                    sock = link.to_socket.name
                    if sock == "Base Color":
                        return "D"
                    if sock in ("Roughness", "Metallic"):
                        return "MR"
                    if sock.startswith("Emission"):
                        return "E"
                    if sock == "Alpha":
                        return "D"
                if to.type == "NORMAL_MAP":
                    return "N"
                stack.append(to)
    return "X"


def resave_textures(obj, base, out_dir, tex):
    """Reduce cada textura a `tex` px y la guarda como T_<base>_<rol>.png junto al FBX."""
    saved = {}
    mats = [s.material for s in obj.material_slots if s.material]
    for mi, mat in enumerate(mats):
        mat.name = f"M_{base}" if len(mats) == 1 else f"M_{base}_{mi}"
        if not mat.use_nodes:
            continue
        for node in mat.node_tree.nodes:
            if node.type != "TEX_IMAGE" or not node.image:
                continue
            img = node.image
            role = image_role(mat.node_tree, node)
            key = f"{role}" if len(mats) == 1 else f"{role}{mi}"
            if key in saved:
                key = f"{key}_{len(saved)}"
            w, h = img.size
            if w == 0 or h == 0:
                continue
            nw, nh = (tex, tex) if max(w, h) > tex else (w, h)
            px = list(img.pixels[:])  # fuerza la carga
            if (nw, nh) != (w, h):
                img.scale(nw, nh)
                px = list(img.pixels[:])
            out = bpy.data.images.new(f"T_{base}_{key}", width=nw, height=nh, alpha=(role == "D"))
            out.colorspace_settings.name = "sRGB" if role in ("D", "E") else "Non-Color"
            out.pixels = px
            path = os.path.join(out_dir, f"T_{base}_{key}.png")
            out.filepath_raw = path
            out.file_format = "PNG"
            out.save()
            node.image = out
            saved[key] = {"file": os.path.basename(path), "from": [w, h], "to": [nw, nh]}
    return [m.name for m in mats], saved


# ---------------------------------------------------------------- colisión y sockets (mismo formato que SourceArt/Building)
def add_ucx(obj, size, kind):
    name = f"UCX_{obj.name}_00"
    bm = bmesh.new()
    sx, sy, sz = (s / 100.0 for s in size)
    if kind == "box":
        bmesh.ops.create_cube(bm, size=1.0)
        bmesh.ops.scale(bm, vec=(sx, sy, sz), verts=bm.verts)
    else:  # cilindro convexo con eje Y (ruedas)
        r = sx / 2
        bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=12, radius1=r, radius2=r, depth=sy)
        bmesh.ops.rotate(bm, cent=(0, 0, 0), matrix=Matrix.Rotation(math.radians(90), 3, "X"), verts=bm.verts)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    col = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(col)
    col.parent = obj
    col.display_type = "WIRE"
    col.hide_render = True
    return col


def add_socket(obj, name, face, size):
    ue_dir = FACES[face]
    half = Vector((size[0], size[1], size[2])) / 2
    pos_ue = Vector((ue_dir.x * half.x, ue_dir.y * half.y, ue_dir.z * half.z))
    d = ue(ue_dir * 100.0).normalized()
    emp = bpy.data.objects.new(f"SOCKET_{name}", None)
    emp.empty_display_type = "ARROWS"
    emp.empty_display_size = 0.08
    emp.location = ue(pos_ue)
    emp.rotation_mode = "QUATERNION"
    emp.rotation_quaternion = Vector((1, 0, 0)).rotation_difference(d)
    bpy.context.scene.collection.objects.link(emp)
    emp.parent = obj
    return emp


# ---------------------------------------------------------------- exportar y vista previa
def export_fbx(obj, path):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    for c in obj.children:
        c.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"MESH", "EMPTY"},
                             mesh_smooth_type="FACE", add_leaf_bones=False, bake_anim=False,
                             path_mode="RELATIVE", embed_textures=False)


def render_preview(obj, size, path):
    """Vista 3/4 desde adelante-derecha-arriba (marco UE) con una flecha roja en +X para revisar la orientación."""
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    sh = scene.display.shading
    sh.light = "STUDIO"
    sh.color_type = "TEXTURE"
    sh.show_cavity = True
    sh.background_type = "VIEWPORT"
    sh.background_color = (0.55, 0.72, 0.86)
    scene.render.resolution_x = 1024
    scene.render.resolution_y = 768
    r = max(size) / 100.0
    # Flecha +X (adelante en UE)
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=8, radius1=r * 0.06, radius2=0.0, depth=r * 0.25)
    bmesh.ops.rotate(bm, cent=(0, 0, 0), matrix=Matrix.Rotation(math.radians(90), 3, "Y"), verts=bm.verts)
    bmesh.ops.translate(bm, vec=(size[0] / 200.0 + r * 0.25, 0, 0), verts=bm.verts)
    me = bpy.data.meshes.new("Arrow")
    bm.to_mesh(me)
    bm.free()
    arrow = bpy.data.objects.new("Arrow", me)
    mat = bpy.data.materials.new("ArrowRed")
    mat.diffuse_color = (1, 0.1, 0.1, 1)
    arrow.data.materials.append(mat)
    scene.collection.objects.link(arrow)
    cam_data = bpy.data.cameras.new("Cam")
    cam = bpy.data.objects.new("Cam", cam_data)
    scene.collection.objects.link(cam)
    # adelante (+X), derecha UE (+Y_ue = -Y_blender), arriba
    cam.location = Vector((1.6, -1.3, 1.1)).normalized() * r * 3.2
    cam.rotation_euler = (Vector((0, 0, 0)) - cam.location).to_track_quat("-Z", "Y").to_euler()
    cam_data.lens = 50
    scene.camera = cam
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)


def main():
    a = parse_args()
    src = os.path.abspath(a.src)
    out_dir = os.path.abspath(a.out)
    os.makedirs(out_dir, exist_ok=True)
    base = a.name[3:] if a.name.startswith("SM_") else a.name

    clear_scene()
    import_model(src)
    obj = join_meshes(a.name)
    mn, mx = bbox(obj)
    report = {"in": src, "name": a.name, "size_cm": a.size,
              "raw_bbox_m": [round(x, 4) for x in (mx - mn)]}

    if a.rot:
        apply_rotations(obj, a.rot)
    report["auto_axis"] = auto_axis(obj, a.size) if a.auto_axis else "off"
    if a.zclip:
        zclip(obj, *a.zclip)
        report["zclip"] = a.zclip
    report["tris_before"], report["tris_after"] = decimate(obj, a.tris)
    report["stretch"] = fit_height(obj, a.size, a.overflow) if a.fit == "height" else fit_to_box(obj, a.size)
    if a.fit == "height":
        mn, mx = bbox(obj)
        report["bbox_cm"] = [round((mx[i] - mn[i]) * 100, 1) for i in range(3)]
    report["materials"], report["textures"] = resave_textures(obj, base, out_dir, a.tex)
    if len(report["materials"]) > 1:
        report["warning"] = "Más de un material: el código solo hace brillar el slot 0."
    if max(report["stretch"]) > 1.3:
        report["warning_stretch"] = "El modelo se estiró >30% en un eje: revisa orientación (--rot) o regenera."

    if a.ucx != "none":
        add_ucx(obj, a.size, a.ucx)
    faces = list(FACES) if a.sockets == "six" else (["Left", "Right"] if a.sockets == "lr" else [])
    for f in faces:
        add_socket(obj, f"Attach_{f}", f, a.size)
    for fx in a.fx:
        n, f = fx.split(":")
        add_socket(obj, n, f, a.size)

    if a.preview:  # antes de mover al centro del mundo: la cámara de la vista previa mira al origen
        os.makedirs(os.path.dirname(os.path.abspath(a.preview)), exist_ok=True)
        render_preview(obj, a.size, os.path.abspath(a.preview))
        report["preview"] = os.path.abspath(a.preview)
        for o in [o for o in bpy.data.objects if o.name in ("Arrow", "Cam")]:
            bpy.data.objects.remove(o, do_unlink=True)
    if a.center:
        obj.data.transform(Matrix.Translation(ue(a.center)))
        obj.data.update()
        report["center_cm"] = a.center
    fbx = os.path.join(out_dir, f"{a.name}.fbx")
    export_fbx(obj, fbx)
    report["fbx"] = fbx
    if a.blend:
        os.makedirs(os.path.dirname(os.path.abspath(a.blend)), exist_ok=True)
        bpy.ops.wm.save_as_mainfile(filepath=os.path.abspath(a.blend))
    print("TRIPO_REPORT " + json.dumps(report, ensure_ascii=False))


if __name__ == "__main__":
    main()
