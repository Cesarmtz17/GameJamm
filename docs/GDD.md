# GDD — GameJamm (título por definir)

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

## MVP (tiene que existir el sábado a mediodía)
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
| Construcción | `ABuildPart`, `UAttachPointComponent`, `UBuilderComponent` | `UBuildPartData`, BP por pieza |
| IA bots | `AJamAIController`, StateTree tasks | StateTrees, carriles (spline) |
| Asistente | `UAIAssistantSubsystem` (HTTP → LLM → JSON) | `UBuildRecipeData` (recetas fallback) |
| Loot | `ALootCamp`, `ALootChest` | Tablas de loot |

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
No va en el repo. Para la build de los jueces: key con límite de gasto bajo en config del build **o** solo fallback. Decidir el viernes.
