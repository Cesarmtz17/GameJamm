---
title: Encargo de assets para Emilio — todos los elementos de construcción
date: 2026-11-13
tags:
  - construccion
  - assets
  - handoff
status: borrador
---

# Encargo de assets para Emilio — todos los elementos de construcción

> [!info]
> El juego se basa en **construcción emergente**: el jugador pide en lenguaje natural y el modelo de decisión elige piezas básicas que el código ensambla. **Cada pieza que exista aquí tiene que tener su malla**, porque las mecánicas salen de las propiedades de las piezas. Hoy el juego usa formas básicas del motor como reemplazo.

**Estado:** 12 de 45 elementos con FBX en `SourceArt/Building/FBX/`; el resto falta. Lista armada con el mapeo de elementos de Construcción (contrastar con `Content/Game/Building/Data/BuildKnowledge.json` y el catálogo si algún dato no coincide).

## Reglas de entrega (las mismas de las 12 piezas ya subidas)

| Regla | Detalle |
|---|---|
| Medidas | Las de la columna *Tamaño* (cm: X adelante, Y lado, Z arriba), para que el ensamblador no las reescale |
| Sockets | Un `Attach_<cara>` por cara usable (`Top`, `Bottom`, `Front`, `Back`, `Left`, `Right`); `Fx_Out` donde sale fuego, bala o rayo |
| Colisión | Cajas o formas simples `UCX_` |
| Nombres | `SM_<GRUPO>_<NOMBRE>.fbx`, igual que las ya subidas |
| Contenido público | No nombrar el juego de referencia en archivos, comentarios ni commits |

## Elementos (ordenados por prioridad; 5 = imprescindible para el prototipo)

| Prio | Tipo | Grupo | Tamaño (cm) | Masa (kg) | Sockets por defecto | Estado |
|---|---|---|---|---|---|---|
| 5 | `Seat` | Control | 40×40×30 | 5 | Top | ✅ `SM_CTRL_SEAT.fbx` |
| 5 | `Stabilizer` | Sustentación | 30×30×60 | 4 | Top_Back | pendiente |
| 5 | `Wheel` | Movilidad | 50×20×50 | 4 | Left, Right | ✅ `SM_DEV_WHEEL_S.fbx` |
| 5 | `Fan` | Empuje | 15×50×50 | 3 | Back | ✅ `SM_DEV_FAN.fbx` |
| 5 | `Rocket` | Empuje | 60×20×20 | 4 | Back | ✅ `SM_DEV_ROCKET.fbx` |
| 5 | `Cannon` | Armas y efectos | 70×20×20 | 8 | Top | ✅ `SM_DEV_CANNON.fbx` |
| 5 | `Flamethrower` | Armas y efectos | 40×20×20 | 3 | Front | ✅ `SM_DEV_FLAME.fbx` |
| 4 | `Stake` | Control | 15×15×60 | 3 | Bottom | pendiente |
| 4 | `TargetHead` | Control | 40×40×40 | 5 | Top | pendiente |
| 4 | `Balloon` | Sustentación | 90×90×90 | 1 | Top | ✅ `SM_DEV_BALLOON.fbx` |
| 4 | `HoverStone` | Sustentación | 45×45×45 | 2 | Bottom_Front, Bottom_Back | pendiente |
| 4 | `Wing` | Sustentación | 80×160×6 | 3 | Left, Right | pendiente |
| 4 | `BigWheel` | Movilidad | 80×30×80 | 8 | — | ✅ `SM_DEV_WHEEL_L.fbx` |
| 4 | `Wall` | Estructura | 200×30×150 | 30 | Front | pendiente |
| 4 | `Beam` | Armas y efectos | 50×12×12 | 3 | Top_Back | pendiente |
| 4 | `Bomb` | Armas y efectos | 30×30×30 | 2 | Right_Top | pendiente |
| 4 | `Frost` | Armas y efectos | 40×25×25 | 3 | Top_Front | pendiente |
| 3 | `Light` | Efectos | 25×25×25 | 2 | Front_Top | pendiente |
| 3 | `Battery` | Energía | 20×20×20 | 3 | Top_Back | ✅ `SM_PWR_BATTERY_S.fbx` |
| 3 | `Motor` | Uniones | 40×40×30 | 5 | Top | pendiente |
| 3 | `Cart` | Movilidad | 120×80×30 | 12 | Bottom | pendiente |
| 3 | `Sled` | Movilidad | 160×70×15 | 6 | Bottom | pendiente |
| 3 | `Spring` | Movilidad | 30×30×40 | 2 | Bottom | pendiente |
| 3 | `Block` | Estructura | 60×60×60 | 15 | Top_Back | ✅ `SM_PART_BLOCK.fbx` |
| 3 | `Panel` | Estructura | 100×10×100 | 6 | Top_Front | pendiente |
| 3 | `Plank` | Estructura | 200×60×10 | 5 | Top_Back | ✅ `SM_PART_PLANK.fbx` |
| 3 | `Jet` | Empuje | 60×40×40 | 6 | Back_Top | pendiente |
| 3 | `Shock` | Armas y efectos | 40×25×25 | 3 | Top | pendiente |
| 2 | `Hydrant` | Efectos | 40×30×30 | 5 | Front | pendiente |
| 2 | `BigBattery` | Energía | 35×35×35 | 8 | Top_Back | pendiente |
| 2 | `Hinge` | Uniones | 20×60×20 | 3 | Front | pendiente |
| 2 | `Pivot` | Uniones | 30×30×30 | 3 | Back | pendiente |
| 2 | `HomingCart` | Movilidad | 100×70×30 | 10 | Bottom | pendiente |
| 2 | `Track` | Movilidad | 180×25×40 | 12 | Left, Right | pendiente |
| 2 | `Ball` | Estructura | 60×60×60 | 10 | Front | pendiente |
| 2 | `Cylinder` | Estructura | 150×50×50 | 10 | Top | ✅ `SM_PART_CYLINDER.fbx` |
| 2 | `Girder` | Estructura | 300×20×20 | 6 | Front | pendiente |
| 2 | `Ramp` | Estructura | 100×100×50 | 10 | Front | pendiente |
| 2 | `Sail` | Empuje | 10×120×100 | 2 | Top | pendiente |
| 2 | `Turbine` | Empuje | 25×90×90 | 6 | Back_Bottom | pendiente |
| 1 | `Mirror` | Efectos | 60×10×60 | 4 | Top | pendiente |
| 1 | `Rope` | Uniones | 200×8×8 | 2 | Back | pendiente |
| 1 | `Slider` | Uniones | 100×20×20 | 5 | Top | pendiente |
| 1 | `Arch` | Estructura | 200×30×150 | 12 | Top | pendiente |
| 1 | `Rod` | Estructura | 150×8×8 | 2 | Top_Back | pendiente |

## Notas

- Los tamaños y masas de las piezas que **ya están en el código** son los reales; los de las piezas nuevas son un punto de partida y se pueden ajustar si el modelo lo pide, avisando a Construcción.
- Las uniones (`Hinge`, `Pivot`, `Motor`, `Slider`, `Rope`) son piezas que conectan otras; para el prototipo basta una malla simple.
- La lista puede crecer: cualquier elemento nuevo que se agregue a `BuildKnowledge.json` entra aquí.
- Detalle de cada mecánica: [13-conocimiento-ia-construccion](../mechanics-research/13-conocimiento-ia-construccion.md) y [catálogo de elementos](../assets/CATALOGO-ELEMENTOS.md).
