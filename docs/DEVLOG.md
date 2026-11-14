# Devlog

> [!tip] Cómo escribir aquí (para personas y agentes)
> **Quién:** todo el equipo y sus agentes (excepción explícita a "solo tus carpetas" en
> [CLAUDE.md](../CLAUDE.md) — lo demás de `docs/` respeta dueños). **Cuándo:** al cerrar
> trabajo notable —feature, decisión, algo que falló y se arregló—. **Formato de
> entrada:** `## <fecha> — <tema> (quién)`, con 3-6 bullets: qué se hizo, cómo se usó la
> IA (agente + prompt clave), qué falló, qué se aprende. Convención de MD para docs
> nuevos: [mechanics-research/FORMATO.md](mechanics-research/FORMATO.md).

Para el premio **Glass Box** (Modifier: mejor devlog de cómo construiste el juego; $2,500 + una ASUS ROG Ally por integrante). Cada quien agrega entradas cortas: qué hicimos, cómo usamos la IA (prompts, agentes, MCP), qué falló y qué aprendimos. Capturas/GIFs en `docs/devlog/` (LFS).

## Publicación (Glass Box exige un link público)

Aquí escribimos; antes de enviar hay que poner un **link público** al devlog en el formulario ([SUBMISSION.md](SUBMISSION.md)).
El repo de GitHub está hoy **público** (verificado con `gh repo view` el 13 nov; el README todavía dice "privado"), así que este
archivo ya tiene URL pública y sirve de **respaldo**. Pero un `.md` dentro de un repo se lee mal para jueces, y el link muere si
el repo vuelve a privado.

- **Dónde:** posts de devlog en la página de itch.io del juego (la misma donde subimos el build) como link principal; el archivo en GitHub como respaldo. Cualquier página pública sirve si itch no alcanza.
- **Cómo:** este archivo es la fuente. Al cerrar cada día, copiar las entradas nuevas al post. Sube también las capturas/GIFs de `docs/devlog/` al post, para no depender del repo.
- **Cuándo:** 13 nov decidir dónde y quién; 14 nov crear la página y publicar lo que haya; **antes del feature freeze (15 nov, 12:00 CDMX) el link debe existir y abrir sin iniciar sesión**.
- **Responsable de publicar:** — (anotar nombre). **Decisión pendiente.**

## Qué registrar para que cuente

Los jueces premian el **proceso**. Al terminar algo, deja en una entrada:
- **Cómo usamos la IA:** qué agente o herramienta, el **prompt clave** (pegado o resumido) y qué salió.
- **Qué falló y cómo lo arreglamos.** Es lo que más se lee. Ya tenemos uno: una primera versión de los docs interpretó el tema como cine y se corrigió contra el texto oficial del jam (ver la entrada de tema).
- **Quién decidió:** si una decisión de diseño no la confirmó el equipo, escribe "propuesta" en vez de "cerrado".
- **Una captura o GIF** cuando haya algo visible.

**Entradas que faltan** (el historial de git las respalda, falta la voz de quien las hizo):
- [x] Research de mecánicas con agentes (`docs/mechanics-research/`): cómo se hizo y qué se descartó. *(hecha abajo, 13-14 nov)*
- [ ] Prompt a estructura: catálogo de piezas, ensamblador (receta a constraints) y el cliente de decisión con LLM y fallback.
- [ ] Cinco personas con cinco agentes en un solo repo: la regla de sync pasó de pull cada 5–10 min a cada 1 min en el mismo día; un dueño por carpeta.
- [ ] Uso de MCP (Unreal MCP, Blender MCP) en el flujo de trabajo.

---

## 14 nov — Arte con Tripo y Rodin: las 45 piezas y las estructuras del mapa (Javier)
- Qué: las 35 piezas de construcción (más 10 nuevas de solo datos) pasaron de primitivas de color plano a modelos texturizados a mano; después torres, fortalezas, banderas, barricadas y plataformas de spawn de cada equipo. Comparación en `SourceArt/Tripo/previews/_comparacion_vehiculo.png`.
- Cómo con IA: referencia 2D por pieza en Tripo (Nano Banana, con el concepto 01-C como guía de estilo; gratis) → imagen a 3D (Tripo Modelo HD, 30 créditos, 5k polígonos) o **Rodin Gen-2.5 por el MCP oficial de Hyper3D** (0.5 créditos). Prompt base: *"Game asset, single object only: <pieza>, in the exact art style of the reference image (stylized hand-painted 3D game art, chunky toy-like proportions…)"*.
- Pipeline sin abrir programas: un script de **Blender en modo headless** encaja cada modelo en la caja `Size` exacta de `JamBuildPart.cpp` (pivote, +X adelante, ≤5k tris, texturas 1K) y un **commandlet de Python de Unreal** lo importa con un material maestro que conserva el brillo al encender. Las estructuras del mapa se reemplazaron en su lugar leyendo la caja de cada malla, y los 6 `ATeamStructure` se reasignaron con un script dentro del editor.
- Qué falló: Chrome bloqueaba descargas en serie (se capturó la URL firmada de cada exportación); un script de lanzamiento se cortó por tiempo pero siguió corriendo en la página (se verificó por créditos que no hubo duplicados); 10 piezas salieron mirando al revés (se revisa con vista superior y lateral); reimportar encima dejaba dos slots de material; los actores del mapa seguían en solo lectura tras quitarles el lock; y dos agentes en la misma carpeta hicieron una carrera de `git pull` a media compilación.
- Qué aprendimos: el contrato de la pieza (caja exacta, pivote, un slot) permitió cambiar todo el arte sin tocar código; la verificación por imagen (render + captura del juego) atrapó lo que los números no.

---

## 13-14 nov — Research de mecánicas con agentes (docs/mechanics-research)
- Pipeline: agente **deep-research** recopiló fuentes públicas (panel de física de GDC 2024, prensa técnica, wikis, datamines de la comunidad, papers) → 8 docs con valores y fórmulas (01-08); una segunda pasada de **auditoría open source** añadió el análisis del repo de referencia ComponentFuseMechanic y el diseño del ensamblado con grafo/joints/JSON validado (docs 09-11). Todo con formato Obsidian y citado en FUENTES.md.
- El repo es público del jam: se aplicó un **scrub** para no nombrar el juego fuente ni enlazar URLs de su ecosistema — títulos, personajes, lugares y habilidades → genérico ("el juego de referencia"); los valores datamineados se conservaron. Verificado con `git grep` en todo el árbol: 0 menciones.
- Qué falló: una pasada paralela del equipo subió 3 docs con el nombre del juego en el filename y el contenido. Se reescribieron como 09-11 (el trabajo no se tiró) y se borraron los originales; también salió una única mención residual en la nota de provenance de un skill vendido.
- Cómo se conectó: `FORMATO.md` como convención de formato, `AGENTS.md` + skills en `.claude/skills/`/`.opencode/skills/` para que cualquier agente (Claude, GPT/Codex, Gemini) herede la misma base de MD.
- Herramienta de análisis: REA (reverse-engineer-anything, vendido en `.claude/vendor/`) para los planes de análisis del binario del juego de referencia **solo con copia legal** (doc 07); en esta pasada aún no se corrió sobre binarios.

---

## 13 nov — Arranque
- Elegimos Unreal Engine 5.8, C++ + Blueprints, Git LFS con locking.
- Idea: tower-push PvP con fusión de armas y construcción de artefactos asistida por IA.

## 13 nov — Tema y dirección de arte
- Tema del jam: DIRECTOR'S CUT = "la función que siempre se recorta" + "tú diriges a tus agentes de IA". Lo cumplimos con la construcción libre con física (la mecánica que nunca entra en el alcance) y con el asistente de IA al que le das la visión y decides qué se queda. Una primera versión de los docs le había puesto por error una ficción de estudio de cine; se retiró.
- Dirección de arte: archipiélago de islas flotantes con islas grandes de campo abierto, low-poly estilizado, artefactos de madera, hierro y cuerda. Conceptos 01-A/01-B/01-C en docs/concepts/ (generados con gpt-image-2 vía OpenClaw a partir del GDD; elegidos por el equipo). Guía en docs/ART.md.

## 13-14 nov — Arte 3D, iconos y audio por código (Emilio)
- Sin experiencia previa en Blender: el agente (Claude vía OpenClaw) escribió **scripts de Python para Blender en modo headless** en lugar de modelar a mano o usar Blender MCP. Un script = un lote completo en segundos (las 12 primeras piezas salieron en 13 s).
- Las medidas salen de la tabla de `JamBuildPart.cpp` (fuente de verdad); cada FBX trae caja exacta, pivote al centro, colisión `UCX_` y sockets `Attach_*`. Cuando Construcción subió 35 piezas con otras medidas, se regeneraron todas en minutos.
- Una sola textura de paleta (16 colores) para todo: un material, cero texturas por pieza.
- Resultado: 104 modelos, mapa blockout + versión vestida, 62 iconos (inventario + HUD), hoja de estilo, todo regenerable.
- Audio también por código (numpy): 23 efectos y 3 loops de música sintetizados, originales y sin licencias de terceros.
- Qué falló: un instalador que pedía permisos de administrador (se usó Blender portátil); certificados TLS de la red del agente; el arco salió en pedazos en el primer intento y las vistas previas cortaban piezas (se corrigió el encuadre).