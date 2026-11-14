"""Solo lectura: imprime caja (cm) y posición del pivote de cada StaticMesh de una carpeta.

Uso: TRIPO_BOUNDS_DIR=/Game/Game/Core/World/MapDressed UnrealEditor-Cmd GameJamm.uproject -run=pythonscript -script=<este>
Sirve para reemplazar una malla del mapa "en su lugar": el modelo nuevo debe tener la misma caja relativa al pivote.
"""
import json
import os

import unreal

folder = os.environ.get("TRIPO_BOUNDS_DIR", "/Game/Game/Core/World/MapDressed")
out = {}
for path in unreal.EditorAssetLibrary.list_assets(folder, recursive=False):
    a = unreal.load_asset(path)
    if not isinstance(a, unreal.StaticMesh):
        continue
    bb = a.get_bounding_box()
    out[a.get_name()] = {"min": [round(bb.min.x, 1), round(bb.min.y, 1), round(bb.min.z, 1)],
                         "max": [round(bb.max.x, 1), round(bb.max.y, 1), round(bb.max.z, 1)],
                         "tris": a.get_num_triangles(0),
                         "mats": [str(m.material_interface.get_path_name()) if m.material_interface else None
                                  for m in a.static_materials]}
dst = os.environ.get("TRIPO_BOUNDS_OUT", "")
if dst:
    json.dump(out, open(dst, "w", encoding="utf-8"), indent=1)
unreal.log(f"TRIPO_BOUNDS {len(out)} mallas -> {dst}")
