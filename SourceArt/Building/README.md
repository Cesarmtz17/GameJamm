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

### Lote 4: todas las demás piezas de `JamBuildPart.cpp` (`gen_assets.py ... lot4`)

Mismas medidas que la tabla del código (fuente de verdad); con estas + las 12 de arriba están cubiertas las 35 recetas.

| Receta | Asset | Size (cm) | Receta | Asset | Size (cm) |
|---|---|---|---|---|---|
| `Wall` | `SM_PART_WALL` | 30 × 200 × 150 | `Wing` | `SM_DEV_WING` | 80 × 160 × 6 |
| `Panel` | `SM_PART_PANEL` | 10 × 100 × 100 | `Sail` | `SM_DEV_SAIL` | 10 × 120 × 100 |
| `Girder` | `SM_PART_GIRDER` | 300 × 20 × 20 | `Stabilizer` | `SM_DEV_STABILIZER` | 30 × 30 × 20 |
| `Ball` | `SM_PART_BALL` | 60 × 60 × 60 | `Head` | `SM_CTRL_HEAD` | 40 × 40 × 40 |
| `Sled` | `SM_DEV_SLED` | 160 × 70 × 15 | `BigBattery` | `SM_PWR_BATTERY_L` | 35 × 35 × 35 |
| `Hinge` | `SM_JOINT_HINGE` | 20 × 60 × 20 | `Frost` | `SM_DEV_FROST` | 40 × 25 × 25 |
| `Turntable` | `SM_JOINT_TURNTABLE` | 40 × 40 × 30 | `Shock` | `SM_DEV_SHOCK` | 40 × 25 × 25 |
| `Stake` | `SM_ANCHOR_STAKE` | 15 × 15 × 20 | `Beam` | `SM_DEV_BEAM` | 50 × 12 × 12 |
| `Spring` | `SM_DEV_SPRING` | 30 × 30 × 18 | `Bomb` | `SM_CTRL_TIMEBOMB` | 30 × 30 × 30 |
| `Jet` | `SM_DEV_JET` | 60 × 40 × 40 (tobera −X) | `Hydrant` | `SM_DEV_HYDRANT` | 40 × 30 × 30 |
| `Turbine` | `SM_DEV_TURBINE` | 25 × 90 × 90 | `Light` | `SM_DEV_LIGHT` | 25 × 25 × 25 |
| `HoverStone` | `SM_DEV_HOVER` | 45 × 45 × 45 | | | |

`Fx_Out` (+X) en Turbine, Head, Frost, Shock, Beam, Hydrant y Light; en `Jet` va en −X. Vista previa: `preview_extra.png`.
El mismo script genera también (cada carpeta con `FBX/`, `Blend/` y `preview.png`):

- **`SourceArt/Weapons/`**: `SM_WPN_SWORD` (100 cm, X = punta; sockets `Grip`, `Fuse_Slot`), `SM_WPN_BOW` (120 cm de alto;
  `Grip`, `Arrow_Nock`), `SM_WPN_ARROW` (80 cm; `Fuse_Slot` en la punta).
- **`SourceArt/Fusion/`**: `SM_FUSE_FIRE/ICE/ROCK/SHOCK/BOMB`, caja de 25 cm con **pivote abajo** y `SOCKET_Attach_Bottom`.
- **`SourceArt/World/Props/`** (pivote al centro de la base): `SM_PROP_CHEST`, `SM_TOWER_WATCH` (socket `Shoot` en el
  cristal), `SM_BASE_KEEP`, `SM_ISLAND_SMALL` (pivote en la superficie; sin UCX: usar *complex as simple*),
  `SM_PROP_RUIN_ARCH`, `SM_PROP_WINDMILL`, `SM_PROP_BRIDGE` (pivote en la tabla), `SM_PROP_TREE` (colisión solo en el
  tronco), `SM_PROP_ROCK`.

### Lote 3 (`gen_assets.py ... lot3`)

- **`SourceArt/Characters/`** (accesorios del maniquí, socket `Head` en la base): `SM_ACC_BEANIE` (jugador),
  `SM_ACC_GOGGLE_HELMET` (bots héroe), `SM_ACC_JUNK_HELMET` (enemigos de campamento). Sin colisión.
- **`SourceArt/Pickups/`**: `SM_PROJ_CANNONBALL` (20 cm, el proyectil que hoy es una esfera en `JamBuildPart.cpp`),
  `SM_PICKUP_PART` (caja de pieza), `SM_PICKUP_MATERIAL` (bolsa de material).
- **`SourceArt/World/Props/`**: `SM_ISLAND_MEDIUM` (30 m, pivote en la superficie, sin UCX), `SM_PROP_FENCE` (tileable 2 m),
  `SM_PROP_BARREL`, `SM_PROP_CRATE`, `SM_PROP_SCRAP_PILE`, `SM_PROP_BUSH` (sin colisión), `SM_PROP_FLAG` (bandera de equipo),
  `SM_PROP_RAMP` (rampa de salto 4 m, sin UCX: *complex as simple*), `SM_PROP_BRAZIER`, `SM_PROP_LANE_SIGN` (flecha a +X).

### Lote 5 (`gen_assets.py ... lot5`) e iconos

- **Cofre que se abre:** `SM_PROP_CHEST_BASE` (socket `Lid_Hinge`) + `SM_PROP_CHEST_LID` con **pivote en la bisagra
  trasera** (gira sobre X para abrir). El `SM_PROP_CHEST` de una pieza sigue disponible para cofres que no se abren.
- `SM_PROP_SPAWN_PAD` (plataforma de reaparición 6 m, socket `Spawn`; anillo en color de equipo), `SM_PROP_WATERFALL`
  (cascada 15 m para orillas, pivote arriba), `SM_PROP_FLOATING_ROCK`.
- `Pickups/SM_PROJ_BOLT`: proyectil de cristal que dispara la atalaya (+X).
- **`SourceArt/FX/`**: `SM_FX_SHARD_WOOD` y `SM_FX_SHARD_STONE` para destrucción (Niagara mesh renderer / Chaos).
- **`SourceArt/Icons/`**: 46 iconos de inventario (ver su README).

### Lote 6 (`gen_assets.py ... lot6`): kit para vestir el mapa (`SourceArt/World/Props/`)

- **Vegetación:** `SM_PROP_TREE_ROUND`, `SM_PROP_TREE_TALL`, `SM_PROP_TREE_DEAD` (colisión solo en tronco), `SM_PROP_GRASS_TUFT`,
  `SM_PROP_FLOWERS`, `SM_PROP_MUSHROOM` (sin colisión: úsalos con Foliage Tool).
- **Roca y ruinas:** `SM_PROP_ROCK_FLAT`, `SM_PROP_ROCK_SPIRE`, `SM_PROP_RUIN_PILLAR`, `SM_PROP_RUIN_WALL`, `SM_PROP_RUIN_STAIRS`.
- **Madera:** `SM_PROP_PLATFORM` (4 m, caminable), `SM_PROP_STAIRS_WOOD`, `SM_PROP_LADDER`, `SM_PROP_SCAFFOLD`.
- **Campamento:** `SM_PROP_TENT`, `SM_PROP_CAMPFIRE` (socket `Fx_Fire` para el Niagara), `SM_PROP_WORKBENCH` (mesa de
  construcción), `SM_PROP_WEAPON_RACK`, `SM_PROP_SACKS`.
- **Carril:** `SM_PROP_BARRICADE` (franja de equipo), `SM_PROP_LANTERN_POST` (socket `Light` para un Point Light).
- **Islas:** `SM_ISLAND_LARGE` (60 m, pivote en la superficie) y `SM_ISLAND_CLIFF_EDGE` (orilla para tapar bordes).

Lo que no trae UCX (`none`) se usa con *Use Complex Collision As Simple* si debe pisarse (rampas, escaleras, plataformas, islas).

La paleta tiene 16 colores (los 8 originales + piedra, naranja, chispa, blanco, azul, pasto, cobre y lona). Si se
regenera un lote, regenerar también el otro para que todos usen la misma `T_Part_Palette.png`.

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
