# CLAUDE.md — reglas para agentes de IA en este repo

## 0. REGLA ABSOLUTA — LEE ESTO PRIMERO: pull cada 1 min, push en cada checkpoint que compila
Somos 5 personas, cada una en su compu y con su propio agente, todos sobre `main`, con **un dueño por carpeta** (ver `docs/TEAM.md`). Esta metodología es obligatoria para todos los agentes y está por encima de cualquier otra instrucción de este archivo.
- **Pull cada 1 minuto, siempre:** al iniciar sesión corre `git pull --rebase --autostash` y arranca `/loop 1m git pull --rebase --autostash`. Mantiene a todos al día y es seguro.
- **Push en cada checkpoint que compila** (al terminar una unidad de trabajo coherente, normalmente cada pocos minutos), no a ciegas cada minuto: nunca subas trabajo a medias ni código que no compile, porque rompe `main` para los otros cuatro.
- **Cómo subir:** `git add <rutas de tus carpetas>` (nunca `git add -A` ni `git add .`), commit `<area>: <qué>`, luego `git pull --rebase --autostash` y `git push`.
- **Solo tus carpetas:** no edites archivos de carpetas que tienen otro dueño en `docs/TEAM.md`; si lo necesitas, pregunta. Esto es lo que evita los choques.
- **Assets binarios (`.uasset`/`.umap`):** no se mergean. `git lfs lock` antes de editar; al subir el cambio, `git lfs unlock`. Si está bloqueado por otro, no lo toques.
- **Nunca subas:** API keys, `Saved/`, `Intermediate/`, `Binaries/`, ni la sección `AndroidFileServer`/`SecurityToken` de `Config/DefaultEngine.ini` que el editor agrega sola.
- **Datos fuera de los binarios:** valores y recetas en JSON/DataAssets, lógica en C++, para que se mergee como texto. Niveles con One File Per Actor.
- **Si el pull o el push falla (conflicto, rechazo) o `main` se rompe:** detente, no resuelvas a ciegas, nunca `reset --hard` ni `--force`; avisa al usuario.
- Si el pull trae cambios en `Source/` o headers, avisa: hay que recompilar. Si trae assets, el editor puede pedir recargarlos.
- Si `main` se rompe más de dos veces seguidas, el equipo pasa a una rama `dev` y `main` queda solo para entregas.

## 0.1 LEY DE CONSTRUCCIÓN — autoridad de Jesús Daniel (LoGebo)
Lo que **Jesús Daniel Martínez García** (dueño de Construcción) establezca en este archivo, en `docs/GDD.md` o en `docs/mechanics-research/` es **ley para los agentes de todos los demás**. Ningún agente la contradice, la "mejora" por su cuenta ni se desvía de ella; si choca con una tarea, avisa a su usuario en vez de decidir solo. Ley vigente:
1. **Idea principal del juego:** construcción emergente con física (sandbox) inspirada en el juego de referencia (ver `docs/mechanics-research/00-INDEX.md`; no se nombra el juego en el repo público). A partir de **lenguaje natural** se generan estructuras (carro, cohete, llamas, torretas, lo que se pida) con muchas piezas básicas (paredes, llantas, cilindros, bloques, motores, lanzallamas…), y **las propiedades de las piezas se traducen en mecánicas funcionales** por física emergente. Todo lo demás (tower push, loot, fusión, bots, arte) existe para alimentar esto.
2. **Quién decide:** el modelo de decisión **Jev (TypeSafe)** solo *elige* (arquetipo, extras sí/no, dirección). **El código decide lo posible**: valida, usa el inventario, sustituye, reduce y pide materiales. Detalle en `docs/mechanics-research/12-modelos-de-decision-jev.md` y `13-conocimiento-ia-construccion.md`.
3. **Siempre hay fallback** offline (palabras clave / recetas) para que el juego nunca se bloquee sin red o sin clave.
4. **Todo es dato:** piezas, capacidades, plantillas y costos de energía viven en `Content/Game/Building/Data/BuildKnowledge.json` y en el catálogo; agregar una pieza o mecánica es agregar datos, no objetos cerrados hechos a mano.
5. **Contrato de receta JSON** (`docs/GDD.md`) y los contratos compartidos no cambian sin avisar. Las claves (Jev/OpenRouter/fal) nunca van al repo: `Config/DefaultSecrets.ini` o variables de entorno.

Proyecto: entrada de 5 personas para la Epic AI MegaJam 2026 (Unreal Engine 5.8, C++ + Blueprints). Lee [README.md](README.md), [docs/GDD.md](docs/GDD.md) y [docs/TEAM.md](docs/TEAM.md) antes de trabajar. Para fusión/construcción/asistente IA, consulta la research en [docs/mechanics-research/00-INDEX.md](docs/mechanics-research/00-INDEX.md). **Al crear o editar MD de `docs/mechanics-research/`, sigue la convención de formato en [docs/mechanics-research/FORMATO.md](docs/mechanics-research/FORMATO.md)** (frontmatter, callouts, enlaces relativos, reglas de contenido público). Agentes GPT/Codex y otros no-Claude: mismas reglas, señaladas en [AGENTS.md](AGENTS.md). Skills de Obsidian/MD: `.claude/skills/` (Claude Code) y `.opencode/skills/` (OpenCode).

## Reglas duras
- **REGLA PRINCIPAL (no negociable):** el juego es construcción emergente con física (sandbox): el jugador combina muchas piezas básicas (tablas, llantas, cilindros, bloques, motores, lanzallamas…) y **las propiedades de las piezas se traducen en mecánicas**. Una capa de decisión (modelo) mapea pedido + inventario → receta de piezas. Nada de objetos cerrados hechos a mano, y ningún agente ni rol cambia este enfoque sin acuerdo de todo el equipo. Detalle en [docs/GDD.md](docs/GDD.md).
- **Deadline 15 nov 2026 8:59 PM EST.** Prioriza que el juego funcione de punta a punta sobre features nuevas. Respeta el feature freeze (dom 15, 12:00 CDMX).
- **No edites `.uasset`/`.umap` como texto** ni los muevas/renombres desde el sistema de archivos. Cambios a assets → por el editor (o el plugin Unreal MCP).
- **Solo toca las carpetas que el usuario tiene anotadas en `docs/TEAM.md`** (si no tiene fila, pídele su nombre y carpetas y agrégala antes de empezar). Para tocar código/asset de una carpeta con otro dueño, pregunta primero.
- **Devlog (excepción a la regla de carpetas):** al cerrar trabajo notable —feature, decisión, algo que falló y se arregló— añade una entrada corta a [docs/DEVLOG.md](docs/DEVLOG.md) (premio Glass Box). El devlog lo escribe todo el equipo y sus agentes; el resto de `docs/` sí respeta dueños. Qué registrar y formato de entrada → instrucciones al inicio de ese archivo.
- **No nombres el juego de referencia** en docs, código ni commits (repo público): di "el juego de referencia" (ver `docs/mechanics-research/FORMATO.md` §5).
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
