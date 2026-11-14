---
title: 08 — Mapeo de mecánicas → UE5 (GameJamm)
date: 2026-11-13
tags:
  - research
  - ue5
  - implementacion
status: listo
---

# 08 — Mapeo de mecánicas del juego de referencia → Unreal Engine 5 (GameJamm)

> [!info]
> Cómo traducir cada mecánica de la referencia a nuestra base UE 5.8 (C++ + DataAssets).
> Los valores numéricos que lleguen de la investigación (docs 01–04) van a los DataAssets,
> nunca a constantes de código. Estado: borrador operativo para el jam.

## 0. Principio rector (el de la desarrolladora, aplicado a 2 días de jam)

La referencia logra "emergencia" con: (1) mundo 100% física + (2) interacciones
**multiplicativas** sin implementación dedicada. Nosotros no tenemos tiempo de física
emergente real → **fingimos la emergencia con una tabla de interacciones** (elemento ×
elemento × material → efecto) y constraints rígidos. La sensación de "lo que intento
funciona" es lo que se evalúa, no el motor de reacciones en cadena.

## 1. Construcción → sistema de construcción

| Mecánica (referencia) | Implementación UE5.8 | Notas |
|---|---|---|
| Agarrar objeto (rango limitado, glow) | Trace/canal interactuable + `UPhysicsHandleComponent` para cuerpos con física; para piezas de construcción, mover kinemático con spring | El glow = material emissive en el preview mesh |
| Attach points (masilla verde) | `UAttachPointComponent` (sockets en el mesh) + overlap de proximidad: al acercar pieza B a A, buscar el socket más cercano dentro de radio → snap con orientación | Preview = mesh fantasma translúcido en el punto de snap |
| Pegar = **soldar** (cuerpos rígidos, no articulación) | Al confirmar: `SetSimulatePhysics(false)` en la pieza hija + `AttachToComponent(root, KeepWorld)` + desactivar colisión pieza-pieza. El ensamblado se comporta como UN cuerpo rígido gobernado por el root | Alternativa si se necesita detach sin romper todo: `UPhysicsConstraintComponent` con X/Y/Z/Angular en Locked. Lo simple y robusto para jam = attach kinemático. Verificar sensación al empujarla: debe destrabarse con física del root |
| Rotación 90° + fino | Teclas: Q/E = 90° yaw; mantener Shift + mouse/stick = ajuste fino (pitch/roll en pasos de 15°) | La referencia usa pasos de **45°** (confirmado) con marco de referencia del jugador; una tecla apoya plano. Mapear Q/E a 45° |
| Despegar por *wiggle* | Input acumulado: mover la pieza agarrada con "shake" (movimiento alternado rápido) suma a un contador; superado el umbral → detach | El detalle de la referencia: se despega la pieza **que mueves**, no la creación entera; el despegue no es instantáneo (umbral) |
| Límite de piezas | Constante en `UBuilderComponent` (TUNABLE). **La referencia usa 21 exacto** ("You can't attach any more") | Valor confirmado por datamine/comunidad — usar 21 como default |
| Creación persistente en el mundo | El ensamblado vive como Actor propio (root + children); al "activar", se simula física | Si se aleja el jugador mucho: opcional despawn + respawn en invocación rápida |

Contrato con el asistente IA: la receta JSON del GDD (`parts`, `attach_to`, `socket`,
`activate`) es EXACTAMENTE un ensamblado de construcción simplificado. El builder debe poder
ejecutar la receta programáticamente: `BuildFromRecipe(recipe)` → misma ruta de código que
el grab manual. **Un solo pipeline**: manual = receta construida en vivo por el jugador.

## 2. Fusión → fusión de armas

| Mecánica (referencia) | Implementación UE5.8 | Notas |
|---|---|---|
| Material + arma | `UFusionComponent::Fuse(UJamFusionMaterialData*)`: aplica `AttackPowerBonus`, `DurabilityBonus`, `EffectType` al arma; material sale del inventario | DataAsset `UJamFusionMaterialData`: `AttackBonus`, `DurabilityBonus`, `EJamEffectType` (Fire/Ice/Shock/Bomb/Knockback), `EffectMagnitude`, VFX/SFX refs |
| Dos armas NO se fusionan | En la referencia **SÍ se puede arma↔arma** (aporta el attack base, sin bonos). Implementar: material = otra pieza armable o DataAsset | [02-mecanicas-fusion.md](02-mecanicas-fusion.md) §1 |
| Flechas fusionadas | Al disparar, si el arco tiene material: el proyectil hereda `EJamEffectType` | Bomba = AoE; fuego = DoT; hielo = slow/freeze; rayo = shock AoE contra metal; "ojo" = homing |
| Escudo fusionado | Solo visual + bonus de defensa (no snowboard en MVP) | Stretch |
| Daño total | `Attack = BaseWeapon + MaterialBonus` (fórmula confirmada de la referencia) | Tabla de FuseAP de referencia en [02](02-mecanicas-fusion.md) §2 |
| Durabilidad | `Uses = BaseUses + DurabilityBonus`; al llegar a 0 → arma rota (VFX + sonido). La referencia: **+25** con material de monstruo, **una sola vez por arma** | La degradación es core: fuerza lootear. Bonus por familia si hay tiempo (+10 noble de guardia, +5 artesanía del desierto…) |

MVP del GDD: 3 materiales (Piedra/Fuego/Hielo) × 2 armas (Espada/Flecha). La tabla soporta
más materiales sin cambiar código.

## 3. Vehículos / piezas del kit

| Mecánica (referencia) | Implementación UE5.8 | Notas |
|---|---|---|
| Piezas (Llanta, Tabla, Motor/Cohete, Lanzallamas, Roca, Asiento) | `ABuildPart` + `UBuildPartData` (DataAsset): `ThrustForce`, `EnergyCostPerSec`, `bSteerable`, `Torque` | Consumos de referencia: ventilador **0.156** pozos/seg, ruedas 0.015, emisores 0.18, cañón 0.3 ([03](03-mecanicas-vehiculos-dispositivos.md)) |
| Propulsión (ventilador/cohete) | Fuerzas de Chaos: `AddForce`/`AddTorque` desde el part mientras `activate` y haya energía | Cohete = impulso fuerte ~3 s; ventilador = fuerza sostenida apilable; "activar uno activa todos" (comportamiento de la referencia) |
| Dirección (asiento de dirección) | Si la creación tiene pieza `bSteerable` y el jugador está sentado/atado: input del jugador rota el ensamblado (torque). En la referencia el asiento **enciende todos los dispositivos conectados** | |
| Batería (celda de energía) | Pool global del jugador (`float`); cada pieza activa drena `EnergyCostPerSec`; **recarga solo cuando nada consume** (regla de la referencia); recarga en estaciones del mapa | Baterías desechables pegadas al build drenan ANTES que la del jugador (regla de la referencia) — permite PvP sin castigar con el pool global |
| Lanzallamas sobre rueda | Composición libre: cualquier pieza se attach a cualquier socket; el efecto es de la pieza, no del vehículo | La combinatoria ES el juego: "no hay programa de vehículo" (Takayama, GDC 2024) |

## 4. Chemistry engine (fingido) → tabla de interacciones

Actor de mundo con tags de material: `Metal`, `Wood`, `Fire`, `Ice`, `Water`, `Grass`,
`Electric`, `Wind` (componente `UJamMaterialTagComponent` o tags del actor).

**Tabla `UJamInteractionTable`** (rows: A×B → efecto):
- Fire × Wood → propagar (timer + radio, quemar grass/torres de madera)
- Fire × Ice → Water (derrite)
- Fire × Grass → área quemada breve
- Ice × Water → plataforma temporal (hiela superficie)
- Electric × Metal → conduce: AoE en cadena a metálicos cercanos
- Electric × Water → AoE grande
- Wind × Fire → apaga
- Metal × Ice/Metal: knockback de choque físico ya lo da Chaos

Implementación: en `BeginPlay`/overlap, los actores se registran en un `UJamChemSubsystem`
por proximidad; el subsystem tick evalúa pares contra la tabla (barato: pocos actores).
Nada de grillas de temperatura: volumen + proximidad alcanza para la sensación.

**Valores de partida del chem engine real del predecesor** (`system.chmres.yml`, unidades
físicas reales — copiar el orden de magnitud, no el valor exacto; detalle en
[05](05-re-engine-interno.md) §3): `heat_transfer_speed_rate 24.0`, `burn_speed_rate 0.4`,
`lightning_distance 25-35 m`, `electric_water_extra_radius_rate 500.0` (el agua "fuga"
electricidad 5× más lejos), `min_ice_size 0.15 m`, `character_ignition_count 90` frames.
Materiales con `ignition_point [°C]` / `burn_speed` / `wetting_amount [kg/m²]` por material
(`terrain_grass_mesh`: ignition 250°, burn 1.0, wetting 2.0).

## 5. Atravesar techos / Rebobinar — fuera de alcance

Decisión: **no en el MVP**. Atravesar techos rompe el tower-push (saltarse torres).
Rebobinar es la excepción deseable pero costosa (grabado de trayectorias); solo si sobra
tiempo: `UJamRecallComponent` que graba posición por 8 s y rewinds la creación activa.

## 6. Checklist de integración con el código existente

- `Source/GameJamm/Building/BuildRecipe.h` ya define el contrato de recetas → el builder
  debe implementar `BuildFromRecipe` (misma ruta que el snap manual).
- `UInventoryComponent` ya existe → la fusión consume de ahí.
- DataAssets nuevos (sin tocar contratos compartidos): `UJamFusionMaterialData`,
  `UBuildPartData`, `UJamInteractionTable`.
- Valores tuning → BP/DataAssets con `UPROPERTY(EditDefaultsOnly)` (regla CLAUDE.md).
- No romper formato JSON de recetas (contrato compartido con el asistente IA).

## 7. Implementación de referencia existente (ComponentFuseMechanic)

Existe una implementación pública y funcional de exactamente este sistema, en UE 5.2+:
**[EvelynSchwab/ComponentFuseMechanic](https://github.com/EvelynSchwab/ComponentFuseMechanic)**
(actor component inspirado en la construcción de la referencia, construido sobre Chaos).
Licencia propia en el repo (LICENSE); verificar condiciones antes de copiar código — el
patrón es lo valioso.

### Arquitectura que usa (validada en juego real)

`UFFuseComponent : UPhysicsHandleComponent` con máquina de estados:
`NONE → SEARCHING → FUSING → ACTIVEFUSING`.

| Paso | Cómo lo hace |
|---|---|
| 1. Buscar fusible | Sweep esférico por `ECC_PhysicsBody` desde la cámara (radio 16, distancia 1000). Es fusible si **algún socket contiene "Attach" en su nombre** (`FusableSocketSubName`) |
| 2. Agarrar | `GrabComponentAtLocationWithRotation` (PhysicsHandle). Distancia objetivo: min 50 / max 1000 cm + radio de bounds; altura ±500 |
| 3. Rotación | Input × multiplicador de rotación (**45° por paso**, redondeado al múltiplo más cercano) — equivale al D-pad de la referencia |
| 4. Encontrar par de sockets | `SweepMulti` radio = bounds de la pieza + `MaxFuseDistance` (75). Bucle doble sobre sockets ("Attach") de pieza agarrada × vecinas; se queda con el **par más cercano** |
| 5. Anti-clipping | Para cada par candidato: calcular transform objetivo alineando socket→socket (rotación redondeada a 45°) → `ComponentOverlapMulti` → si colisionaría con otro fusible, **rechazar el par** (debug: cápsula roja) |
| 6. Sockets suplementarios | Con el par ideal fijado, ubicar dónde caerían los *demás* sockets de la fuente; los que queden a ≤5 cm de sockets del target se añaden como **pares suplementarios** → constraints adicionales (anclaje multi-punto, como la "masilla" de la referencia que refuerza la unión) |
| 7. Fusionar | Spawn de `APhysicsConstraintActor` en el socket del target → `SetConstrainedComponents(target, src)`; durante la operación: **limits LOCKED** (twist/swing1/swing2) = soldadura rígida; interp de la pieza hacia el objetivo (`VInterpTo`, speed 10) con `BreakConstraint` + re-constrain por tick; si supera 1.5 s → interp directa sin física; failsafe 10 s → snap directo |
| 8. Después de fusionar | Limits a `ACM_Limited 1°` (casi rígido, con un mínimo de cedencia) |
| 9. Despegar | Buscar los `PhysicsConstraintActor` cercanos/adjuntos, `BreakConstraint` los que involucren la pieza agarrada |
| 10. Preview | `FFuseOrthoProjectionActor`: **3 SceneCapture2D + render targets** para dibujar proyecciones ortográficas (top/front/right) del objeto agarrado — la vista tipo de la referencia del objeto flotante |
| 11. Debug | CVAR `f.drawdebugfuser` dibuja trazas, cápsulas válidas/inválidas, flechas de pares |

### Qué copiar / qué cambiar para GameJamm

- **Copiar el patrón**: nombre de sockets con subnombre ("Attach"), par más cercano +
  suplementarios, clipping check, constraint LOCKED + interp.
- **Cambiar**: el autor usa constraints sueltos por unión; su propio README reconoce que
  las relaciones serían mejor en un **actor de ensamblado** (los constraints como hijos de
  un solo actor de creación) — que es exactamente lo que la referencia hace (la creación es
  una entidad) y lo que propone nuestro `ABuildAssemblyActor`: 1 actor root + piezas attach
  kinemáticas (doc 08 §1) + constraints opcionales para las uniones "articuladas" (ruedas,
  motores). Para el jam: **attach kinemático como base**, y si una pieza necesita girar
  (rueda), constraint de revolución (swing libre) en ese par.
- Extrapolación útil: el magnetismo del snap en la referencia prioriza par más cercano +
  descarta clipping + **redondea orientación a 45°** — 3 reglas y ya se siente
  "inteligente".

## 8. Risks

1. **Attach kinemático + física**: al attach un cuerpo simulando física, desactivar su
   simulación y colisión mutua; si la creación "vuela al activarla", revisar orden de
   attach/simulación (lo clásico de Chaos).
2. **Snap para 6 piezas**: el preview ghost necesita mesh o caja genérica si falta el asset.
3. **No cayeron en el overbuild**: el límite de piezas y la batería son los frenos de
   diseño; sin ellos el mapa se llena de junk en 5 min.
