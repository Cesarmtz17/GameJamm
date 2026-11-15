"""Variante de equipo de una textura de personaje: cambia el tono de la ropa roja al azul de equipo (ART.md §3).

Uso:  python team_recolor.py <entrada_D.png> <salida_D.png> [--hue 224] [--src-hue 357] [--width 22] [--min-sat 0.28]

Solo toca los píxeles cuyo tono está a menos de `width` grados de `src-hue` y con saturación >= `min-sat` (la ropa de
equipo). Piel, cuero, madera y metal quedan igual: el color de equipo va en la ropa, no como tinte de todo el modelo.
Rojo #C94248 = 357°, Azul #355DC6 = 224°.
"""
import argparse

import numpy as np
from PIL import Image


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("src")
    ap.add_argument("dst")
    ap.add_argument("--hue", type=float, default=224.0, help="tono destino en grados")
    ap.add_argument("--src-hue", type=float, default=357.0, help="tono de origen en grados")
    ap.add_argument("--width", type=float, default=12.0, help="+- grados alrededor del tono de origen")
    ap.add_argument("--min-sat", type=float, default=0.5)
    ap.add_argument("--value", type=float, default=1.0, help="multiplica el brillo de lo recoloreado")
    a = ap.parse_args()

    img = Image.open(a.src).convert("RGB")
    hsv = np.asarray(img.convert("HSV")).astype(np.float32)
    h = hsv[..., 0] * (360.0 / 255.0)
    s = hsv[..., 1] / 255.0
    d = np.abs((h - a.src_hue + 180.0) % 360.0 - 180.0)
    # Peso suave en el borde del rango para no dejar halos.
    w = np.clip((a.width - d) / 6.0, 0.0, 1.0) * np.clip((s - a.min_sat) / 0.08, 0.0, 1.0)
    new_h = (h + ((a.hue - a.src_hue + 180.0) % 360.0 - 180.0)) % 360.0
    out = hsv.copy()
    out[..., 0] = np.where(w > 0, new_h * (255.0 / 360.0), hsv[..., 0])
    out[..., 2] = np.clip(hsv[..., 2] * (1.0 + (a.value - 1.0) * w), 0, 255)
    rec = np.asarray(Image.fromarray(out.astype(np.uint8), "HSV").convert("RGB")).astype(np.float32)
    src = np.asarray(img).astype(np.float32)
    mix = src * (1.0 - w[..., None]) + rec * w[..., None]
    Image.fromarray(np.clip(mix, 0, 255).astype(np.uint8), "RGB").save(a.dst)
    print(f"RECOLOR {a.dst}: {100.0 * float((w > 0.5).mean()):.1f}% de los píxeles cambiados")


if __name__ == "__main__":
    main()
