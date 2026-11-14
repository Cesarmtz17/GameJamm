"""Editor (no commandlet): asigna a las torres/bases de L_Main (ATeamStructure) la malla de Tripo/Rodin de su equipo.

Uso: UnrealEditor GameJamm.uproject /Engine/Maps/Entry -ExecutePythonScript="<ruta>/ue_assign_structures.py"
(el editor se cierra solo al terminar). Solo toca los actores ATeamStructure cuya malla es SM_TOWER_WATCH o SM_BASE_KEEP:
- malla -> /Game/Game/Art/Tripo/Structures/SM_STRUCT_{TOWER|KEEP}_{RED|BLUE} (equipo según su MI_Structure_Red/Blue)
- quita los materiales forzados del componente (la textura viene en el MI de la malla).
No cambia posición, escala, vida ni nada del gameplay.
"""
import unreal

MAP = "/Game/Game/Maps/L_Main"
NEW = "/Game/Game/Art/Tripo/Structures/SM_STRUCT_{kind}_{team}"
KIND = {"SM_TOWER_WATCH": "TOWER", "SM_BASE_KEEP": "KEEP"}


def log(m):
    unreal.log("TRIPO_ASSIGN " + m)


les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les.load_level(MAP)

# World Partition: en el editor los actores pueden estar sin cargar; carga los TeamStructure por su descriptor.
try:
    descs = unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
    if isinstance(descs, tuple):
        descs = descs[-1]
    guids = [d.guid for d in descs if "TeamStructure" in str(d.native_class) or "TeamStructure" in str(d.class_)]
    if guids:
        unreal.WorldPartitionBlueprintLibrary.load_actors(guids)
    log(f"descriptores TeamStructure: {len(guids)}")
except Exception as e:  # noqa: BLE001
    log(f"sin carga por descriptores ({e}); uso los actores ya cargados")

done = 0
for actor in eas.get_all_level_actors():
    if "TeamStructure" not in actor.get_class().get_name():
        continue
    for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
        sm = comp.get_editor_property("static_mesh")
        if not sm or sm.get_name() not in KIND:
            continue
        overrides = [m.get_name() for m in comp.get_editor_property("override_materials") if m]
        team = "BLUE" if any("Blue" in n for n in overrides) else "RED" if any("Red" in n for n in overrides) else None
        if team is None:
            team = "BLUE" if actor.get_actor_location().x > 0 else "RED"  # Rojo en X-, Azul en X+ (map_layout.json)
        new = unreal.load_asset(NEW.format(kind=KIND[sm.get_name()], team=team))
        if not new:
            log(f"ERROR falta {NEW.format(kind=KIND[sm.get_name()], team=team)}")
            continue
        actor.modify()
        comp.modify()
        comp.set_static_mesh(new)
        comp.set_editor_property("override_materials", [])
        done += 1
        log(f"OK {actor.get_actor_label()} {sm.get_name()} -> {new.get_name()} (overrides antes: {overrides})")

ok = unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
log(f"actores cambiados: {done}; guardado: {ok}")
