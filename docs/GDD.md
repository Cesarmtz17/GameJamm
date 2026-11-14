# GDD — Backlot Brawl (título de trabajo)

## Pitch
PvP de dos equipos tipo *tower push*: avanza por el mapa destruyendo torres hasta llegar a la base enemiga y destruirla. Apareces **sin loot**; el mapa tiene **estructuras/campamentos** donde peleas y looteas materiales y piezas. Con ellos:
- **Fusionas armas** (piedra en la espada, fuego en la flecha…).
- **Construyes artefactos** con piezas (llantas, tablas, lanzallamas…) — p. ej. una llanta con lanzallamas que rueda por el mapa quemando todo.
- Un **asistente de IA** te ayuda a diseñar creaciones con las piezas que tienes.

## Tema: DIRECTOR'S CUT — cerrado
**Título de trabajo: _Backlot Brawl_** (cambiable hasta el viernes; afecta solo a la UI y a la página de itch).

**Ficción:** dos estudios de cine de bajo presupuesto filman en el mismo *backlot* y solo una película puede terminarse. Tú eres
**el Director**. Todo en el mundo es utilería: armas, piezas y sets. Gana quien destruya la **Cámara A** del otro estudio:
"¡CORTE!", y su película se queda en la sala de edición.

**Cómo el tema toca las mecánicas** (no es solo un reskin):
1. **Construir = rodar una toma.** Cada creación es una *toma* con su claqueta (`ESCENA n · TOMA m · nombre`). Activarla = **"¡Acción!"**.
2. **"¡Corte!" (re-toma):** si tu creación se atora o se rompe, gritas "¡Corte!" y **la desarmas y recuperas sus piezas** en el inventario
   para volver a intentarlo. Fomenta experimentar y es barato de implementar (destruir el build + devolver los items).
3. **El Asistente de Dirección (la IA).** Tú das la dirección en lenguaje natural ("algo que ruede y queme todo") y la IA elige qué escena
   montar con la utilería que tienes. **El *Director's Cut*:** puedes pedirle que re-edite tu última toma ("más rápida", "ahora con
   fuego"), y la IA recibe la receta anterior junto con tu instrucción. *(P1: solo si el MVP del asistente ya funciona.)*
4. **Fusión = efectos prácticos de cine:** pirotecnia (fuego), hielo seco (hielo), chispero (rayo).
5. **Final = créditos de tu película**, con tus stats y tu mejor toma.

**Glosario (solo afecta textos de UI; las clases C++ y los contratos NO se renombran):**

| Sistema / código | En pantalla |
|---|---|
| Torre (`ATeamStructure`) | Torre de reflector |
| Base | Cámara A |
| Minions | Extras |
| Bots héroe | Dobles de riesgo |
| Campamento de loot / cofre | Bodega de utilería / baúl de utilería |
| Enemigos de campamento | Utileros |
| Receta / construcción | Toma |
| Asistente IA | Asistente de Dirección |
| Equipos `Red` / `Blue` | Estudio Rojo (set western) / Estudio Azul (set sci-fi) |

Dirección de arte, UI y audio: **[ART.md](ART.md)**.

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
- Pantalla de "repetición" estilo director.

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
    {"id": "p1", "type": "Flamethrower", "attach_to": "p0", "socket": "Side"}
  ],
  "activate": ["p0", "p1"]
}
```
El asistente solo puede usar piezas que el jugador tiene en el inventario; el juego valida la receta antes de construir.

### API key del LLM
No va en el repo. Para la build de los jueces: key con límite de gasto bajo en config del build **o** solo fallback. Decidir el viernes.
