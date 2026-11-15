# Arte con Tripo — nota para los agentes del equipo

> [!info] Para quién es
> Para los agentes de Construcción (Jesús), Core/IA (César) y Arte (Emilio). Resume qué arte se reemplazó con Tripo,
> qué **no** hay que pisar y qué cambios de código pide el arte nuevo. Dueño: Javier (ver [TEAM.md](TEAM.md)).
> Pipeline y registro de créditos: [SourceArt/Tripo/README.md](../SourceArt/Tripo/README.md).

## Acuerdo del equipo (14 nov)

Javier puede reemplazar **arte** de cualquier carpeta sin pedir permiso, dejando constancia aquí. El **código** sigue
siendo de su dueño: lo que el arte necesita de código se pide en la sección "Pendiente" de abajo.

## Qué cambió

- **Las 35 piezas de construcción** de `Content/Game/Building/Parts/SM_*` (todas las de `RealMeshNames()` en
  `JamBuildPart.cpp`) ahora son modelos de Tripo. Misma ruta y nombre, así que el código no cambió.
- Cada malla cumple el contrato de `AJamBuildPart::InitFromDef`: caja **exactamente igual a `Size`**, pivote al centro,
  X = adelante, Y = derecha, Z = arriba, ruedas con eje en Y, sockets `Attach_*` / `Fx_Out` y **un solo slot de material**.
- Materiales y texturas: `Content/Game/Art/Tripo/` (`M_Tripo_Master` + `MI_<id>` + `T_<id>_D/_N/_MR`, 1K o 512).
  `M_Tripo_Master` tiene el escalar `Emissive` que usa `SetGlow`: al encender, brilla la textura teñida de cálido.
- Comparación antes/después: `SourceArt/Tripo/previews/_comparacion_vehiculo.png`.

## Mundo (14 nov, Rodin)

- **Torres y bases del juego:** los 6 actores `ATeamStructure` de `L_Main` ahora usan
  `Content/Game/Art/Tripo/Structures/SM_STRUCT_{TOWER|KEEP}_{RED|BLUE}` (pivote abajo al centro, mismo tamaño que
  `SM_TOWER_WATCH`/`SM_BASE_KEEP`) y ya **no** llevan `MI_Structure_Red/Blue` forzado: la textura del equipo viene en la malla.
  Rojo mira a +X y azul a −X. Script: `SourceArt/Tripo/scripts/ue_assign_structures.py`. Posición, escala y vida no cambiaron.
- **`MapDressed`:** banderas, barricadas y plataformas de spawn de cada equipo reemplazadas **en su lugar** (misma caja y
  posición del mundo; `batch_world.json`). También se reemplazaron `SM_MAP_Dressed_Tower*` y `*Base` aunque hoy ningún actor
  de `L_Main` las usa.
- Los archivos `.uasset` de actores de `L_Main` pueden quedar en **solo lectura** en copias viejas (eran bloqueables): si el
  editor no guarda, quitar el atributo de solo lectura a esos archivos.

## Islas (14 nov, PBRgen) — PROVISIONAL

> Look todavía en evaluación (Javier): se puede volver al plano con `DetailStrength = 0` o con `git checkout` de los
> `M_Map_XX` previos a este commit.

- `M_Map_00` (cima de islas), `M_Map_02` (pradera y cerros), `M_Map_01` (roca de abajo), `M_Map_04` (tablas de puente) y
  `M_Map_05` (caminos y cuerdas) de `Content/Game/Core/World/MapDressed/` se reconstruyeron **en su lugar** (los mismos
  ~170 actores de `L_Main` los siguen usando): `Color` conserva su nombre y valor (ancla de paleta de `docs/ART.md`) y se le
  suma detalle pintado de PBRgen proyectado en el mundo (las islas no tienen UVs fiables): desde arriba en pasto, tablas y
  caminos (en caras verticales el detalle se apaga), triplanar en roca. Texturas en `Content/Game/Art/Tripo/Textures/World/`.
- Perillas en cada material: `DetailStrength` (**0 = el look plano de antes**), `TileSize` (cm) y `Roughness`.
- Anti-repetición en pasto (`M_Map_00`, `M_Map_02`) y tierra (`M_Map_05`): segunda lectura rotada 37° a otra escala
  (`BigScale`, `BigStrength`), manchas grandes de ruido (`MacroStrength`), pasto seco (`DryTint`, `HueVariation`) y menos
  detalle a lo lejos (`FarStart`, `FarRange`, `FarDetail`). Costo: ~4 lecturas de textura más por píxel (sin medir FPS aún).
- Script: `SourceArt/Tripo/scripts/ue_world_mats.py` (datos arriba del archivo). `M_Map_03` (agua) y `M_Map_06` (molino)
  no se tocaron. Si alguien regenera `MapDressed` con `gen_map_dressed.py` y reimporta los materiales, volver a correr el script.

## Matas y flores 3D (14 nov, Rodin) — PROVISIONAL

- 4 mallas en `Content/Game/Art/Tripo/Foliage/` (`SM_FOL_GRASS_A/B`, `SM_FOL_FLOWERS_A/B`, 290–390 tris, pivote abajo, sin
  colisión) regadas sobre el pasto (`M_Map_00`/`M_Map_02`) con un solo actor nuevo en `L_Main`: **`FOL_Scatter`** (carpeta
  `Dressing/Foliage`), un HISM por malla, ~1,560 instancias, **sin colisión** (no frenan vehículos ni bots), sin sombra, corte
  a 45–65 m y siempre cargado.
- Script re-ejecutable: `SourceArt/Tripo/scripts/ue_scatter_foliage.py` (borra y vuelve a regar con la misma semilla; solo
  coloca donde el rayo pega en el pasto, así quedan fuera caminos, árboles y bases). Densidad: `CLUSTER_M2`.
- Si algo estorba (p. ej. tapa una mecánica), se puede borrar el actor `FOL_Scatter` sin tocar nada más.

## Props del mapa (14 nov, Rodin) — PROVISIONAL

- **128 mallas de `MapDressed` reemplazadas en su lugar** (mismo nombre y ruta; los actores de `L_Main` no cambiaron): 13
  árboles, 33 matas de conos (ahora arbustos), 8 hongos, 13 flores, 16 rocas, 6 faroles, 2 agujas, 2 letreros de base, 2
  tiendas (roja y azul), los 4 campamentos (arco, pilar, barril, cofre + tapa, caja, fogata, chatarra) y el molino.
- 25 modelos compartidos (`SourceArt/Tripo/raw/SM_PROP_*`) con MI y texturas compartidas
  (`Content/Game/Art/Tripo/Materials/MI_PROP_*`, `Textures/T_PROP_*`). Las flores reusan `MI_FOL_FLOWERS_A/B` de las matas.
- **Misma base y casi la misma caja:** cada modelo conserva la altura de la malla vieja y sus propias proporciones
  (`"fit": "height"`), así que el ancho puede variar unos cm. Colisión compleja `BlockAll` igual que antes.
- **Cofres:** el cuerpo ahora toma la huella de su tapa (antes estaba girado respecto a ella); cuerpo y tapa salen del mismo
  modelo partido en dos (`"zclip"`).
- **Molino:** `SM_MAP_Dressed_Windmill_Base` es ahora el molino completo, con las aspas hacia el camino. Las aspas viejas
  (`Windmill_Sail1/2`) flotaban sobre la torre: sus 2 actores de `L_Main` quedan **ocultos y sin colisión, no borrados**.
- Costo: ~100k triángulos entre las 128 (antes 11k), sin LOD. Medido en la GTX 1650 (14 nov): 60 FPS sostenidos con la
  cámara en el spawn (GPU 11.5 ms, antes 9.8; p95 21.7 ms); en PIE la vista de los cerros cuesta ~24 % más GPU que el spawn,
  así que ahí queda poco margen. Si baja de 60: primero `CLUSTER_M2` de las matas y los `tris` del lote.
- Lote re-ejecutable: `SourceArt/Tripo/batch_props.json` → `normalize_batch.py -j 6` → importador. Si alguien regenera
  `MapDressed` con `gen_map_dressed.py`, hay que repetirlo.
- **Para el dueño del mapa (César):** arbustos, hongos y flores chocan (`BlockAll`), igual que los conos de antes. Si frenan
  vehículos o bots, quitarles la colisión es decisión del mapa.
- **Para César (loot):** los cofres de `Loot.json` (`chests`, ±8600/±3600 y ±4000/∓6000) caen justo en el cofre decorativo de
  cada campamento (`CampXX_Chest` + `ChestLid`); `GroundAt` choca con su tapa y el cofre de loot queda encima. Ya pasaba con
  el cofre viejo. Si se quiere uno solo, mover el de `Loot.json` o pedirme que oculte el decorativo.

## No pisar

- **No reimportar los FBX viejos** de `SourceArt/Building/FBX/` sobre `Content/Game/Building/Parts/`: reemplazaría el
  arte nuevo. Las fuentes nuevas están en `SourceArt/Tripo/FBX/` y se importan con `SourceArt/Tripo/scripts/ue_import_tripo.py`.
- Si cambias el `Size` de una pieza en `JamBuildPart.cpp`, avisa a Javier: la malla visual se dibuja con escala absoluta
  y tiene que medir exactamente `Size` (se regenera en segundos con `normalize_batch.py`).

## Pendiente de código (lo pide el arte)

### Construcción (Jesús): 10 piezas de solo datos sin malla

Las mallas ya están importadas en `Content/Game/Building/Parts/` con la caja de su `Size` actual. Para que se vean
basta agregar estas entradas a `RealMeshNames()` en `Source/GameJamm/Building/JamBuildPart.cpp`:

```cpp
{"Deck", "SM_PART_DECK"}, {"Rod", "SM_PART_ROD"}, {"UBlock", "SM_PART_UBLOCK"}, {"Cart", "SM_DEV_CART"},
{"RailSkid", "SM_DEV_RAILSKID"}, {"IceBlock", "SM_DEV_ICEBLOCK"}, {"Spikes", "SM_DEV_SPIKES"},
{"Launcher", "SM_DEV_LAUNCHER"}, {"WagonWheel", "SM_DEV_WHEEL_WAGON"}, {"Propeller", "SM_DEV_PROPELLER"},
```

`Spikes` lleva las púas hacia +X (cara frontal) y `Propeller` es un disco en YZ como `Fan`.

### Personajes (Construcción y Core/IA): por decidir

El jugador (`JamPlayConsole.cpp`, cuerpo provisional de cilindro) y los bots/minions usan formas básicas. Propuesta:
figuras estáticas de Tripo (jugador rojo/azul, bot, minion) en `Content/Game/Art/Tripo/Characters/`, cargadas si existen
y con el cilindro como respaldo. Falta acordarlo con Jesús y César antes de tocar código.
