# CLAUDE.md — reglas para agentes de IA en este repo

## 0. CRÍTICO — LEE ESTO PRIMERO: pull Y push cada 1 minuto (metodología de todos los agentes)
Somos 5 personas, cada una en su compu y con su propio agente, todos sobre `main`. **Todos hacemos pull y push cada minuto, sin excepción**, para no pisarnos y ver el trabajo de los demás en tiempo real.
- **Al iniciar sesión:** corre `git pull --rebase --autostash` y arranca `/loop 1m` con este ciclo: commitear lo tuyo → `git pull --rebase --autostash` → `git push`. Un ciclo cada 1 min mientras la sesión esté viva.
- **Commit en cada ciclo, pero solo de lo tuyo:** `git add <rutas de tu carpeta/rol>` (nunca `git add -A` ni `git add .`), mensaje `<area>: <qué>`. Si no hay cambios tuyos, solo haz pull.
- **Nunca subas:** API keys, `Saved/`, `Intermediate/`, `Binaries/`, ni `Config/DefaultEngine.ini` con la sección `AndroidFileServer`/`SecurityToken` que el editor agrega solo. No subas código que no compile: si rompe el build, arréglalo en el siguiente ciclo antes de seguir.
- **Assets binarios (`.uasset`/`.umap`):** no se mergean. `git lfs lock` antes de editar; sube el asset en cuanto termines un cambio coherente y haz `git lfs unlock`.
- `--autostash` cubre cambios sin commitear; nunca `reset --hard` ni `--force` para "arreglar" un pull o un push rechazado.
- **Si el pull o el push falla (conflicto, rechazo):** detente, no resuelvas a ciegas, avisa al usuario.
- Si el pull trae cambios en `Source/` o headers, avisa: hay que recompilar. Si trae assets, el editor puede pedir recargarlos.

Proyecto: entrada de 5 personas para la Epic AI MegaJam 2026 (Unreal Engine 5.8, C++ + Blueprints). Lee [README.md](README.md), [docs/GDD.md](docs/GDD.md) y [docs/TEAM.md](docs/TEAM.md) antes de trabajar. Para fusión/construcción/asistente IA, consulta la research en [docs/mechanics-research/00-INDEX.md](docs/mechanics-research/00-INDEX.md). **Al crear o editar MD de `docs/mechanics-research/`, sigue la convención de formato en [docs/mechanics-research/FORMATO.md](docs/mechanics-research/FORMATO.md)** (frontmatter, callouts, enlaces relativos, reglas de contenido público).

## Reglas duras
- **Deadline 15 nov 2026 8:59 PM EST.** Prioriza que el juego funcione de punta a punta sobre features nuevas. Respeta el feature freeze (sáb 15, 12:00 CDMX).
- **No edites `.uasset`/`.umap` como texto** ni los muevas/renombres desde el sistema de archivos. Cambios a assets → por el editor (o el plugin Unreal MCP).
- **Solo toca la carpeta del rol del usuario** (ver TEAM.md). Para tocar código/asset de otro rol, pregunta primero.
- **No rompas contratos compartidos** (`UHealthComponent`, `EJamTeam`, `UInventoryComponent`, formato JSON de recetas) sin avisar al usuario; esos cambios afectan a todo el equipo.
- **Nunca commitees API keys** ni archivos de `Saved/`, `Intermediate/`, `Binaries/`.
- Antes de editar un asset binario existente: `git lfs lock <ruta>`.
- **SIEMPRE `git pull --rebase` antes de mover, renombrar o borrar cualquier archivo/carpeta** (ver sección Sincronización para la cadencia) — un repo desactualizado genera conflictos y puede pisar trabajo de otros.

## Convenciones C++
- Módulo `GameJamm`, código en `Source/GameJamm/<Area>/` (Core, Combat, Building, AI).
- Prefijo de clases: `AJam…`, `UJam…` cuando sea genérico; nombres descriptivos para componentes (`UHealthComponent`).
- Lógica y estado en C++; valores ajustables con `UPROPERTY(EditDefaultsOnly)` o en DataAssets para que diseño los afine en BP sin recompilar.
- Exponer a BP con `UFUNCTION(BlueprintCallable/BlueprintImplementableEvent)` en los puntos de extensión.
- Preparar para red con lo mínimo (`SetIsReplicatedByDefault`, autoridad en servidor) pero **sin invertir tiempo en multijugador** hasta que el MVP vs bots esté listo.
- Cambios en headers fuerzan recompilar a todos: agrúpalos y avisa en el mensaje de commit.

## Commits
`<area>: <qué>` — `core`, `combat`, `build`, `ai`, `map`, `ui`, `audio`, `docs`. Commits chicos.
