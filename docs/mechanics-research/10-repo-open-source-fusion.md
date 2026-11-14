---
title: 10 — ComponentFuseMechanic: análisis del repo de referencia (arquitectura, bugs, licencia)
date: 2026-11-13
tags:
  - research
  - ue5
  - open-source
  - fusion
status: listo
---

# 10 — ComponentFuseMechanic: análisis del repo de referencia

> [!info]
> Auditoría del único repo open source público que hace unión de piezas por sockets
> para un builder con física:
> [EvelynSchwab/ComponentFuseMechanic](https://github.com/EvelynSchwab/ComponentFuseMechanic)
> — prototipo MIT para UE 5.2 de Evelyn Schwab, inspirado en la construcción del juego
> de referencia. Lectura de código verificada en clon local (2026-11-13, commit
> `e26604e`). Resumen de su arquitectura en juego: [08-mapeo-ue5-impl.md](08-mapeo-ue5-impl.md) §7.

> [!tip] Veredicto
> **Adoptar**: la búsqueda de sockets + la selección de candidatos por sweep (MIT, con
> atribución). **Reescribir**: el loop de fusión por tick, el fallback physics-off y el
> detach por sweep. **Arreglar primero** los 4 bugs pequeños (§5) y el posible
> auto-rechazo (§5, último).

## 1. Un solo repo MIT hace fusión por sockets; el resto son adyacentes

De 10 candidatos, solo uno implementa la mecánica. La tabla separa licencias
verificadas de etiquetadas: "verificada" = se leyó el texto del LICENSE (el LICENSE de
ComponentFuseMechanic en el clon local dice MIT, Copyright (c) 2024 Evelyn Schwab);
todas las demás son etiquetas vistas en la página del repo o en snippets de búsqueda.

| Repo / proyecto | URL | Licencia (estado) | Motor | Qué implementa | Madurez |
|---|---|---|---|---|---|
| ComponentFuseMechanic | [github.com/EvelynSchwab/ComponentFuseMechanic](https://github.com/EvelynSchwab/ComponentFuseMechanic) | MIT, **verificada del LICENSE** | UE 5.2 (`EngineAssociation: 5.2`) | Búsqueda de pares de sockets cuyo nombre contiene "Attach", check de clipping, snap por physics-interp, constraints, detach rudimentario. Inspirado en el juego de referencia | Prototipo autodeclarado "no production-ready"; 1 commit visible en el clon local |
| godot-wheeled-vehicle-template | [github.com/fswienty/godot-wheeled-vehicle-template](https://github.com/fswienty/godot-wheeled-vehicle-template) | MIT, solo etiqueta | Godot 4 (probable), 2D | Drive y steering por rueda en layouts arbitrarios. Sin joints ni snapping de piezas | ~32 estrellas |
| Godot-AdvancedVehicle | [github.com/Dechode/Godot-AdvancedVehicle](https://github.com/Dechode/Godot-AdvancedVehicle) | MIT, solo etiqueta (README) | Godot | Drivetrain, modelos de llanta, wear, fuel, gearbox. Sin ensamblado | ~198 estrellas; datado 2022 por un roundup de foro |
| Tork | [github.com/adrenak/Tork](https://github.com/adrenak/Tork) | MIT, solo etiqueta | Unity | Vehículo arcade: ruedas, Ackermann, motor, frenos, anti-roll. Sin joints | ~444 estrellas; demo scene sin verificar |
| godot-snappy | [github.com/jgillich/godot-snappy](https://github.com/jgillich/godot-snappy) | MIT, solo snippet de búsqueda | Godot 4 | Vertex snapping de editor, no ensamblado en runtime | Sin verificar |
| Rigs of Rods | [github.com/RigsOfRods/rigs-of-rods](https://github.com/RigsOfRods/rigs-of-rods) | GPL-3.0, solo etiqueta (texto en COPYING) | Ogre, AngelScript | Simulación soft-body de vehículos y maquinaria | Maduro; copyleft → no reutilizable en código cerrado |
| Modular Movement Component | [github.com/irajsb/ModularMovementComponent](https://github.com/irajsb/ModularMovementComponent) | Sin revisar | UE 5 | Drivetrain modular (engine, gearbox, wheels, tires). Sin flujo de attach de piezas | Docs: [irajsb.github.io/ModularMovementDocs](https://irajsb.github.io/ModularMovementDocs) |
| Slingshot Community | [github.com/Slingshot-Physics/slingshot-community](https://github.com/Slingshot-Physics/slingshot-community) | Sin revisar | Motor C++ propio | Constraints de igualdad revolute/prismatic/gear. Primitivas, no un builder | Sin revisar |
| Box2DPhysicsSandbox | [github.com/bombpersons/Box2DPhysicsSandbox](https://github.com/bombpersons/Box2DPhysicsSandbox) | Desconocida; el repo daba 404 | Box2D | Sandbox de carro y terreno con joints de Box2D | No disponible al consultar |

Fuera del open source: el **Chaos Modular Vehicle System** de Epic viene en el motor, y
un hilo del foro de UE describe un toolkit de ensamblado en runtime con previews de snap
y multiplayer para UE 5.7/5.8
([Team Expotential](https://forums.unrealengine.com/t/team-expotential-customizable-assembly-toolkit/2743405))
— su licencia y disponibilidad no están confirmadas: tratarlo como cerrado. La proposal
[#1423 de Godot](https://github.com/godotengine/godot-proposals/issues/1423) pide un
weld joint rígido para vehículos modulares y su mantenedor dijo que no había planes de
nuevos tipos de joint → **el motor no va a proveer el primitivo que un mecanismo de
fusión querría**.

## 2. Los sockets son baratos de autorar pero no cargan las reglas

ComponentFuseMechanic trata un socket como joint **solo por nombre**: `IsComponentFusable`
y la búsqueda de pares prueban `Contains("Attach")` o la propiedad
`FusableSocketSubName`, así que cualquier socket con ese substring es un joint — sin
metadata de eje, tipo ni compatibilidad (`FFuseComponent.cpp:137, 380, 384`). Es la
arquitectura más barata posible: no necesita modelo de datos y los anchors viven en el
asset del DCC. El costo aparece en cuatro sitios:

1. **Validación solo en el momento del pick**: una query de overlap (línea 404) y un
   sphere sweep al despegar (línea 658). No hay estructura persistente para detectar
   ciclos, piezas huérfanas ni pares de joints ilegales.
2. **El detach redescubre constraints** barriendo actores cercanos y recorriendo
   `GetAttachedActors` — el comentario del propio repo lo llama *"Not an ideal
   solution"* (líneas 640-641).
3. **La conectividad es lo que diga el constraint de Chaos**: cualquier break/reset del
   motor se filtra directo al estado del juego.
4. **Cero replicación**: todo el breaking/re-creación de joints corre por tick en el
   cliente local.

Un **grafo de piezas con joints tipados** invierte esas desventajas: nodos = piezas con
IDs estables, aristas = registros de joint con tipo, transforms de anchor en espacio
local de cada pieza, límites y tags de compatibilidad. Validar = check de grafo sin
física; detach = quitar arista; networking = replicar el grafo y reconstruir constraints
en local. El precio: un paso de autoría por anchor y una capa de datos que el diseño de
sockets evita. Para specs generadas por LLM la diferencia es tajante: un JSON de
piezas+joints se valida contra schema y se rechaza **antes** de correr física; un build
solo-sockets no tiene formato de spec. No se encontró fuente pública que compare ambos
diseños para builders con física — **esta comparación es razonamiento de diseño, no un
hallazgo con fuente**. El híbrido recomendado: sockets como geometría de anchor y el
grafo como única autoridad de conectividad.

## 3. El riesgo de versión está en el loop de Chaos, no en las firmas

Los calls del engine que usa el repo (sweeps, overlaps, sockets,
`AttachToComponent`, constraints, `SetWorldLocationAndRotation`) son de larga vida; no
se pudo confirmar cambio de firma para 5.8 (las páginas de API devolvieron solo
tablas de contenido). Las release notes de 5.8 — vistas por excerpts de búsqueda —
describen trabajo de Chaos Core para simulación "más responsive y escalable" y un
Enhanced Input unificado con Common UI
([release notes](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-5-8-release-notes)).
Esos cambios son **fuente única** y no se confirmó si tocan la ruta de fusión.

El riesgo mayor es de comportamiento: el loop rompe y re-crea el constraint cada tick
hasta terminar la interp (`FFuseComponent.cpp:600-613`), así que depende del
comportamiento de constraints de Chaos, con historial problemático:

- Reporte de UE 5.0.2: el evento "broken" del constraint a veces **no dispara**
  ([foro](https://forums.unrealengine.com/t/5-0-2-chaos-bug-physics-constraint-broken-event-not-firing/594556)).
- Tracker de Epic: activar plasticity en un joint de Chaos **pone a cero el drive
  target** al inicializar, estado en 5.8 sin verificar
  ([UE-368639](https://issues.unrealengine.com/issue/UE-368639)).
- Un usuario reportó que un objeto simulado **volvía a su posición vieja** al
  reactivar la física, con `TeleportPhysics` sugerido como workaround en vez de
  `ResetPhysics`
  ([foro](https://forums.unrealengine.com/t/physic-simulated-object-snaps-back-to-old-location-after-re-enabling-simulated-physics/2658764)).

Los tres son fuente única y anecdóticos. El repo llama `ResetPhysics` en las líneas 575
y 606 → **la pregunta del snap-back es la primera que hay que probar en 5.8**.

## 4. Cinco defectos de código (y por qué el loop importa más)

Leídos en el clon local y confirmados cada uno. El null-deref y el auto-rechazo pueden
romper gameplay; el loop por tick es el problema de diseño que ningún fix de una línea
elimina.

1. **Null dereference en `ReleaseComponent`.** Línea 43:
   `GetGrabbedComponent()->SetRenderCustomDepth(false)` sin null check.
   `TryFuseObjects` llama `ReleaseComponent()` en las líneas 547 y 553 sin verificar
   que algo esté agarrado. Llamar a fusionar con las manos vacías llegaría a la línea
   43 con puntero nulo (no reproducido en editor; depende de cómo sobrevive
   `bHasValidFuse` entre llamadas). Fix: un guard en `ReleaseComponent`, que cubre a
   todos los callers.
2. **String `"Attach"` hardcodeado.** `IsComponentFusable` (línea 137) prueba
   `SocketName.ToString().Contains("Attach")`, mientras la búsqueda de pares (líneas
   380/384) usa `FusableSocketSubName`: cambiar la propiedad cambia la búsqueda pero
   no el check del objeto agarrado. Rutear la línea 137 por la propiedad.
3. **Precedencia en `IsFusing`.** Línea 218: `return !CurrentFuserState == FSTATE_NONE;`.
   Con un `UENUM` sin scope (header: `enum EFuserState`) y `FSTATE_NONE == 0`, la
   expresión evalúa bien **por accidente**: el NOT lógico da 0 para cualquier estado
   distinto de NONE, que compara igual a `FSTATE_NONE`. Fix:
   `CurrentFuserState != FSTATE_NONE`.
4. **`TryDetachGrabbedComponent` siempre devuelve `false`.** Rompe y destruye los
   constraint actors que corresponden (líneas 638-696) y devuelve `false` en la línea
   695 sin importar. Fix: devolver `true` cuando se removió algún constraint.
5. **Posible auto-rechazo en `ComponentOverlapMulti`.** La línea 404 pasa
   `GetGrabbedComponent()` como componente de la query y solo lo ignora a él (línea
   400): el componente objetivo no se ignora y la query usa `ECC_PhysicsBody`, así que
   en la pose ideal de contacto el objetivo puede solapar la pieza agarrada y rechazar
   todos los candidatos. Probar en editor con `f.drawdebugfuser 1` (dibuja cápsulas
   rojas/azules para candidatos rechazados/aceptados).

Dos más, a criterio: la interp completa usa `FQuat::Equals` con tolerancia 0.2 (línea
580, floja) y los sockets suplementarios usan tolerancia 5.0 (línea 469) en unidades
distintas al test de posición 0.5. **El asunto grande es el loop**: las líneas 600-613
rompen el constraint, apagan la simulación de la fuente, la teleportan con
`ResetPhysics` y re-crean el constraint **cada tick** hasta que acaba la interp; la
línea 617 apaga la simulación del objetivo al pasar la ventana de interp de 1.5 s —
medida que el repo llama forma de *"avoid particularly bad physics clipping issues"* y
cuyo propio comentario reconoce como *"a fairly unpleasant solution"*. Ese fallback
impide que el objetivo siga simulando: probablemente la causa del clipping que el repo
peleaba.

## 5. Licencias: el MIT es fácil; los assets incluidos, no

- **El código es MIT** y su única condición es conservar el aviso de copyright y de
  permiso en copias y porciones sustanciales
  ([OSI, MIT](https://opensource.org/license/mit)). Reutilizar el código = mantener el
  archivo LICENSE y el aviso (el commit `e26604e` ya lo tiene). Marcar archivos
  modificados como modificados (práctica que Apache-2.0 exige) es barato de seguir.
- **Hueco 1 — props:** el README del repo acredita "Props" a Evelyn Schwab sin licencia
  separada; si los props están cubiertos por el MIT es una pregunta abierta → tratarlos
  como algo que requiere permiso propio.
- **Hueco 2 — assets de terceros:** el README lista el personaje (template third-person
  de Epic) y las animaciones de movimiento (Caleb Longmire, ALS). El repo tiene
  `Content/ThirdPerson` y solo un LICENSE raíz → **esos assets no están cubiertos por
  él**. No se verificaron los términos del template de Epic ni las licencias de
  Longmire/ALS: reemplazarlos o confirmarlos antes de que el build del jam salga.
- **La línea idea/expresión:** el derecho de EE.UU. excluye de copyright "cualquier
  idea, procedimiento, proceso, sistema, método de operación"
  ([17 U.S.C. §102(b) comentado por Samuelson](https://www.law.berkeley.edu/center-article/why-copyright-law-excludes-systems-and-processes-from-the-scope-of-its-protection/),
  en línea con Baker v. Selden
  ([Lexplug](https://www.lexplug.com/casebrief/baker_v_selden_65bd562dabbe1c5b9f8a38dd))).
  El mecanismo "fusionar dos piezas por sockets, revisar clipping, despegar" es una
  idea reimplementable desde su descripción pública; copiar clases, nombres de
  funciones o assets del repo es copiar expresión (lo que gobierna el MIT). La línea no
  siempre es clara y las copias estructurales cercanas pueden contar: **reescribir
  siguiendo la descripción, no el código, es la ruta segura**.
- **RE de un juego comercial:** extraer o decompilar un juego comercial (como el de la
  referencia) no es aceptable: el derecho de EE.UU. restringe sortear controles de
  acceso ([17 U.S.C. §1201(a)(1)(A)](https://www.law.cornell.edu/uscode/text/17/1201))
  y la interoperabilidad de §1201(f) es angosta; los términos de cuenta de Nintendo
  prohíben RE y decompilación de sus servicios
  ([Nintendo Life, 2025](https://www.nintendolife.com/news/2025/05/nintendo-updates-its-user-agreement-to-crack-down-on-emulation)).
  Este análisis y el prototipo usan solo la **descripción pública del diseño**, nunca
  código o assets extraídos.
- **Reglas del jam:** las reglas oficiales del Epic AI MegaJam no eran legibles; un
  excerpt de foro cita la sección 15(c) permitiendo assets con fuente si se declaran en
  la entrega y se tienen los derechos
  ([excerpt de foro](https://forums.unrealengine.com/t/what-is-unclear-to-me-is-the-expected-form-and-timing-of-the-proof-of-authorization-license/2849537)).
  Es fuente única y truncada: **leer el texto oficial antes de entregar**. El EULA de
  Unreal y los términos de Fab devolvieron HTTP 403 y no se leyeron.

## 6. Recomendación: adoptar los sockets, rehacer el loop

1. **Adoptar tal cual (MIT):** la búsqueda de sockets por nombre y la selección de
   candidatos por sphere sweep (`FFuseComponent.cpp` ~líneas 370-420), conservando el
   LICENSE y la línea de copyright, y marcando los archivos modificados.
2. **Arreglar los 4 defectos pequeños** (§4.1-4.4) y el auto-rechazo: ignorar el par
   del componente objetivo en el overlap, o comprobar solo la pieza agarrada contra
   cuerpos no-objetivo.
3. **Rehacer el loop de fusión:** un solo movimiento kinematic (o physics-off) al
   transform ideal, y **un constraint creado una sola vez** al final; eliminar el
   fallback physics-off del objetivo; conservar `ResetPhysics` solo como failsafe de
   10 s con log.
4. **Introducir el grafo de joints como fuente de verdad** (detach = quitar arista, no
   sweep) y para validar specs JSON de piezas generadas por LLM antes de correr
   física. Diseño concreto en
   [11-diseno-ensamblado-open-source.md](11-diseno-ensamblado-open-source.md).
5. **Fuera de alcance para el jam:** el sistema de proyección ortográfica de 3 captures
   (el README propio lo llama "not ideal"), replicación, migración completa a
   Blueprint, el contenido de `L_Fuse_TestMap`, y la ingesta de specs LLM más allá de
   la validación de schema. Reemplazar el personaje de Epic y las animaciones
   Longmire/ALS por assets con licencia confirmada (o placeholders) antes del build.

## 7. Gaps y lo que quedó sin verificar

- Ninguna firma de API de UE 5.8 confirmada contra fuente web (las páginas de
  referencia devolvieron solo tablas de contenido). El claim de que el código compila
  en 5.8 está sin probar, igual que si el snap-back de `ResetPhysics` está arreglado
  en 5.8.
- Los ítems de foro/tracker de Chaos son fuente única y anecdóticos.
- No se encontró repo open source que haga unión de piezas por sockets en UE, Godot o
  Unity → la comparación socket-vs-grafo es razonamiento de diseño.
- Los bugs se confirmaron leyendo código, no corriendo el editor.
- Las licencias de los repos de Godot, Unity y Ogre son solo etiquetas; el sandbox de
  Box2D daba 404.
- Las reglas oficiales del jam (sección 15) se vieron solo por excerpt de foro.

## → Aplicable a GameJamm

1. Copiar el **patrón** de búsqueda de sockets + par más cercano + sweep de candidatos
   (ya resumido en [08-mapeo-ue5-impl.md](08-mapeo-ue5-impl.md) §7), con LICENSE y
   atribución si se copia código.
2. **No copiar el loop por tick**: constraint único creado tras el snap; el detach por
   sweep se reemplaza por remoción de arista en el grafo.
3. Corregir en nuestro código los mismos defectos si aparecen: guard de null, sin
   strings hardcodeados para sockets, comparaciones de enum explícitas, return
   consistente en detach, overlap que ignore el par objetivo.
4. Probar en 5.8 lo primero: snap-back al reactivar física y que el evento "broken"
   dispare (usar `f.drawdebugfuser` como referencia de debug).
5. Antes de entregar: confirmar licencias de assets, leer las reglas oficiales del jam
   y declarar los assets con fuente.
