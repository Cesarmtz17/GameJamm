"""Importa a Unreal las piezas normalizadas por tripo_normalize.py (FBX + T_<id>_D/_N/_MR.png).

Uso (sin abrir el editor; el proyecto tiene que estar compilado):
    UnrealEditor-Cmd.exe GameJamm.uproject -run=pythonscript -script="<ruta>/ue_import_tripo.py" -unattended -nosplash -nullrhi
con la variable de entorno TRIPO_IMPORT_JOB apuntando a un JSON:
    {"dest": "/Game/Developers/javie/Tripo", "fbx_dir": "D:/.../SourceArt/Tripo/FBX", "assets": ["SM_DEV_WHEEL_S"]}

Qué hace:
- Crea (si falta) `<dest>/Materials/M_Tripo_Master`: texturas BaseColor/Normal/MR (convención glTF: G = rugosidad,
  B = metálico) y el escalar `Emissive`, que es lo que JamBuildPart.cpp (SetGlow) pone en 1 cuando la pieza se enciende.
- Importa las texturas con su espacio de color (Normal con el verde invertido: glTF usa la convención OpenGL).
- Importa la malla sin materiales ni texturas embebidas, crea `MI_<id>` del maestro y se lo asigna al slot 0.
"""
import json
import os

import unreal

AT = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
LOG = "TRIPO_IMPORT "


def log(msg):
    unreal.log(LOG + msg)
    print(LOG + msg)


def import_file(path, dest, name, options=None):
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
    return [p for p in task.imported_object_paths]


def make_master(path):
    if EAL.does_asset_exist(path):
        return unreal.load_asset(path)
    folder, name = path.rsplit("/", 1)
    mat = AT.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())

    def tex_param(pname, sampler, x, y):
        e = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
        e.set_editor_property("parameter_name", pname)
        e.set_editor_property("sampler_type", sampler)
        return e

    base = tex_param("BaseColor", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -800, -300)
    normal = tex_param("Normal", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, -800, 300)
    mr = tex_param("MR", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, -800, 0)
    # Defaults neutrales para que el maestro compile sin texturas asignadas
    base.set_editor_property("texture", unreal.load_asset("/Engine/EngineResources/DefaultTexture.DefaultTexture"))
    normal.set_editor_property("texture", unreal.load_asset("/Engine/EngineMaterials/DefaultNormal.DefaultNormal"))
    mr.set_editor_property("texture", unreal.load_asset("/Engine/EngineResources/Black.Black"))

    MEL.connect_material_property(base, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    MEL.connect_material_property(mr, "G", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(mr, "B", unreal.MaterialProperty.MP_METALLIC)

    # Brillo al encender (ART.md §4 / GDD: ámbar #E7B65A): brilla la propia textura teñida de cálido, no un color
    # plano encima (un ámbar plano tapaba la textura y las ruedas están encendidas todo el tiempo que se maneja).
    # Emissive = Emissive * BaseColor * GlowColor * GlowStrength
    emis = MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -800, 600)
    emis.set_editor_property("parameter_name", "Emissive")
    emis.set_editor_property("default_value", 0.0)
    glow = MEL.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -800, 750)
    glow.set_editor_property("parameter_name", "GlowColor")
    glow.set_editor_property("default_value", unreal.LinearColor(1.0, 0.72, 0.35, 1.0))
    strength = MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -800, 900)
    strength.set_editor_property("parameter_name", "GlowStrength")
    strength.set_editor_property("default_value", 0.6)
    m0 = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -600, 550)
    m1 = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -450, 650)
    m2 = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -300, 700)
    MEL.connect_material_expressions(emis, "", m0, "A")
    MEL.connect_material_expressions(base, "RGB", m0, "B")
    MEL.connect_material_expressions(m0, "", m1, "A")
    MEL.connect_material_expressions(glow, "", m1, "B")
    MEL.connect_material_expressions(m1, "", m2, "A")
    MEL.connect_material_expressions(strength, "", m2, "B")
    MEL.connect_material_property(m2, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    MEL.recompile_material(mat)
    EAL.save_loaded_asset(mat)
    log(f"maestro creado {path}")
    return mat


def import_texture(png, dest, name, role):
    paths = import_file(png, dest, name)
    if not paths:
        log(f"ERROR textura {png}")
        return None
    tex = unreal.load_asset(paths[0])
    if role == "N":
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("flip_green_channel", True)
    elif role == "MR":
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        tex.set_editor_property("srgb", False)
    else:
        tex.set_editor_property("srgb", True)
    EAL.save_loaded_asset(tex)
    return tex


def mesh_options():
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", False)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("import_animations", False)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    sm = ui.get_editor_property("static_mesh_import_data")
    sm.set_editor_property("combine_meshes", True)
    sm.set_editor_property("auto_generate_collision", False)
    sm.set_editor_property("convert_scene", True)
    sm.set_editor_property("import_uniform_scale", 1.0)
    return ui


def clear_existing(path):
    """Borra la malla vieja antes de importar para que la nueva quede con UN slot de material.

    Reimportar encima conserva los slots viejos y agrega uno nuevo; el código del juego solo hace brillar el slot 0.
    El juego carga las piezas por ruta (FindRealMesh), así que la ruta se conserva. Si otro asset la referencia, no se
    borra y se reimporta encima (habrá que revisar los slots a mano).
    """
    if not EAL.does_asset_exist(path):
        return True
    refs = [r for r in EAL.find_package_referencers_for_asset(path, False)
            if r != path.rsplit(".", 1)[0] and not str(r).startswith("/Game/Developers/")]  # niveles de prueba no cuentan
    if refs:
        log(f"AVISO {path} tiene referencias {refs}: se reimporta encima sin borrar")
        return False
    ok = EAL.delete_asset(path)
    log(f"borrada la malla vieja {path}: {ok}")
    return ok


def main():
    job = json.load(open(os.environ["TRIPO_IMPORT_JOB"], encoding="utf-8"))
    dest = job["dest"].rstrip("/")                       # raíz de Materials/ y Textures/
    mesh_dest = job.get("mesh_dest", f"{dest}/Meshes").rstrip("/")  # p. ej. /Game/Game/Building/Parts (reemplaza)
    fbx_dir = job["fbx_dir"]
    # Importador FBX clásico: respeta import_materials=False (Interchange crea materiales propios).
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
    master = make_master(f"{dest}/Materials/M_Tripo_Master")
    for sm_name in job["assets"]:
        base = sm_name[3:] if sm_name.startswith("SM_") else sm_name
        tex_dest = f"{dest}/Textures"
        texs = {}
        for role in ("D", "N", "MR"):
            png = os.path.join(fbx_dir, f"T_{base}_{role}.png")
            if os.path.exists(png):
                texs[role] = import_texture(png, tex_dest, f"T_{base}_{role}", role)
        clear_existing(f"{mesh_dest}/{sm_name}")
        paths = import_file(os.path.join(fbx_dir, f"{sm_name}.fbx"), mesh_dest, sm_name, mesh_options())
        mesh = None
        for p in paths:
            a = unreal.load_asset(p)
            if isinstance(a, unreal.StaticMesh):
                mesh = a
        if not mesh:
            log(f"ERROR malla {sm_name}: {paths}")
            continue
        mi_path = f"{dest}/Materials/MI_{base}"
        if EAL.does_asset_exist(mi_path):
            mi = unreal.load_asset(mi_path)
        else:
            mi = AT.create_asset(f"MI_{base}", f"{dest}/Materials", unreal.MaterialInstanceConstant,
                                 unreal.MaterialInstanceConstantFactoryNew())
        MEL.set_material_instance_parent(mi, master)
        for role, pname in (("D", "BaseColor"), ("N", "Normal"), ("MR", "MR")):
            if texs.get(role):
                MEL.set_material_instance_texture_parameter_value(mi, pname, texs[role])
        MEL.update_material_instance(mi)
        EAL.save_loaded_asset(mi)
        mesh.set_material(0, mi)
        EAL.save_loaded_asset(mesh)
        bb = mesh.get_bounding_box()
        size = bb.max - bb.min
        names = ["Attach_Top", "Attach_Bottom", "Attach_Front", "Attach_Back", "Attach_Left", "Attach_Right", "Fx_Out"]
        sockets = [n for n in names if mesh.find_socket(n)]
        log(f"OK {sm_name} slots={len(mesh.static_materials)} tris={mesh.get_num_triangles(0)} "
            f"bbox_cm=({size.x:.1f},{size.y:.1f},{size.z:.1f}) sockets={sockets}")


main()
