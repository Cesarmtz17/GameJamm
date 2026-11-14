---
title: Research de mecánicas — índice
date: 2026-11-13
tags:
  - research
  - indice
  - gamejamm
status: listo
---

# Research de mecánicas del juego de referencia → GameJamm

> [!info]
> Investigación de mecánicas + ingeniería inversa de un **juego de referencia** (aventura
> de mundo abierto con física sandbox, 2023), dirigida a la implementación del GameJamm
> (UE 5.8): fusión de armas, construcción de artefactos y asistente IA.
> Integrado en `main` (2026-11-13).

**Deadline del jam: dom 15 nov 8:59 PM EST — feature freeze dom 15 12:00 CDMX.**
Este documento existe para *atajar decisiones de diseño*, no para leerse por placer.
Cada doc termina en una sección "→ aplicable a GameJamm" con lo accionable.

## Índice

| Doc | Contenido | Estado |
|---|---|---|
| [01-mecanicas-construccion.md](01-mecanicas-construccion.md) | Construcción: grab, attach points, pegamento, rotación, límite 21 piezas | ✅ |
| [02-mecanicas-fusion.md](02-mecanicas-fusion.md) | Fusión: fórmula de daño, tabla FuseAP, durabilidad +25, flechas | ✅ |
| [03-mecanicas-vehiculos-dispositivos.md](03-mecanicas-vehiculos-dispositivos.md) | 27 dispositivos, consumos de batería, habilidades de movimiento | ✅ |
| [04-quimica-fisica-gdc.md](04-quimica-fisica-gdc.md) | Chemistry engine, física GDC 2024, mundo sin kinematics | ✅ |
| [05-re-engine-interno.md](05-re-engine-interno.md) | KingSystem/uking/Phive/Havok, ksys::chm con unidades físicas | ✅ |
| [06-re-formatos-herramientas.md](06-re-formatos-herramientas.md) | BYML/AAMP/Phive/RESTBL, repos GitHub, data mining | ✅ |
| [07-guia-rea.md](07-guia-rea.md) | **REA (Reverse Engineer Anything)**: cómo usarlo sobre el binario | ✅ |
| [08-mapeo-ue5-impl.md](08-mapeo-ue5-impl.md) | Traducción mecánica→UE5 + implementación de referencia | ✅ |
| [09-auditoria-fuentes-publicas.md](09-auditoria-fuentes-publicas.md) | Qué documenta el mundo exterior (durabilidad MaxLife, huecos de reglas, plan de tests) | ✅ |
| [10-repo-open-source-fusion.md](10-repo-open-source-fusion.md) | ComponentFuseMechanic a fondo: bugs, licencias, qué adoptar/no | ✅ |
| [11-diseno-ensamblado-open-source.md](11-diseno-ensamblado-open-source.md) | Más repos open source + diseño del ensamblado (grafo, joints, JSON del LLM) | ✅ |
| [12-programacion-piezas-ue5.md](12-programacion-piezas-ue5.md) | **Cómo programarlo en UE5**: headers, DataAssets, snap, weld vs constraint, COM, fuerzas, ruedas, energía, separación, pruebas; contraste con el POC `c75b9c9` | 🟡 borrador |
| [13-conocimiento-ia-construccion.md](13-conocimiento-ia-construccion.md) | **Conocimiento para la IA**: capacidades por pieza, reglas por cara, plantillas, preguntas Jev desde el inventario, solver (sustituir/reducir/pedir materiales), Seat infinito, energía. Datos en `Content/Game/Building/Data/BuildKnowledge.json` | 🟡 borrador |
| [FORMATO.md](FORMATO.md) | **Convención de formato** de estos MD (frontmatter, callouts, reglas de contenido) | ✅ |
| [FUENTES.md](FUENTES.md) | Lista maestra de todas las fuentes | ✅ |

## TL;DR para el equipo (decisión rápida)

> [!tip] Resumen en 6 puntos
> 1. **La construcción se puede replicar** con: piezas con sockets "Attach*" + snap por
>    proximidad de superficies + **pegar = soldar** (la referencia: *"every single thing
>    without exception is built with dynamic rigid bodies and constraints"*, GDC 2024).
>    **Límite duro: 21 piezas** ("You can't attach any more"). Rotación en pasos de 45°.
>    Despegar = *wiggle* con umbral acumulado.
> 2. **La fusión se replica con una tabla**: `Attack = Base + FuseAP(material)`,
>    `Durabilidad += 25` (una vez por arma), efectos por material (fuego/hielo/rayo/bomba/
>    homing), ×2 de colmillos/garras de monstruo en flechas, arma↔arma permitida (aporta
>    attack base).
> 3. **Construcción de vehículos** = piezas con thrust + consumo (referencias: ventilador
>    0.156 pozos/seg, emisores 0.18, cañón 0.3) + batería global que recarga solo en reposo
>    + baterías desechables en piezas que drenan primero.
> 4. **El secreto técnico** (GDC 2024): mundo 100% dirigido por física (cero kinematics)
>    + interacciones *multiplicativas* sin implementación dedicada. Para 2 días de jam:
>    **fingir la emergencia** con reglas cruzadas en una tabla de interacciones.
> 5. **Implementación UE de referencia ya existe**:
>    [ComponentFuseMechanic](https://github.com/EvelynSchwab/ComponentFuseMechanic)
>    (doc 08 §7).
> 6. **Las reglas del builder (attach, límites, pesos) NO son datos públicos**: son
>    decisiones nuestras, calibradas con playtests (doc 09) y montadas sobre un grafo de
>    ensamblado propio (docs 10-11).

> [!warning] Aviso legal
> Esta investigación usa solo conocimiento público: charlas GDC, artículos de prensa técnica,
> wikis, papers y repos open source de herramientas de modding. No incluye ni enlaza ROMs,
> keys, emuladores ni instrucciones de extracción de una copia pirata. El análisis con REA
> (doc 07) está pensado **solo sobre una copia legalmente adquirida y para
> investigación/interoperabilidad**, sin distribuir assets extraídos.

> [!note] Nota de edición (2026-11-13)
> Esta carpeta se reescribió para no nombrar el juego fuente (repo público del jam): títulos,
> personajes, lugares, objetos y URLs del ecosistema se reemplazaron por términos neutrales
> ("el juego de referencia", "el predecesor"). Los valores, fórmulas y patrones son los de la
> investigación original. Los docs 09–11 se incorporaron de una pasada de research adicional
> del equipo (originalmente en `docs/research/`) y recibieron el mismo scrub: mismos datos,
> sin términos ni URLs del ecosistema. Cómo citar cada afirmación → [FUENTES.md](FUENTES.md).
