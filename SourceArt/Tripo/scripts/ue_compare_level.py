"""Nivel de comparación (editor abierto): pieza actual vs pieza de Tripo, con luz de día como la del juego.

Uso: abrir el editor con -ExecutePythonScript="<ruta>/ue_compare_level.py" (o `py <ruta>` en la consola del editor).
Crea o reabre /Game/Developers/javie/L_Test_Tripo (solo en la carpeta de pruebas de Javier) con:
  - Emilio:  /Game/Game/Building/Parts/<SM>   (lo que el juego usa hoy)
  - Tripo:   /Game/Developers/javie/Tripo/Meshes/<SM>
  - Tripo encendida: misma malla con Emissive = 1 (lo que hace SetGlow cuando la pieza está prendida)
"""
import unreal

LEVEL = "/Game/Developers/javie/L_Test_Tripo"
ASSETS = ["SM_DEV_WHEEL_S"]
TRIPO = "/Game/Developers/javie/Tripo"

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary


def rot(pitch=0.0, yaw=0.0, roll=0.0):
    return unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw)  # el orden posicional de unreal.Rotator es (roll, pitch, yaw)


def spawn(cls, loc, r=(0, 0), label=None):
    a = eas.spawn_actor_from_class(cls, unreal.Vector(*loc), rot(*r))
    if label:
        a.set_actor_label(label)
    return a


def text(msg, loc, size=14):
    t = spawn(unreal.TextRenderActor, loc, (0, -90), f"Label_{msg}")  # mira hacia -Y (la cámara)
    c = t.text_render
    c.set_editor_property("text", msg)
    c.set_editor_property("world_size", size)
    c.set_editor_property("horizontal_alignment", unreal.HorizTextAligment.EHTA_CENTER)
    c.set_editor_property("text_render_color", unreal.Color(30, 30, 30, 255))
    return t


def glow_mi(base):
    path = f"{TRIPO}/Materials/MI_{base}_Glow"
    if EAL.does_asset_exist(path):
        return unreal.load_asset(path)
    mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        f"MI_{base}_Glow", f"{TRIPO}/Materials", unreal.MaterialInstanceConstant,
        unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, unreal.load_asset(f"{TRIPO}/Materials/MI_{base}"))
    MEL.set_material_instance_scalar_parameter_value(mi, "Emissive", 1.0)
    MEL.update_material_instance(mi)
    EAL.save_loaded_asset(mi)
    return mi


def build():
    if EAL.does_asset_exist(LEVEL):
        les.load_level(LEVEL)
        for a in eas.get_all_level_actors():
            eas.destroy_actor(a)
    else:
        les.new_level(LEVEL, False)

    # Luz de día cálida (ART.md §8): sol #E7B65A, cielo, luz de cielo y niebla de altura.
    sun = spawn(unreal.DirectionalLight, (0, 0, 500), (-40, -60), "Sun")
    lc = sun.light_component
    lc.set_editor_property("intensity", 8.0)
    lc.set_editor_property("light_color", unreal.Color(255, 236, 205, 255))
    lc.set_editor_property("atmosphere_sun_light", True)
    spawn(unreal.SkyAtmosphere, (0, 0, 0), label="Sky")
    sky = spawn(unreal.SkyLight, (0, 0, 300), label="SkyLight")
    sky.light_component.set_editor_property("real_time_capture", True)
    spawn(unreal.ExponentialHeightFog, (0, 0, 0), label="Fog")

    floor = eas.spawn_actor_from_object(unreal.load_asset("/Engine/BasicShapes/Plane"), unreal.Vector(0, 0, 0))
    floor.set_actor_scale3d(unreal.Vector(30, 30, 1))
    floor.set_actor_label("Floor")

    for i, sm in enumerate(ASSETS):
        base = sm[3:]
        y = i * 150
        old = unreal.load_asset(f"/Game/Game/Building/Parts/{sm}")
        new = unreal.load_asset(f"{TRIPO}/Meshes/{sm}")
        half = new.get_bounding_box().max.z if new else 25
        if old:
            a = eas.spawn_actor_from_object(old, unreal.Vector(-90, y, half))
            a.set_actor_label(f"Emilio_{sm}")
            text("Emilio (hoy)", (-90, y - 40, half * 2 + 20))
        if new:
            b = eas.spawn_actor_from_object(new, unreal.Vector(0, y, half))
            b.set_actor_label(f"Tripo_{sm}")
            text("Tripo", (0, y - 40, half * 2 + 20))
            c = eas.spawn_actor_from_object(new, unreal.Vector(90, y, half))
            c.set_actor_label(f"TripoGlow_{sm}")
            c.static_mesh_component.set_material(0, glow_mi(base))
            text("Tripo encendida", (90, y - 40, half * 2 + 20))

    # Cámara mirando las caras de las ruedas (+Y), un poco desde arriba.
    ues.set_level_viewport_camera_info(unreal.Vector(0, -260, 90), rot(-12, 90))
    les.save_current_level()
    unreal.log("TRIPO_COMPARE listo " + LEVEL)


build()
