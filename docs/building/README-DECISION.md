---
title: Construcción por lenguaje natural — cómo decide el modelo y cómo agregar piezas
date: 2026-11-13
tags:
  - building
  - decision-models
  - construccion
status: listo
---

# Construcción por lenguaje natural: del prompt a la construcción

> [!info]
> Describe lo que **hoy está en el código** de `Source/GameJamm/Building/`: cómo un texto libre llega al modelo de
> decisión (Jev), qué tablas usa, cómo agregar un elemento y qué límites tiene. Diseño de fondo:
> [13-conocimiento-ia-construccion.md](../mechanics-research/13-conocimiento-ia-construccion.md) y
> [12-modelos-de-decision-jev.md](../mechanics-research/12-modelos-de-decision-jev.md). Resultados medidos:
> [REPORTE-POC.md](REPORTE-POC.md) (lo genera el comando `Jam.Test`).

> [!tip] Regla
> El modelo **no elige un objeto hecho** ("carro", "catapulta"): decide **qué piezas** forman lo que pide la persona y
> **cuántas**. El **código** arma con esas piezas: elige el chasis, lo alarga si hace falta, busca una cara libre para cada
> pieza, respeta el límite (60, hasta 100) y valida. La función sale de la **capacidad de cada pieza** (`EJamPartKind`) y de la
> cara donde va pegada: 25 ruedas son 25 ejes con motor, 4 cañones son 4 cañones.

## 1. Camino del prompt

| Paso | Dónde | Qué hace |
|---|---|---|
| 1. Texto o voz | Mantener `G` y hablar, `T` y escribir, teclas `1`-`9`, o `Jam.Say <texto>` | La voz se transcribe a texto (§6) y sigue el mismo camino |
| 2. Decisión por piezas | `FJamDecisionClient::RequestChoice` | Una llamada a Jev (`POST https://api.typesafe.ai/v1/systemone`, `jev-1.13.0`) con ~50 preguntas: por cada pieza `use_<Tipo>` (*noul*: ¿hace falta?), `n_<Tipo>` (*choice*: ¿cuántas?) en las contables, y `mode_lift` / `mode_launch` (*noul*) |
| 3. Numerales | `UJamBuildGenerator::ApplyNumerals` | "25 llantas", "cuatro cañones": el número del texto manda sobre la cantidad |
| 4. Composición | `UJamBuildGenerator::ComposeFromBag` | Bolsa de piezas → receta: raíz, asiento gratis si se mueve, pares espejados, filas y pilas, chasis que se alarga, armas sobre la cabeza o el plato si hay |
| 5. Respaldo | `BagFromKeywords` | Sin clave, sin red o tras 8 s: conceptos con nombre + palabras de piezas + numerales, compuesto igual por piezas |
| 6. Mundo | `UJamAssembler::Assemble` | Piezas físicas. Lo que no se mueve se **suelda** en un solo cuerpo; ruedas, plato, cabeza y bisagra van con constraint |
| 7. Encendido | Tecla `C` (o solo, en consola y pruebas) | Enciende todo lo que no es estructura, asiento ni batería |

**Cómo decide el modelo qué piezas hacen falta.** Cada pieza lleva su criterio en `PieceTable()`: no solo "el usuario la
nombra" sino "para qué cosas hace falta". Ejemplo, hélice: *"menciona un ventilador, o pide algo que vuela con hélices: dron,
helicóptero..."*. Así "un dron" da hélices + estabilizador y "una catapulta" da resorte + viga + bola + bloque + tabla, sin que
exista una plantilla de dron ni de catapulta. Medido en el editor con Jev real: ver [REPORTE-POC.md](REPORTE-POC.md).

**Límites medidos de Jev en esta tarea.** Acepta las ~50 preguntas en una llamada. Acierta bien "¿hace falta esta pieza?".
Falla en "¿cuántas?": contesta "1" con confianza alta aunque la frase diga 25; por eso la cantidad se toma del texto
(numerales) y la respuesta de Jev solo se usa si dice 2 o más con confianza ≥ 0.8. Si Jev devuelve menos de 3 tipos de pieza,
se completa con el concepto del respaldo.

Orden de claves: `TYPESAFE_API_KEY` o `[Jam] DecisionsApiKey` en `Config/DefaultSecrets.ini` (ignorado por git); después
OpenRouter y fal. Las claves nunca van al repo. Log: `LogJamDecision: backend=typesafe-jev ... pieces=N [...]` y
`LogJamBuild: OK N parts (jev pieces=[Plank x5, Seat x1, Wheel x25])`.

## 2. Las tablas

| Tabla | Archivo | Qué tiene |
|---|---|---|
| Piezas (35) | `JamBuildPart.cpp`, `Table()` | Tipo, capacidad (`EJamPartKind`), tamaño, masa, potencia, energía por segundo. Iguales a `Content/Game/Building/Data/BuildKnowledge.json` (el test lo verifica) |
| Reglas por pieza | `JamAssembler.cpp`, `PieceTable()` | Caras candidatas, pares espejados, cara por la que se encadenan las repetidas, cantidad por defecto, criterio sí/no para Jev, palabras para numerales y respaldo |
| Conceptos (15) | `JamAssembler.cpp`, `ArchetypeTable()` | **Solo respaldo sin red**: bolsas de piezas con nombre (carro, tanque, catapulta, dron...). Jev ya no las recibe |
| Extras (27) | `JamAssembler.cpp`, `ExtraTable()` | Origen de los textos, caras y palabras de las piezas no estructurales; ruta vieja plantilla + extras (respuestas simuladas, respaldo fal) |

**Patrones de colocación.** Ruedas, alas y piedras flotantes: pares espejados izquierda/derecha. Tablas, bloques, muros y
vigas repetidos: se encadenan (fila al frente = puente; pila hacia arriba = torre). Cuando ya no caben piezas, el chasis se
alarga con otra tabla adelante o atrás (un carro de 25 llantas usa 5 tablas). Modo `lift`: hélices y cohetes van abajo y
sustentan. Modo `launch`: el resorte va arriba y lanza.

**Caras.** El campo `socket` de la receta sigue siendo un texto con el mismo formato de siempre (`Top`, `Left_Front`...). El
ensamblador ahora acepta además un tercer dato opcional: `Cara_Borde_Mira`, por ejemplo `Top_Back_Back` = encima, pegado
atrás, mirando hacia atrás (un ventilador ahí empuja hacia delante). `Center` como borde = sin correrse. Las recetas viejas
funcionan igual. En `ExtraTable` cada entrada lista candidatas separadas por `|` y admite padre (`seat:Top`, `mount:Front`).

**Mount.** Una cabeza (`head`) o un plato giratorio (`spin`) se agrega con el id `mount`; las armas elegidas se montan sobre
él antes que en el cuerpo. Cabeza + cañón = torreta que apunta sola; plato + lanzallamas = aspersor.

## 3. Capacidades e interacciones (por pieza, no por plantilla)

| Capacidad | Piezas | Qué hace |
|---|---|---|
| Rueda | Wheel, BigWheel | Pegada a algo: motor de eje. **Como raíz**: gira sola y se mantiene de pie (rueda que rueda) |
| Empuje | Fan, Jet, Turbine, Rocket | Empujan al lado contrario de donde miran; el cohete dura 3 s |
| Sustentación | Balloon, HoverStone, Wing | Globo sube (el doble con fuego cerca); piedra mantiene ~80 cm sobre el suelo; ala sube con la velocidad |
| Viento | Sail | Viento global hacia +X del mundo; nunca más rápido que el viento |
| Equilibrio | Stabilizer | Endereza la construcción |
| Giro | Turntable, Head, Hinge | Gira lo montado / apunta al objetivo ajeno más cercano / gira libre ±60° |
| Ancla | Stake | Fija la construcción al mundo al encenderse |
| Daño y estado | Flamethrower, Frost, Beam, Shock, Cannon, Bomb, Hydrant | Queman (daño por `ApplyDamage`), frenan 3 s, empujan, apagan construcciones ajenas, disparan, explotan, apagan fuego |
| Salto / lanzar | Spring | Mirando al suelo hace saltar; mirando arriba lanza lo que tenga encima (o una bola) |
| Luz | Light | Foco |
| Energía | Battery, BigBattery | Reserva de 30 / 80 que se gasta antes que la barra del jugador |

Interacciones que salen de combinar: fuego + globo (sube el doble), agua + fuego (lo apaga), bomba + dispositivos apagados (su
onda los enciende), rayo eléctrico + construcción ajena (la apaga entera), cabeza + arma (torreta autónoma), plato + arma
(aspersor), cohete + alas (despegue), vela o ventilador + trineo o piedras flotantes (se desliza).

**Energía** (`jam.Build.Energy`): `2` (por defecto) = cada dispositivo gasta primero las baterías de su construcción y luego
la **barra del jugador**, con los valores del GDD §5: mínimo garantizado 50 (se recarga sola +3 por segundo sin gastar),
máximo 150 (lo que pase de 50 solo llega por loot; hoy `Jam.Energy <valor>`). El cohete cuesta 6 por encendido y tarda 8 s en
recargar. `1` = reserva propia de 100 por construcción; `0` = infinita. La barra vive en `JamBuildPart.cpp` (un jugador local)
y el HUD de pruebas la dibuja en 15 segmentos.

## 4. Agregar un elemento nuevo

1. **Una pieza**: fila en `Table()` de `JamBuildPart.cpp` (tipo, `EJamPartKind`, tamaño, masa, potencia, energía). Si hace
   algo nuevo, un `case` en `AJamBuildPart::Tick`. Agregar el mismo tipo a `BuildKnowledge.json`.
2. **Una fila de extra** en `ExtraTable()`: clave corta, texto *true* y *false* que hablen solo de esa cosa (y excluyan a sus
   vecinos: "las turbinas no cuentan como ventilador"), pieza, caras candidatas, plantillas permitidas, palabras clave.
3. Correr el test `Jam.Building.Tables` (falla si la pieza no existe, una cara no es válida, hay solapes, se pasa de 21
   piezas, hay más de 30 preguntas o la tabla contradice a `BuildKnowledge.json`):
   `UnrealEditor-Cmd GameJamm.uproject -ExecCmds="Automation RunTests Jam.Building;Quit" -unattended -nullrhi`
4. Probar en el juego: `Jam.Say <frase>` y, para todas las frases, `Jam.Test`.

## 5. Probar en el editor

Nivel `Content/Developers/geboou/L_Test_Islands` (GameMode `AJamTestGameMode`). Play:

| Tecla | Qué hace |
|---|---|
| WASD, mouse, Espacio | Caminar, mirar, saltar (tercera persona; `V` cambia a primera) |
| **Mantener `G`** | Hablar: al soltar se transcribe, se muestra lo entendido y se construye |
| `T`, `B` o Enter | Caja de texto: escribir la frase, Enter construye, Esc cierra |
| `1`-`9` | Frases de ejemplo |
| **`C`** | Encender o apagar la construcción (lo pedido jugando sale apagado) |
| `E` | Subir al asiento o bajar. Montado: `W/S` acelera y frena, `A/D` gira, **clic** dispara las armas, **Espacio** enciende cohetes o arma bombas |
| `X` / `H` | Borrar todo / ocultar la ayuda |

HUD: barra de energía, micrófono con tiempo de respuesta, construcción actual (piezas, gasto, controles disponibles según sus
piezas, estado de cada mecánica) y lista de elementos.

| Comando de consola (`~`) | Qué hace |
|---|---|
| `Jam.Say <frase>` | Construye y enciende sola |
| `Jam.Type <frase>` / `Jam.Power` / `Jam.Seat` | Como la caja de texto / tecla `C` / tecla `E` |
| `Jam.Drive <acel> <giro> [s]`, `Jam.Fire [s]`, `Jam.Fire boost` | Entrada simulada estando montado (pruebas) |
| `Jam.VoiceFile <ruta.wav>` | Prueba de voz sin micrófono |
| `Jam.Clear`, `Jam.Energy [valor]` | Borrar todo; ver o fijar la energía |
| `Jam.Test [segundos]` | Pruebas aisladas por mecánica + batería de frases; escribe [REPORTE-POC.md](REPORTE-POC.md) |
| `jam.Build.MaxParts`, `jam.Build.Weld`, `jam.Build.Debug`, `jam.Build.RealMeshes`, `jam.Build.Fx`, `jam.Build.Sfx`, `jam.Build.AirDrag`, `jam.Build.FanCeiling`, `jam.Mouse.Sens`, `jam.Mouse.InvertY`, `jam.Shake` | Ajustes |

## 6. Voz

`Building/JamVoice`: graba el micrófono mientras se mantiene `G` (módulo `AudioCaptureCore`), recorta el silencio, arma un WAV
mono de 16 kHz y transcribe en cadena, siempre con tiempo límite y aviso en pantalla:

| Orden | Motor | Medido en esta Mac | Notas |
|---|---|---|---|
| 1 | **Local**: whisper.cpp (`whisper-server` en `127.0.0.1:8177`, modelo `ggml-base.bin`, español forzado) | 0.15-0.4 s por frase corta; 10 de 10 frases de prueba correctas | El juego lo arranca solo. Se le pasa el vocabulario del juego (palabras de las tablas) como pista: sin ella el modelo chico falla 2 de 10 |
| 2 | **Nube**: fal (`storage/upload` + `fal-ai/wizper`) | 1.8-23 s (arranque en frío) | Respaldo si no hay motor local; límites de 5-8 s |
| 3 | Caja de texto (`T`) | — | Siempre disponible |

Instalar la voz local (nada de esto va al repo):

1. `brew install whisper-cpp` (deja `whisper-server` en `/opt/homebrew/bin`).
2. Descargar `ggml-base.bin` (148 MB, modelo multilingüe) a `~/Library/Application Support/GameJamm/whisper/ggml-base.bin`.
3. Rutas distintas: variables de entorno `JAM_WHISPER_SERVER` y `JAM_WHISPER_MODEL`.
4. La primera vez macOS pide permiso de **Micrófono** para UnrealEditor (Ajustes del Sistema → Privacidad y seguridad →
   Micrófono).

## 7. Capa de decisión del GDD

El GDD pide una interfaz única `IBuildDecisionProvider` (`Source/GameJamm/AI/`): entra `{pedido, inventario, contexto}` y sale
una receta JSON. Esa interfaz todavía no existe en el repo. Lo de aquí encaja así: `FJamDecisionClient` (Jev) +
`UJamBuildGenerator::ComposeRecipe` son un proveedor completo que ya devuelve una `FBuildRecipe` con el formato del contrato;
`GenerateRecipe` (palabras clave) es el respaldo de recetas fijas. Cuando exista la interfaz, basta envolver esas dos
funciones. `AJamBuildSpawner::RecipeJsonOverride` ya acepta la receta JSON de cualquier otro proveedor (por ejemplo un LLM).

## 8. Diferencias con otras fuentes

| Tema | `BuildKnowledge.json` / research 13 | Código hoy |
|---|---|---|
| Lectura del JSON | "Todo es dato" | Las tablas siguen en C++; el test verifica que no se contradigan (tipos, masa, energía, capacidad de batería, plantillas con `code_archetype`, `max_parts`) |
| Preguntas a Jev | Dinámicas, solo con lo que hay en el inventario | Fijas por pieza (todas las piezas del catálogo); **no se usa el inventario** |
| Quién elige | `CLAUDE.md` §0.1 dice "arquetipo, extras sí/no, dirección" | Por instrucción posterior del dueño de Construcción, Jev decide piezas y cantidades; las plantillas son solo conceptos del respaldo |
| Planificador LLM | Propuesto para peticiones nuevas | No implementado: hoy Jev por pieza + numerales + conceptos |
| Solver | Sustituye, reduce y pide materiales | No existe: solo caras libres y límite de piezas |
| Piezas sin implementar | Ramp, Rod, BallJoint, Rope, Cart, Mirror | No están en el catálogo |
| Mapa de elementos (complemento) | Orugas, vagón teledirigido, corredera, cuerda, rótula | No implementados; bisagra y motor sí, como piezas (sin tocar el formato de receta) |

## 9. Límites conocidos

- Jev decide bien el arquetipo en frases directas; en frases ambiguas puede marcar extras de más. Los textos *true/false* se
  afinan en `ExtraTable`; cada caso visto está en [REPORTE-POC.md](REPORTE-POC.md).
- El daño usa `ApplyDamage` de Unreal: solo baja vida en actores que tengan `UHealthComponent`. Las bolas del cañón empujan
  pero no explotan.
- La cabeza apunta a peones, cuerpos físicos y otras construcciones; todavía no distingue equipo (`EJamTeam`).
- El rayo eléctrico apaga la construcción ajena de forma permanente (no hay reinicio automático).
- El hielo restaura el frenado al valor por defecto de Unreal, no al que tenía el cuerpo.
- Conducir montado: giro diferencial en ruedas y guiñada asistida; no hay control de altura para lo que vuela.
- Las piezas no tienen vida propia ni se rompen por daño.
- La cantidad la decide el texto (numerales); "muchas" o "varias" no se traducen a un número.
- Efectos visuales: esferas emisivas de vida corta; solo las explosiones usan Niagara (plantilla del motor).
- Las mallas de arte se muestran sobre un cuerpo físico de forma básica (caja, cilindro, esfera) con la misma caja.
- El layout coloca cajas alineadas a los ejes: una pieza no puede ir girada 45°.

## → Aplicable a GameJamm

1. Envolver `FJamDecisionClient` + `ComposeRecipe` en `IBuildDecisionProvider` cuando el área de IA cree la interfaz.
2. Pasar el inventario al estado de Jev y generar las preguntas solo con lo que el jugador tiene.
3. Conectar la barra de energía del jugador al HUD y a los pickups.
4. Cargar las tablas desde `BuildKnowledge.json` (hoy solo se verifica la coherencia).
