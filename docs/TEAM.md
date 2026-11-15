# Quién trabaja en qué

Un dueño por carpeta: solo esa persona edita sus assets (o pide permiso / lock). No hay roles ni jerarquía. Si vas a trabajar en
una carpeta sin dueño, escribe tu nombre en la tabla y haz push; si ya tiene dueño, pregunta antes de tocarla.

| Área | Carpetas | Código C++ | Dueño |
|---|---|---|---|
| **1 · Core** — partida, torres, base, equipos, respawn, victoria, inventario | `Content/Game/Core/`, `Content/Game/Maps/` (persistent level) | `Source/GameJamm/Core/` | Cesar |
| **2 · Combate + Fusión** — armas, arco, materiales, efectos | `Content/Game/Combat/` | `Source/GameJamm/Combat/` | Cesar (TAREAS-PC tarea 2: la carpeta no existía) |
| **3 · Construcción** — piezas, anclajes, física, ensamblado, recetas JSON y su integración con la IA | `Content/Game/Building/` | `Source/GameJamm/Building/` | Jesús Daniel Martínez García (LoGebo) |
| **4 · IA** — bots (StateTree) + asistente LLM | `Content/Game/AI/` | `Source/GameJamm/AI/` | Cesar |
| **5 · Mundo / Arte / Audio / UI** — mapa, look, SFX/música, HUD, video y devlog | `Content/Game/Art/`, `Content/Game/Audio/`, `Content/Game/UI/`, actores del mapa | — | Emilio — arte 3D (`Content/Game/Art/`, con Blender MCP) |
| **6 · Docs / research** — research de mecánicas y su documentación | `docs/mechanics-research/` | — | Javier Fernández |
| **7 · Arte con Tripo** — reemplazo del arte del juego con Tripo (las 35 piezas ya; después personajes y mundo). Acordado el 14 nov: puede reemplazar **arte** de cualquier carpeta dejando nota en [ARTE-TRIPO.md](ARTE-TRIPO.md) | `SourceArt/Tripo/`, `Content/Game/Art/Tripo/` (+ mallas reemplazadas, ver nota) | — | Javier Fernández |

El número de área es solo una etiqueta: algunos docs (research 12, `ART.md`, `PROMPTS-ASSETS.md`) dicen "rol 3" para referirse a Construcción.

Compartido: `Content/Game/Shared/` (lock obligatorio). Pruebas: `Content/Developers/<nombre>/` (nadie más lo toca, no se referencia desde el juego).

## Mapa
- `Content/Game/Maps/L_Main` con **World Partition + One File Per Actor**: cada actor es su propio archivo, así varias personas pueden colocar cosas a la vez. Aun así, **no editar el mismo actor** a la vez.
- Cada quien tiene un mapa de pruebas: `Content/Developers/<nombre>/L_Test_<nombre>`.

## Contratos entre áreas (no romper sin avisar)
- **Daño:** todo lo que recibe daño usa `UHealthComponent` y el `ApplyDamage` de UE. Equipo vía `EJamTeam` (`Red`/`Blue`/`Neutral`) en `UHealthComponent::Team`; helpers en `UJamStatics`.
- **Inventario:** `UInventoryComponent` en el personaje (materiales + piezas) — lo hace Core, lo usan Combate/Construcción/IA.
- **Receta de construcción:** JSON en [GDD.md](GDD.md#formato-de-receta-contrato-entre-ia-y-sistema-de-construcción).

**Deadline:** ver [README.md](../README.md) (sin feature freeze desde el 15 nov).
