"""Riega matas y flores 3D (SM_FOL_*) sobre el pasto de L_Main con instancias (HISM).

Uso (editor abierto con L_Main, sin PIE): caja Cmd -> py "<ruta>/ue_scatter_foliage.py"
- Superficies: los StaticMeshActor que usan M_Map_00 (cimas de islas) o M_Map_02 (pradera y cerros).
- Traza rayos desde arriba: solo coloca si el rayo pega en ESA superficie (asi quedan fuera caminos, arboles, bases y
  rocas) y si la cara mira hacia arriba (normal Z > 0.85).
- Racimos de 3-6 matas y a veces flores; giro, escala y hundido al azar (la base de disco queda bajo el piso).
- Un solo actor `FOL_Scatter` (carpeta Dressing/Foliage) con un HISM por malla: sin colision (no frena vehiculos),
  sin sombra y con distancia de corte (GTX 1650). Siempre cargado (no depende de celdas de World Partition).
- Re-ejecutable: borra el FOL_Scatter anterior y vuelve a regar con la misma semilla.
"""
import json
import math
import os
import random

import unreal

SEED = 1415
CLUSTER_M2 = 28.0          # un racimo por cada N m2 de pasto
SURF_MATS = ("M_Map_00", "M_Map_02")
MESHES = {  # nombre -> (peso dentro de un racimo, alto en cm, escala min/max)
    "SM_FOL_GRASS_A": (5, 38, 0.8, 1.3),
    "SM_FOL_GRASS_B": (3, 60, 0.7, 1.2),
    "SM_FOL_FLOWERS_A": (1, 30, 0.8, 1.2),
    "SM_FOL_FLOWERS_B": (1, 32, 0.8, 1.2),
}
SINK = 0.10                # fraccion del alto que se hunde (oculta la base)
CULL_START, CULL_END = 4500.0, 6500.0
LABEL = "FOL_Scatter"
MESH_DIR = "/Game/Game/Art/Tripo/Foliage"

EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
rng = random.Random(SEED)


def surfaces():
    out = []
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.StaticMeshActor):
        c = a.static_mesh_component
        if not c.static_mesh:
            continue
        names = {c.get_material(i).get_name() for i in range(c.get_num_materials()) if c.get_material(i)}
        if names & set(SURF_MATS):
            out.append(a)
    return out


def trace(x, y, ztop, zbot):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, ztop), unreal.Vector(x, y, zbot),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    if not hit:
        return None
    t = hit.to_tuple()
    vecs = [v for v in t if isinstance(v, unreal.Vector)]
    actor = next((v for v in t if isinstance(v, unreal.Actor)), None)
    if len(vecs) < 4 or not actor:
        return None
    return vecs[1], vecs[3], actor  # impact_point, impact_normal, actor


def pick_mesh():
    names = list(MESHES)
    return rng.choices(names, weights=[MESHES[n][0] for n in names])[0]


def main():
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.StaticMeshActor):
        if a.get_actor_label() == LABEL:
            EAS.destroy_actor(a)

    xforms = {n: [] for n in MESHES}
    per_surface = {}
    for s in surfaces():
        o, e = s.get_actor_bounds(False)
        area_m2 = (2 * e.x) * (2 * e.y) / 1e4
        n_clusters = max(1, int(area_m2 / CLUSTER_M2))
        placed = 0
        for _ in range(n_clusters):
            cx = rng.uniform(o.x - e.x, o.x + e.x)
            cy = rng.uniform(o.y - e.y, o.y + e.y)
            for _ in range(rng.randint(3, 6)):
                r = rng.uniform(0, 160)
                ang = rng.uniform(0, 2 * math.pi)
                x, y = cx + r * math.cos(ang), cy + r * math.sin(ang)
                h = trace(x, y, o.z + e.z + 500, o.z - e.z - 100)
                if not h:
                    continue
                p, nrm, actor = h
                if actor.get_path_name() != s.get_path_name() or nrm.z < 0.85:
                    continue
                name = pick_mesh()
                _, height, smin, smax = MESHES[name]
                sc = rng.uniform(smin, smax)
                loc = unreal.Vector(p.x, p.y, p.z - SINK * height * sc)
                rot = unreal.Rotator(roll=rng.uniform(-4, 4), pitch=rng.uniform(-4, 4), yaw=rng.uniform(0, 360))
                xforms[name].append(unreal.Transform(location=loc, rotation=rot, scale=unreal.Vector(sc, sc, sc)))
                placed += 1
        per_surface[s.get_actor_label()] = placed

    holder = EAS.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 0))
    holder.set_actor_label(LABEL)
    holder.set_folder_path("Dressing/Foliage")
    try:
        holder.set_editor_property("is_spatially_loaded", False)
    except Exception:
        pass
    root = holder.static_mesh_component
    root.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    root_handle = sds.k2_gather_subobject_data_for_instance(holder)[0]
    counts = {}
    for name, xs in xforms.items():
        if not xs:
            continue
        params = unreal.AddNewSubobjectParams(parent_handle=root_handle,
                                              new_class=unreal.HierarchicalInstancedStaticMeshComponent)
        handle, fail = sds.add_new_subobject(params)
        data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
        comp = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
        sds.rename_subobject(handle, unreal.Text(name.replace("SM_", "HISM_")))
        comp.set_static_mesh(unreal.load_asset(MESH_DIR + "/" + name))
        comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        comp.set_editor_property("cast_shadow", False)
        comp.set_editor_property("instance_start_cull_distance", int(CULL_START))
        comp.set_editor_property("instance_end_cull_distance", int(CULL_END))
        comp.add_instances(xs, False, True)
        counts[name] = comp.get_instance_count()
    report = {"per_surface": per_surface, "counts": counts, "actor": holder.get_path_name()}
    json.dump(report, open(os.path.join(unreal.Paths.project_saved_dir(), "ue_scatter_foliage_report.json"), "w"), indent=1)
    unreal.log("ue_scatter_foliage: " + json.dumps(counts))


main()
