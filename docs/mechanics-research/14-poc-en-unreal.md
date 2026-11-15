---
title: 14 — Traspaso a la sesión MacBook y PoC en Unreal
date: 2026-11-13
tags:
  - research
  - construccion
  - poc
  - handoff
status: borrador
---

# 14 — Traspaso a la sesión MacBook y PoC en Unreal

> [!info]
> Documento de traspaso entre dos sesiones de Claude Code del mismo usuario (Jesús Daniel, dueño de Construcción).
> La sesión del **Mac mini** orquesta y escribe código/datos **sin poder compilar** (no hay Unreal ahí). La sesión de la
> **MacBook Air M5** es donde vive el editor de Unreal abierto y el MCP de Unreal: **ahí se compila, se prueba y se ve**.
> Etiquetas: **[code]** = existe en el repo · **[sin verificar]** = escrito pero no compilado ni probado en el editor.

> [!tip] Qué tiene que lograr la sesión MacBook
> Compilar el proyecto en macOS, abrir el editor y comprobar, con el MCP y los comandos `Jam.*`, que **a partir de lenguaje
> natural se arman estructuras (carro, cohete, llamas, torretas…) y que las propiedades de las piezas se traducen en
> mecánicas funcionales**. Cualquier fallo de compilación o de comportamiento se corrige en `Source/GameJamm/Building/`
> y se reporta con el log real.

## 1. Meta del usuario (resumen fiel)

| Punto | Detalle |
|---|---|
| Idea | Usar el modelo de decisión (Jev) con el mapa de elementos y mecánicas de la referencia ([01](01-mecanicas-construccion.md), [03](03-mecanicas-vehiculos-dispositivos.md), [13](13-conocimiento-ia-construccion.md)) como mecánicas principales |
| Entrada | Lenguaje natural: "un carro", "un cohete", "unas llamas"… cualquier cosa que el catálogo pueda componer |
| Salida | Estructuras armadas con piezas físicas que **funcionan** (se mueven, disparan, queman) por propiedades, no por objetos cerrados |
| Dónde | En Unreal, con el MCP, en la MacBook Air M5 |
| Insumos | Toda la carpeta `docs/mechanics-research/` y `Content/Game/Building/Data/BuildKnowledge.json` |
| Autoridad | Desde el 14 nov 2026 la dirección del diseño la fija [docs/GDD.md](../GDD.md) v2; la regla de construcción de Jesús Daniel sigue como base ([CLAUDE.md §0.1](../../CLAUDE.md)) |

## 2. Reglas que no se rompen

| Regla | Detalle |
|---|---|
| Quién decide | Jev elige (arquetipo, extras sí/no, dirección); el **código** valida, usa inventario, sustituye y pide materiales |
| Fallback | Siempre hay camino offline (palabras clave). Probar **con y sin clave** |
| Datos | Piezas, plantillas y energía viven en `BuildKnowledge.json` y el catálogo |
| Contratos | Formato JSON de receta, `UHealthComponent`, `EJamTeam`, `UInventoryComponent`: no cambian sin avisar |
| Claves | Nunca al repo: env `TYPESAFE_API_KEY` o `Config/DefaultSecrets.ini` (`[Jam] DecisionsApiKey`) |
| Contenido público | No nombrar el juego de referencia en archivos, comentarios ni commits |
| Git | `git pull --rebase --autostash`; commits `build: …`; nunca `git add -A`; solo código que compila va a `main` |

## 3. Estado al escribir esto

| Qué | Estado |
|---|---|
| Catálogo de piezas, ensamblador, spawner, cliente Jev/OpenRouter/fal | **[code]** en `main` |
| Rama `build/recipe-hardening-v2` (parser robusto, layout sin piezas encimadas, tests) | Sin integrar a `main` |
| Solver por capacidades (inventario, sustituir, reducir, pedir materiales) + comandos `Jam.Build`, `Jam.Inv*`, `Jam.Clear`, `Jam.Prompts` | **[sin verificar en Unreal]** en `build/poc-solver` (subida; Python: 72–73 casos, 0 fallas; el C++ del solver compila con clang contra un mock y pasa 664 comprobaciones) |
| Comportamientos (rueda raíz, daño de llamas/cañón/frío, energía, asiento, Turntable/Head/Stake/Spring) | **[sin verificar]** en `build/poc-devices` (subida; lógica pura probada en Python: 75 casos, 0 fallas; no compilada) |
| Comandos de consola `Jam.*` para ver todo en el editor | **[sin verificar]**, ver §5 |
| Piezas 3D reales (FBX con sockets) | **[code]** `Content/Game/Art/` (Emilio); el PoC usa mallas básicas |

Las ramas se confirman en §4 cuando el Mac mini termina de subirlas.

## 4. Ramas a traer y orden

| Orden | Rama | Contiene |
|---|---|---|
| 1 | `build/poc-integracion` | `main` + `build/recipe-hardening-v2` (base común) |
| 2 | `build/poc-solver` | Base + solver + `Jam.Build`, `Jam.Inv*`, `Jam.Clear`, `Jam.Prompts`, `Jam.Reload` |
| 3 | `build/poc-devices` | Base + piezas con comportamiento + `Jam.Dummy`, `Jam.Energy`, `Jam.Seat`, `Jam.Debug`, `Jam.Device`, `Jam.Power` |
| 4 | `build/poc-final` | **Las dos anteriores unidas** (un conflicto solo en este doc 13, resuelto) |

Atajo: si se quiere ir rápido, compilar directamente `build/poc-final`; si falla la compilación, bajar a las ramas 2 y 3 por separado para aislar de qué lado viene el error. Ver detalle de cada lado en [14-handoff-solver.md](14-handoff-solver.md) y [14-handoff-dispositivos.md](14-handoff-dispositivos.md) (vienen dentro de las ramas).

> [!warning]
> Al unir las ramas, `Head` pasó a existir en el catálogo: el caso del solver "torreta que apunta sola" ya no devuelve `Head` como "aún no existe" sino como material faltante si no está en el inventario. Se actualizaron la prueba de Python y el espejo `JamSolverCases.inl`.

## 5. Runbook para la sesión MacBook

1. `git fetch --all` y `git checkout build/poc-integracion`. Pull cada minuto ([CLAUDE.md §0](../../CLAUDE.md)).
2. Confirmar herramientas: Xcode instalado, versión del motor y la ruta de la raíz del motor (el MCP o `~/Library/Application Support/Epic` la indican).
3. Generar proyecto y compilar el target de editor para Mac (`GameJammEditor`, `Mac`, `Development`). Si falla, copiar el **primer** error real, corregir solo en `Source/GameJamm/Building/` y repetir. Si falta un módulo, agregarlo a `GameJamm.Build.cs` y decirlo en el commit.
4. Abrir el editor, abrir un mapa vacío o `Content/Developers/<nombre>/L_Test_<nombre>`, y comprobar el MCP (listar actores del nivel).
5. Correr los tests automáticos filtrando por `Jam` (`Automation RunTests Jam` en la consola del editor, o la ventana Session Frontend). Anotar los que fallan.
6. `git checkout build/poc-final` (o `build/poc-solver` para aislar), recompilar, Play (PIE), abrir la consola con `~` y correr la tabla de §6 sin clave de Jev (respaldo por palabras clave).
7. Poner la clave de Jev (variable de entorno, **no** en el repo), reiniciar el editor y repetir un subconjunto para comparar Jev contra el respaldo (el resumen en pantalla dice cuál usó).
8. Con `build/poc-final` (o `build/poc-devices` para aislar), repetir §6 con `Jam.Dummy`, `Jam.Energy`, `Jam.Seat`, `Jam.Debug 1`.
9. Corregir lo que falle, commits chicos `build: …`, pull/rebase y push a la rama de trabajo. **No** empujar a `main` código que no compile y no haya pasado §6.
10. Reportar al usuario: qué compiló, qué pasó en cada fila de §6 y qué quedó fallando, con el texto del log.

## 6. Tabla de aceptación (PoC visible)

Preparar inventario con `Jam.InvAll 10`. El estado real de cada fila se anota al correrla.

| # | Comando | Debe verse | Mecánica que prueba |
|---|---|---|---|
| 1 | `Jam.Build un carro` | Chasis, 4 ruedas y asiento; avanza al activarse | Tracción por ruedas |
| 2 | `Jam.Build un carro con cohete` | Lo anterior + cohete trasero; impulso fuerte unos 3 s | Empuje por cara |
| 3 | `Jam.Build un cohete` | Sube y se apaga | Empuje de un uso |
| 4 | `Jam.Build un avión con alas y ventilador` | Vuela o planea | Sustentación y empuje |
| 5 | `Jam.Build una rueda acostada con dos lanzallamas que gire y queme todo` | Gira sobre sí misma; con `Jam.Dummy` los blancos pierden vida | Rueda raíz + daño de fuego |
| 6 | `Jam.Build una torreta que apunte sola` | Gira hacia el blanco más cercano y dispara | Apuntado + cañón |
| 7 | `Jam.Build un globo con fuego debajo` | Sube más rápido que sin fuego | Interacción fuego–globo |
| 8 | `Jam.Build un tanque` | Oruga/ruedas, cañón, avanza | Plantilla compuesta |
| 9 | `Jam.InvClear` y `Jam.Inv Plank 2` y `Jam.Build un carro` | Mensaje de sustituciones y materiales que faltan, sin romperse | Solver con inventario pobre |
| 10 | `Jam.Build` con más de 21 piezas pedidas | Recorta a 21 y lo avisa | Límite de piezas |
| 11 | `Jam.Energy 20` y mantener encendido algo que gaste | La barra baja y todo se apaga al llegar a 0 | Energía tipo maná |
| 12 | `Jam.Seat` sobre un carro | Se monta y dirige con WASD | Asiento y dirección |
| 13 | `Jam.Build invoca un dragón` | No se rompe: usa la receta más parecida y lo dice | Fallback |

## 7. Qué reportar y cómo

| Dato | Detalle |
|---|---|
| Compilación | Éxito o primer error real con archivo y línea |
| Tests | Lista de pasados y fallados |
| Tabla §6 | Estado por fila: ok / falla (con log) / no probado |
| Jev | Si se usó clave y si Jev y el respaldo coinciden |
| Cambios hechos | Commits y ramas en el remoto |

## 8. Hallazgos de la sesión Mac mini

| Tema | Hallazgo |
|---|---|
| Modelo de decisión | "Jev" es el modelo de decisiones de TypeSafe AI (elige entre opciones definidas; no genera texto). El cliente ya existe en `JamDecisionClient` con tres rutas: TypeSafe directo (`TYPESAFE_API_KEY`), OpenRouter y fal. Es una arquitectura distinta a los modelos predictivos de investigación: no hay que integrar nada más para el MVP |
| Rama sin integrar | `build/recipe-hardening-v2` (parser que repara JSON del LLM, layout sin piezas encimadas, tests) no estaba en `main`; se unió sin conflictos en `build/poc-integracion` **[sin compilar]** |
| Qué falta de código | Las marcas **[todo]** de [13](13-conocimiento-ia-construccion.md): rueda raíz que gira, daño real de llama/cañón/frío, Turntable, Head, Spring, Stake, energía y montarse en el asiento |
| Qué falta de verificación | Nada de lo nuevo se ha compilado: el Mac mini no tiene Unreal. Solo hay validación en Python (datos y algoritmo) |
| Créditos del concurso (aplican a Construcción) | Claude $25 (reclamar 12–27 nov), fal $30 (hasta 30 nov), Meshy con API (hasta 17 nov), Tripo y Hyper3D para modelos 3D (hasta 16 nov). Una cuenta por persona; las claves nunca al repo |
| Contenido público | La convención de [FORMATO.md](FORMATO.md) prohíbe nombrar el juego de referencia; hubo una fuga en `CLAUDE.md` y se corrigió (queda en el historial de git) |
| Agentes | Dos agentes (solver y piezas) terminaron; sus ramas y archivos de contexto (`14-handoff-solver.md`, `14-handoff-dispositivos.md`) están en `build/poc-solver` y `build/poc-devices` |

## 9. Rama `build/poc-devices` (subida)

Detalle completo, contratos y riesgos en [14-handoff-dispositivos.md](14-handoff-dispositivos.md) (viene dentro de la rama).

| Tema | Resumen |
|---|---|
| Qué trae | Rueda raíz que gira, daño de llama/cañón/hielo/rayo, energía tipo maná, asiento (montar y dirigir), Turntable, Head, Stake, Spring y fuego→globo ×2 |
| Comandos extra | `Jam.Device <preset>`, `Jam.DeviceClear`, `Jam.Power`, `Jam.Dummy`, `Jam.Energy`, `Jam.Seat`, `Jam.Debug` (ver tabla del handoff, sirven sin `Jam.Build`) |
| Header change | `JamBuildPart.h` (valores nuevos en `EJamPartKind`, campo `EnergyPerSec`, API nueva): **recompilar todos** |
| Riesgos de compilación | `OverrideComponent1` debe ser público; `OverlapMultiByObjectType` con `AllObjects`; `SetConstrainedComponents` para anclar al mundo; reconfigurar constraints ya creadas; ambigüedades `float`/`double` |
| Pendiente de integración | Llamar `SetBuildOwner(Jugador)` tras `UJamAssembler::Assemble`; poner `UJamEnergyComponent` en el personaje y conectar HUD e input real; la plantilla `Spinner` debe poner los lanzallamas en `Front`/`Back` |
| No hecho | Daño por impacto a más de 8 m/s, hielo sobre agua, Hydrant, BigBattery, elevación de Head, tipo Shock |

## 10. Qué está verificado y qué no (honesto)

| Qué | Cómo | Estado |
|---|---|---|
| Datos de `BuildKnowledge.json` | `validate_knowledge.py` | 0 errores, 0 avisos (41 elementos, 16 plantillas) |
| Algoritmo del solver | `solver_proto.py` | 72 casos, 0 fallas en `build/poc-final` |
| Lógica numérica de piezas | `devices_proto.py` | 0 fallas (75 casos) |
| C++ del solver | clang++ contra un mock de Unreal (en `build/poc-solver`) | Compila y pasa 664 comprobaciones; **no es UnrealBuildTool** |
| C++ de piezas y rama unida | Sin compilador de Unreal; el mock no cubre los símbolos de `JamBuildPart` | **Sin compilar** |
| Pruebas de Automation, comandos `Jam.*`, física | Requieren el editor | **Sin probar** |

## → Aplicable a GameJamm

1. Traer `build/poc-integracion`, compilar en Mac y confirmar que el proyecto abre.
2. Correr la tabla §6 por ramas, en orden, y corregir en `Source/GameJamm/Building/`.
3. Integrar en `build/poc-poc` solo lo que compile y pase; avisar al usuario antes de tocar `main`.
4. Mantener `BuildKnowledge.json` como fuente única de datos y el fallback siempre activo.
5. Subir a este doc los resultados reales (cambiar `status` a `listo` cuando §6 esté probada).
