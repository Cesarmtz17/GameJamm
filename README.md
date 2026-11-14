# GameJamm — Epic AI MegaJam 2026

Entrada del equipo para la [2026 Epic AI MegaJam](https://itch.io/jam/2026-epic-ai-megajam). Tema: **DIRECTOR'S CUT**.

> **DEADLINE: sábado 15 nov, 8:59 PM EST / 5:59 PM PST / 7:59 PM CDMX.** Tarde = descalificados.
> Freeze de features: **15 nov 12:00 PM CDMX**. Después de eso solo bugs, build, video y página.

- Diseño: [docs/GDD.md](docs/GDD.md)
- Equipo, roles y carpetas: [docs/TEAM.md](docs/TEAM.md)
- Checklist de entrega: [docs/SUBMISSION.md](docs/SUBMISSION.md)
- Devlog (premio Glass Box): [docs/DEVLOG.md](docs/DEVLOG.md)
- Investigación de mecánicas (fusión/construcción/IA): [docs/mechanics-research/00-INDEX.md](docs/mechanics-research/00-INDEX.md)
- Formato de los MD de research (frontmatter, callouts, reglas): [docs/mechanics-research/FORMATO.md](docs/mechanics-research/FORMATO.md)
- Reglas para agentes de IA: [CLAUDE.md](CLAUDE.md)

## Stack

| Cosa | Elección |
|---|---|
| Motor | **Unreal Engine 5.8** (misma versión exacta para todos, último hotfix) |
| Código | **C++ para sistemas** + Blueprints/DataAssets para contenido y tuning |
| Plataforma | Windows 64-bit (Development/Shipping) |
| Versionado | Git + **Git LFS con file locking**, GitHub privado, trunk-based en `main` |
| Agentes | Claude Code (u otro) + plugin **Unreal MCP** de UE 5.8 |
| Mapa | World Partition + **One File Per Actor** (varios editan el mapa a la vez) |

## Setup (una vez, cada integrante)

1. Instalar **UE 5.8** desde Epic Launcher.
2. Instalar **Visual Studio 2022** (o la versión que indique la doc de UE 5.8) con el workload **"Game development with C++"**.
3. Instalar Git y Git LFS, luego:
   ```bash
   git lfs install
   git clone <url-del-repo>
   cd GameJamm
   git config lfs.locksverify true
   ```
4. Clic derecho en `GameJamm.uproject` → **Generate Visual Studio project files** → abrir el `.sln` → Build (Development Editor, Win64) → abrir el editor.
   - Si el editor pregunta "rebuild modules?" → **Sí**.

## Flujo de trabajo (LEER)

Los `.uasset`/`.umap` son binarios: **no se pueden mergear**. Si dos personas editan el mismo asset, uno pierde su trabajo. Por eso:

1. **Antes de empezar:** `git pull` (siempre). **Y pull cada 1 minuto, push en cada checkpoint que compila** mientras trabajes (ver "Sincronización" abajo): somos 5 personas, cada una con su compu y su agente.
2. **Antes de editar un asset existente:** bloquéalo.
   ```bash
   git lfs lock "Content/Game/Combat/BP_Sword.uasset"
   git lfs locks              # ver quién tiene qué
   ```
   Si está bloqueado por otro → no lo toques, avísale por el chat.
3. **Commits chicos y frecuentes** (cada 30–60 min), directo a `main`:
   ```bash
   git pull --rebase
   git add <archivos>
   git commit -m "combat: fuego en flechas"
   git push
   ```
4. **Después de push:** `git lfs unlock "<ruta>"` de lo que ya no estés editando.
5. **Trabaja dentro de tu carpeta** (ver [docs/TEAM.md](docs/TEAM.md)). Pruebas personales en `Content/Developers/<tu-nombre>/`.
6. **No renombres/muevas assets de otros** sin avisar (rompe referencias y deja redirectors). Si mueves algo: *Fix Up Redirectors* y commit todo junto.
7. **C++:** si cambias headers, avisa; los demás tienen que recompilar.
8. **Nunca commitear API keys.** Van en `Config/DefaultSecrets.ini` (ignorado por git).

### Sincronización: pull cada 1 min, push por checkpoint que compila (REGLA ABSOLUTA)
Cada sesión de Claude Code arranca:
```
/loop 1m git pull --rebase --autostash
```
y hace push (solo de sus archivos, `git add` de su carpeta, nunca `-A`) cada vez que termina algo que compila. Sin keys ni el token de Android de `DefaultEngine.ini`. Si el pull o el push falla, **para y avisa**; sin `--force`. Detalle en [CLAUDE.md](CLAUDE.md), sección 0.

### Mensajes de commit
`<area>: <qué>` — áreas: `core`, `combat`, `build`, `ai`, `map`, `ui`, `audio`, `docs`.

## Build empaquetado
Platforms → Windows → Package Project → carpeta `Packaged/` (ignorada).
Zip final: `TeamName_GameName.zip` (< 1 GB). **Empaquetamos una vez al día como mínimo** para detectar errores de cooking temprano.
