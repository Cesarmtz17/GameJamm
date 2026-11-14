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
> El modelo **solo elige** entre opciones finitas (una plantilla y extras sí/no). El **código** arma la receta, busca una
> cara libre para cada pieza, respeta el límite de 21 piezas y valida. Las mecánicas salen de la **capacidad de cada pieza**
> (`EJamPartKind`) y de la cara donde va pegada, nunca de la plantilla.

## 1. Camino del prompt

| Paso | Dónde | Qué hace |
|---|---|---|
| 1. Texto | `Jam.Say <texto>` (consola) o `AJamBuildSpawner::Prompt` | Entra la frase del jugador |
| 2. Decisión | `FJamDecisionClient::RequestChoice` | Una llamada `POST https://api.typesafe.ai/v1/systemone` (modelo `jev-1.13.0`): 1 pregunta *choice* (plantilla, 14 opciones) + 27 preguntas *noul* (un extra cada una). `noul > 0.5` = sí |
| 3. Respaldo | `UJamBuildGenerator::ChoiceFromKeywords` | Si no hay clave, no hay red o tarda más de 8 s: mismas tablas, por palabras clave. Nunca se bloquea |
| 4. Receta | `UJamBuildGenerator::ComposeRecipe` | Copia la plantilla y agrega cada extra en su **primera cara libre** (prueba de cajas, sin solapes). Lo que no cabe o no aplica sale en el log como `skipped=[...]` |
| 5. Mundo | `UJamAssembler::Assemble` | Piezas físicas + constraints según la capacidad: soldadura, eje con motor (rueda, plato), eje libre (cabeza) o bisagra |
| 6. Encendido | `AJamBuildSpawner` | A los 1.5 s enciende todo lo que no es estructura, asiento ni batería |

Orden de claves: variable de entorno `TYPESAFE_API_KEY` o `[Jam] DecisionsApiKey` en `Config/DefaultSecrets.ini` (ignorado por
git); después OpenRouter y fal como respaldo de red. Las claves nunca van al repo.

Log de cada construcción: `LogJamDecision: backend=typesafe-jev ... archetype=X extras=[...]` y
`LogJamBuild: OK N parts (...)`.

## 2. Las tres tablas

| Tabla | Archivo | Qué tiene |
|---|---|---|
| Piezas (35) | `JamBuildPart.cpp`, `Table()` | Tipo, capacidad (`EJamPartKind`), tamaño, masa, potencia, energía por segundo. Nombres, masas y energía iguales a `Content/Game/Building/Data/BuildKnowledge.json` |
| Plantillas (14) | `JamAssembler.cpp`, `ArchetypeTable()` | Nombre, descripción para Jev, palabras clave y **receta base de piezas básicas**. Sin lógica propia |
| Extras (27) | `JamAssembler.cpp`, `ExtraTable()` | Clave, texto sí/no para Jev, pieza, caras candidatas, plantillas permitidas, palabras clave |

Plantillas: `Tank`, `Fortress`, `Catapult`, `Spinner`, `Roller`, `Airship`, `Glider`, `Bike`, `Sled`, `Hover`, `Flyer`, `Turret`,
`Car`, `Rocket`. Extras: `head`, `spin`, `fan`, `jet`, `turbine`, `rocket`, `flame`, `cannon`, `balloon`, `wing`, `sail`, `hover`,
`wall`, `frost`, `beam`, `shock`, `bomb`, `spring`, `stabilizer`, `stake`, `battery`, `light`, `hydrant`, `hinge`, `ball`, `girder`,
`bigwheels`.

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
la **barra del jugador** (60 al empezar, máximo 100, recarga 2 por segundo tras 3 s sin gastar); `1` = reserva propia de
100 por construcción; `0` = infinita. La barra vive por ahora en `JamBuildPart.cpp` (un jugador local), lista para conectarse
al HUD.

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

Nivel `Content/Developers/geboou/L_Test_Islands` (GameMode `AJamTestGameMode`: caminar con WASD, mouse, espacio). Play y abrir
la consola con `~`:

| Comando | Qué hace |
|---|---|
| `Jam.Say un carro con cohete` | Construye ~6 m delante del jugador, de perfil. En pantalla: decisión, piezas y mecánicas |
| `Jam.Clear` | Borra todo lo construido |
| `Jam.Seat` | Te sube al asiento más cercano (solo vas montado) o te baja |
| `Jam.Energy [valor]` | Muestra o fija la barra de energía |
| `Jam.Test [segundos]` | Corre la batería de 28 frases y escribe [REPORTE-POC.md](REPORTE-POC.md) |
| `jam.Build.Debug 1` | Log por segundo de posición, velocidad, giro y energía de cada construcción |
| `jam.Build.RealMeshes 0` / `jam.Build.Fx 0` | Formas básicas en vez de mallas de arte / sin efectos |

## 6. Capa de decisión del GDD

El GDD pide una interfaz única `IBuildDecisionProvider` (`Source/GameJamm/AI/`): entra `{pedido, inventario, contexto}` y sale
una receta JSON. Esa interfaz todavía no existe en el repo. Lo de aquí encaja así: `FJamDecisionClient` (Jev) +
`UJamBuildGenerator::ComposeRecipe` son un proveedor completo que ya devuelve una `FBuildRecipe` con el formato del contrato;
`GenerateRecipe` (palabras clave) es el respaldo de recetas fijas. Cuando exista la interfaz, basta envolver esas dos
funciones. `AJamBuildSpawner::RecipeJsonOverride` ya acepta la receta JSON de cualquier otro proveedor (por ejemplo un LLM).

## 7. Diferencias con otras fuentes

| Tema | `BuildKnowledge.json` / research 13 | Código hoy |
|---|---|---|
| Lectura del JSON | "Todo es dato" | Las tablas siguen en C++; el test verifica que no se contradigan (tipos, masa, energía, capacidad de batería, plantillas con `code_archetype`, `max_parts`) |
| Preguntas a Jev | Dinámicas, solo con lo que hay en el inventario | Fijas (27 extras); **no se usa el inventario** |
| Plantillas | 16 (`Hoverbike`, `Ram`, `Wall`, `Free`...) | 14; `Tank`≈`Ram`, `Hover`≈`Hoverbike`, `Fortress`≈`Wall`; no hay `Free` |
| Solver | Sustituye, reduce y pide materiales | No existe: solo caras libres y límite de piezas |
| Piezas sin implementar | Ramp, Rod, BallJoint, Rope, Cart, Mirror | No están en el catálogo |
| Mapa de elementos (complemento) | Orugas, vagón teledirigido, corredera, cuerda, rótula | No implementados; bisagra y motor sí, como piezas (sin tocar el formato de receta) |

## 8. Límites conocidos

- Jev decide bien el arquetipo en frases directas; en frases ambiguas puede marcar extras de más. Los textos *true/false* se
  afinan en `ExtraTable`; cada caso visto está en [REPORTE-POC.md](REPORTE-POC.md).
- El daño usa `ApplyDamage` de Unreal: solo baja vida en actores que tengan `UHealthComponent`. Las bolas del cañón empujan
  pero no explotan.
- La cabeza apunta a peones, cuerpos físicos y otras construcciones; todavía no distingue equipo (`EJamTeam`).
- El rayo eléctrico apaga la construcción ajena de forma permanente (no hay reinicio automático).
- El hielo restaura el frenado al valor por defecto de Unreal, no al que tenía el cuerpo.
- `Jam.Seat` solo monta; dirigir con WASD es parte del asiento real (research 13 §5).
- Efectos visuales: esferas de color de vida corta en lugar de Niagara.
- Las mallas de arte se muestran sobre un cuerpo físico de forma básica (caja, cilindro, esfera) con la misma caja.
- El layout coloca cajas alineadas a los ejes: una pieza no puede ir girada 45°.

## → Aplicable a GameJamm

1. Envolver `FJamDecisionClient` + `ComposeRecipe` en `IBuildDecisionProvider` cuando el área de IA cree la interfaz.
2. Pasar el inventario al estado de Jev y generar las preguntas solo con lo que el jugador tiene.
3. Conectar la barra de energía del jugador al HUD y a los pickups.
4. Cargar las tablas desde `BuildKnowledge.json` (hoy solo se verifica la coherencia).
