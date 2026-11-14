# GDD — GameJamm (título por definir)

## ⚠️ REGLA PRINCIPAL (no negociable): construcción emergente con física (sandbox)
**La idea central del juego es un sistema donde el jugador arma estructuras combinando muchos elementos básicos (tablas/paredes, llantas, cilindros, bloques, rocas, motores, lanzallamas…) y las propiedades de esas piezas se traducen en mecánicas.** Lo que construyes determina lo que hace. Todo lo demás (tower push, loot, fusión, bots, arte) existe para alimentar este sistema.

Reglas derivadas, valen para todos los roles y para cualquier agente de IA:
1. **Las mecánicas salen de las propiedades de las piezas, no de objetos cerrados.** Una "rueda con lanzallamas" rueda y quema porque combina `Llanta` (rotación) con `Lanzallamas` (daño de fuego), no porque exista un objeto "Rueda Infernal" programado a mano.
2. **Catálogo grande de piezas, definido por datos:** [`Content/Game/Building/Data/BuildKnowledge.json`](../Content/Game/Building/Data/BuildKnowledge.json) (capacidades, caras válidas, masa, energía, sustitutos y sinónimos para la IA) más la tabla física de `UJamPartCatalog` en `Source/GameJamm/Building/JamBuildPart.cpp` (tamaño, masa, potencia; pasará a DataAssets). Cada pieza declara forma, masa, puntos de anclaje y *capacidades* (rotar, empujar, dañar, quemar, soportar peso, bloquear…). Diseño completo: [research 13](mechanics-research/13-conocimiento-ia-construccion.md).
3. **Capa de decisión:** un modelo recibe el pedido del jugador y su inventario y *decide* qué piezas usar y cómo ensamblarlas (receta JSON). El juego valida y construye. Ver [Capa de decisión](#capa-de-decisión).
4. **Siempre hay fallback** (recetas fijas) para que el juego nunca se bloquee.
5. **Prioridad de recorte:** cualquier feature que no sirva a esta regla se recorta primero. **Nadie del equipo, ni ningún agente, cambia este enfoque sin acuerdo de todos.**

### De propiedades a mecánicas (ejemplos)
| Propiedad de la pieza | Mecánica que produce |
|---|---|
| Rotación (llanta, cilindro) | Rodar, vehículos, rodillos que aplastan |
| Empuje (motor, cohete) | Movimiento autónomo, lanzamientos, saltos |
| Daño de contacto / fuego (lanzallamas) | Daño en área, quemadura |
| Masa y solidez (tabla, bloque, roca) | Muros, puentes, escudos, contrapesos |
| Anclaje (puntos de unión) | Qué piezas se pueden conectar y con qué orientación (constraints Chaos) |
| Fusión de material (piedra/fuego/hielo) | Modifica la pieza: más dura, incendiaria, ralentizadora |

## Pitch
PvP de dos equipos tipo *tower push*: avanza por el mapa destruyendo torres hasta llegar a la base enemiga y destruirla. Apareces **sin loot**; el mapa tiene **estructuras/campamentos** donde peleas y looteas materiales y piezas. Con ellos:
- **Fusionas armas** (piedra en la espada, fuego en la flecha…).
- **Construyes artefactos** con piezas (llantas, tablas, lanzallamas…) — p. ej. una llanta con lanzallamas que rueda por el mapa quemando todo.
- Un **asistente de IA** te ayuda a diseñar creaciones con las piezas que tienes.

## Tema: DIRECTOR'S CUT

Texto oficial del jam ([itch.io](https://itch.io/jam/2026-epic-ai-megajam)):

> *Your vision, your edits, your final cut. Every developer has that one feature that didn't quite make it: the mechanic you
> scoped out at 2 AM, or the idea you shelved and never found a place for. Now's your chance to finally build it. Using AI
> agents? You're directing those too. Set the vision, delegate tasks, and decide what makes the cut.*

El tema **no** es de cine: habla de **la función que siempre se recorta** y de **dirigir a tus agentes de IA**. El mundo no
lleva ficción de estudio de cine (una versión anterior de este doc la tenía por error y se retiró).

**Cómo lo cumplimos** (en las mecánicas, no en el decorado):
1. **La función que siempre se recorta:** construcción libre con física de artefactos improvisados dentro de un PvP. Es la
   típica mecánica que se saca del alcance a las 2 AM; aquí es el centro del juego. Contarlo así en el devlog y en la página.
2. **Diriges a tu agente:** el **asistente de IA** recibe tu visión en lenguaje natural ("algo que ruede y queme todo") y arma
   la creación con las piezas que tienes. Tú decides qué se queda.
3. **Tus ediciones, tu corte final:** puedes pedirle que edite tu última creación ("más rápida", "ahora con fuego"); la IA
   recibe la receta anterior junto con tu instrucción. *(P1: solo si el MVP del asistente ya funciona.)*
4. **Desarmar y reintentar:** si tu creación se atora o se rompe, la desarmas y **recuperas sus piezas** en el inventario para
   volver a intentarlo. Fomenta experimentar y es barato de implementar (destruir el build + devolver los items).

Título del juego: **por definir** (afecta solo a la UI y a la página de itch).

Dirección de arte, UI y audio: **[ART.md](ART.md)** (islas flotantes + campo abierto).

## Decisiones clave de alcance
1. **Modo principal: jugador vs bots** (aliados y enemigos IA). Los jueces juegan solos. Multijugador online = stretch goal.
2. **El asistente de IA nunca bloquea el juego**: si no hay red/API, usa recetas predefinidas.
3. Contenido mínimo y pulido > mucho contenido roto.

## MVP (tiene que existir el 15 nov a mediodía, antes del freeze)
- [ ] 1 mapa de **1 carril** (o 3 cortos): por equipo **2 torres + 1 base**.
- [ ] Personaje 3ra persona: moverse, atacar cuerpo a cuerpo, arco.
- [ ] Torres/base con vida, disparan a enemigos cercanos; victoria/derrota.
- [ ] Bots: minions que avanzan por el carril + 2–4 bots "héroe" por equipo (StateTree/Behavior Tree).
- [ ] **3–4 campamentos de loot** con enemigos y cofre.
- [ ] **Fusión**: 3 materiales × 2 armas. Ej.: Piedra (+daño/knockback), Fuego (DoT), Hielo (slow). Espada y flecha.
- [ ] **Construcción**: ~6 piezas con puntos de anclaje (Llanta, Tabla, Motor/Cohete, Lanzallamas, Roca, Asiento opcional). Snap + física (Chaos constraints). Activar/desactivar.
- [ ] **Asistente IA**: el jugador escribe un prompt → LLM devuelve receta JSON → el juego la arma con las piezas del inventario. Fallback a recetas fijas.
- [ ] HUD: vida, inventario de materiales/piezas, estado de torres. Menú inicio + pantalla final.
- [ ] Audio: música + SFX básicos (golpes, fuego, destrucción de torre).

## Stretch (solo si el MVP está listo)
- Multijugador LAN/listen server.
- Más piezas/materiales, vehículos manejables.
- Pantalla de repetición de la mejor jugada.

## Arquitectura técnica (C++ base + BP/DataAssets)
| Sistema | Clases C++ (base) | Contenido BP/Data |
|---|---|---|
| Partida | `AJamGameMode`, `AJamGameState`, `ATeamStructure` (torre/base) | BP de torres, reglas en DataAsset |
| Combate | `UHealthComponent`, `AJamWeapon`, `UFusionComponent` | `UFusionMaterialData` (DataAsset por material) |
| Construcción | `AJamBuildPart`, `UJamPartCatalog`, `UJamAssembler` (receta → piezas + constraints), `AJamBuildSpawner` | `BuildKnowledge.json`; tabla de piezas en C++ (pasará a DataAssets) |
| IA bots | `AJamAIController`, StateTree tasks | StateTrees, carriles (spline) |
| Asistente / decisión | `FJamDecisionClient` (Jev → OpenRouter → fal), `UJamBuildGenerator` (decisiones → receta); interfaz `IBuildDecisionProvider` por hacer | Arquetipos y extras en `JamAssembler.cpp`; `BuildKnowledge.json` |
| Loot | `ALootCamp`, `ALootChest` | Tablas de loot |

### Capa de decisión
La decisión de *qué construir y con qué piezas* va detrás de una interfaz única (`IBuildDecisionProvider`, **por hacer**, junto al cliente actual en `Source/GameJamm/Building/`): entra `{pedido, inventario, contexto}` y sale una receta JSON (formato de abajo, sin cambios). Así el modelo se puede cambiar sin tocar construcción ni combate.

| Proveedor | Estado | Notas |
|---|---|---|
| **Jev** (modelo de decisión de TypeSafe; respaldo vía OpenRouter y fal) | **MVP, ya en código** (`FJamDecisionClient`) | Elige forma, piezas y orientación entre opciones generadas **desde el inventario**; el código arma la receta (solver: sustituye, reduce y pide materiales). Ver [research 13](mechanics-research/13-conocimiento-ia-construccion.md) §3 |
| LLM que escribe el JSON (p. ej. Claude `claude-haiku-5-5`) | Alternativa | Más libre y más lento; el parser ya repara JSON mal escrito (rama `build/recipe-hardening-v2`). Entra por la misma interfaz si Jev se queda corto |
| Palabras clave + recetas fijas | **Fallback obligatorio, ya en código** (`UJamBuildGenerator::ChoiceFromKeywords`) | Se usa sin red, si tarda más de 3 s o si la receta no valida |
| Modelo predictivo tipo JEPA | **Por definir** | Idea del equipo: predecir/evaluar el resultado de varias estructuras candidatas y elegir la mejor. Falta definir qué modelo concreto y cómo se integra (pendiente del material que compartió el equipo) |

El proveedor **solo puede usar piezas del inventario**, y el juego valida la receta antes de construir.

### Formato de receta (contrato entre IA y sistema de construcción)
```json
{
  "name": "Rueda Infernal",
  "parts": [
    {"id": "p0", "type": "Wheel"},
    {"id": "p1", "type": "Flamethrower", "attach_to": "p0", "socket": "Right"}
  ],
  "activate": ["p0", "p1"]
}
```
El asistente solo puede usar piezas que el jugador tiene en el inventario; el juego valida la receta antes de construir. Valores válidos de `socket` y de `type`: [CATALOGO-ELEMENTOS §0.1](assets/CATALOGO-ELEMENTOS.md).

### API key del LLM
No va en el repo. Para la build de los jueces: key con límite de gasto bajo en config del build **o** solo fallback. Decidir el 14 nov.
