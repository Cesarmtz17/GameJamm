"""Junta todos los T_Icon_*.png en una sola lámina (cuadrícula) para revisarlos de un vistazo.

Uso:
    blender --background --factory-startup --python icon_sheet.py -- <carpeta_iconos> <salida.png>
"""
import glob
import math
import os
import sys

import bpy
import numpy as np

argv = sys.argv[sys.argv.index("--") + 1:]
icons = sorted(glob.glob(os.path.join(argv[0], "T_Icon_*.png")))
cols = 8
rows = math.ceil(len(icons) / cols)
S = 256
sheet = np.zeros((rows * S, cols * S, 4), dtype=np.float32)
sheet[..., :3] = (0.93, 0.91, 0.86)
sheet[..., 3] = 1.0
for i, path in enumerate(icons):
    img = bpy.data.images.load(path)
    px = np.empty(S * S * 4, dtype=np.float32)
    img.pixels.foreach_get(px)
    px = px.reshape(S, S, 4)
    r, c = rows - 1 - i // cols, i % cols  # las filas de Blender van de abajo hacia arriba
    cell = sheet[r * S:(r + 1) * S, c * S:(c + 1) * S]
    a = px[..., 3:4]
    cell[..., :3] = px[..., :3] * a + cell[..., :3] * (1 - a)
out = bpy.data.images.new("sheet", width=cols * S, height=rows * S, alpha=True)
out.pixels.foreach_set(sheet.ravel())
out.filepath_raw = argv[1]
out.file_format = "PNG"
out.save()
print("SHEET_OK", len(icons))
