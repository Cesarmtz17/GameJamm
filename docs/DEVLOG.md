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

## 14 nov — Props del mapa con Rodin: árboles, rocas, campamentos y molino (Javier + Claude Code) — provisional
- Qué: las 128 mallas de decoración del mapa (árboles, matas de conos, hongos, flores, rocas, faroles, tiendas, letreros, los 4 campamentos y el molino) pasaron de primitivas de color plano a 25 modelos texturizados que se reusan, sin mover ningún actor del mapa.
- Cómo con IA: Claude Code agrupó las mallas por su número de triángulos (las 82 `P_###` eran solo 9 formas), las renderizó en Blender para saber qué era cada una y generó los 25 modelos en **Rodin por MCP** (solo texto, con los hex de la paleta de `docs/ART.md` en el prompt; 12.5 créditos). Después, Blender headless encajó cada modelo en la caja de la malla vieja (6 en paralelo, 3.5 min para 115) y el importador las reemplazó en su lugar desde el editor.
- Qué falló: encajar exacto en la caja vieja estiraba las copas de los árboles hasta 60 %; ahora cada modelo conserva su altura y sus proporciones (`fit: height`) y se gira 90° si así deforma menos. Las aspas viejas del molino flotaban sobre la torre: se usó un molino completo y se ocultaron (sin borrarlas). Una lista con finales de línea de Windows hizo que `git lfs lock` bloqueara rutas con un `\r` al final, que no existen: el editor no pudo guardar y hubo que liberar 115 bloqueos fantasma por ID.
- Qué aprendimos: con `\r` el bloqueo sale "exitoso" pero no protege nada; siempre verificar con `git lfs locks` y que el archivo quede escribible. Costo: ~100k triángulos entre las 128 mallas; falta medir FPS en la GTX 1650.

![Antes y después: pradera, cerros, molino y campamento](devlog/2026-11-14-props-rodin.png)

---

## 14 nov — Torres, espada/arco y loot (Cesar + Claude Code)
- **Torres:** a jugadores y bots les hacen 10 por disparo (+10 % por disparo seguido al mismo, tope ×1.5); los minions siguen con 20. Medido con `Jam.Goto 3300 0`: parado debajo mueres en 16 s (antes 12).
- **Armas (`Source/GameJamm/Combat/`):** espada 20 de daño con empuje y arco con flecha física; nunca dañan aliados. Se probaron con un muñeco (`Jam.Dummy`, `Jam.Attack`) leyendo el log, sin pantalla.
- **Loot:** todo sale de `Content/Game/Core/Data/Loot.json`: cofre inicial con el kit del GDD, cofres por isla que reponen, chatarra en el carril, cofre raro central al minuto 5. Al morir sueltas una bolsa con la mitad de tus piezas y un orbe con energía.
- **Qué falló:** los cofres iban a abrir con E, pero E ya monta la construcción más cercana, así que se abren por cercanía. La bolsa de muerte la recogía el mismo muerto en el mismo frame; ahora se suelta después de quitarle la colisión y los muertos no recogen nada. Dos montones de chatarra caían en huecos del mapa; ahora buscan suelo en anillos de 2-6 m.
- **IA:** el agente compila, abre el juego con `-ExecCmds` y revisa el log en cada cambio. Cada número de arriba sale de esas corridas.

## 14 nov — Matas y flores 3D sobre el pasto (Javier + Claude Code) — provisional
- Qué: 4 modelos de Rodin (2 matas de pasto, 2 racimos de flores) regados ~1,560 veces sobre el pasto de islas y pradera; es lo que más rompe la repetición de la textura.
- Cómo con IA: Rodin por su MCP oficial (solo texto, 0.5 créditos c/u) → Blender headless (290–390 tris, pivote abajo) → importador de Python → un script del editor (`ue_scatter_foliage.py`) que traza rayos y solo coloca donde pega en el pasto (fuera caminos, árboles y bases).
- Cuidados de rendimiento para una GTX 1650: instancias jerárquicas (HISM), sin colisión, sin sombra, corte a 45–65 m.
- Qué falló: la primera pasada salió muy rala (un racimo por cada 55 m²; se dejó en 28); el estiramiento por eje marcó las matas (caja más ancha que el modelo) y se corrigió con la caja a sus proporciones reales. Falta medir FPS.

![Matas y flores](devlog/2026-11-14-matas-flores.png)

---

## 14 nov — Islas con materiales de PBRgen (Javier + Claude Code) — provisional
- Qué: pasto de islas y pradera, roca de abajo, tablas de puente y caminos pasaron de color plano a detalle pintado a mano, sin salir de la paleta de `docs/ART.md`: el material conserva su `Color` y la textura solo suma variación (`Color × textura / su promedio`).
- Cómo con IA: 4 materiales en **PBRgen** (estilo Hand-painted Game Art; 18 créditos c/u) manejado por Claude Code con el MCP de Chrome; un script de Python del editor (`SourceArt/Tripo/scripts/ue_world_mats.py`) reconstruye los `M_Map_XX` en su lugar con proyección en el mundo (las islas no tienen UVs fiables): desde arriba en pasto y caminos, triplanar en roca.
- Qué falló: la proyección desde arriba se estiraba en las orillas de las islas (se apaga el detalle en caras verticales); el pasto se veía repetido y Javier preguntó cómo lo evitan los juegos grandes → segunda escala rotada, manchas grandes de ruido, pasto seco y menos detalle a lo lejos, todo con la misma textura. Un primer intento borró el color base (se leía por referencia después de borrar el nodo); ahora los colores originales van fijos en el script.
- Estado: **provisional**; `DetailStrength = 0` regresa al look plano. Falta medir FPS en la GTX 1650.

![Antes y después del pasto](devlog/2026-11-14-islas-pbrgen.png)

---

## 14 nov — Arte de Tripo probado en partida (Javier + Claude Code)
- Qué: 15 construcciones reales en PIE (`Jam.Type`) que generan los 35 tipos de pieza con mallas de Tripo; capturas apagadas y prendidas desde el mismo ángulo.
- Cómo con IA: Claude Code manejó el editor por **Unreal MCP** (iniciar/detener PIE, captura del editor) y corrió Python del editor desde la caja **Cmd** (`py script.py`) para construir, congelar la física de las piezas y encuadrar la cámara.
- Qué falló: la cámara no obedecía porque el Tick del personaje reimpone el brazo con colisión y se encogía dentro del suelo; se resolvió poniendo al jugador invisible entre cámara y construcción y cerrando el FOV. `Jam.Say` enciende la construcción y el carro cohete se iba solo; se usó `Jam.Type` (sale apagada).
- Resultado (**propuesta, no cerrado**): con la luz del mapa las piezas no se ven pálidas, así que por ahora no se toca `M_Tripo_Master`; ajustarlo sigue abierto. El brillo al prender (GlowStrength 0.6) casi no se nota de día.
- Visto de paso: los minions usan el material por defecto (tablero gris) y el pasto no tiene textura.

![Carro con cohete en partida, piezas de Tripo](devlog/2026-11-14-tripo-en-partida-carro.png)

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