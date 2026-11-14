---
title: 11 — Ensamblado: más repos open source + diseño propio (grafo, joints, validación)
date: 2026-11-13
tags:
  - research
  - ue5
  - open-source
  - diseno
  - dsl
status: listo
---

# 11 — Ensamblado: más repos open source + diseño propio

> [!info]
> Complemento de [10-repo-open-source-fusion.md](10-repo-open-source-fusion.md)
> (ComponentFuseMechanic y los repos ya listados ahí no se repiten). Solo fuentes
> públicas y código open source; nada de un juego comercial fue extraído ni
> decompilado. **[code]** = código leído en clon local (2026-11-13) · **[page]** =
> visto en página del repo / snippet / metadata de la API de GitHub · **[inferred]** =
> razonamiento propio, sin verificar · **[memory]** = conocimiento de la API de Unreal
> de memoria, sin contrastar con headers de 5.8.

> [!tip] Hallazgo en una línea
> **Nadie ha publicado un clon decente del sistema de construcción de la referencia.**
> Las piezas útiles son pequeñas y están dispersas, y la mejor idea estructural viene
> de un estilo-Besiege (creasiege), no de una demo del modo construcción.

## 1. Repos nuevos encontrados

| Repo | URL | Licencia (cómo se verificó) | Motor | Mecánica | Uso para nosotros |
|---|---|---|---|---|---|
| Demo de construcción (Unity, nombre omitido por el scrub) | — | **Ninguna** (sin archivo LICENSE; `gh api` license null) = todos los derechos reservados | Unity C# | Grab, rotate, "attach" del cuerpo más cercano con `FixedJoint` en runtime [code] | Solo concepto. **No copiar código** |
| creasiege (BenPyton) | [github.com/BenPyton/creasiege](https://github.com/BenPyton/creasiege) | MIT [archivo LICENSE presente; `gh api` = MIT] | Unity C# | Árbol de piezas estilo Besiege construido sobre sockets `PartJoint`, save/load JSON, la destrucción parte el árbol [code] | **Mejor referencia del modelo de datos.** Reutilizable con aviso MIT |
| Clingy (all-iver) | [github.com/all-iver/clingy](https://github.com/all-iver/clingy) | MIT [LICENSE presente; `gh api` = MIT] | Unity C# | Librería de attachments en runtime: objetos estrategia (physics joint, parenting, following) configurables en editor, joints creados en runtime [code: README + strategy files] | Referencia para "attachment = data asset que construye un joint" |
| OwnWar (Demindiro) | [github.com/Demindiro/OwnWar](https://github.com/Demindiro/OwnWar) | **AGPL-3.0** [LICENSE; `gh api`] | Godot 3 (GDScript + GDNative) | Vehículos por bloques con carpetas de chassis, ruedas, alas, torretas, armas [code: solo listing de directorios] | **No leer para código** (copyleft, viral por red). Solo idea |
| godot-jolt-physics-joint (PRS-Organization) | [github.com/PRS-Organization/godot-jolt-physics-joint](https://github.com/PRS-Organization/godot-jolt-physics-joint) | Apache-2.0 [LICENSE presente; `gh api`] | Godot 4.6, Jolt | Brazo de 5 ejes con `Generic6DOFJoint3D`, límites soft/hard, spring + servo de motor [page: README; no se leyó `robot_arm.gd`] | Evidencia de que un joint 6-DOF con límites y motor cubre hinge, slider y fixed |
| besiege-like (Alsctiho) | [github.com/Alsctiho/besiege-like](https://github.com/Alsctiho/besiege-like) | **Ninguna** en el repo; incluye un addon propietario "Godot MCP Pro" [code: addons/LICENSE] | Godot 4.x, Jolt | Núcleo-jugador estilo Besiege que ensambla piezas en seis caras, check de colisión de ensamblado con shapes convexas por pieza + discos barridos de rotores, preview-then-confirm [code: README, `assembly_collision.gd`]. Sin código de joints (grep) | Solo concepto. El takeaway es el flujo **"preview, check, commit"** |
| MGS.Machinery (mogoson) | [github.com/mogoson/MGS.Machinery](https://github.com/mogoson/MGS.Machinery) | Apache-2.0 [`gh api`] | Unity C# | Binding de joints de maquinaria en editor de escena [page] | No es ensamblado en runtime. Bajo valor |
| godot-proposals #1423 | [github.com/godotengine/godot-proposals/issues/1423](https://github.com/godotengine/godot-proposals/issues/1423) | MIT (repo) | Godot | Petición de weld joint nativo | Confirma que los motores no traen primitivo "weld"; usamos constraint Fixed o merge de cuerpos |

**No usables / no encontrados:** el assembly toolkit de Team Expotential (foro de UE,
cerrado, licencia desconocida — citado en
[10-repo-open-source-fusion.md](10-repo-open-source-fusion.md) §1). No apareció ningún
clon de Scrap Mechanic o Besiege con licencia y joints funcionando en la búsqueda de
GitHub ("vehicle builder joints" etc. no devolvió nada relevante más allá de las filas
de arriba). Existen varios repos escolares de una estrella sin licencia; ignorados.

## 2. Qué hace cada approach (verificado del código)

**Demo de construcción, `Blocks.cs` (197 líneas) [code]**
- Cada bloque tiene un `FixedJoint` saliente + una lista de joints entrantes de vecinos.
- `Attach()` toma `OverlapSphere` radio 2, elige el rigidbody más cercano, le añade un
  `FixedJoint` a sí mismo y le avisa al vecino.
- **Bug visible en el código:** tras elegir `obj`, registra el joint en `results[1]`,
  no en `obj` — el vecino equivocado siempre que el cuerpo más cercano no sea el slot
  1. Tampoco chequea `obj != null`: un miss añade un joint a nada.
- "Agarrar el ensamblado completo" = recorrer las listas de joints y llamar
  `Grab(false)` en cada vecino. El movimiento sostenido pone `rigidbody.velocity`
  hacia un punto frente a la cámara; gravedad off y rotación congelada mientras se
  sostiene.
- El detach destruye joints y actualiza ambas listas. **Sin socket, sin alineación, sin
  clip check**, y el grafo vive en dos listas por bloque (fácil de desincronizar).

**creasiege, `Part.cs` / `PartJoint.cs` / `VehicleEditorController.cs` [code]**
- Los sockets son objetos hijo (`PartJoint`) con sphere trigger collider en una layer
  "Joints" dedicada. Una pieza tiene una lista; el joint 0 es el "root" que lo une al
  padre.
- `PartJoint.AttachTo` enlaza dos sockets en ambas direcciones, desactiva ambos
  colliders (no se pueden elegir dos veces) y registra la pose objetivo
  (`other.rotation * Euler(0,180,0)`, i.e. los sockets se miran entre sí).
- `Part.AttachTo` resuelve el transform de la pieza desde el socket:
  `rot = jointRot * Euler(0,0,angle) * inverse(localRot)`,
  `pos = jointPos - rot * localPos`. También soporta ciclar cuál socket es root y un
  `angle` de spin sobre el eje del socket.
- La estructura es un **árbol**: cada pieza tiene exactamente un socket padre. Las
  piezas son objetos hijo de un solo rigidbody; attach suma la masa al rigidbody root
  (`rb.mass += mass`). **No hay physics joints en absoluto** [code].
- El formato de guardado es `JSONVehicle { version, name, parts[] }`; cada `JSONPart`
  lleva el nombre del prefab, `partTargetId` y `constraintJoint` (índice del socket en
  el padre) [code, struct visto; el resto de campos no se leyó]. Load = instanciar
  prefab, `AttachTo(GetPartByID(target).GetJointFromID(socket))`. Es una receta de
  piezas+sockets, **muy cerca de lo que queremos como salida del LLM**.
- Daño: `Part.Destroy()` despega la pieza y mueve cada subárbol hijo a un rigidbody
  `Debris` nuevo recalculando el centro de masa. Mecánica de **split estructural** sin
  joints de física.

**Clingy [code, README + firmas de estrategias]**
- Un `AttachStrategy` es un ScriptableObject. La estrategia Physics toma anchor /
  connected anchor / axis / swing axis de "param selectors" que leen los attach points
  de cada objeto. El componente de joint se crea en runtime desde esa descripción.
  Variantes Chain / One-to-One / Many-to-One. **Takeaway: los parámetros del joint
  viven en un data asset, no en el código que decide conectar.**

**besiege-like (Godot) [code]**
- `assembly_collision.gd` construye una shape convexa por mesh (cacheada) y cambia las
  piezas rotatorias por un cilindro barrido, así un **preview puede ser rechazado antes
  del commit**. Coincide con la idea del "overlap clip check" de ComponentFuseMechanic
  pero con shapes reales de las piezas y soportando piezas móviles. El README dice que
  el preview no cambia el loadout real hasta confirmar.

## 3. Comparación

| Aspecto | ComponentFuseMechanic | Demo (Unity) | creasiege | Clingy | besiege-like |
|---|---|---|---|---|---|
| Autoridad de conectividad | Constraints de Chaos [report] | Dos listas de joints por bloque | Árbol de piezas (sockets) | Objetos attachment | Datos de ensamblado |
| Tipo de joint | Constraint actor (config no leída aquí) | Siempre Fixed | Ninguno (un solo cuerpo) | Cualquier joint de Unity vía data | No encontrado |
| Metadata de socket | Substring del nombre | Ninguna | Pose + id de padre + índice | Attach points con params | Índice de cara |
| Clip check | Query de overlap | Ninguno | Solo trigger colliders | Ninguno | Shape + rotor barrido |
| Persistencia | Ninguna | Ninguna | JSON (id + índice de socket) | Solo eventos | Save/restore |
| Break / split | Sweep para hallar constraints | Destruir joints | Subárbol a cuerpo de debris | `Detach()` | n/a |

Takeaways [inferred de la tabla]:
- **Quien tiene resultado estable usa un grafo o árbol como autoridad**; el joint del
  engine es una salida derivada (o no existe). Coincide con la recomendación de
  [10-repo-open-source-fusion.md](10-repo-open-source-fusion.md) §6.
- El "un rigidbody + masa sumada" de creasiege es el build más barato y robusto, pero
  pierde articulación. Queremos ambos: **clusters rígidos para joints fijos y
  constraints reales solo para hinge/slider/motor**.
- **Nadie aquí tiene un modelo de estrés estructural.** Eso es trabajo nuevo nuestro
  (§4.4).

## 4. Propuestas de diseño para GameJamm

> [!note] Contexto del repo
> El repo ya tiene `Source/GameJamm/Building` y `Core`. Los contratos de CLAUDE.md
> (`UHealthComponent`, `EJamTeam`, `UInventoryComponent`, recipe JSON) no cambian sin
> preguntar: todo lo de abajo es **aditivo**.

### 4.1 Grafo de sockets como fuente de verdad

Conservar la búsqueda de sockets de ComponentFuseMechanic para hallar candidatos, pero
guardar la conectividad en datos propios:

```
FJamPartNode   { FGuid Id; TWeakObjectPtr<UPrimitiveComponent> Comp; FName PartType; float Mass; float MaxLoad; }
FJamSocketDef  { FName Name; FName Tag /*"Bolt","Plate","AxleM","AxleF"*/; FVector Axis; EJamJointKind Allowed; }
FJamEdge       { FGuid A, B; FName SocketA, SocketB; EJamJointKind Kind; FJamJointParams P; }
```

- `UJamAssemblyGraph` (UObject simple, owned por el actor root) con
  `TMap<FGuid,FJamPartNode>` y `TArray<FJamEdge>`.
- Reemplazar el test por substring con una **tabla de compatibilidad de tags** (data
  asset: `Tag -> Tags compatibles + joint kinds permitidos`). La autoría sigue barata:
  una convención de nombre de socket `Jam_<Tag>_<Index>` parseada una vez al cargar.
- **Detach = quitar arista**, destruir el constraint de esa arista y correr
  connected-components; cada componente se vuelve su propio cluster rígido (§4.2).
  Sin sweeps. Esto corrige la debilidad del "sweep para hallar constraints" del
  reporte anterior.
- La resolución de pose del socket de creasiege
  (`pos = jointPos - rot*localPos`, spin sobre el eje con `angle`) es una buena
  rutina de snap: implementarla como
  `FTransform SolveSnap(FJamSocket A_world, FJamSocketDef B_local, float SpinDeg)`.
  Re-derivarla: son 3 líneas.

### 4.2 Joints tipados (fixed / hinge / slider / motor)

Mapear un enum a una configuración de constraint de Chaos
[memory: nombres de `FConstraintInstance`, sin contrastar con 5.8]:

| Kind | Setup del constraint | Notas |
|---|---|---|
| Fixed | Todo bloqueado (lineal y angular) | **No crear constraint si se puede evitar.** Merge del cluster: attach del hijo al root del cluster (`AttachToComponent` + `KeepWorld`) → un solo cuerpo rígido. Más barato, sin jitter, espeja el resultado de un solo cuerpo de creasiege. Constraint bloqueado solo para piezas que deben quedar en cuerpos separados (ej. rompibles bajo carga) |
| Hinge | Lineal bloqueado, Twist libre (o limitado), Swing bloqueado, límite angular opcional | El eje viene del `Axis` del socket |
| Slider | Lineal X libre dentro de límite, resto bloqueado, angular bloqueado | |
| Motor | Hinge + drive de velocidad angular (RPM objetivo, torque máximo) | El motor es un hinge con drive. Exponer el torque como `EditDefaultsOnly` |

- Siempre activar "disable collision between constrained bodies" para vecinos
  hinge/slider, si no las piezas adyacentes pelean [memory].
- Crear el constraint **una vez**, cuando el snap termina. **No copiar el loop de
  break-and-recreate por tick** (la objeción principal del reporte anterior).
- Helper de creación de joints en un solo lugar:
  `FJamJointFactory::Build(const FJamEdge&) -> UPhysicsConstraintComponent*`. Todos
  los caminos (fusión del jugador, spec del LLM, load game) lo llaman: el
  comportamiento no puede divergir.
- Lección de Clingy: defaults por kind en un DataAsset (`UJamJointProfile`) para que
  diseño tunee stiffness, límites y break force en BP sin recompilar.

### 4.3 Clip check

- Conservar la query de overlap, pero **ignorar el par que se une** (el reporte
  anterior marca el auto-rechazo como probable). Usar las shapes de colisión reales de
  las piezas, no una cápsula, como hace besiege-like con shapes convexas cacheadas.
  [inferred: UE ya tiene `ComponentOverlapMulti`, usado por el repo.]
- **Preview-then-commit**: dibujar un ghost en la pose del snap, correr el check sobre
  el ghost, commitear solo al confirmar. Cancelar deja el mundo intacto (mismo flujo
  de besiege-like).

### 4.4 Límites estructurales (tamaño jam)

Ningún repo de arriba tiene esto; todo lo de aquí es diseño propio [inferred].
Mantenerlo como **check de números, no un solver**:

- `Mass` por pieza, `MaxLoad` por arista (en Newtons, del profile).
- En cualquier cambio del grafo, BFS desde la pieza anchor del cluster (la más pesada
  o la que toca suelo/root del vehículo), acumulando masa aguas abajo. Si
  `downstreamMass * g > MaxLoad` en una arista → sobrecargada.
- Límites duros baratos que cubren la mayoría del abuso: **máx piezas por cluster
  (ej. 40), máx profundidad de grafo (ej. 8), máx cadena de hinges, máx masa total
  por motor**. Imponerlos al attach y **mostrar por qué se rechaza la colocación**.
- Rompimiento en runtime: usar el break threshold del propio constraint
  (APIs tipo `SetLinearBreakable` / `SetAngularBreakable`) [memory] y escuchar el
  evento broken → remover la arista del grafo. El reporte anterior cita el claim del
  foro de UE 5.0.2 de que el evento a veces no dispara: **no depender solo de él**;
  fallback: poll de la fuerza del constraint en un timer lento (0.2 s) para aristas
  sobrecargadas.
- Debris al partirse: como `Part.Destroy()` de creasiege, cada componente conexo se
  vuelve su propio cuerpo; recalcular centro de masa y masa de los clusters nuevos.

### 4.5 Networking-lite

Coincide con la regla de CLAUDE.md: mínimo, autoridad en el server, sin inversión
profunda. [inferred]

- El server es dueño de `UJamAssemblyGraph`. Replicar **solo la lista de aristas**
  (`FFastArraySerializer` de `FJamEdge`: 2 Guids, 2 nombres, kind, 1 float). Los
  clientes reconstruyen constraints en local y tratan la física como local, con
  corrección periódica del root transform.
- Requests por RPC: `Server_RequestFuse(PartA, SocketA, PartB, SocketB, Kind)` y
  `Server_RequestDetach(EdgeId)`. El server re-valida (§4.1, §4.3, §4.4) y responde
  cambiando el array replicado. **Los clientes nunca confían en su propio snap.**
- No intentar client-side prediction ni replicación de física para el jam.
  Single-player vs bots primero.

### 4.6 Ensamblado dirigido por spec de LLM (JSON validado)

Reutilizar la idea de creasiege (partes + `targetId` + índice de socket) pero con
joints tipados y un validador estricto. Receta propuesta, versionada, **archivo nuevo,
no el contrato JSON de recetas existente**:

```json
{ "version": 1, "name": "cart",
  "parts":  [ {"id":"p0","type":"Plank"}, {"id":"p1","type":"Wheel"}, {"id":"p2","type":"Wheel"} ],
  "joints": [ {"kind":"fixed","a":"p0","sa":"Jam_Axle_0","b":"p1","sb":"Jam_Axle_0"},
              {"kind":"hinge","a":"p0","sa":"Jam_Axle_1","b":"p2","sb":"Jam_Axle_0","limit":null} ] }
```

Pipeline: `texto LLM -> JSON -> schema validate -> graph validate -> spawn -> build`.
Validar en C++ **antes** de spawnear nada:

1. **Schema**: version, ids únicos, `type` en el catálogo de piezas, `kind` del joint
   en el enum, rangos numéricos clamped, tamaños máximos (piezas, joints).
2. **Referencial**: cada `a`/`b` existe, `sa`/`sb` son sockets reales de ese tipo de
   pieza, tags compatibles, `kind` permitido por ambos sockets, cada socket usado a lo
   sumo una vez.
3. **Grafo**: conexo (o clusters múltiples declarados), sin ciclos salvo que toda
   arista sea hinge/slider/motor (ciclos de joints fijos sobre-constrañen), dentro de
   los límites del §4.4.
4. **Geometría**: resolver los transforms de snap sobre un spanning tree, correr el
   clip check del layout resultante en una pasada descartable (sin física). Rechazar
   por overlap.
5. Solo entonces spawnear actors y llamar `FJamJointFactory::Build`.

- Al rechazar, devolver una lista corta de errores machine-readable
  (`{"path":"joints[1].sb","reason":"socket_in_use"}`) para que el LLM reintente. Cap
  de 2 reintentos.
- **Tratar la salida del LLM como input no confiable**: ningún tipo de pieza ni path
  que invente se carga; el catálogo es un allowlist. Nunca dejar que el texto de la
  spec elija asset paths ni llame funciones. (Precedentes y lecciones de error
  handling: [09-auditoria-fuentes-publicas.md](09-auditoria-fuentes-publicas.md) §6.)
- Save/load usa el mismo JSON → **fixtures de test gratis**.

### 4.7 Lista de trabajo ordenada (deadline del jam: 15 nov 2026)

Hoy es 13 nov y el feature freeze es dom 15 nov 12:00 CDMX → **solo los primeros
tres**; el resto es post-jam:

1. Arreglar los 4 defectos pequeños del reporte anterior, más
   `Fixed = merge del hijo al cluster` en vez de constraint. [Cambio mínimo, máxima
   ganancia de estabilidad.]
2. Constraint creado una sola vez tras el snap; borrar el loop por tick.
3. Grafo + quitar arista para el detach (`UJamAssemblyGraph`), sin replicación.
4. Profile de hinge y motor (un DataAsset).
5. Validador de JSON de spec (solo pasos 1-3 de §4.6; sin pasada de geometría).
6. Límites estructurales, networking y validación de geometría: **después del jam**.

## 5. Gaps e incertidumbre

- ComponentFuseMechanic no se releyó; las afirmaciones sobre él vienen del reporte
  anterior ([10-repo-open-source-fusion.md](10-repo-open-source-fusion.md)) y del
  brief de la tarea.
- Los nombres de la API de joints de UE (`SetLinearBreakable`, APIs de drive, flag de
  disable-collision) son de memoria, sin contrastar con headers de UE 5.8. Verificar
  en el editor antes de depender de ellos.
- Clingy y OwnWar no se leyeron más allá del README, nombres de archivo y firmas de
  estrategias. godot-jolt-physics-joint: solo README.
- La demo de construcción y besiege-like no tienen licencia: bajo copyright por
  default no son reutilizables; aplica la regla del reporte anterior (reescribir desde
  la descripción, nunca copiar expresión).
- Las secciones de límites estructurales, networking y schema de spec son propuestas
  de diseño, no probadas. No se compiló ni corrió código.
- El bug de `results[1]` en la demo de construcción es una lectura del código, no un
  fallo reproducido.

## → Aplicable a GameJamm

1. **Grafo de ensamblado aditivo** (`UJamAssemblyGraph` + structs §4.1) sin tocar los
   contratos compartidos de CLAUDE.md.
2. Fixed = merge de cluster; hinge/slider/motor = constraint creado **una vez**;
   factory única (`FJamJointFactory`).
3. Clip check con shapes reales + preview-then-commit (besiege-like) ignorando el par
   objetivo (lección del auto-rechazo del repo de referencia).
4. Límites estructurales baratos (BFS de carga + caps) y debris por connected
   components.
5. La spec JSON del asistente IA (§4.6) es un archivo **nuevo** versionado con
   validación estricta y errores machine-readable — el contrato JSON de recetas del
   GDD no se toca.
6. Orden de trabajo para el freeze: §4.7 ítems 1-3; el resto post-jam.
