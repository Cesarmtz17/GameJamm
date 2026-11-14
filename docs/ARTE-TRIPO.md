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
