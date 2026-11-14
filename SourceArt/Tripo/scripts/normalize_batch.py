"""Normaliza todas las piezas de un lote que ya tengan GLB en raw/ (la versión más alta de cada una).

Uso:  python normalize_batch.py SourceArt/Tripo/batch_vehicle.json [SM_NOMBRE ...]
Llama a Blender (tripo_normalize.py) por pieza e imprime una tabla con triángulos y estiramiento por eje.
Un estiramiento > 1.3 significa que el modelo no tenía las proporciones de `Size`: revisar el preview o regenerar.
"""
import glob
import json
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)  # SourceArt/Tripo
BLENDER = os.environ.get("BLENDER_EXE", r"D:\Blender Foundation\blender5.2\blender.exe")


def latest_raw(name):
    files = glob.glob(os.path.join(ROOT, "raw", f"{name}_v*.glb"))
    if not files:
        return None
    return max(files, key=lambda p: int(re.search(r"_v(\d+)\.glb$", p).group(1)))


def main():
    batch = json.load(open(sys.argv[1], encoding="utf-8"))
    only = set(sys.argv[2:])
    rows = []
    for p in batch["parts"]:
        if only and p["name"] not in only:
            continue
        src = latest_raw(p["name"])
        if not src:
            rows.append((p["name"], "sin GLB", "", ""))
            continue
        cmd = [BLENDER, "--background", "--factory-startup", "--python", os.path.join(HERE, "tripo_normalize.py"), "--",
               "--in", src, "--name", p["name"], "--size", *[str(s) for s in p["size"]],
               "--out", os.path.join(ROOT, "FBX"), "--auto-axis", "--sockets", p.get("sockets", "six"),
               "--tex", str(p.get("tex", 1024)), "--preview", os.path.join(ROOT, "previews", p["name"] + ".png")]
        if p.get("rot"):
            cmd += ["--rot", p["rot"]]
        for fx in p.get("fx", []):
            cmd += ["--fx", fx]
        out = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace").stdout
        m = re.search(r"TRIPO_REPORT (\{.*\})", out)
        if not m:
            rows.append((p["name"], "ERROR", os.path.basename(src), out[-400:]))
            continue
        r = json.loads(m.group(1))
        flag = "REVISAR" if max(r["stretch"]) > 1.3 else "ok"
        rows.append((p["name"], flag, f"{r['tris_after']} tris", f"stretch={r['stretch']} eje={r['auto_axis']}"))
    for row in rows:
        print(" | ".join(str(c) for c in row))


if __name__ == "__main__":
    main()
