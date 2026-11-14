# Arte con Tripo

Dueño: Javier Fernández (ver [docs/TEAM.md](../../docs/TEAM.md)). Objetivo: subir la calidad visual al nivel del concepto
[01-C](../../docs/concepts/concept-01C-drifting-isles.png) sin salirse de [docs/ART.md](../../docs/ART.md).

| Carpeta | Qué hay |
|---|---|
| `raw/` | GLB tal como sale de Tripo Studio (`<SM>_v<n>.glb`) |
| `FBX/` | FBX normalizado al contrato de piezas + texturas `T_<id>_D/_N/_MR.png` (1K) |
| `previews/` | Render de revisión (flecha roja = +X, adelante) |
| `scripts/tripo_normalize.py` | Blender: une, orienta, reduce, encaja en `Size`, pivote al centro, sockets, exporta FBX |
| `scripts/ue_import_tripo.py` | Unreal (commandlet de Python): maestro `M_Tripo_Master`, texturas, malla y `MI_<id>` |

## Flujo por pieza

1. **Imagen de referencia** en Tripo Studio → Imagen (Nano Banana, gratis: 100 por cuenta), con el concepto 01-C como
   referencia de estilo, 1:1, 2 variantes. Prompt: `Game asset, single object only: <objeto>, in the exact art style of the
   reference image (stylized hand-painted 3D game art, chunky toy-like proportions, warm soft daylight, matte finish). <detalles>.
   Three-quarter view, centered, whole object visible, plain light grey background. No ground, no shadows, no text, no other objects.`
2. **Imagen a 3D**: Modelo HD, IA `H3.1`, malla Ultra **apagada**, textura **2K**, PBR y quitar iluminación encendidos,
   triángulos, **5,000 polígonos** → **30 créditos**. Cada modelo trae 3 reintentos gratis.
3. **Exportar** GLB 2K → `raw/<SM>_v<n>.glb`.
4. **Normalizar** (Blender 5.x):
   ```
   blender --background --factory-startup --python SourceArt/Tripo/scripts/tripo_normalize.py -- \
     --in SourceArt/Tripo/raw/SM_DEV_WHEEL_S_v1.glb --name SM_DEV_WHEEL_S --size 50 20 50 \
     --out SourceArt/Tripo/FBX --auto-axis --sockets lr --tex 1024 --preview SourceArt/Tripo/previews/SM_DEV_WHEEL_S.png
   ```
   `Size` sale de la tabla de `Source/GameJamm/Building/JamBuildPart.cpp` (fuente de verdad). Si el reporte avisa de
   estiramiento > 30 %, revisar la orientación con `--rot` o regenerar.
5. **Importar a UE** (proyecto compilado, editor cerrado o sin esos assets abiertos):
   `TRIPO_IMPORT_JOB=<job.json> UnrealEditor-Cmd GameJamm.uproject -run=pythonscript -script=SourceArt/Tripo/scripts/ue_import_tripo.py -unattended -nosplash -nullrhi`
   con `{"dest": "/Game/Developers/javie/Tripo", "fbx_dir": ".../SourceArt/Tripo/FBX", "assets": ["SM_DEV_WHEEL_S"]}`.
   Revisar después `git status`: el editor a veces agrega la sección `AndroidFileServer` a `Config/DefaultEngine.ini` (no se sube).

## Contrato que cumple cada malla

- La pieza visual se dibuja con escala absoluta (`AJamBuildPart::InitFromDef`): la caja mide **exactamente** `Size` en cm.
- Pivote al centro de la caja; X = adelante, Y = derecha, Z = arriba; ruedas con eje en Y.
- Un solo slot de material: `SetGlow` pone el escalar `Emissive` del slot 0 en 1 al encender la pieza (brillo ámbar).
- ≤ 5k triángulos y texturas de 1K (512 en piezas chicas): presupuesto de memoria de video para PCs modestas y zip final < 1 GB.

## Estado

Las mallas que usa el juego se cargan desde `Content/Game/Building/Parts/` (Construcción) y el mapa desde
`Content/Game/Core/World/MapDressed/` (Core). Hasta acordar el reemplazo con sus dueños, todo se importa a
`Content/Developers/javie/Tripo/`.

## Registro (para "Tools used" en SUBMISSION.md y el devlog)

| Asset | Imagen de referencia | Modelo 3D | Créditos | Fecha |
|---|---|---|---|---|
| `SM_DEV_WHEEL_S` | Nano Banana + concepto 01-C, "small wooden wheel… thick dark iron rim… four big iron bolts" | Modelo HD H3.1, 2K, PBR, 5k polys → 4,783 tris | 30 | 14 nov |
| Lote 1 (10 piezas: tabla, bloque, rueda grande, ventilador, asiento, batería, cañón, lanzallamas, cohete, globo) | Nano Banana + concepto 01-C; prompts en `batch_vehicle.json` | Modelo HD H3.1, 2K, PBR, 5k polys | 300 | 14 nov |
| Lote 2 (24 piezas de `batch_wave2.json`: con esto las 35 piezas de `RealMeshNames()` son de Tripo) | Nano Banana + concepto 01-C; prompts en `batch_wave2.json` | Modelo HD H3.1, 2K, PBR, 5k polys | 720 | 14 nov |

Total al 14 nov 13:45: 35 modelos, 1,050 créditos (quedan ~2,150 de 3,200); 41 de 100 imágenes gratis.

Exportar en serie: Chrome bloquea descargas múltiples; se captura la URL firmada de cada exportación y se descarga directo
(ver el historial de la sesión). Las referencias de imagen son gratis hasta 100 por cuenta.

Comparación antes/después del lote 1: `previews/_comparacion_vehiculo.png`.
