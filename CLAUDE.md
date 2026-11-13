# CLAUDE.md — reglas para agentes de IA en este repo

Proyecto: entrada de 5 personas para la Epic AI MegaJam 2026 (Unreal Engine 5.8, C++ + Blueprints). Lee [README.md](README.md), [docs/GDD.md](docs/GDD.md) y [docs/TEAM.md](docs/TEAM.md) antes de trabajar.

## Reglas duras
- **Deadline 15 nov 2026 8:59 PM EST.** Prioriza que el juego funcione de punta a punta sobre features nuevas. Respeta el feature freeze (sáb 15, 12:00 CDMX).
- **No edites `.uasset`/`.umap` como texto** ni los muevas/renombres desde el sistema de archivos. Cambios a assets → por el editor (o el plugin Unreal MCP).
- **Solo toca la carpeta del rol del usuario** (ver TEAM.md). Para tocar código/asset de otro rol, pregunta primero.
- **No rompas contratos compartidos** (`UHealthComponent`, `EJamTeam`, `UInventoryComponent`, formato JSON de recetas) sin avisar al usuario; esos cambios afectan a todo el equipo.
- **Nunca commitees API keys** ni archivos de `Saved/`, `Intermediate/`, `Binaries/`.
- Antes de editar un asset binario existente: `git lfs lock <ruta>`. Siempre `git pull --rebase` antes de `git push`.

## Convenciones C++
- Módulo `GameJamm`, código en `Source/GameJamm/<Area>/` (Core, Combat, Building, AI).
- Prefijo de clases: `AJam…`, `UJam…` cuando sea genérico; nombres descriptivos para componentes (`UHealthComponent`).
- Lógica y estado en C++; valores ajustables con `UPROPERTY(EditDefaultsOnly)` o en DataAssets para que diseño los afine en BP sin recompilar.
- Exponer a BP con `UFUNCTION(BlueprintCallable/BlueprintImplementableEvent)` en los puntos de extensión.
- Preparar para red con lo mínimo (`SetIsReplicatedByDefault`, autoridad en servidor) pero **sin invertir tiempo en multijugador** hasta que el MVP vs bots esté listo.
- Cambios en headers fuerzan recompilar a todos: agrúpalos y avisa en el mensaje de commit.

## Commits
`<area>: <qué>` — `core`, `combat`, `build`, `ai`, `map`, `ui`, `audio`, `docs`. Commits chicos.
