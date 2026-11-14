"""Normaliza todas las piezas de un lote que ya tengan GLB en raw/ (la versión más alta de cada una).

Uso:  python normalize_batch.py SourceArt/Tripo/batch_vehicle.json [-j 4] [SM_NOMBRE ...]
Llama a Blender (tripo_normalize.py) por pieza e imprime una tabla con triángulos y estiramiento por eje.
Un estiramiento > 1.3 significa que el modelo no tenía las proporciones de `Size`: revisar el preview o regenerar.

Mallas del mapa que comparten modelo (`"src"` + `"mat"`): las texturas son las mismas para todas, así que se guardan una
sola vez como T_<mat>_D/_N/_MR.png (las de cada instancia se borran) y solo la primera instancia de cada `mat` deja preview.
`-j N` corre N Blender en paralelo.
"""
import glob
import json
import os
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)  # SourceArt/Tripo
BLENDER = os.environ.get("BLENDER_EXE", r"D:\Blender Foundation\blender5.2\blender.exe")
FBX_DIR = os.path.join(ROOT, "FBX")


def latest_raw(name):
    files = glob.glob(os.path.join(ROOT, "raw", f"{name}_v*.glb"))
    if not files:
        return None
    return max(files, key=lambda p: int(re.search(r"_v(\d+)\.glb$", p).group(1)))


def base_of(name):
    return name[3:] if name.startswith("SM_") else name


def run_part(p, preview):
    # "src": GLB de raw/ compartido por varias mallas (p. ej. las 2 torres rojas del mapa usan el mismo modelo)
    src = os.path.join(ROOT, "raw", p["src"]) if p.get("src") else latest_raw(p["name"])
    if not src or not os.path.exists(src):
        return (p["name"], "sin GLB", p.get("src", ""), "")
    cmd = [BLENDER, "--background", "--factory-startup", "--python", os.path.join(HERE, "tripo_normalize.py"), "--",
           "--in", src, "--name", p["name"], "--size", *[str(s) for s in p["size"]],
           "--out", FBX_DIR, "--sockets", p.get("sockets", "six"), "--tex", str(p.get("tex", 1024))]
    if preview:
        cmd += ["--preview", os.path.join(ROOT, "previews", p["name"] + ".png")]
    if p.get("auto_axis", True):  # "auto_axis": false si el modelo ya viene de pie (p. ej. un aro arriba que lo haría más alto)
        cmd += ["--auto-axis"]
    if p.get("rot"):
        cmd += ["--rot", p["rot"]]
    if p.get("tris"):
        cmd += ["--tris", str(p["tris"])]
    if p.get("fit"):  # "height": props del mapa, misma altura con proporciones propias (ver tripo_normalize.py)
        cmd += ["--fit", p["fit"]]
    if p.get("zclip"):  # [desde, hasta] en fracción de la altura: p. ej. el cuerpo y la tapa de un cofre del mismo modelo
        cmd += ["--zclip", *[str(z) for z in p["zclip"]]]
    if p.get("center"):  # mallas del mapa: traen su posición del mundo integrada (ver ue_dump_bounds.py)
        cmd += ["--center", *[str(c) for c in p["center"]]]
    for fx in p.get("fx", []):
        cmd += ["--fx", fx]
    out = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace").stdout
    m = re.search(r"TRIPO_REPORT (\{.*\})", out)
    if not m:
        return (p["name"], "ERROR", os.path.basename(src), out[-400:])
    r = json.loads(m.group(1))
    if p.get("mat"):
        share_textures(p["name"], p["mat"], first=preview)
    flag = "REVISAR" if max(r["stretch"]) > 1.3 else "ok"
    extra = f" caja={r['bbox_cm']} de {p['size']}" if r.get("bbox_cm") else ""
    return (p["name"], flag, f"{r['tris_after']} tris", f"stretch={r['stretch']} eje={r['auto_axis']}{extra}")


def share_textures(name, mat, first):
    """T_<instancia>_* -> T_<mat>_* (la primera instancia de cada mat) o se borran (las demás: son idénticas)."""
    for path in glob.glob(os.path.join(FBX_DIR, f"T_{base_of(name)}_*.png")):
        role = path.rsplit("_", 1)[1]
        if first and base_of(name) != mat:
            os.replace(path, os.path.join(FBX_DIR, f"T_{mat}_{role}"))
        elif base_of(name) != mat:
            os.remove(path)
    prev = os.path.join(ROOT, "previews", name + ".png")
    if first and os.path.exists(prev) and base_of(name) != mat:
        os.replace(prev, os.path.join(ROOT, "previews", f"SM_{mat}.png"))  # la vista previa es del modelo, no de la instancia


def main():
    args = sys.argv[1:]
    jobs = 1
    if "-j" in args:
        i = args.index("-j")
        jobs = int(args[i + 1])
        del args[i:i + 2]
    batch = json.load(open(args[0], encoding="utf-8"))
    only = set(args[1:])
    parts = [p for p in batch["parts"] if not only or p["name"] in only]
    # La primera instancia de cada `mat` corre sola y antes (deja preview y las texturas T_<mat>); el resto en paralelo.
    seen, firsts, rest = set(), [], []
    for p in parts:
        key = p.get("mat") or p["name"]
        (rest if key in seen else firsts).append(p)
        seen.add(key)
    with ThreadPoolExecutor(max_workers=jobs) as ex:
        rows = list(ex.map(lambda p: run_part(p, True), firsts))
        rows += list(ex.map(lambda p: run_part(p, not p.get("mat")), rest))
    for row in rows:
        print(" | ".join(str(c) for c in row))


if __name__ == "__main__":
    main()
