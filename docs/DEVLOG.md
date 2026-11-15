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
- **Cuándo:** 13 nov decidir dónde y quién; 14 nov crear la página y publicar lo que haya; **antes de enviar la entrega (15 nov, 19:59 CDMX) el link debe existir y abrir sin iniciar sesión**.
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

## 15 nov (tarde) — Mochila plegable, daño visible y piezas destructibles (Javier + Codex)

- **Pedido de Javier:** «un botón para ocultar el inventario». Codex añadió **I** y un botón de mostrar/ocultar (Alt+clic): plegado queda una barra pequeña con la cantidad. Se probó la tecla I y que no altere el estado mientras se escribe con B. [Captura de la mochila plegada](devlog/inventory-collapsed-15nov.png).
- **Cómo usamos la IA para depurar:** el encargo fue observar la visibilidad dentro del impacto real, sin llamar al HUD desde el test. Un observador privado de `OnHealthChanged` registró ocho impactos: el propio proyectil bloqueaba `Visibility` durante el daño; al ignorar flechas, la línea quedaba libre. La corrección hace que la flecha ignore solo ese canal, manteniendo colisiones de vuelo e impactos. Tras recompilar, los disparos reales produjeron avisos en cola y su dibujo en el HUD; el diagnóstico queda apagado por defecto.
- **Piezas con vida activadas:** antes de cambiar el valor por defecto se probaron minions enemigos rompiendo piezas. Después se verificaron por separado daño propio/aliado bloqueado, salida del asiento roto y de la raíz rota con colisión, movimiento y 150 de vida intactos. X devolvió solo las piezas vivas: con cuatro ruedas rotas devolvió una tabla y un lanzallamas, ninguna rueda; con raíz rota no devolvió piezas. Se conserva `jam.Build.PartHealth 0` para construcciones nuevas.
- **Física y límites:** la pasada de controles reales registró movimiento de carro, tanque, hovercraft y rueda única, y separación de la bola de catapulta de más de cinco metros respecto a su raíz. El avión subió y después cayó; eso no acredita vuelo controlado. El respaldo por palabras clave se comprobó por separado; las pruebas rechazan inventarios que ya no coinciden con el cofre inicial, sin rellenarlos con comandos.
- **Entrega y medios:** a petición de Javier se delegó un tráiler con gameplay real y voz local inglesa Microsoft David Desktop (Windows SAPI). Se descartó una captura de escritorio que mostraba otra ventana; después se usaron fotogramas reales y una captura de escena de Unreal para fotos limpias. La cadencia de exportación es limitada y queda documentada; no se generaron imágenes falsas de gameplay.
- **Integración:** cambios en Core, Combat y Building con la autorización de Javier para todas las áreas. El editor compiló correctamente; el cambio de header del HUD requiere recompilar.

## 15 nov (tarde) — Pedidos de una sola rueda y pruebas con inventario real (Javier + Codex)

- **Cómo usamos la IA:** Codex revisó las nueve frases rápidas con el cofre inicial real de 39 piezas, sin rellenar el inventario entre pedidos. El encargo fue conservar las capacidades y cantidades pedidas, y separar evidencia de receta de funcionamiento físico.
- **Qué falló y se corrigió:** una torreta láser con bloque y emisor se enriquecía como si estuviera incompleta; ahora conserva esa combinación válida. La palabra «single» no se contaba y una sola rueda terminaba convertida en carro; ahora se conserva la rueda única y su accesorio, también al reparar la receta. Los numerales ya no cruzan «con/with/que/that» ni reinician cantidades de piezas no mencionadas.
- **Coherencia:** el cálculo del hover usaba la posición del emisor aunque la fuerza real se aplica al centro de masa de la raíz. Se corrigió el cálculo, sin cambiar la física ni rebajar los umbrales. El hovercraft pasó de 69 a 100; la rueda con un lanzallamas conserva su aviso real de desequilibrio (69), pendiente de evaluación física.
- **Validación y límites:** compilación del editor correcta y nueva pasada de nueve pedidos: torreta láser con un bloque y un emisor, hovercraft completo y rueda única conservada. La catapulta conserva resorte, bola, viga y bloque, con un ajuste de receta todavía visible. Estas comprobaciones no acreditan por sí solas que los nueve vehículos se conduzcan o vuelen. En una prueba separada, un minion enemigo real rompió una rueda y un lanzallamas, sin aplicar daño desde el script; el jugador montado conservó 150 de vida. La salud de piezas sigue apagada por defecto hasta terminar expulsión y reembolso.
- **Integración:** cambios de Building realizados con la autorización de Javier para todas las áreas. El primer enlace de la DLL falló por otra instancia abierta del editor; al cerrarla, el enlace terminó correctamente.

## 15 nov (tarde) — Relevo a Codex, Prompt & Conquer y selección de audio (Javier + Codex)

- **Dirección y relevo:** Javier eligió el título **Prompt & Conquer** y el equipo **cavinos**, en minúsculas; el zip previsto es `cavinos_PromptAndConquer.zip`. Al agotarse la suscripción de Claude, Codex retomó la sesión desde Orca, leyó el estado real del repo y continuó el trabajo del equipo con el encargo de terminar el juego y preparar la entrega.
- **Cómo usamos la IA en arte:** con la herramienta integrada `image_gen`, Codex generó una hoja de seis íconos: viga, bola, vela, hielo, luz y estabilizador. El pedido fue mantener el estilo de juguete, las seis piezas reconocibles y el fondo transparente. Se verificó el alfa real y se recortó la hoja a seis PNG de 128×128, conservando los 25 anteriores: **31 íconos**. Las seis texturas nuevas quedaron importadas y guardadas como UI, sin mipmaps.
- **Audio dirigido por Javier:** comparó las tomas de Stable Audio 3 y aprobó siete efectos: `SFX_Build_Pop` v1, `SFX_Hit` v3, `SFX_Sword_Swing` v1, `SFX_Explosion` v2, `SFX_Tower_Shot` v1, `SFX_Tower_Destroyed` v1 y `SFX_Chest_Open` v4. Se preservaron los originales y el editor confirmó la importación y guardado de los siete reemplazos. Javier mantuvo la música de partida, el pickup y la fanfarria originales. Para el cofre pidió rehacer las primeras tomas, eligió la v4 corta (1.05 s) y se importó sobre el asset existente con lock. Crédito: "Sound effects made with Stable Audio 3 (local). Powered by Stability AI."
- **Qué falló y cómo se arregló:** generar audio y compilar a la vez agotó la memoria disponible de Windows: fallaron la carga de pesos (`MemoryError`) y la compilación (`C3859` / error 1455). Se separaron ambas operaciones y el cargador pasó a leer los pesos tensor por tensor con `mmap`, evitando cargar dos copias completas en RAM. La tanda de tres cofres terminó en **40.4 s**, incluida la carga del modelo.
- **Idioma y reglas:** Javier pidió la interfaz completa en inglés. Después se recuperó el PDF oficial y se confirmó que §8(i) lo exige para juego, audio, video e información de entrega. Se tradujeron los textos del jugador y se ampliaron aliases ingleses conservando los españoles. Los ejemplos son `an ice rifle` y `a car that burns everything`.
- **Verificación (16:00):** importación de siete sonidos y seis texturas confirmada; compilación del editor y Shipping correctas. En PIE se verificaron el título inglés, pausa real hasta Enter, cofre con exactamente 39 piezas y carro pedido en inglés con sus cuatro ruedas por OpenAI. Siguen pendientes la prueba del paquete, las nueve frases completas y la rotura/eyección con salud de piezas (desactivada por defecto). La prueba automatizada de puntería aún debe converger la cámara antes de atribuir un fallo a los proyectiles.
- **Otra corrección:** el carro quedaba cerca del borde y del corredor de la primera oleada. Se cambió la colocación para comprobar apoyo en toda una huella y buscar un punto más adentro o lateral; compiló y falta verificar su estabilidad en el paquete. No se modificó el freno físico existente sin evidencia.

![Pantalla de título en inglés: objetivo, controles y Enter para empezar con la partida pausada](devlog/2026-11-15-title-english.png)

## 15 nov (tarde) — La OpenAI Decisions API elige las piezas, loot con íconos de Codex y piezas con vida (Javier + Claude Code + Codex)
- **Qué:** la capa de decisión con clave ahora es la **OpenAI Decisions API** (`gpt-6-luna`): un modelo que no escribe texto, solo contesta preguntas cerradas. Por cada pedido de construcción le mandamos ~59 preguntas ("¿lleva ruedas?", "¿qué pieza es la raíz?") y por cada arma 6 (cuerpo, boca y extras de `Weapons.json`); el código valida, usa tu inventario y arma con física. El HUD dice "OpenAI eligió: Tabla, Asiento, Rueda x4, Lanzallamas". Medido en PIE: 0.7 s el carro y 1.35 s el rifle. Además: avisos de loot con ícono y cantidad, tira de mochila con íconos, faro de color sobre cada cofre, etiqueta ENCENDIDA/APAGADA sobre tu construcción y **piezas con vida** (apagadas por defecto hasta probarlas más).
- **Cómo usamos la IA:** Claude Code de director con tres subagentes a la vez (Fable para OpenAI y para las piezas con vida, Sonnet para el audio con Stable Audio 3 y los textos de entrega) y un **worker de Codex** lanzado por Orca para la UI de loot. Codex generó los 25 íconos con su herramienta de imagen en **2 generaciones** (dos hojas de 4×4 recortadas con Pillow) en vez de 25. Antes de escribir código, el director hizo una llamada real a la API para fijar el contrato (la guía oficial manda, no los resúmenes de segunda mano).
- **Qué falló y cómo se arregló:** con "un carro que queme todo", la pregunta "¿lleva lanzallamas (dispara fuego)?" hacía que el modelo **se negara 3 de 3 veces** (respuesta `refusal` solo en esa pregunta). Con un mensaje de contexto ("juego de construcción con piezas de juguete, efectos de juego") y "pieza de juguete" en cada pregunta: 0 negativas en 15 llamadas. Por si acaso, una negativa en una pregunta la decide la palabra clave. Otro: compilamos mientras un subagente iba a la mitad (el `.h` ya declaraba funciones que el `.cpp` aún no tenía) y falló el enlace; se le pidió dejar primero una versión que compile.
- **Qué aprendimos:** un modelo de decisión encaja con nuestra regla ("el modelo elige, el código decide lo posible") mejor que un LLM que escribe JSON: no hay que parsear texto libre y cada respuesta trae su probabilidad. Las negativas existen también en preguntas de sí/no: el contexto de juego importa.

![Cofre inicial: cada cosa que entra sale con su ícono y la mochila se llena abajo](devlog/2026-11-15-loot-iconos-mochila.png)
![OpenAI eligió las piezas del carro; encima, la etiqueta APAGADA - C para encender](devlog/2026-11-15-openai-eligio-carro.png)

## 15 nov (madrugada) — Noche con un director y subagentes: arma pedida, HUD legible, ciclo arreglado y el juego suena (Javier + Claude Code)
- **Qué:** sobre el GDD v2, en la rama `noche/15nov` (PR a `main`): **arma de mano pedida** ("un rifle que dispare hielo", "una escopeta de fuego"; sin clave decide por palabras de `Weapons.json`, se arma visible en la mano con piezas y dispara con el efecto de su boca gastando energía), **HUD legible** (objetivo por pasos arriba, avisos del asistente abajo en vez del canal de depuración que Shipping no muestra, ayuda cerrada), **ciclo arreglado** (X desarma solo lo tuyo, renaces con la mitad, E solo sube a lo tuyo), **audio** (23 efectos y 3 músicas conectados), pantalla final "tu corte final" con R para la revancha y 10 piezas de arma nuevas con Rodin.
- **Cómo usamos la IA:** Javier dejó un handoff con tareas, reglas "para no congelarse" y una tabla de qué modelo usar para qué, y se fue a dormir. Claude Code hizo de **director**: repartió el código en subagentes con archivos distintos (Sonnet para HUD, avisos, ciclo, audio y pantalla final; Fable para el arma, que era lo más difícil), integró, compiló y probó cada parte en PIE por MCP antes de subirla. Al final, otro subagente Fable revisó el diff completo "con ojos frescos" (solo lectura) y encontró 6 detalles de lógica que se corrigieron antes del paquete.
- **Qué falló y cómo se arregló:** la tecla 1 ("un carro que queme todo") salía **sin lanzallamas**: la palabra clave `quem` mide 4 letras y el comparador trata esas como palabra exacta, así que "queme" nunca coincidía; se agregaron las formas completas. Un subagente editó con Python y dejó fines de línea `\r\r\n` (líneas duplicadas): se detectó con un script antes de compilar. El arma salía demasiado delgada para la cámara en tercera persona: con las piezas de Rodin y `hand.scale` en el JSON (sin compilar) ya se lee. Los sonidos "en un punto" se oían en todo el mapa porque los WAV no traen atenuación: ahora el volumen baja con la distancia.
- **Qué aprendimos:** repartir por **dueño de archivo** (no por tarea) evita choques entre agentes; "compila" no es "funciona": cada parte se probó en PIE con capturas y el log (`JamMsg:` / `JamSfx:`), porque de noche nadie puede oír el juego ni mirar la pantalla.
- **Pendiente:** las decisiones que quedaron para la mañana están en el PR (cofre inicial más grande para que las 9 frases salgan completas, Development vs Shipping por el aviso del firewall de Windows).

![Rifle de hielo pedido por texto, armado con piezas de Rodin en la mano](devlog/2026-11-15-arma-pedida-rifle-hielo.png)
![Escopeta de fuego: el muñeco de práctica se quema](devlog/2026-11-15-escopeta-fuego-muneco.png)
![HUD: objetivo por pasos y lo que dio el cofre](devlog/2026-11-15-hud-objetivo-cofre.png)
![Pantalla final "tu corte final" con la creación más usada y R = revancha](devlog/2026-11-15-corte-final.png)

## 14 nov — Revisión del GDD contra el código y GDD v2 (Javier + Claude Code)
- **Qué:** revisamos el GDD v1 punto por punto contra el código y los datos, no contra sus casillas. Tres agentes Explore de solo lectura cubrieron partida/IA/combate, loot/energía/balance y construir/HUD/arquitectura; lo más grave se comprobó a mano.
- **Hallazgos:** los avisos del cofre inicial y del asistente usan `AddOnScreenDebugMessage`, que la build Shipping no muestra; X borra todas las construcciones del mapa; al morir se renace con 0 piezas; las piezas no tienen vida; el juego no suena porque el código busca los sonidos en una carpeta donde no hay nada importado.
- **Decisión** (Javier, único integrante activo esa noche, con el equipo avisado): GDD v2. Entran las armas de mano pedidas a la IA, el objetivo siempre en pantalla y los primeros 2 minutos guiados; quedan fuera el juego en red y el segundo modo. Se conservan la regla de construcción y el contrato de receta JSON (`CLAUDE.md` §0.1).
- **Cómo usamos la IA:** Claude Code con el prompt *"Vamos a revisar docs/GDD.md punto por punto: qué está bien, qué quedó mal, qué sí se desarrolló en el código y cómo mejorarlo"*. El formato nuevo (✅/❓, números en los datos, "En el código" y "Hecho cuando") viene del que Javier usa en sus proyectos.
- **Qué falló:** el GDD v1 tenía números objetivo que ya no coincidían con el código y casillas `[x]` viejas. En la v2 los números viven en los datos.
- **Qué aprendimos:** una casilla marcada no prueba nada; antes de decidir, leer el código.

## 14 nov — Espadazo y tiro con arco sin clips nuevos: los huesos se giran en C++ (Cesar + Claude Code)

- **Qué:** las figuras de Rodin traen un solo clip de ataque, un jab con la izquierda, y uno de lanzar en lugar de tensar el arco. Con la espada en la izquierda la hoja también atravesaba el cuerpo al correr. Ahora la espada va en la derecha y el espadazo, el tiro con arco y la espada "cargada" se hacen girando hombro, codo y mano en C++ encima del clip de caminar.
- **Cómo:** `UJamFigureAnim` hereda de la instancia de animación suelta y no necesita Anim Blueprint. Después de evaluar el clip, cada pose se describe como **direcciones** en el personaje (hombro→codo, codo→mano, hoja). El código busca la rotación mínima que lleva cada hueso a esa dirección, así que no depende de los ejes del rig.
- **Cómo se probó:** el agente congela el golpe en un avance fijo (`jam.Figure.Freeze`) y saca capturas de frente y de lado con `Jam.Cam` y `Jam.ShotIn`. Así afinó las poses de carga, golpe, remate, tensado y soltado sin tocar la pantalla del usuario.
- **Hitbox de la espada (segunda vuelta):**
  - Antes el daño entraba al apretar, antes de que la hoja llegara. Ahora entra cuando la hoja cruza al frente (0.19 s); la flecha sale cuando la mano suelta la cuerda (0.17 s).
  - El golpe cubre un arco de ±75° frente al cuerpo y 2 m hasta el borde del blanco (las torres grandes cuentan desde su borde).
  - Ya no pega por la espalda ni a través de paredes: se revisa la línea de vista.
  - El personaje voltea a donde apuntas mientras ataca y el torso gira con el tajo.
  - Al pegar, el jugador siente una pausa de 0.06 s.
  - **Qué falló:** con la cámara fija de pruebas (`Jam.Cam`), el personaje volteaba hacia donde miraba la cámara y no hacia donde apunta el control. En la primera prueba le pegó al muñeco de atrás. Se vio en el mosaico de capturas con `jam.Sword.Debug 1` (dibuja el arco y los impactos).
- **Cuerpo natural (tercera vuelta):** sin Anim Blueprint, la misma instancia cruza los clips en 0.2 s (antes saltaba de quieto a correr o a recibir golpe), inclina el tronco al acelerar y hacia la curva, y voltea cuello y cabeza a donde mira el control. Aplica a todas las figuras (jugador, minions, bots, guardias); `jam.Figure.Natural 0` lo apaga.
- **Trampa evitada:** `PlayAnimation` cambia la malla a modo de animación suelta y borra cualquier instancia propia. Ahora los clips se cambian directo en la instancia.

## 14 nov — Arco, asiento y muerte de las figuras afinados en PIE (Javier + Claude Code)

![Arco antes y después, jugador sentado en el carro y muerte a 1.1, 1.6 y 2.3 s](devlog/2026-11-14-personajes2-arma-muerte.png)

- **Qué:** después de que César pegó las armas a la mano, Claude Code revisó en PIE la espada, el arco, la vista V, el asiento y la muerte. Los valores se cambiaron en `Characters.json`, sin recompilar.
- **Cómo, sin tocar la pantalla de Javier:** todo se hizo con scripts de Python del editor vía MCP. Uno calcula el giro del arco **en el hueso de la mano** a partir del giro deseado respecto al personaje y lo prueba con `Jam.Grip`. Otro mide en la pose `Sit` a qué altura queda la pelvis sobre la raíz de cada figura. Las capturas a tiempos exactos salen con `Jam.ShotIn` (0.6, 1.1, 1.6 y 2.3 s después de morir).
- **Qué se arregló:**
  - El arco iba de canto y detrás del brazo; desde la cámara normal solo se veía la cuerda. Ahora va inclinado hacia afuera.
  - El bot héroe tiene escala de actor 1.1, así que con el mismo ajuste de asiento que el jugador se hundía ~9 cm. Ahora tiene su propio valor.
  - La muerte del jugador y del bot héroe no se veía porque el código los escondía al instante. Ahora se esconden 2 s después (con aviso a César, dueño del código).
- **Qué falló en las pruebas:** la primera medición de la pose sentada salió igual que de pie. Las figuras invocadas fuera de cámara no actualizan huesos (`OnlyTickPoseWhenRendered`); con `AlwaysTickPoseAndRefreshBones` coincidió con lo medido en el carro (49.8 contra 49.7 cm). La primera toma de la muerte la tapó el cofre de botín, grande porque el jugador llevaba 20 piezas de cada tipo.
- **Build Shipping** (la del jurado) de prueba: compila y arranca a la partida con figuras y mapa. En Shipping `-ExecCmds` no corre, así que la prueba fue solo visual, con captura de la ventana desde fuera.

## 14 nov — Bots que manejan sus carros, trampas de lanzallamas y armas en la mano (Cesar + Claude Code)

- **Bots:** el Constructor ya no deja su carro solo: se sube y lo maneja con las mismas reglas que el jugador (giro diferencial, par de guiñada, ayuda de estabilidad). El Peleador pone torretas de lanzallamas/hielo a un lado del carril que se encienden al pasar enemigos. Todo por APIs públicas de Construcción.
- **Cómo se probó:** el agente corrió partidas de 2-4 min leyendo un log del piloto cada 3 s (posición, velocidad, inclinación, error de rumbo). Así salió por qué se volcaban (los carros se armaban en el aire y caían de cabeza; el piso fuera de la pradera central es angosto) y un escalón invisible en X=±500 que se resolvió con un brinco del carro.
- **Armas en la mano:** espada y arco van pegados al hueso de la figura de Rodin. Para encontrar el agarre, el agente hizo comandos de prueba (`Jam.Grip`, `Jam.Cam`, `Jam.Later`) y comparó 6 ángulos por corrida en mosaicos de capturas, sin tocar la pantalla del usuario.
- **Qué falló:** la animación `Attack` resultó ser un jab con la **izquierda**; con la espada en la derecha nunca golpeaba. Se pasó a la mano izquierda y quedó como estocada.

## 14 nov — Personajes rigeados con Rodin: jugador, minion, bot héroe y guardia (Javier + Claude Code)
- Qué: los cilindros grises pasan a 4 personajes con esqueleto y 10 animaciones cada uno (quieto, caminar, correr, golpe, lanzar, recibir golpe, morir, sentado…), con variante roja y azul (el guardia neutral en amarillo). Siguen `docs/ART.md` §6: el jugador con gorro, bufanda y chaleco; el bot héroe con casco de gogles; el minion sin accesorios; el guardia con casco de chatarra.
- Cómo con IA: Claude Code generó cada modelo en **Rodin por MCP** (solo texto, en pose A y sin armas para que el rig salga limpio) y luego lo rigeó también con Rodin (`rodin_generate_rigging`, esqueleto con los nombres de UE5 y animaciones "en el lugar" del catálogo). Midió en Blender la velocidad del pie apoyado de cada ciclo para que no patinen, y un importador de Python lo pasó al editor. El azul sale de recolorear solo la ropa roja de la textura (`team_recolor.py`).
- Decisión de equipo: el arte es de Javier y el código de César. Para no frenarlo antes del freeze, todo lo ajustable (malla, material por equipo, escala, giro, velocidades) vive en `Characters.json`. El código solo lo lee y, si falta algo, deja el cilindro. El pedido está en `docs/ARTE-TRIPO.md`, y César ajustó el empaquetado en minutos.
- Qué falló: el primer rig se rechazó ("el esqueleto debe tener un hueso Hips"). La plantilla con nombres de UE no acepta esas animaciones, así que se rigea con la plantilla humana genérica y la exportación renombra los huesos. El importador olvidó guardar el esqueleto y las animaciones quedaron huérfanas. La versión azul del jugador salió con manchas moradas, porque su rojo es más carmesí y caía en la zona de mezcla del recoloreo; ahora se mide el tono de cada personaje.
- Captura: es una **vista previa**. Un script del editor les puso las figuras al jugador y a los minions en partida antes de que existiera el código del juego. César subió `UJamCharacterLook`, que lee el JSON, en menos de una hora tras el pedido.

![Antes y después de los personajes](devlog/2026-11-14-personajes-rodin.png)

---

## 14 nov — Props del mapa con Rodin: árboles, rocas, campamentos y molino (Javier + Claude Code) — provisional
- Qué: las 128 mallas de decoración del mapa (árboles, matas de conos, hongos, flores, rocas, faroles, tiendas, letreros, los 4 campamentos y el molino) pasaron de primitivas de color plano a 25 modelos texturizados que se reusan, sin mover ningún actor del mapa.
- Cómo con IA: Claude Code agrupó las mallas por su número de triángulos (las 82 `P_###` eran solo 9 formas), las renderizó en Blender para saber qué era cada una y generó los 25 modelos en **Rodin por MCP** (solo texto, con los hex de la paleta de `docs/ART.md` en el prompt; 12.5 créditos). Después, Blender headless encajó cada modelo en la caja de la malla vieja (6 en paralelo, 3.5 min para 115) y el importador las reemplazó en su lugar desde el editor.
- Qué falló: encajar exacto en la caja vieja estiraba las copas de los árboles hasta 60 %; ahora cada modelo conserva su altura y sus proporciones (`fit: height`) y se gira 90° si así deforma menos. Las aspas viejas del molino flotaban sobre la torre: se usó un molino completo y se ocultaron (sin borrarlas). Una lista con finales de línea de Windows hizo que `git lfs lock` bloqueara rutas con un `\r` al final, que no existen: el editor no pudo guardar y hubo que liberar 115 bloqueos fantasma por ID. Y un comodín de nombre (`T_MAP_Dressed_P_*` también atrapa `P_001…P_081`) dejó a 2 modelos sin textura en la corrida en paralelo: los arbustos salían grises y brillantes. Lo detectó la captura en partida, no los números.
- Qué aprendimos: con `\r` el bloqueo sale "exitoso" pero no protege nada; siempre verificar con `git lfs locks` y que el archivo quede escribible. Costo: ~100k triángulos entre las 128 mallas; la GTX 1650 sostiene 60 FPS en el spawn (GPU 11.5 ms, antes 9.8) y la vista de los cerros cuesta ~24 % más.

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
