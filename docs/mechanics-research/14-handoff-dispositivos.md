---
title: 14 — Handoff de dispositivos (comportamiento de piezas, energía, asiento y PoC en consola)
date: 2026-11-13
tags:
  - research
  - construccion
  - dispositivos
  - handoff
status: borrador
---

# 14 — Handoff de dispositivos (comportamiento de piezas, energía, asiento y PoC en consola)

> [!info]
> Traspaso completo del trabajo de la rama `build/poc-devices` (base `build/poc-integracion`) para la sesión que **sí tiene el
> editor de Unreal y el MCP**. Aquí se escribió el comportamiento de las piezas de [13](13-conocimiento-ia-construccion.md) §2, §5, §6 y §7
> en `Source/GameJamm/Building/`. Etiquetas: **[py]** = verificado ejecutando Python en esta máquina, **[sin compilar]** = escrito
> con cuidado pero nunca pasó por el compilador ni por el editor, **[inferencia]** = decisión de diseño nuestra.

> [!tip] Lo que hay que saber si hay prisa
> 1. **Nada de este C++ se ha compilado ni probado en el editor.** Lo único ejecutado es la lógica pura: 75 casos en Python, 0 fallas.
> 2. El primer paso en la máquina con Unreal es **compilar** y arreglar lo que salga (§5 lista dónde es más probable).
> 3. Después, cada mecánica se ve con un comando de consola en PIE: `Jam.Device <preset>`, `Jam.Dummy`, `Jam.Energy`, `Jam.Seat`,
>    `Jam.Power`, `Jam.Debug` (§6 es el runbook con lo que debe verse).

## 1. Qué se leyó y qué hallazgos importan

| Fuente | Hallazgo que condiciona el código |
|---|---|
| [13](13-conocimiento-ia-construccion.md) §2, §5, §6, §7 | Lista de comportamientos pendientes, control del asiento, valores de energía y tabla de interacciones |
| [03](03-mecanicas-vehiculos-dispositivos.md) §1–§2 | El asiento enciende todo lo conectado y no gasta; las baterías pegadas se gastan antes que la barra; recarga solo en reposo |
| `JamAssembler.cpp` | Cada pieza hija es dueña de **una** `UPhysicsConstraintComponent` creada con `SetConstrainedComponents(Padre->Mesh, …, Hijo->Mesh, …)`. Las ruedas hijas llevan `DriveConstraint`; la raíz no tiene ninguna constraint |
| `JamAssembler.cpp` | El ensamblador **no guarda** padre, lista de piezas ni dueño en la pieza. Solo fija `RecipeId`, `LocalDirection` y `DriveConstraint` |
| `JamAssembler.cpp` | `LocalDirection` de un arma es la cara donde va: un Cannon en `Top` apuntaba **al cielo**. La raíz siempre recibe "adelante" |
| `JamBuildPart.cpp` (antes) | La rueda raíz no giraba (sin constraint no hay motor); Flamethrower era una esfera de debug; Frost dejaba el amortiguamiento en 8 **para siempre** |
| `HealthComponent.cpp` | El daño entra por `OnTakeAnyDamage`; el equipo del atacante sale de `UJamStatics::GetActorTeam(InstigatedBy o DamageCauser)`, que sube por `Instigator` y `Owner` |
| `JamStatics.cpp` | Dos actores son enemigos si sus equipos son **distintos**; dos `Neutral` no lo son |
| `BuildKnowledge.json` | `energy_cost_per_s` por tipo y bloque `energy` (100 / 60 / +2 por s / 3 s de espera / cristales 15 y 40) |

## 2. Decisiones de diseño y por qué

| Decisión | Por qué |
|---|---|
| La construcción conectada se **deduce de las constraints** (`OverrideComponent1` de la constraint de cada hijo = mesh del padre) y se cachea en cada pieza | No se podía tocar `JamAssembler.*` (lo edita otra rama). Así `GetBuildParts`, `GetBuildRoot` y `GetParentPart` funcionan con el ensamblador tal cual |
| Rueda raíz: control P de velocidad angular con `AddTorqueInRadians(…, bAccelChange=true)` sobre su propio eje | No hay padre contra el cual girar. La ganancia se limita a `0.8/dt` para que no oscile con pocos fps **[py]** |
| Turntable y Head convierten la soldadura de sus hijos en un **eje libre** y giran lo de encima con torque directo, no con el motor de la constraint | El signo del motor de constraint no se podía comprobar sin motor; con torque el signo sale de `omega × r` y está probado en lazo cerrado **[py]** |
| Un arma en la cara `Top` apunta al **frente** de la construcción; en `Bottom` sigue apuntando abajo | Sin esto el tanque dispara al cielo y "Head + arma encima" no puede ser torreta. Se corrige en la pieza la primera vez que se enciende |
| Equipos: se daña solo si los equipos son distintos (misma regla que `UJamStatics::AreEnemies`) | Pedido: no dañar aliados. Una construcción **sin dueño es `Neutral`**: daña a Red y Blue pero no a monstruos neutrales |
| El dueño de la construcción se fija con `SetBuildOwner(Actor)` (hace `SetOwner` en todas las piezas) | De ahí salen el equipo (por la cadena `Owner`) y la energía. `Mount` lo llama solo |
| Energía: si el dueño no tiene `UJamEnergyComponent`, las piezas **no gastan** | No romper el flujo actual del spawner |
| Costos de energía en una tabla C++ (`EnergyCostFor`) espejo del JSON | Más simple que leer el JSON en runtime; el prototipo Python comprueba que tabla y JSON coincidan **[py]** |
| Con asiento, Fan y Wheel gastan según el multiplicador del acelerador | Acelerador en 0 = sin gasto: estar sentado sin moverse deja recargar |
| Frost usa `AActor::CustomTimeDilation` desde un componente temporal (`UJamSlowComponent`) | Mecanismo mínimo que sirve para personajes, bots y blancos sin tocar `UHealthComponent` ni el movimiento de nadie; se restaura solo |
| El asiento mantiene la altura del hover (`HoverScale`) y con W lo inclina 15° | Dos ventiladores levantan más de lo que pesa la tabla: sin esto el hover solo subía. Inclinarlo hace que avance con empuje vertical **[inferencia]** |
| Yaw por torque directo en la raíz, escalado por una inercia aproximada de toda la construcción | Es lo que pide [13](13-conocimiento-ia-construccion.md) §5; la inercia evita que una construcción pesada no gire |
| `Mount` funciona en cualquier pieza, no solo en `Seat` | "El asiento es infinito": el PoC monta la raíz si la construcción no trae Seat |
| El asiento **no** enciende bombas; un Rocket quemado no se vuelve a encender | Subirse no debe hacer explotar la construcción; el cohete es de un solo uso |
| Bala del Cannon sin gravedad | La Head solo apunta en yaw; con caída la bala fallaba a 15 m |

## 3. Archivos y contrato

Todo en `Source/GameJamm/Building/` salvo que se indique. No se agregó ningún módulo a `GameJamm.Build.cs`.

| Archivo | Qué contiene |
|---|---|
| `JamDeviceMath.h/.cpp` (nuevo) | Namespace `JamDevices`: constantes de diseño y funciones puras (energía, cono, objetivo, ángulo, control de giro, asiento, cooldown) |
| `JamEnergyComponent.h/.cpp` (nuevo) | `UJamEnergyComponent`: barra tipo maná |
| `JamSlowComponent.h/.cpp` (nuevo) | `UJamSlowComponent`: estado lento |
| `JamProjectile.h/.cpp` (nuevo) | `AJamProjectile`: bala con daño |
| `JamPocDevices.h/.cpp` (nuevo) | Comandos `Jam.*`, `AJamPocDummy` y lectura de teclas del PoC |
| `JamBuildPart.h/.cpp` (editado) | Comportamiento por pieza, construcción conectada, asiento; tipos `Turntable`, `Head`, `Stake` en el catálogo |
| `Tests/JamDeviceTests.cpp` (nuevo) | 5 tests de Automation `GameJamm.Building.Devices.*` |
| `Content/Game/Building/Data/devices_proto.py` (nuevo) | Espejo ejecutable de la lógica pura |
| `Content/Game/Building/Data/BuildKnowledge.json` (editado) | `Turntable`, `Head`, `Stake` pasan a `in_code: true` |

**`UJamEnergyComponent`** (ponerlo en el personaje; también se busca en su Controller):

| Miembro | Contrato |
|---|---|
| `MaxEnergy` 100, `StartEnergy` 60, `RegenPerSecond` 2, `RegenDelay` 3 | Editables en el Blueprint del personaje |
| `bool Spend(float)` | Gasta reserva y luego barra. Si no alcanza deja todo en 0 y devuelve `false` (la pieza se apaga) |
| `Add(float)`, `SetEnergy(float)`, `AddFromItem(FName)` | Loot: `Energy.Small` = 15, `Energy.Large` = 40 |
| `AddReserve` / `RemoveReserve` | Lo usan las piezas `Battery` (30 cada una) al tener dueño y al destruirse |
| `GetEnergy`, `GetEnergyPercent`, `GetReserve`, `GetDrainPerSecond`, `IsEmpty` | Para el HUD |
| `OnEnergyChanged(Componente, NuevaBarra, Delta)` | Como mucho una vez por frame |
| `bDebugOnScreen` | Barra en pantalla del PoC |

**`AJamBuildPart`** (API nueva, toda `BlueprintCallable`):

| Función | Contrato |
|---|---|
| `GetBuildParts()`, `GetBuildRoot()`, `GetParentPart()` | Construcción conectada. Llamar **después** de `UJamAssembler::Assemble` |
| `SetBuildOwner(AActor*)` | Dueño de todas las piezas: equipo y energía |
| `SetBuildPower(bool)` | Prende o apaga todo lo conectado (menos estructura, asiento, batería y bomba) |
| `Mount(Rider, bAttachRider=true)` / `Dismount(bKeepRunning=false)` | Subirse: fija dueño, enciende todo, control en 0. Bajarse: apaga (salvo `bKeepRunning`) |
| `SetThrottle`, `SetSteer`, `SetLift` (−1…+1) | El Blueprint o Core los llama con su input; no dependen de ningún mapeo |
| `FindNearestSeat(Contexto, Posición, Distancia, bAnyPart)` | Para "E para subirse" |
| `SetDriveScale(float)` | Multiplicador de potencia de una pieza (lo usa el asiento) |
| `InvalidateBuild()` | Llamar si se agregan o quitan piezas a mano |

Comportamiento por tipo (todo **[sin compilar]**):

| Pieza | Qué hace encendida | Valores |
|---|---|---|
| Wheel / BigWheel raíz | Gira sobre su eje; lo pegado gira con ella | 10 / 8 rad/s |
| Flamethrower | Cono hacia afuera de su cara; daño cada 0.25 s | 15/s, 400 cm, 25° |
| Frost | Cono; lento 50 % por 3 s, se renueva | 350 cm, 30° |
| Beam | Línea; empuja lo físico y daña | 20/s, 1500 cm |
| Cannon | `AJamProjectile` cada 1.5 s | 25 de daño, 2500 cm/s |
| Balloon | ×2 mientras una llama lo toca | dura 0.5 s tras la última llama |
| Turntable | Gira lo de encima | 2 rad/s |
| Head | Apunta lo de encima al enemigo vivo más cercano | 2000 cm, solo yaw |
| Stake | Constraint contra el mundo; al apagarse se suelta | — |
| Spring | Con algo encima: lo suelta y lo lanza. Sin nada encima: la construcción salta cada 1.5 s | 700 cm/s (×2 al lanzar) |
| Battery | Reserva que se gasta antes que la barra | 30 |

## 4. Casos de prueba y resultado real

| Prueba | Cómo se corre | Resultado en esta máquina |
|---|---|---|
| `devices_proto.py` | `python3 -I Content/Game/Building/Data/devices_proto.py` | **75 ok, 0 fallas** (Python 3.14) **[py]** |
| Automation `GameJamm.Building.Devices.*` (5 tests, mismos casos) | Editor → Tools → Session Frontend → Automation, o `UnrealEditor-Cmd GameJamm.uproject -ExecCmds="Automation RunTests GameJamm.Building.Devices; Quit" -unattended -nullrhi` | **No corridos: no hay motor aquí** |
| Automation `GameJamm.Building.Recipe.*` (los que ya existían) | Igual | **No corridos.** No deberían cambiar: no se tocó el parser ni el layout |

Lo que cubre el prototipo Python (y repiten los tests de Automation):

| Grupo | Casos destacados (salida real) |
|---|---|
| Energía | Reserva antes que barra; carro de 2.3/s dura **43.5 s** con barra llena; rueda + lanzallamas con batería dura **40.9 s** y la barra sigue en **59.97** cuando se acaba la reserva; sin recarga antes de 3 s y luego **10.03** en ~5 s |
| Cono y equipos | Borde a 24° dentro y a 26° fuera; si la base gira el cono gira; sin fuego amigo; 100 de vida = 27 ticks de llama |
| Objetivo | Más cercano vivo y hostil; ignora aliados y muertos; converge al objetivo en lazo cerrado (error **0.0000 rad** en 4 s) |
| Giro y asiento | Rueda raíz llega a **10.000 rad/s** sin pasarse, también a 10 fps; diferencial; hover alcanza **300.0 cm/s** de subida y frena una caída de 500 cm/s en **158 cm**; nivelado de 30° a **0.00°** en 2 s |
| Cooldown y estado | 4 disparos en 6 s; el lento se renueva y nunca se acorta |
| Datos | La tabla de costos del C++ coincide con `BuildKnowledge.json` en los **26 tipos** del catálogo |

> [!warning]
> El prototipo prueba **fórmulas**, no física. Que una rueda raíz gire de verdad, que el eje libre de la Head no se trabe o que
> el carro gire con el torque de yaw solo se sabe en el editor. Las ganancias de §3 son un primer valor razonable, no un ajuste.

## 5. Riesgos de compilación (macOS, UE 5.8) y supuestos de API

No hay nada específico de Windows: ni cabeceras de plataforma, ni rutas con barra invertida, ni módulos nuevos. Los puntos donde
el compilador puede quejarse, de más a menos probable:

| # | Dónde | Supuesto | Si falla |
|---|---|---|---|
| 1 | `AJamBuildPart::FindParentJoint` | `UPhysicsConstraintComponent::OverrideComponent1` es un `TWeakObjectPtr<UPrimitiveComponent>` **público** | Pedir a la rama del ensamblador que guarde el padre en la pieza, o leer `ComponentName1` y resolverlo; es una sola función |
| 2 | `ApplyCone`, `FindAimTarget` | `UWorld::OverlapMultiByObjectType(…, FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllObjects), FCollisionShape, Params)` y `FOverlapResult::GetActor()/GetComponent()` | Cambiar a `OverlapMultiByChannel` con `ECC_Pawn` + `ECC_WorldDynamic` |
| 3 | `SetStaked` | `SetConstrainedComponents(Mesh, NAME_None, nullptr, NAME_None)` ancla al mundo | Alternativa: `Mesh->SetSimulatePhysics(false)` en la estaca |
| 4 | `SetupRotorJoints` | Volver a llamar `SetConstrainedComponents` tras mover/rotar la constraint recalcula los marcos con la pose actual | Si lo de encima "salta" al encender una Head, ese es el sitio |
| 5 | `FireSpring`, `SetStaked` | `BreakConstraint()` + `DestroyComponent()` sobre la constraint | Dejar solo `BreakConstraint()` |
| 6 | Todo `AddTorqueInRadians(Vector, NAME_None, bAccelChange)` y `GetPhysicsAngularVelocityInRadians()` | Firmas de `UPrimitiveComponent` | — |
| 7 | `JamPocDevices.cpp` | `FAutoConsoleCommandWithWorldAndArgs` + `FConsoleCommandWithWorldAndArgsDelegate::CreateStatic`, `APlayerController::IsInputKeyDown(FKey)`, `AController::SetIgnoreMoveInput/ResetIgnoreMoveInput` | — |
| 8 | `JamEnergyComponent.cpp`, `JamPocDevices.cpp` | `GEngine->AddOnScreenDebugMessage(int32 Key, …)`; la clave sale de `GetUniqueID()` | Si hay ambigüedad de sobrecarga, castear a `uint64` |
| 9 | `Mount` | `AActor::SetActorLocationAndRotation(FVector, FRotator)`, `AttachToComponent(Mesh, KeepWorldTransform)`, `UCharacterMovementComponent::DisableMovement()` | — |
| 10 | Mezcla `float`/`double` | Todas las llamadas a `FMath::Clamp/Min/Max` usan tipos iguales a propósito | Si aparece "ambiguous call", castear el argumento que diga el error |
| 11 | Sombras de nombres (Clang lo trata como error) | Ningún local se llama como un miembro (`Owner`, `Instigator`, `Mesh`, `Power`…) | Renombrar el local que señale el error |
| 12 | Builds unity | Los helpers privados llevan nombre propio (`JamPocDevicesPrivate`, `PartAxisQuat`…) para no chocar con otros `.cpp` | Renombrar el que choque |

Supuestos de comportamiento (compilan, pero hay que **mirarlos**):

| Supuesto | Cómo se nota si es falso |
|---|---|
| Velocidad angular positiva sobre +Y hace rodar hacia +X | La rueda raíz de `Jam.Device wheel` rueda hacia atrás. Arreglo: cambiar el signo en el caso `Wheel` de `Tick` |
| El jugador puede ir pegado (`AttachToComponent`) a un cuerpo que simula física | Al hacer `Jam.Seat` la cámara no sigue a la construcción |
| `CustomTimeDilation` frena al actor afectado | El blanco dice `LENTO` pero se mueve igual |
| La inercia aproximada alcanza para girar un carro apoyado en 4 ruedas | El carro no gira con A/D: subir `SeatYawGain` o `SeatMaxYawRate` en `JamDeviceMath.h` |

> [!warning]
> **Cambios de header que obligan a recompilar a todos:** `JamBuildPart.h` (tres valores nuevos **al final** de `EJamPartKind`,
> campo `FJamPartDef::EnergyPerSec`, API nueva de `AJamBuildPart`). No se tocó `JamAssembler.*`, `BuildRecipe.*`, `JamLayout.*`,
> `JamDecisionClient.*`, `Core/` ni `Config/`.

## 6. Runbook para la sesión con el editor

**Paso 0 — compilar.** Abrir el proyecto (o compilar desde Xcode / el MCP). Si hay errores, ver §5; arreglar solo en
`Source/GameJamm/Building/`. Luego correr los tests `GameJamm.Building.Devices` y `GameJamm.Building.Recipe`: deben pasar todos.

**Paso 1 — entrar a PIE** en cualquier mapa con suelo y abrir la consola con `~`. Los comandos usan el primer jugador y arman
todo delante de la cámara. `Jam.Debug` viene en `1`: los conos y flechas **son** el efecto visual, no hay assets.

| # | Mecánica | Comandos, en orden | Qué debe verse |
|---|---|---|---|
| 1 | Rueda raíz que gira | `Jam.Device wheel` | Una rueda negra grande con dos conos naranjas. Rueda sola hacia adelante y las llamas giran con ella. Flecha amarilla en su eje |
| 2 | Rueda acostada | `Jam.Device spinner` | La misma rueda cae acostada y gira como trompo; los dos conos barren un anillo |
| 3 | Llamas con daño | `Jam.Dummy 3` y luego `Jam.Device flame` | Tres cilindros con `RED 100/100` encima, paseando. El cono naranja los alcanza y la vida baja **3.75 cada 0.25 s** (15/s); mensaje naranja por golpe; a 0 desaparecen |
| 4 | Cañón | `Jam.Dummy` y luego `Jam.Device cannon` | Una bala cada 1.5 s en línea recta; cada impacto quita **25** |
| 5 | Hielo | `Jam.Dummy` y luego `Jam.Device frost` | Cono cian; el blanco muestra `LENTO` en cian y pasea a la mitad de velocidad; se recupera 3 s después de salir del cono |
| 6 | Torreta autónoma | `Jam.Dummy 2` y luego `Jam.Device turret` | La esfera (Head) gira el cañón hacia el blanco más cercano (línea roja al objetivo, flecha verde de puntería) y dispara. Al morir uno, apunta al otro |
| 7 | Plataforma giratoria | `Jam.Device turntable` | El lanzallamas de encima da vueltas a velocidad constante |
| 8 | Resorte | `Jam.Device catapult` | El bloque de encima sale disparado hacia arriba una sola vez |
| 9 | Fuego y globo | `Jam.Device balloon` | La tabla con lastre solo empieza a subir porque la llama calienta el globo (flecha naranja sobre el globo) |
| 10 | Energía | `Jam.Energy 20`, `Jam.Device flame` | Línea amarilla `ENERGIA [##--------] …  gasto -2.0/s`. Baja hasta 0, la línea se pone roja con `VACIA`, el cono desaparece. Tras 3 s sube a +2/s |
| 11 | Batería | `Jam.Energy full`, `Jam.Device car`, `Jam.Seat`, mantener W | `reserva 30.0` aparece al armar el carro; al acelerar baja la reserva y la barra no se toca hasta que la reserva llega a 0 |
| 12 | Asiento en carro | (sigue de 11) W/S, A/D | El carro avanza y retrocede, gira por torque; flecha blanca con la dirección pedida. `Jam.Seat` otra vez: te bajas y se apaga |
| 13 | Asiento en hover | `Jam.Device hover`, `Jam.Seat` | Con Space sube y se queda flotando al soltar; con W se inclina y avanza; A/D gira; Ctrl baja; se mantiene derecho |
| 14 | Limpiar | `Jam.DeviceClear` | Desaparecen piezas, blancos y balas |

Otros comandos: `Jam.Power 0|1` apaga o prende la construcción más cercana; `Jam.Debug 0` quita los dibujos;
`Jam.Energy empty|full|<valor>`. Con las construcciones que arme la otra rama (`Jam.Build`), `Jam.Seat`, `Jam.Power` y
`Jam.Energy` funcionan igual porque buscan cualquier `AJamBuildPart`.

**Si algo falla:**

| Síntoma | Dónde mirar |
|---|---|
| No compila | §5, tabla de riesgos |
| `Jam.Device` dice que no hay jugador | Hay que estar en PIE, no en el editor parado |
| Las llamas no dañan a los blancos | ¿El blanco dice `RED`? Si el jugador ya es Red, `Jam.Dummy` los crea Blue. Revisar `ApplyCone` (riesgo 2) |
| La rueda raíz no gira | Caso `Wheel` de `AJamBuildPart::Tick`; comprobar que `DriveConstraint` sea nulo en la raíz |
| La Head no gira o lo de encima se cae | `SetupRotorJoints` (riesgos 1 y 4) |
| El texto de vida no aparece sobre el blanco | El GameMode no tiene HUD; los mensajes naranjas de golpe siguen saliendo |
| El carro no gira o gira demasiado | Constantes `Seat*` en `JamDeviceMath.h` |

**Cómo reportarlo:** por cada fila de la tabla, anotar `OK` / `FALLA` y, si falla, el mensaje exacto del log (`LogTemp: [JamPoc] …`
y cualquier error de compilación con archivo y línea). Cambiar en este doc las etiquetas **[sin compilar]** por **[code]** de lo
que quede comprobado y dejar el resultado en una tabla nueva en §4.

## 7. Pendientes y peticiones a otras ramas

| Pendiente | Para quién |
|---|---|
| Llamar `Parts[0]->SetBuildOwner(Jugador)` después de `UJamAssembler::Assemble` (spawner, `Jam.Build`, flujo real). Sin dueño la construcción es `Neutral` y no gasta energía | Integración |
| Poner `UJamEnergyComponent` en el personaje y conectar `OnEnergyChanged` a la barra del HUD; el inventario llama `AddFromItem` al recoger `Energy.*` | Core / UI |
| Conectar el input real: `FindNearestSeat` → `Mount`, ejes → `SetThrottle`/`SetSteer`/`SetLift`, tecla → `SetBuildPower` | Core |
| Opcional: que el ensamblador guarde padre y lista de piezas en `AJamBuildPart`, para no depender de `OverrideComponent1` | Ensamblador |
| La plantilla `Spinner` de [13](13-conocimiento-ia-construccion.md) §4 pone los lanzallamas en `Left`/`Right` de la rueda: acostada quedan sobre el eje (arriba y abajo). Para el anillo de fuego deben ir en `Front`/`Back`, como el preset `spinner` | Solver / plantillas |
| No implementado: Shock (no existe el tipo), daño por impacto a más de 8 m/s, hielo sobre agua, Hydrant, BigBattery, elevación de la Head | — |
| Fila de este doc en [00-INDEX.md](00-INDEX.md) | Quien edite el índice |

## → Aplicable a GameJamm

1. **Compilar primero** en la máquina con Unreal y corregir contra §5; después correr `GameJamm.Building.Devices`.
2. **Recorrer el runbook de §6** y anotar `OK`/`FALLA` por mecánica; nada de esto cuenta como probado hasta entonces.
3. **Constantes tunables** en `Source/GameJamm/Building/JamDeviceMath.h`: daño y alcance de cada arma, `SpinGain`, `Seat*`, `HeadRange`,
   `BatteryReserve`. Los costos de energía están en `EnergyCostFor` (`JamBuildPart.cpp`) y deben seguir iguales a `BuildKnowledge.json`
   (`devices_proto.py` lo comprueba).
4. **Integración mínima** para que funcione en el juego real: `SetBuildOwner` tras ensamblar, `UJamEnergyComponent` en el personaje e input del asiento (§7).
5. **Tests:** al cambiar una fórmula, cambiarla en `JamDeviceMath.cpp`, en `devices_proto.py` y en `Tests/JamDeviceTests.cpp` a la vez.
