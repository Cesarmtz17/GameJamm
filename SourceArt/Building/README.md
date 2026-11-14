# Piezas de construcción (fuente)

Dueño: Emilio (arte 3D). Generadas por script en Blender 5.3 a partir de la tabla de `Source/GameJamm/Building/JamBuildPart.cpp`
y de [docs/assets/PROMPTS-ASSETS.md](../../docs/assets/PROMPTS-ASSETS.md); estilo en [docs/ART.md](../../docs/ART.md).

| Carpeta | Qué hay |
|---|---|
| `FBX/` | Un FBX por pieza, listo para importar a `Content/Game/Building/Parts/` |
| `Blend/` | Un `.blend` por pieza (editable a mano; `git lfs lock` antes de tocarlo) |
| `scripts/gen_parts.py` | Generador: regenera las 12 piezas en segundos |
| `T_Part_Palette.png` | Textura de paleta compartida (8 colores de ART.md) |
| `preview.png` | Vista previa de todas las piezas (agrandadas para que se vean) |

## Qué trae cada FBX

- **Bounding box = `Size` exacto** de la tabla del código, pivote en el centro de la caja, X = adelante, Y = derecha, Z = arriba.
- **Colisión** `UCX_<SM>_00`: caja convexa; cilindro convexo en las ruedas (eje Y).
- **Sockets** (Empties `SOCKET_*`): `Attach_Top/Bottom/Front/Back/Left/Right` en el centro de cada cara, con X saliendo de la
  superficie. Ruedas: solo `Attach_Left`/`Attach_Right`. `Fx_Out` en Fan (+X), Cannon (+X), Flamethrower (+X) y Rocket (−X).
- **Un material** `M_Part_Master` con la textura de paleta (cada sub-pieza manda sus UVs a un color).

| Receta | Asset | Size (cm) | Tris |
|---|---|---|---|
| `Plank` | `SM_PART_PLANK` | 200 × 60 × 10 | 148 |
| `Block` | `SM_PART_BLOCK` | 60 × 60 × 60 | 140 |
| `Cylinder` | `SM_PART_CYLINDER` | 150 × 50 × 50 | 132 |
| `Wheel` | `SM_DEV_WHEEL_S` | 50 × 20 × 50 | 376 |
| `BigWheel` | `SM_DEV_WHEEL_L` | 80 × 30 × 80 | 520 |
| `Fan` | `SM_DEV_FAN` | 15 × 50 × 50 | 192 |
| `Seat` | `SM_CTRL_SEAT` | 40 × 40 × 30 | 156 |
| `Battery` | `SM_PWR_BATTERY_S` | 20 × 20 × 20 | 112 |
| `Cannon` | `SM_DEV_CANNON` | 70 × 20 × 20 | 176 |
| `Flamethrower` | `SM_DEV_FLAME` | 40 × 20 × 20 | 120 |
| `Rocket` | `SM_DEV_ROCKET` | 60 × 20 × 20 | 208 |
| `Balloon` | `SM_DEV_BALLOON` | 90 × 90 × 90 | 232 |

## Importar en Unreal

FBX import con la configuración por defecto (Convert Scene activado): Blender trabaja en metros y UE los convierte a cm.
Activar **"Import Mesh LODs" no hace falta**; las `UCX_` y los `SOCKET_` se detectan solos.

> [!warning] Sin probar en Unreal
> Verificado reimportando en Blender (nombres de sockets y colisión, medidas). **Falta confirmar en UE** que la orientación
> sale con X adelante y que los sockets `Left`/`Right` quedan del lado correcto.

## Regenerar

```
blender --background --factory-startup --python SourceArt/Building/scripts/gen_parts.py -- SourceArt/Building <carpeta_preview>
```
