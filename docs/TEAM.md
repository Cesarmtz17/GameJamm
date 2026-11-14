# Equipo, roles y propiedad de carpetas

Cada rol es **dueño** de sus carpetas: solo esa persona edita esos assets (o pide permiso / lock). Nombres TBD.

| # | Rol | Persona | Carpetas Content | Código C++ |
|---|---|---|---|---|
| 1 | **Core / Lead** — partida, torres, base, equipos, respawn, victoria; integración y builds | TBD | `Content/Game/Core/`, `Content/Game/Maps/` (persistent level) | `Source/GameJamm/Core/` |
| 2 | **Combate + Fusión** — armas, arco, materiales, efectos | TBD | `Content/Game/Combat/` | `Source/GameJamm/Combat/` |
| 3 | **Construcción / Mecánicas** — piezas, anclajes, física, ensamblado; recetas JSON y su integración con la IA generativa | Jesús Daniel Martínez García (LoGebo) | `Content/Game/Building/` | `Source/GameJamm/Building/` |
| 4 | **IA** — bots (StateTree) + asistente LLM | TBD | `Content/Game/AI/` | `Source/GameJamm/AI/` |
| 5 | **Mundo / Arte / Audio / UI** — mapa, look, SFX/música, HUD, **video y devlog** | TBD | `Content/Game/Art/`, `Content/Game/Audio/`, `Content/Game/UI/`, actores del mapa | — |

Compartido: `Content/Game/Shared/` (lock obligatorio). Pruebas: `Content/Developers/<nombre>/` (nadie más lo toca, no se referencia desde el juego).

## Mapa
- `Content/Game/Maps/L_Main` con **World Partition + One File Per Actor**: cada actor es su propio archivo, así varias personas pueden colocar cosas a la vez. Aun así, **no editar el mismo actor** a la vez.
- Cada quien tiene un mapa de pruebas: `Content/Developers/<nombre>/L_Test_<nombre>`.

## Contratos entre roles (acordar temprano, no romper)
- **Daño:** todo lo que recibe daño usa `UHealthComponent` y el `ApplyDamage` de UE. Equipo vía `EJamTeam` (`Red`/`Blue`/`Neutral`) en `UHealthComponent::Team`; helpers en `UJamStatics`.
- **Inventario:** `UInventoryComponent` en el personaje (materiales + piezas) — lo hace Core, lo usan Combate/Construcción/IA.
- **Receta de construcción:** JSON en [GDD.md](GDD.md#formato-de-receta-contrato-entre-ia-y-sistema-de-construcción).

## Calendario (hora CDMX)
| Cuándo | Hito |
|---|---|
| **Jue 13 noche** | Proyecto creado, todos compilan, contratos (Health/Team/Inventory) en `main`. Mapa blockout. |
| **Vie 14 mediodía** | Cada sistema funciona aislado en su mapa de pruebas. Primer build empaquetado. |
| **Vie 14 noche** | Todo integrado en `L_Main`: partida jugable de inicio a fin vs bots. |
| **Sáb 15 12:00** | **FEATURE FREEZE.** Solo bugs, pulido, audio. |
| **Sáb 15 16:00** | Build final, grabar video 30–60 s, página de itch. |
| **Sáb 15 18:30** | **Enviado.** (Deadline real 19:59 CDMX — no lo apuramos.) |

## Comunicación
- Chat del equipo: avisar locks largos, cambios de headers C++ y "voy a hacer push de X".
- Checkpoint rápido 3 veces al día (mañana, tarde, noche): qué hice, qué sigue, qué me bloquea.
