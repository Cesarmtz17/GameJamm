---
title: Prompts y especificación de assets (para Emilio)
date: 2026-11-14
tags:
  - assets
  - prompts
  - gamejamm
status: listo
---

# Prompts y especificación de assets

> [!info] Para quién es
> Para **Emilio** (generación de assets con IA). Aquí está **qué hace cada asset en el juego, cuánto debe medir, cómo va
> orientado y el prompt para generarlo**. El estilo visual está en [ART.md](../ART.md). Las reglas técnicas generales
> (colisión `UCX_`, nombres `SM_/MI_`, ≤5k tris) están en [CATALOGO-ELEMENTOS.md](CATALOGO-ELEMENTOS.md) §0, y el contrato
> receta↔pieza en §0.1.

## 1. Cómo funcionan las piezas en el juego (lee esto antes de generar)

Cuando el jugador pide una construcción, el código **no mira el modelo 3D para decidir dónde va cada pieza**: trata cada
pieza como una **caja de tamaño fijo** (`Size`) y la pega a la cara de su padre. Después **escala tu mesh para que llene
esa caja**. De ahí salen tres reglas:

1. **Proporciones = las de la tabla.** El código escala el mesh a su caja, así que el tamaño absoluto puede variar un poco, pero las
   **proporciones** no. Si la tabla es 200 × 60 × 10 y tu modelo es casi un cubo, va a salir aplastado.
2. **Pivote en el centro de la caja** (no en la base). El código asume que el centro del mesh es el centro de la pieza.
3. **Orientación del vehículo:** **X = adelante, Y = derecha, Z = arriba** (como en UE). Modela cada pieza **tal como se ve
   montada en un carrito que mira hacia +X**. Las ruedas llevan el eje a lo largo de **Y** y los cohetes y cañones apuntan a lo largo de **X**.

> [!warning] Aviso para rol 3 (al cambiar los BasicShapes por estos meshes)
> En `JamBuildPart.cpp`, cada pieza con mesh de arte va con `Axis = 0`: el mesh ya viene orientado en el marco del
> vehículo. `Axis = 1/2` existe solo para rotar los cilindros y conos de `/Engine/BasicShapes` (que miran a +Z).

Además, **todo lo que se mueve o brilla es código o VFX, no el mesh**: los ventiladores giran por código o material
(entrega las aspas como mesh aparte si puedes), el fuego es Niagara y la rueda gira por física.

## 2. Flujo de generación (para que todo se vea del mismo juego)

1. **Primero una hoja de estilo** (imagen 2D): genera **una sola imagen** con 4–6 props juntos usando el prefijo de estilo. Elige la
   mejor y úsala como **imagen de referencia** para todo lo demás (image-to-3D o "style reference" en Meshy/Tripo/Rodin).
2. **Image-to-3D mejor que text-to-3D:** genera la imagen del asset con el prompt de abajo y la referencia de estilo, y luego
   pásala a 3D. Así sale más consistente.
3. **Limpieza en Blender** (≈10 min por pieza): escala y proporciones según la tabla, pivote al centro, orientación (+X adelante),
   reducir a ≤5k tris, colisión `UCX_<id>_00` (una caja o un convex), sockets `Attach_*`.
4. **Importar** a `Content/Game/Building/Parts/` (piezas) o `Content/Game/Art/` (mundo), y luego `MI_<id>` de `M_Part_Master`.
5. **Anotar la herramienta** usada en [SUBMISSION.md](../SUBMISSION.md), porque cuenta para los premios de partners.

**Prefijo de estilo** (al inicio de todos los prompts):

```
stylized low-poly game prop, handcrafted from wood planks, iron brackets, bolts and rope,
chunky beveled shapes, toy-like proportions, matte finish, single object, centered, neutral grey background,
three-quarter view
```

**Negativo:**

```
realistic, photoreal, glossy metal, chrome, sci-fi panels, text, letters, logo, brand, face, character, multiple objects,
scene, ground plane, shadows baked
```

## 3. Piezas de construcción (las 12 que ya existen en el código)

`Size` en cm, X × Y × Z en el marco del vehículo. La columna **"Debe leerse como"** dice qué tiene que reconocer el jugador a 20 m.

### Imprescindibles (MVP)

| Receta | Asset | Size (X×Y×Z) | Qué hace en el juego | Debe leerse como | Prompt (después del prefijo) |
|---|---|---|---|---|---|
| `Plank` | `SM_PART_PLANK` | 200 × 60 × 10 | Chasis base de casi todo (carro, volador) | Tabla larga | `long flat wooden plank, visible wood grain, two iron bolts at each end, slightly worn edges` |
| `Block` | `SM_PART_BLOCK` | 60 × 60 × 60 | Base de torretas y pieza comodín | Caja maciza | `solid wooden crate cube, iron corner brackets, plain sides` |
| `Cylinder` | `SM_PART_CYLINDER` | 150 × 50 × 50 (largo en X) | Cuerpo del cohete y ejes | Tubo grueso | `thick wooden barrel-like tube lying horizontally, iron bands near both ends` |
| `Wheel` | `SM_DEV_WHEEL_S` | 50 × 20 × 50 (eje en Y) | Rueda con motor: gira al activarse | Rueda | `small wooden wheel seen from the side, iron rim, round hub with four bolts` |
| `Fan` | `SM_DEV_FAN` | 15 × 50 × 50 (disco en YZ) | Empuja: en Back = turbo, en Bottom = vuela | Ventilador / hélice | `wooden propeller fan, three oversized wooden blades inside a round iron-banded frame, front view` |
| `Seat` | `SM_CTRL_SEAT` | 40 × 40 × 30 | Asiento del piloto (montura) | Asiento | `simple wooden pilot seat with a padded leather cushion and short backrest, iron brackets` |
| `Battery` | `SM_PWR_BATTERY_S` | 20 × 20 × 20 | Energía extra (se gasta primero) | Batería | `chunky battery box with iron frame, painted yellow lightning bolt, two terminals on top` |
| `Cannon` | `SM_DEV_CANNON` | 70 × 20 × 20 (boca hacia +X) | Dispara un proyectil cada 1.5 s | Cañón | `squat iron cannon, barrel pointing forward, oversized muzzle ring, small wooden base` |
| `Flamethrower` | `SM_DEV_FLAME` | 40 × 20 × 20 (boquilla hacia +X) | Lanza fuego hacia afuera de su cara | Lanzallamas | `handmade flamethrower, short iron nozzle pointing forward, red fuel canister strapped on top` |

### Alta prioridad

| Receta | Asset | Size | Qué hace | Debe leerse como | Prompt |
|---|---|---|---|---|---|
| `BigWheel` | `SM_DEV_WHEEL_L` | 80 × 30 × 80 (eje en Y) | Rueda grande, más tracción y menos velocidad | Rueda todoterreno | `large chunky wooden wheel, thick iron rim with blocky treads, big hub, side view` |
| `Rocket` | `SM_DEV_ROCKET` | 60 × 20 × 20 (tobera hacia −X) | Empuje fuerte 3 s y se apaga | Cohete | `handmade rocket lying horizontally, riveted metal body with painted red stripes, three small fins, nozzle at the back` |
| `Balloon` | `SM_DEV_BALLOON` | 90 × 90 × 90 | Sube la construcción | Globo | `round balloon made of patched canvas, ropes hanging to a small wooden ring at the bottom` |

**Sockets** en cada pieza (Socket Manager): `Attach_Top`, `Attach_Bottom`, `Attach_Front`, `Attach_Back`, `Attach_Left`,
`Attach_Right`, en el centro de cada cara de la caja y con X saliendo de la superficie. Hoy el código coloca por caja, pero los
sockets los va a usar después. Ruedas: solo `Attach_Left`/`Attach_Right` (el eje).

**Estados por material** (en `MI_<id>`, vía `Emissive` de `M_Part_Master`, ver [ART.md](../ART.md) §4): normal, **activo** (brilla
ámbar `#E7B65A`), sin energía (apagado) y por expirar (parpadeo). No hace falta un mesh por estado.

## 4. Materiales de fusión (se pegan al arma)

En el juego: el jugador junta un material con la espada o la flecha. Tienen que verse **pegados encima del arma**.
Tamaño: caben en una caja de **25 × 25 × 25 cm**, con pivote en la cara que toca el arma (`Attach_Bottom`).

| ID | Item de inventario | Efecto | Prompt | VFX (Niagara) |
|---|---|---|---|---|
| `FUSE_FIRE` | `Mat.Fire` | Quema (daño en el tiempo) | `small fire canister with painted orange flames, leather strap to tie it onto a weapon` | Llama gruesa naranja `#FF7A1A` + humo negro |
| `FUSE_ICE` | `Mat.Ice` | Ralentiza | `chunky faceted ice crystal, pale cyan` | Niebla de hielo seco `#8FE3FF` |
| `FUSE_ROCK` | `Mat.Rock` | Más daño y knockback | `heavy grey boulder, rough chunky shape` | Polvo al golpear |
| `FUSE_SHOCK` | *(alta)* | Electrocuta | `small tesla coil gadget, copper coil on a small wooden base` | Chispas amarillas en zigzag `#F5F06A` |
| `FUSE_BOMB` | *(alta)* | Explota al golpear | `round cartoon bomb, black iron shell, short rope fuse` | Destello blanco + humo redondo + astillas |

## 5. Armas del jugador

| Asset | Tamaño | Prompt | Notas |
|---|---|---|---|
| `SM_WPN_SWORD` | ~100 cm de largo | `simple adventurer sword, broad steel blade, leather wrapped grip, chunky crossguard` | Socket `Fuse_Slot` cerca de la punta (ahí se pega el material) |
| `SM_WPN_BOW` | ~120 cm de alto | `simple wooden longbow, rope string, leather wrapped grip` | La flecha va aparte |
| `SM_WPN_ARROW` | ~80 cm | `arrow with wooden shaft, feather fletching, iron tip` | Socket `Fuse_Slot` en la punta |

## 6. Mundo (rol 5; útil para Emilio si le sobra tiempo)

| Asset | Qué es en el juego | Tamaño aprox. | Prompt |
|---|---|---|---|
| `SM_TOWER_WATCH` | Torre: dispara a enemigos cercanos | 3 × 3 × 8 m | `tall wooden watchtower on stilts, team banner, glowing crystal on top, small rocky base` |
| `SM_BASE_KEEP` | Base: si se destruye, pierdes | 5 × 5 × 8 m | `chunky stone and wood castle keep, large team banner hanging on the front, toy proportions` |
| `SM_PROP_CHEST` | Cofre (loot) | 120 × 70 × 70 cm | `wooden treasure chest with iron corners, yellow and black striped band` |
| `SM_ISLAND_SMALL` | Isla chica (torres, loot) | 15 × 15 × 10 m | `small floating rock island, flat grassy top, jagged rock underside tapering to a point` |
| `SM_PROP_RUIN_ARCH` | Ruina para islas de loot | 6 × 1.5 × 5 m | `broken stone archway ruin, mossy sandstone blocks` |
| `SM_PROP_WINDMILL` | Detalle de isla | 4 × 4 × 9 m | `small wooden windmill with four cloth sails` |
| `SM_PROP_BRIDGE` | Puente entre islas (tileable) | 4 × 2 × 1 m | `rope and wooden plank bridge segment, tileable` |
| `SM_ACC_BEANIE` | Gorro del jugador | — | `simple knit beanie, plain, matte fabric` (color por material de equipo) |
| `SM_ACC_GOGGLE_HELMET` | Casco de los bots héroe | — | `simple round leather helmet with goggles, matte` |

Las islas grandes de campo abierto se arman en el editor con terreno o con varias `SM_ISLAND_*` escaladas, no como un solo mesh.

## 7. Iconos (256×256 para el inventario y el menú)

Prompt base: `game inventory icon of <objeto>, handcrafted wood and iron style, thick dark outline, centered,
flat warm lighting, transparent background`. Uno por cada pieza y material de las tablas 3 y 4. Nombre: `T_Icon_<id>`.

## 8. Orden de producción sugerido (14 nov)

1. `Plank`, `Wheel`, `Seat`: con estos ya sale el carro de la demo (arquetipo `Car`).
2. `Fan`, `Cannon`, `Flamethrower`, `Block`, `Battery`: completan los arquetipos Volador y Torreta y la "Rueda Infernal".
3. `FUSE_FIRE`, `FUSE_ICE`, `FUSE_ROCK` y la espada: la fusión del MVP.
4. Torre, base, cofre e isla chica: sin estos el mapa es BasicShapes.
5. Lo demás si da tiempo.

Checklist de entrega por pieza: [CATALOGO-ELEMENTOS.md](CATALOGO-ELEMENTOS.md) §10.
