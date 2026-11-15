"""Importa a Unreal los personajes rigeados de Rodin (FBX con esqueleto UE5 + animaciones) y sus texturas por equipo.

Uso (editor cerrado; el proyecto tiene que estar compilado):
    UnrealEditor-Cmd.exe GameJamm.uproject -run=pythonscript -script="<ruta>/ue_import_chars.py" -unattended -nosplash -nullrhi
con TRIPO_CHAR_JOB apuntando a un JSON (ejemplo en SourceArt/Tripo/batch_chars.json):
    {"dest": "/Game/Game/Art/Tripo/Characters",
     "chars": [{"name": "MINION", "dir": "D:/.../SourceArt/Tripo/raw/chars/minion", "fbx": "rig_pbr.fbx",
                "teams": {"RED": "T_diffuse.png", "BLUE": "T_diffuse_BLUE.png"}, "max_tex": 1024,
                "anims": {"human_000093": "Idle", "human_000089": "Walk"}}]}

Qué crea en `dest` (contrato con el código, ver docs/ARTE-TRIPO.md, sección Personajes):
- `SK_CHAR_<name>` (SkeletalMesh, un slot de material, pivote en los pies) + `SK_CHAR_<name>_Skeleton`.
- `A_CHAR_<name>_<Anim>` (AnimSequence, una por toma del FBX, renombrada con `anims`).
- `MI_CHAR_<name>_<TEAM>` hijas de `M_Tripo_Char` (copia de M_Tripo_Master con "Used with Skeletal Mesh").
- Texturas `T_CHAR_<name>_D_<TEAM>`, `T_CHAR_<name>_N`, `T_CHAR_<name>_MR` (glTF: G = rugosidad, B = metálico).
"""
import json
import os
import re

import unreal

AT = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
LOG = "CHAR_IMPORT "
MASTER_SRC = "/Game/Game/Art/Tripo/Materials/M_Tripo_Master"
MASTER = "/Game/Game/Art/Tripo/Materials/M_Tripo_Char"


def log(msg):
    unreal.log(LOG + msg)
    print(LOG + msg)


def run_task(path, dest, name, options=None):
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = dest
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    if options is not None:
        task.options = options
    AT.import_asset_tasks([task])
    return list(task.imported_object_paths)


def char_master():
    if EAL.does_asset_exist(MASTER):
        mat = unreal.load_asset(MASTER)
    else:
        mat = EAL.duplicate_asset(MASTER_SRC, MASTER)
        log(f"maestro duplicado {MASTER}")
    if not mat.get_editor_property("used_with_skeletal_mesh"):
        mat.set_editor_property("used_with_skeletal_mesh", True)
        MEL.recompile_material(mat)
    EAL.save_loaded_asset(mat)
    return mat


def texture(png, dest, name, role, max_size):
    paths = run_task(png, dest, name)
    if not paths:
        log(f"ERROR textura {png}")
        return None
    tex = unreal.load_asset(paths[0])
    if role == "N":
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("flip_green_channel", True)  # glTF/OpenGL -> DirectX
    elif role == "MR":
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        tex.set_editor_property("srgb", False)
    else:
        tex.set_editor_property("srgb", True)
    if max_size:
        tex.set_editor_property("max_texture_size", max_size)
    EAL.save_loaded_asset(tex)
    return tex


def skel_options():
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", True)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("import_animations", True)
    ui.set_editor_property("create_physics_asset", False)
    sk = ui.get_editor_property("skeletal_mesh_import_data")
    sk.set_editor_property("convert_scene", True)
    sk.set_editor_property("import_morph_targets", False)
    sk.set_editor_property("import_uniform_scale", 1.0)
    an = ui.get_editor_property("anim_sequence_import_data")
    an.set_editor_property("animation_length", unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
    an.set_editor_property("import_bone_tracks", True)
    an.set_editor_property("remove_redundant_keys", True)
    return ui


def main():
    job = json.load(open(os.environ["TRIPO_CHAR_JOB"], encoding="utf-8"))
    dest = job["dest"].rstrip("/")
    only = [s for s in os.environ.get("TRIPO_CHAR_ONLY", "").split(",") if s]
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
    master = char_master()
    for ch in job["chars"]:
        name = ch["name"]
        if only and name not in only:
            continue
        d = ch["dir"]
        if not os.path.isabs(d):  # relativo a la raíz del proyecto
            d = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), d)
        tex_dest = f"{dest}/Textures"
        max_tex = ch.get("max_tex", 1024)
        n_tex = texture(os.path.join(d, ch.get("normal", "T_normal.png")), tex_dest, f"T_CHAR_{name}_N", "N", max_tex)
        mr_tex = texture(os.path.join(d, ch.get("mr", "T_pbr.png")), tex_dest, f"T_CHAR_{name}_MR", "MR", max_tex)
        mis = {}
        for team, png in ch["teams"].items():
            d_tex = texture(os.path.join(d, png), tex_dest, f"T_CHAR_{name}_D_{team}", "D", max_tex)
            mi_name = f"MI_CHAR_{name}_{team}"
            mi_path = f"{dest}/Materials/{mi_name}"
            if EAL.does_asset_exist(mi_path):
                mi = unreal.load_asset(mi_path)
            else:
                mi = AT.create_asset(mi_name, f"{dest}/Materials", unreal.MaterialInstanceConstant,
                                     unreal.MaterialInstanceConstantFactoryNew())
            MEL.set_material_instance_parent(mi, master)
            for pname, t in (("BaseColor", d_tex), ("Normal", n_tex), ("MR", mr_tex)):
                if t:
                    MEL.set_material_instance_texture_parameter_value(mi, pname, t)
            MEL.update_material_instance(mi)
            EAL.save_loaded_asset(mi)
            mis[team] = mi

        sk_name = f"SK_CHAR_{name}"
        paths = run_task(os.path.join(d, ch["fbx"]), dest, sk_name, skel_options())
        mesh = None
        anims = []
        for p in paths:
            a = unreal.load_asset(p)
            if isinstance(a, unreal.SkeletalMesh):
                mesh = a
            elif isinstance(a, unreal.AnimSequence):
                anims.append(a)
        # El importador clásico a veces no devuelve las animaciones en imported_object_paths: buscarlas en dest.
        if not anims:
            for p in EAL.list_assets(dest, recursive=False):
                a = unreal.load_asset(p)
                if isinstance(a, unreal.AnimSequence) and a.get_name().startswith(sk_name + "_Anim"):
                    anims.append(a)
        if not mesh:
            log(f"ERROR malla {sk_name}: {paths}")
            continue
        first = next(iter(mis.values()), None)
        mats = mesh.get_editor_property("materials")
        for m in mats:
            m.set_editor_property("material_interface", first)
        mesh.set_editor_property("materials", mats)
        EAL.save_loaded_asset(mesh)
        skel = mesh.get_editor_property("skeleton")
        if skel:
            EAL.save_loaded_asset(skel, False)  # sin esto el esqueleto queda solo en memoria y las animaciones se rompen

        renamed = []
        for a in anims:
            src = a.get_path_name().split(".")[0]
            m = re.search(r"(human_\d+)", a.get_name())
            key = {**job.get("anim_ids", {}), **ch.get("anims", {})}.get(m.group(1)) if m else None
            if not key:
                renamed.append(f"{a.get_name()}(sin nombre)")
                EAL.save_loaded_asset(a)
                continue
            dst = f"{dest}/Anims/A_CHAR_{name}_{key}"
            if src != dst:
                if EAL.does_asset_exist(dst):
                    EAL.delete_asset(dst)
                EAL.rename_asset(src, dst)
            a2 = unreal.load_asset(dst)
            EAL.save_loaded_asset(a2)
            renamed.append(f"{key}={a2.get_play_length():.2f}s")
        bb = mesh.get_bounds()
        log(f"OK {sk_name} slots={len(mats)} bounds_origin={bb.origin} extent={bb.box_extent} "
            f"skeleton={skel.get_path_name() if skel else None} anims={renamed}")
    # Guarda lo que haya quedado sucio dentro de dest (esqueleto, animaciones renombradas, redirectores).
    for p in EAL.list_assets(dest, recursive=True):
        EAL.save_asset(p, True)


main()
