---
title: 12 — Modelos de decisión (Jev / Decisions API) para generar construcciones
date: 2026-11-13
tags:
  - research
  - decision-models
  - ia
  - construccion
status: borrador
---

# 12 — Modelos de decisión (Jev / Decisions API) para generar construcciones

> [!info]
> Cómo funcionan los **modelos de decisión** (Jev de TypeSafe AI y la Decisions API de OpenAI) y
> cómo usarlos para que el lenguaje natural elija y combine piezas del catálogo
> ([CATALOGO-ELEMENTOS.md](../assets/CATALOGO-ELEMENTOS.md)) con las mecánicas de construcción de la
> referencia ([01](01-mecanicas-construccion.md), [03](03-mecanicas-vehiculos-dispositivos.md)).
> Etiquetas: **[doc]** = publicado en la fuente · **[infer]** = inferencia mía · **[no pub]** = no publicado.

> [!tip] Veredicto central
> Un modelo de decisión **no escribe texto: elige entre respuestas que tú defines**. Para un juego de
> construcción eso encaja: el catálogo de piezas y sockets es el conjunto finito de respuestas, y el
> lenguaje natural solo decide *qué pieza va dónde*. La salida ya viene tipada, no hay JSON que
> parsear ni reintentos, y el código del juego conserva la autoridad (validación, límites, inventario).

## 1. Qué es un modelo de decisión

| Aspecto | Modelo de texto libre (LLM) | Modelo de decisión |
|---|---|---|
| Salida | Texto que el código parsea y valida | Una opción del conjunto definido de antemano |
| Fallos típicos | JSON roto, campos inventados, reintentos | Casi ninguno de forma; puede elegir mal, pero siempre una opción válida |
| Latencia | ~1–2 s | Cientos de ms **[doc]** (Jev P50 0.21 s, P95 0.34 s en OpenRouter; OpenAI declara ~150 ms, afirmación del fabricante) |
| Qué decide | Todo (también la forma) | Solo el juicio; la política queda en tu código |

## 2. Jev (TypeSafe AI) — lo publicado

- Disponible de forma general; **no genera texto** **[doc]**.
- **Tipos de pregunta** **[doc]**: *Choice* (devuelve la opción elegida, una probabilidad por opción y una confianza), *Noul* (probabilidad de que una condición sea cierta) y *Score* (posición ponderada sobre una rúbrica ordenada, con la distribución completa). Se pueden mezclar en una llamada.
- **Endpoint** **[doc]**: `POST https://openrouter.ai/api/alpha/decisions` (vía OpenRouter). Entrada solo de texto (cadena, objeto JSON o arreglo). Contexto 32K en OpenRouter y 64K según TypeSafe.
- **Precio** **[doc]**: $0.042 por millón de tokens de entrada, $0 de salida.
- **Aviso** **[doc]**: las probabilidades de Jev cambian si cambia el orden de las respuestas → fijar el orden y guardar las probabilidades crudas.
- **[no pub]** en la fuente consultada: los nombres exactos de los campos de petición/respuesta. Hay que leer el quick start de TypeSafe / la doc de OpenRouter antes de implementar la llamada.

## 3. Decisions API de OpenAI — lo publicado

- Vista previa limitada para clientes seleccionados **[doc]**; el cliente define preguntas y respuestas finitas; entrada de texto **o imagen** (ventaja sobre Jev) **[doc]**.
- **[no pub]** (al 30 sep 2026): ruta del endpoint, autenticación, método del SDK, esquema de petición/respuesta, si devuelve probabilidades, límites de contexto y precio.
- Conclusión **[infer]**: para el jam **Jev es el único que se puede integrar hoy** con un esquema verificable; la Decisions API de OpenAI solo si el equipo ya tiene acceso.

## 4. Cómo se conecta con los assets y la construcción

Idea: el **vocabulario cerrado** del juego (piezas, sockets, materiales de fusión) *es* el conjunto de respuestas.

| Paso | Tipo de decisión | Opciones (conjunto finito) | Ejemplo con "un coche con ventilador y cañón" |
|---|---|---|---|
| 1. Arquetipo | Choice | `Car`, `Flyer`, `Turret`, `Rocket`, ... | `Car` |
| 2. Extras | Noul (uno por extra) | ¿lleva `Fan`? ¿`Cannon`? ¿`Flame`? | Fan = sí, Cannon = sí, Flame = no |
| 3. Colocación | Choice por extra | sockets válidos del padre (`Back`, `Top`, `Front`...) | Fan → `Back`, Cannon → `Top` |
| 4. Intensidad | Score | rúbrica (lento … rápido) | 0.7 → potencia/ruedas |
| 5. Validación | **código, no modelo** | límite de 21 piezas, sockets compatibles, inventario | rechaza o recorta |

- **"Generar cualquier cosa"** sale de la **combinatoria** (N piezas × sockets × activaciones) más la **física emergente** de la referencia (todo son cuerpos rígidos y constraints; no hay programa de vehículo). El modelo no inventa assets nuevos: compone los existentes. **[infer]**
- Para ideas realmente nuevas (fuera del vocabulario) hace falta un LLM de texto libre que proponga una pieza nueva; es lento y queda como ruta secundaria (REUSE → COMPOSE → CODE).
- Imagen como entrada (Decisions API de OpenAI): permitiría "construye esto" desde un dibujo **[infer]**, no verificable hoy.

## 5. Mecánicas de la referencia que el modelo debe respetar

| Regla de la referencia | Cómo la aplica el sistema |
|---|---|
| Sockets `Attach_*` y pegado = soldadura | El modelo elige socket; el ensamblador crea el constraint |
| Límite de piezas por construcción (21) | Lo impone el validador, no el modelo |
| El asiento enciende todo lo conectado | `Activate` lo calcula el código |
| Consumo y baterías | Fase posterior; el modelo solo propone, el código cobra energía |
| Dispositivos con thrust (ventilador, cohete) y consumo | Valores en la tabla de piezas, no en el prompt |

## 6. Estado del POC (rol Construcción)

- Implementado en `Source/GameJamm/Building/`: catálogo de piezas, ensamblador (receta → piezas físicas + constraints) y `AJamBuildSpawner`.
- `UJamBuildGenerator::ChooseArchetype` es el **punto único** que se reemplaza por la llamada a Jev. Hoy usa palabras clave.
- Probado en el editor con el MCP: el prompt "un coche con ventilador y cañón" produjo 8 piezas con cuerpo, 4 ruedas, asiento, ventilador y cañón, con física activa y proyectiles.
- `AJamBuildSpawner::RecipeJsonOverride` permite inyectar una receta JSON externa (salida de Jev o de un LLM) sin recompilar.

## → Aplicable a GameJamm

1. **Integrar Jev** reemplazando `ChooseArchetype` y los extras por llamadas Choice/Noul/Score. Necesita la **API key** (OpenRouter) y confirmar el esquema real de la petición **antes** de codificar.
2. **Fijar el orden** de las opciones y guardar las probabilidades crudas en el log (el orden las altera).
3. **Mantener la validación en código** (21 piezas, sockets, inventario). El modelo nunca decide límites.
4. **Latencia:** cabe en una llamada por construcción (~0.2–0.4 s por decisión); agrupar preguntas en una sola llamada cuando se pueda.
5. **Fallback offline** (el generador por palabras clave) siempre activo si falla la red o no hay clave.
6. La clave va en `Config/DefaultSecrets.ini` (ignorado por git), nunca en el repo.

## Fuentes

- [OpenAI's Decisions API vs Jev](https://www.firecrawl.dev/blog/openai-decisions-api-vs-jev) (comparativa técnica, consultada).
- Cobertura en prensa técnica sobre Decisions API y Jev (The New Stack y otros): citadas en el buscador, **no pude abrirlas** (403); lo que dependa solo de ellas está marcado [no pub] o [infer].
