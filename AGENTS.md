# AGENTS.md — reglas para agentes de IA (GPT/Codex, Gemini y otros)

Este archivo existe para los agentes que no leen `CLAUDE.md` automáticamente. **La fuente
de verdad de las reglas es [CLAUDE.md](CLAUDE.md)** — léelo completo antes de trabajar
(la sección 0 es la regla absoluta de sincronización). Este archivo no duplica reglas:
solo señala dónde está todo.

## Qué leer antes de tocar algo

1. [CLAUDE.md](CLAUDE.md) — reglas duras: `git pull --rebase --autostash` al iniciar y
   cada 1 min, push en cada checkpoint que compila, un dueño por carpeta
   ([docs/TEAM.md](docs/TEAM.md)), nunca nombrar el juego de referencia (repo público),
   nunca subir API keys / `Saved/` / `Binaries/`.
2. [docs/TEAM.md](docs/TEAM.md) — dueño de cada carpeta; para tocar lo de otro, pregunta.
3. [README.md](README.md) — stack (UE 5.8), deadline (15 nov 8:59 PM EST) y freeze
   (15 nov 12:00 CDMX).
4. [docs/GDD.md](docs/GDD.md) — diseño y contratos compartidos (receta JSON, etc.).

## Base de MD (formato de los documentos del repo)

- **Convención canónica: [docs/mechanics-research/FORMATO.md](docs/mechanics-research/FORMATO.md)**
  — frontmatter YAML (`title`/`date`/`tags`/`status`), callouts
  `[!info]`/`[!tip]`/`[!warning]`/`[!note]`, secciones numeradas, sección final
  `→ Aplicable a GameJamm`, **enlaces relativos a `.md`** (nunca `[[wikilinks]]`), y
  reglas de contenido público en §5: **cero nombres del juego fuente y cero URLs de su
  ecosistema** (di "el juego de referencia"; los datos y valores datamineados sí se
  conservan).
- Obligatorio en `docs/mechanics-research/`. Para cualquier MD nuevo del repo, úsalo
  como referencia.

## Skills de Obsidian/MD (referencia de sintaxis)

El mismo paquete de skills está disponible para los agentes de todo el equipo:

- **Claude Code:** `.claude/skills/obsidian-markdown/` (sintaxis: callouts, properties,
  embeds), `.claude/skills/obsidian-bases/`, `obsidian-cli/`, `obsidian-headless/`,
  `json-canvas/`, `defuddle/`, `knap/`.
- **OpenCode:** `.opencode/skills/` (mismo contenido; copia canónica).
- **Otros agentes (GPT/Codex, Gemini):** esos `SKILL.md` son markdown legible — léelos
  como documentación cuando escribas/edites MD con formato Obsidian.

## Devlog (obligatorio tras trabajo notable)

Al cerrar un trabajo coherente (feature, decisión, algo que falló y se arregló), añade
una **entrada corta** a [docs/DEVLOG.md](docs/DEVLOG.md) — es el premio Glass Box del
jam y lo escribe **todo el equipo** (excepción explícita a la regla de dueños; ver
[CLAUDE.md](CLAUDE.md)). Qué registrar: cómo se usó la IA (qué agente, el prompt clave),
qué falló y cómo se arregló, y captura/GIF si hay algo visible.
