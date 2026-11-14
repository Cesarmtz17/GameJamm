---
title: 04 — Chemistry engine y física (GDC 2024)
date: 2026-11-13
tags:
  - research
  - fisica
  - quimica
  - gdc
status: listo
---

# 04 — Chemistry engine y física (GDC 2024 + hallazgos de la comunidad)

> [!info]
> Investigación deep-research con fuentes (2026-11-13). Charla fuente primaria:
> **GDC 2024 — "Evolving Physics and Sounds"**
> — Takuhiro Dohta (Technical Director), Takahiro Takayama (Lead Physics Programmer),
> Junya Osada (Lead Sound Engineer).
> [GDC Vault](https://gdcvault.com/play/1034667/) ·
> [Transcripción en YouTube](https://www.youtube.com/watch?v=N-dPDsLTrTE)

## 1. Multiplicative gameplay

- Dohta: dos pilares — "un mundo vasto y continuo sin cortes" y **"multiplicative
  gameplay"**: *"a game structure that lets players combine actions and objects to create
  many different ways to play"*.
- Origen (GDC 2017, el predecesor): *"All objects in the game are connected by same world
  rules such as physics and chemistry"* y el lema: **"rather than creating something fun,
  create a system that makes fun things happen"**.
- La semilla: un **globo del predecesor** (material que pegado a un objeto lo hacía flotar)
  → prototipo de "pegar objetos" → construcción y fusión. *(prensa técnica)*

## 2. Stack técnico de la física

- **Havok como base + capa in-house encima** ("on top of that, we have our in-house physics
  library"). *(transcripción GDC · prensa técnica)*
- **Mundo 100% physics-driven, sin excepción:** todo objeto tiene masa, momento de inercia
  y se controla con velocidad/aceleración. Los objetos **kinematic rompían la simulación**
  al chocar con la construcción (engranajes kinematic atravesaban cajas metálicas).
  Solución: **eliminar TODOS los kinematics**:
  - Puertas = slider constraint + motor
  - Engranajes = ruedas conectadas por constraint (transferencia de velocidad)
  - Cadenas = series de cápsulas rígidas unidas por ball-socket constraints
  - **La habilidad de rebobinar se convirtió a física** ("we also changed [it] to be
    physics driven")
  - Resultado: *"a world free from self-destruction"* + soluciones emergentes (una cámara
    de prueba cuyo interruptor se mantenía abierto solo por el nuevo comportamiento físico
    de la puerta). *(cobertura técnica)*
- **Sin implementación dedicada:** no hay código de "vehículo", ni de "puerta elevadiza de
  cadenas", ni de "bote de remos" — solo componentes (ruedas, tablones, cadenas,
  resistencia al agua) que combinados producen la interacción.
- **Cálculo de masa:** asignada una propiedad de material (madera, metal, piedra...), **masa
  e inercia se autocalculan del volumen del shape**; cuando el resultado no cuadra con la
  percepción (tablones visualmente gruesos), corrigen la masa a mano para que "se sienta"
  como el jugador espera. Las cámaras de prueba tienen un "testing room" automatizado para
  verificar física constantemente. *(transcripción GDC)*
- **Resistencia al agua:** área proyectada según la dirección de la velocidad (por eso girar
  una tabla cambia cómo "muerde" el agua).
- **Rueda del kit = 3 rigid bodies** (rueda, motor, eje): el motor transfiere torque; la
  fricción con el suelo genera tracción. Ruedas con suspensión añaden un constraint de rango
  + plano.

## 3. Reglas elementales documentadas (gameplay)

| Elemento | Reglas verificables |
|---|---|
| **Fuego** | quema madera y pasto; agua lo apaga (hidrante moja); emisor de fuego calienta globos (más calor = sube más rápido) |
| **Hielo** | emisor de hielo congela agua (plataformas) y enemigos; los objetos congelados reducen tamaño/peso al derretirse (cámara de pesar documentada en GDC) |
| **Electricidad** | emisor de rayo descarga desde su cuerno; **el metal conduce** (tormenta = peligro para el jugador); la familia acuática requiere estar mojado |
| **Agua** | resistencia por área proyectada; flotabilidad; hidrante produce agua útil |
| **Viento** | ventiladores (empuje), fronda vegetal (derriba), updraft de boca-abajo |

*(enciclopedia de la franquicia · portal de guías · transcripción GDC)*

- El "chemistry engine" del director técnico **rastrea el estado de cada objeto frente a 5
  elementos** (fuego, agua, hielo, viento, electricidad) y lo actualiza continuamente.
  *(análisis del predecesor en prensa · foro de programación de juegos)*

## 4. El interior del chem engine (RE, resumen — detalle en [05](05-re-engine-interno.md))

El sistema `ksys::chm` del predecesor (decompilado) usa **unidades físicas reales**:
`heat_capacity [J/gK]`, `thermal_conductivity [W/m·K]`, `electrical_resistivity [Ω·m]`,
`ignition_point [°C]`, `burn_speed`, `wetting_amount [kg/m²]`... Materiales definidos:
`iron, wood, grass, stone, ice, oil, lava, weapon_metal, terrain_grass_mesh...` Reglas de
mundo: `heat_transfer_speed_rate 24.0`, `lightning_distance 25-35 m`,
`electric_water_extra_radius_rate 500.0` (el agua "fuga" electricidad 5× más lejos),
`min_ice_size 0.15 m`...
*(repo de datos del predecesor convertidos a YAML · visor de código decompilado · wiki
técnica de modding)*

## 5. Hallazgos/explotación de la comunidad

- **Límite de 21 piezas** como frontera dura de todo build grande (trenes, puentes).
  *(enciclopedia de la franquicia)*
- **Autoregistro** (30 builds + 8 favoritos) y autoconstrucción con piezas de mineral
  desechable frágiles. *(portal de guías)*
- **Moto flotante** (2 ventiladores + asiento de dirección) como el build canónico barato.
  *(portal de guías)*
- **Olla portátil como articulación universal**: su ball-socket (diseñado para que la sopa
  no se derrame) se convirtió en el pivote flexible favorito de la comunidad.
  *(transcripción GDC)*
- **Torretas**: cabeza objetivo + emisor/cañón. **Escudo-cohete**: impulso vertical que
  drena energía. **Grind de escudo en rieles** (trenes). *(portal de guías)*
- **Counters anti-ingeniería de la desarrolladora:** expiración de dispositivos (destello
  verde), rugido de monstruo élite que rompe dispositivos cercanos, energía finita.
  *(enciclopedia de la franquicia)*
- Nota honesta: trucos muy citados ("bomba adosada a cabeza objetivo", newtons exactos de
  ventilador/cohete) **no aparecieron en fuentes verificables** en esta sesión.

## 6. Sonido físico (Osada)

- El audio del carro **emerge de los sonidos de ruedas, cadenas y creaking joints simulados
  por la misma física** — no hay "sonido de carreta grabado". El sistema acústico usa eco
  por espacios y absorción del aire. Se construyó un "physics engine for sound" como
  consecuencia, no como meta. *(transcripción GDC · prensa técnica)*

## → Aplicable a GameJamm

1. **Prohibido kinematic para gameplay**: puertas/plataformas/torres = cuerpos dinámicos +
   constraints (slider+motor). Chaos lo soporta todo.
2. **Fingir la emergencia con tabla de interacciones** (fuego quema madera, metal conduce,
   agua apaga): la sensación es lo que se evalúa.
3. Masa por densidad de material × volumen del shape; corregir a mano lo que "no se siente"
   bien.
4. La olla/ball-socket enseña el valor de exponer **un pivote flexible** en el kit de
   piezas — regalarla como pieza es diseño emergente barato.
5. Counters anti-spam: expiración de dispositivos + energía finita + destructibilidad
   (rugido).
6. Sonido: eventos de audio **desde el estado físico** (material del impacto, tamaño,
   velocidad) en vez de sonidos fijos — MetaSounds puede hacerlo si sobra tiempo.
