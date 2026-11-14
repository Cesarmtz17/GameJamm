# Devlog

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
- [ ] Research de mecánicas con agentes (`docs/mechanics-research/`): cómo se hizo y qué se descartó.
- [ ] Prompt a estructura: catálogo de piezas, ensamblador (receta a constraints) y el cliente de decisión con LLM y fallback.
- [ ] Cinco personas con cinco agentes en un solo repo: la regla de sync pasó de pull cada 5–10 min a cada 1 min en el mismo día; un dueño por carpeta.
- [ ] Uso de MCP (Unreal MCP, Blender MCP) en el flujo de trabajo.

---

## 13 nov — Arranque
- Elegimos Unreal Engine 5.8, C++ + Blueprints, Git LFS con locking.
- Idea: tower-push PvP con fusión de armas y construcción de artefactos asistida por IA.

## 13 nov — Tema y dirección de arte
- Tema del jam: DIRECTOR'S CUT = "la función que siempre se recorta" + "tú diriges a tus agentes de IA". Lo cumplimos con la construcción libre con física (la mecánica que nunca entra en el alcance) y con el asistente de IA al que le das la visión y decides qué se queda. Una primera versión de los docs le había puesto por error una ficción de estudio de cine; se retiró.
- Dirección de arte: archipiélago de islas flotantes con islas grandes de campo abierto, low-poly estilizado, artefactos de madera, hierro y cuerda. Conceptos 01-A/01-B/01-C en docs/concepts/ (generados con gpt-image-2 vía OpenClaw a partir del GDD; elegidos por el equipo). Guía en docs/ART.md.
