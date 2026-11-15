# Arte con Tripo — nota para los agentes del equipo

> [!info] Para quién es
> Para los agentes de Construcción (Jesús), Core/IA (César) y Arte (Emilio). Resume qué arte se reemplazó con Tripo,
> qué **no** hay que pisar y qué cambios de código pide el arte nuevo. Dueño: Javier (ver [TEAM.md](TEAM.md)).
> Pipeline y registro de créditos: [SourceArt/Tripo/README.md](../SourceArt/Tripo/README.md).

## Acuerdo del equipo (14 nov)

Javier puede reemplazar **arte** de cualquier carpeta sin pedir permiso, dejando constancia aquí. El **código** sigue
siendo de su dueño: lo que el arte necesita de código se pide en la sección "Pendiente" de abajo.

## Qué cambió

- **Las 35 piezas de construcción** de `Content/Game/Building/Parts/SM_*` (todas las de `RealMeshNames()` en
  `JamBuildPart.cpp`) ahora son modelos de Tripo. Misma ruta y nombre, así que el código no cambió.
- Cada malla cumple el contrato de `AJamBuildPart::InitFromDef`: caja **exactamente igual a `Size`**, pivote al centro,
  X = adelante, Y = derecha, Z = arriba, ruedas con eje en Y, sockets `Attach_*` / `Fx_Out` y **un solo slot de material**.
- Materiales y texturas: `Content/Game/Art/Tripo/` (`M_Tripo_Master` + `MI_<id>` + `T_<id>_D/_N/_MR`, 1K o 512).
  `M_Tripo_Master` tiene el escalar `Emissive` que usa `SetGlow`: al encender, brilla la textura teñida de cálido.
- Comparación antes/después: `SourceArt/Tripo/previews/_comparacion_vehiculo.png`.

## Mundo (14 nov, Rodin)

- **Torres y bases del juego:** los 6 actores `ATeamStructure` de `L_Main` ahora usan
  `Content/Game/Art/Tripo/Structures/SM_STRUCT_{TOWER|KEEP}_{RED|BLUE}` (pivote abajo al centro, mismo tamaño que
  `SM_TOWER_WATCH`/`SM_BASE_KEEP`) y ya **no** llevan `MI_Structure_Red/Blue` forzado: la textura del equipo viene en la malla.
  Rojo mira a +X y azul a −X. Script: `SourceArt/Tripo/scripts/ue_assign_structures.py`. Posición, escala y vida no cambiaron.
- **`MapDressed`:** banderas, barricadas y plataformas de spawn de cada equipo reemplazadas **en su lugar** (misma caja y
  posición del mundo; `batch_world.json`). También se reemplazaron `SM_MAP_Dressed_Tower*` y `*Base` aunque hoy ningún actor
  de `L_Main` las usa.
- Los archivos `.uasset` de actores de `L_Main` pueden quedar en **solo lectura** en copias viejas (eran bloqueables): si el
  editor no guarda, quitar el atributo de solo lectura a esos archivos.

## Islas (14 nov, PBRgen) — PROVISIONAL

> Look todavía en evaluación (Javier): se puede volver al plano con `DetailStrength = 0` o con `git checkout` de los
> `M_Map_XX` previos a este commit.

- `M_Map_00` (cima de islas), `M_Map_02` (pradera y cerros), `M_Map_01` (roca de abajo), `M_Map_04` (tablas de puente) y
  `M_Map_05` (caminos y cuerdas) de `Content/Game/Core/World/MapDressed/` se reconstruyeron **en su lugar** (los mismos
  ~170 actores de `L_Main` los siguen usando): `Color` conserva su nombre y valor (ancla de paleta de `docs/ART.md`) y se le
  suma detalle pintado de PBRgen proyectado en el mundo (las islas no tienen UVs fiables): desde arriba en pasto, tablas y
  caminos (en caras verticales el detalle se apaga), triplanar en roca. Texturas en `Content/Game/Art/Tripo/Textures/World/`.
- Perillas en cada material: `DetailStrength` (**0 = el look plano de antes**), `TileSize` (cm) y `Roughness`.
- Anti-repetición en pasto (`M_Map_00`, `M_Map_02`) y tierra (`M_Map_05`): segunda lectura rotada 37° a otra escala
  (`BigScale`, `BigStrength`), manchas grandes de ruido (`MacroStrength`), pasto seco (`DryTint`, `HueVariation`) y menos
  detalle a lo lejos (`FarStart`, `FarRange`, `FarDetail`). Costo: ~4 lecturas de textura más por píxel (sin medir FPS aún).
- Script: `SourceArt/Tripo/scripts/ue_world_mats.py` (datos arriba del archivo). `M_Map_03` (agua) y `M_Map_06` (molino)
  no se tocaron. Si alguien regenera `MapDressed` con `gen_map_dressed.py` y reimporta los materiales, volver a correr el script.

## Matas y flores 3D (14 nov, Rodin) — PROVISIONAL

- 4 mallas en `Content/Game/Art/Tripo/Foliage/` (`SM_FOL_GRASS_A/B`, `SM_FOL_FLOWERS_A/B`, 290–390 tris, pivote abajo, sin
  colisión) regadas sobre el pasto (`M_Map_00`/`M_Map_02`) con un solo actor nuevo en `L_Main`: **`FOL_Scatter`** (carpeta
  `Dressing/Foliage`), un HISM por malla, ~1,560 instancias, **sin colisión** (no frenan vehículos ni bots), sin sombra, corte
  a 45–65 m y siempre cargado.
- Script re-ejecutable: `SourceArt/Tripo/scripts/ue_scatter_foliage.py` (borra y vuelve a regar con la misma semilla; solo
  coloca donde el rayo pega en el pasto, así quedan fuera caminos, árboles y bases). Densidad: `CLUSTER_M2`.
- Si algo estorba (p. ej. tapa una mecánica), se puede borrar el actor `FOL_Scatter` sin tocar nada más.

## Props del mapa (14 nov, Rodin) — PROVISIONAL

- **128 mallas de `MapDressed` reemplazadas en su lugar** (mismo nombre y ruta; los actores de `L_Main` no cambiaron): 13
  árboles, 33 matas de conos (ahora arbustos), 8 hongos, 13 flores, 16 rocas, 6 faroles, 2 agujas, 2 letreros de base, 2
  tiendas (roja y azul), los 4 campamentos (arco, pilar, barril, cofre + tapa, caja, fogata, chatarra) y el molino.
- 25 modelos compartidos (`SourceArt/Tripo/raw/SM_PROP_*`) con MI y texturas compartidas
  (`Content/Game/Art/Tripo/Materials/MI_PROP_*`, `Textures/T_PROP_*`). Las flores reusan `MI_FOL_FLOWERS_A/B` de las matas.
- **Misma base y casi la misma caja:** cada modelo conserva la altura de la malla vieja y sus propias proporciones
  (`"fit": "height"`), así que el ancho puede variar unos cm. Colisión compleja `BlockAll` igual que antes.
- **Cofres:** el cuerpo ahora toma la huella de su tapa (antes estaba girado respecto a ella); cuerpo y tapa salen del mismo
  modelo partido en dos (`"zclip"`).
- **Molino:** `SM_MAP_Dressed_Windmill_Base` es ahora el molino completo, con las aspas hacia el camino. Las aspas viejas
  (`Windmill_Sail1/2`) flotaban sobre la torre: sus 2 actores de `L_Main` quedan **ocultos y sin colisión, no borrados**.
- Costo: ~100k triángulos entre las 128 (antes 11k), sin LOD. Medido en la GTX 1650 (14 nov): 60 FPS sostenidos con la
  cámara en el spawn (GPU 11.5 ms, antes 9.8; p95 21.7 ms); en PIE la vista de los cerros cuesta ~24 % más GPU que el spawn,
  así que ahí queda poco margen. Si baja de 60: primero `CLUSTER_M2` de las matas y los `tris` del lote.
- Lote re-ejecutable: `SourceArt/Tripo/batch_props.json` → `normalize_batch.py -j 6` → importador. Si alguien regenera
  `MapDressed` con `gen_map_dressed.py`, hay que repetirlo.
- **Para el dueño del mapa (César):** arbustos, hongos y flores chocan (`BlockAll`), igual que los conos de antes. Si frenan
  vehículos o bots, quitarles la colisión es decisión del mapa.
- **Para César (loot):** los cofres de `Loot.json` (`chests`, ±8600/±3600 y ±4000/∓6000) caen justo en el cofre decorativo de
  cada campamento (`CampXX_Chest` + `ChestLid`); `GroundAt` choca con su tapa y el cofre de loot queda encima. Ya pasaba con
  el cofre viejo. Si se quiere uno solo, mover el de `Loot.json` o pedirme que oculte el decorativo.

## No pisar

- **No reimportar los FBX viejos** de `SourceArt/Building/FBX/` sobre `Content/Game/Building/Parts/`: reemplazaría el
  arte nuevo. Las fuentes nuevas están en `SourceArt/Tripo/FBX/` y se importan con `SourceArt/Tripo/scripts/ue_import_tripo.py`.
- Si cambias el `Size` de una pieza en `JamBuildPart.cpp`, avisa a Javier: la malla visual se dibuja con escala absoluta
  y tiene que medir exactamente `Size` (se regenera en segundos con `normalize_batch.py`).

## Pendiente de código (lo pide el arte)

### Construcción (Jesús): 10 piezas de solo datos sin malla

Las mallas ya están importadas en `Content/Game/Building/Parts/` con la caja de su `Size` actual. Para que se vean
basta agregar estas entradas a `RealMeshNames()` en `Source/GameJamm/Building/JamBuildPart.cpp`:

```cpp
{"Deck", "SM_PART_DECK"}, {"Rod", "SM_PART_ROD"}, {"UBlock", "SM_PART_UBLOCK"}, {"Cart", "SM_DEV_CART"},
{"RailSkid", "SM_DEV_RAILSKID"}, {"IceBlock", "SM_DEV_ICEBLOCK"}, {"Spikes", "SM_DEV_SPIKES"},
{"Launcher", "SM_DEV_LAUNCHER"}, {"WagonWheel", "SM_DEV_WHEEL_WAGON"}, {"Propeller", "SM_DEV_PROPELLER"},
```

`Spikes` lleva las púas hacia +X (cara frontal) y `Propeller` es un disco en YZ como `Fan`.

### Personajes 2 (César): dos ajustes antes del freeze (dom 15, 12:00 CDMX) — HECHO

> [!important] Para César (el agente lo puede repetir tal cual)
> "Javier pide dos ajustes chicos a las figuras (~30 min en total), ya probadas en PIE con tu `UJamCharacterLook`:
> (1) que la figura se siente **sobre** el asiento, no 40 cm arriba; (2) que la espada y el arco vayan **en la mano**.
> Los números van en `Characters.json` para que Javier los afine sin recompilar."

1. **Sentado (`UJamCharacterLook`)**:
   - Problema: mientras está montado (`GetAttachParentActor()`), la figura queda ~40 cm arriba del asiento. `Jam.Seat`
     pone la cápsula 110 cm sobre el asiento, y la animación `Sit` es de silla: los pies van en la base de la cápsula y la
     cadera, a altura de silla.
   - Pedido: leer el campo opcional `SitOffset` [x,y,z] de la entrada y, mientras se reproduce `Sit`, poner la malla en
     `Offset + SitOffset`. Al bajarse, volver a `Offset`.
   - Ya está en el JSON para `Player` y `BotHero` (`[0, 0, -40]`); si falta, vale 0.
2. **Arma en la mano (`JamWeaponComponent`)**:
   - Problema: hoy `HandMesh` cuelga de la cápsula en (45, 38, 0) y flota junto a la figura.
   - Pedido: si el dueño tiene `GetMesh()` con SkeletalMesh y el hueso `hand_r` existe, pegar `HandMesh` a ese hueso con
     el campo opcional `Weapon` de su entrada: `{"Socket": "hand_r", "Offset": [x,y,z], "Rot": [pitch,yaw,roll]}`.
   - Con la mano no hace falta el balanceo por código (`SwingAlpha`, ~línea 478): el golpe ya viene en la animación
     `Attack`.
   - Sin SkeletalMesh queda como hoy.
   - `Player` ya trae un `Weapon` inicial en el JSON; Javier afina el giro viéndolo en PIE.

**Hecho cuando:** montado en un carro, la cadera queda en el asiento, y a pie la espada y el arco siguen la mano al
caminar y al atacar. Sin figura, todo queda como hoy.

> [!note] Respuesta de César (14 nov)
> Hecho en `UJamCharacterLook` + `UJamWeaponComponent::AttachToHand`. Campos de `Weapon`: `Socket`, `Rot` [pitch,yaw,roll],
> **`Grip`** [x,y,z] = punto de la malla del arma que queda en la mano (la espada mide 100 cm con pivote al centro: el mango
> está en x = -40), `Offset` (en el hueso) y lo mismo con prefijo `Bow` para el arco (si faltan, usa los de la espada).
> Puse la espada en **`hand_l`**: la animación `Attack` del jugador es un jab con la izquierda (la derecha se queda en el
> pecho), así el golpe sale como estocada. Valores actuales en `Characters.json` (`Player.Weapon`): espada
> `Rot [-75,180,-90]`, `Grip [-40,0,0]`; arco `BowRot [0,-90,90]` en `hand_l`. El arco se puede afinar más.
> Para afinar en vivo: `Jam.Grip P Y R GX GY GZ hueso`, `Jam.Cam dist angulo alto` (cámara fija), `ShowHUD` y
> `Jam.Later seg comando`. `SitOffset` ya se aplica al reproducir `Sit`.

> [!success] Verificado por Javier en PIE (14 nov, ~21:20) y afinado en `Characters.json` (`319572c`)
> - **Espada:** bien en `hand_l`, apunta al frente; sin cambios.
> - **Arco:** con `BowRot [0,-90,90]` quedaba de canto y detrás del brazo; desde la cámara solo se veía la cuerda. Ahora
>   va inclinado hacia afuera: `BowRot [-84.3,-25.2,-122.6]`, `BowGrip [13,0,0]`.
> - **Sentado:** el jugador queda bien con `SitOffset -40` (pelvis a 17 cm sobre el asiento). El bot héroe tiene escala
>   de actor 1.1 (`JamBotHero.cpp:77`): con −40 quedaba ~9 cm más hundido, así que va a `-30`.
> - **Muerte:** jugador y bot héroe ya no se esconden al instante, sino 2 s después y solo si siguen muertos (`40eb197`,
>   aviso a César). `Death.Rate` del jugador sube a 2.0: cae a ~1.1 s, queda en el suelo y se esconde a los 2 s.
>   Minion y bot héroe también se ven caer.
> - **Detalle menor para César (no bloquea):** en vista V el arma sigue pegada a la mano de la figura, que es invisible
>   para el dueño. Mirando al frente no se ve el arma; mirando hacia abajo la espada flota sola. Antes de la figura
>   pasaba algo parecido.
> - **Build Shipping** de prueba OK (6 min 49 s, 0 errores; arranca, figuras y mapa cargan). En Shipping no hay consola
>   y no corre `-ExecCmds`. El panel "Cómo jugar" todavía anuncia "~ consola" y "Consola: Jam.Say…" (es de UI: avisar
>   si se quiere quitar).

> [!note] César (14 nov, noche): espadazo y tiro con arco hechos por código; la espada vuelve a `hand_r`
> - **Por qué:** con la espada en `hand_l` la hoja atravesaba el cuerpo al correr, y el jab de `Attack` no parecía un
>   espadazo. Como Javier estaba con otra cosa, no se pidieron clips nuevos.
> - **Qué hace:** la figura usa `UJamFigureAnim` (C++, sin Anim Blueprint). Reproduce los mismos clips de
>   `Characters.json` y encima gira hombro, codo y mano por dirección:
>   - **Espadazo** en diagonal (0.42 s): carga arriba a la derecha, golpe al frente y remate abajo a la izquierda.
>   - **Arco** (0.6 s): brazo del arco extendido con el arco vertical; la otra mano tensa junto a la cara y suelta hacia
>     atrás. Sigue la mira vertical.
>   - **Espada cargada:** al caminar y correr la punta va al frente y hacia afuera, sin cruzar el cuerpo.
> - Ya no se reproducen los clips `Attack` ni `Shoot` del jugador; siguen en el JSON por si `jam.Figure.Moves 0`.
> - `Player.Weapon.Socket` pasa a `hand_r`; el resto de los valores (incluido el arco afinado) no cambió.
> - Para ver una pose quieta: `jam.Figure.Freeze 0.48` (avance 0..1; `-1` lo apaga).
> - Las poses están en `Source/GameJamm/Core/JamFigureAnim.cpp` (`SlashKeys`, `DrawArmKeys`…). Si luego hay clips de
>   tajo o de tensar el arco, basta con apagar esto y ponerlos en `Attack`/`Shoot`.

### Personajes (César; aviso a Jesús): figuras rigeadas — HECHO (`08a1bdb`, empaquetado `a574410`)

> [!success] Estado (14 nov, 19:45)
> César subió `UJamCharacterLook` y la configuración de empaquetado. Javier lo probó en PIE:
> - Jugador, minions, bots y guardias salen con su figura (24 de 24 agentes) y no hay errores de carga.
> - Caminar, correr, espadazo (`Attack`) y arco (`Shoot`) cambian la animación como se esperaba.
> - FPS en el spawn con la oleada en pantalla: 60 (GPU 9.3 ms; antes ~7.0).
>
> Más tarde se probó en PIE: la primera persona (V) y reaparecer funcionan. Al morir, el jugador se esconde al instante
> (`HandleDied`), así que su animación `Death` no se ve.
>
> **Build empaquetada de prueba (14 nov 20:20, Development, `BuildCookRun` en 5 min, 0 errores y 0 avisos):**
> - Entran los 4 JSON.
> - En el log del juego empaquetado, jugador, bots, guardias y minions cargan su figura ("Look ... con 8 animaciones").
> - El loot sale de `Loot.json` y `Jam.Type` construye con el respaldo de palabras clave.
> - Falta probar la build **Shipping** (la del jurado), que no acepta consola.
>
> Los dos ajustes de arriba son la continuación.

> [!note] Pedido original (para referencia)
> - **César:** "Javier pide ~1 h de código antes del freeze: que jugador, minion, bot héroe y guardia usen las figuras
>   rigeadas de Rodin (SkeletalMesh + animaciones sueltas, sin Anim Blueprint) cuando existan, y el cilindro de hoy si no.
>   Arte, materiales y datos los pone Javier en `Content/Game/Art/Tripo/Characters/`. Detalle abajo."
> - **Jesús:** "Para personajes no hace falta tocar tu código: César oculta `Body`/`Nose` desde `AJamCharacter`. Lo único
>   pendiente tuyo sigue siendo agregar las 10 líneas a `RealMeshNames()` (sección de arriba)."

**Ya existe (Javier, en `Content/Game/Art/Tripo/Characters/`), los cuatro:** `SK_CHAR_{PLAYER,MINION,BOTHERO,GUARD}`
(cada uno con su `_Skeleton`), `Anims/A_CHAR_<X>_{Idle,Walk,Jog,Run,Attack,Throw,Hit,Death,Sit}` (+ `Swing` o
`RunComic`), `Materials/MI_CHAR_<X>_{RED,BLUE}` y `MI_CHAR_GUARD_NEUTRAL`. Esqueleto UE5 (nombres de Manny), pivote en
los pies, mira a +Y como Manny (por eso `Yaw` −90), un solo slot de material. **No hace falta código distinto por
personaje**: todo sale del JSON.

**Datos:** `Content/Game/Art/Tripo/Characters/Characters.json` (lo edita Javier sin recompilar; ya trae las 5 claves). Por
clave: `Mesh`, `Materials` (`Red`/`Blue`/`Neutral`), `Offset` [x,y,z], `Yaw`, `Scale`, `RunAbove` y `Anims` con
`Idle`, `Walk`, `Run`, `Attack`, `Shoot`, `Hit`, `Death`, `Sit`; cada una con `Asset` y opcionales `Speed` (cm/s en que
el ciclo no patina) y `Rate` (velocidad fija de las acciones). Claves: `Player` = `AJamCharacter`, `Minion` =
`AJamMinion` (Melee/Ranged/Heavy), `BotHero` = `AJamBotHero`, `Guard` / `Thrower` = `AJamCampGuard` según `bThrower`.

**Código que se pide (César, en `Core/` y `AI/`; sugerencia: un componente `UJamCharacterLook` en `Core/` que usen los dos):**

1. **Leer el JSON** una vez con caché estática, como `Loot.json`:
   `FPaths::ProjectContentDir() / "Game/Art/Tripo/Characters/Characters.json"`. Si no hay archivo ni entrada, o
   `LoadObject<USkeletalMesh>` falla, **no hacer nada**: queda el cilindro. Nunca crashear.
2. **Aplicar** al iniciar (BeginPlay; en minion/bot/guardia también sirve al final de `InitAgent`):
   - `GetMesh()->SetSkeletalMesh(...)`, `SetRelativeLocationAndRotation(Offset, FRotator(0, Yaw, 0))` y
     `SetRelativeScale3D(FVector(Scale))`.
   - `SetCollisionEnabled(NoCollision)` y `SetAnimationMode(EAnimationMode::AnimationSingleNode)`.
   - En minions, `VisibilityBasedAnimTickOption = OnlyTickPoseWhenRendered`.
   - Ocultar el cuerpo viejo con `SetVisibility(false)`: en el jugador `Body` y `Nose` (públicos en `AJamTestCharacter`),
     en los minions `Body` y `Head`. No cambia la cápsula, la colisión ni la velocidad.
3. **Color de equipo = material**, no `SetColor`. En Tick, si `Health->Team` cambió desde la última vez,
   `GetMesh()->SetMaterial(0, Materials[equipo])`; si no hay material para ese equipo, se deja el de la malla. Hay que
   compararlo en Tick porque el jugador recibe su equipo en `AJamGameMode::SetPlayerDefaults`, después de BeginPlay.
   `SetColor()` puede seguir pintando el `Body`/`Head` ocultos: no estorba.
4. **Animación por estado** en Tick, con `GetMesh()->PlayAnimation(Anim, bLoop)` **solo cuando cambia el estado**:
   - Muerto (`Health->IsDead()`) → `Death` sin loop.
   - Montado (`GetAttachParentActor() != nullptr`) → `Sit`.
   - Acción en curso (`Attack`/`Shoot`/`Hit`, dura `GetPlayLength() / Rate`) → dejarla terminar. Una acción nueva la
     reinicia.
   - Velocidad horizontal > 20 cm/s → `Run` si pasa de `RunAbove`, si no `Walk`, con
     `SetPlayRate(clamp(vel / (Speed * escala del actor), 0.5, 2))`. La escala del actor es la del Heavy (1.35), el bot
     (1.1) y el guardia (1.15).
   - Si no → `Idle`.
   - Si falta una animación en el JSON, usar `Idle`.
5. **Disparadores** (una línea cada uno):
   - Jugador: enlazar `Weapon->OnWeaponAttack` (espada → `Attack`, arco → `Shoot`).
   - Minion, bot y guardia: junto al `ApplyDamage` del ataque (`JamMinion.cpp` ~245), `Shoot` si `AttackRange > 400`; si
     no, `Attack`.
   - Daño recibido: `Health->OnHealthChanged` con `Delta < 0` y vivo → `Hit`.
6. **Primera persona (V):** en el Tick del jugador, `GetMesh()->SetOwnerNoSee(Body->bOwnerNoSee)`. El toggle de Jesús
   solo toca `Body`/`Nose`; así no se edita su archivo.
7. *(Opcional, si sobra tiempo)*: `AJamMinion::HandleDied` hoy esconde al minion al instante (`SetActorHiddenInGame` +
   `SetLifeSpan(0.2)`). Para que se vea `Death`, esconderlo ~1.5 s después.

**Hecho cuando:**
- (a) Sin `Characters.json` (renómbralo para probar) el PIE se ve igual que hoy, con cilindros.
- (b) Con él, los minions rojos y azules caminan, corren y atacan con su figura; las demás clases siguen con cilindro
  hasta que Javier agregue su entrada.
- Sin cambios de colisión, daño ni velocidades.
- Compila limpio. Si tocas headers, pon "recompilar" en el commit.

Si algo se ve mal (de lado, hundido, patinando), es dato: avisa a Javier y él corrige el JSON. No hace falta tocar el
código.

> [!warning] Empaquetado (para quien haga la build final)
> Estos assets se cargan por ruta, igual que las piezas de `RealMeshNames()`, y el cook solo sigue referencias duras.
> Hay que agregar `+DirectoriesToAlwaysCook=(Path="/Game/Game/Art/Tripo/Characters")` (y `/Game/Game/Building/Parts`)
> en `Config/DefaultGame.ini`. Además, los `.json` que se leen con `LoadFileToString` (`Loot.json`, `Fusion.json`,
> `BuildKnowledge.json`, `Characters.json`) tienen que empaquetarse con `+DirectoriesToAlwaysStageAsUFS`. Revisarlo en
> la primera build de prueba: si falta, el juego empaquetado sale con cilindros y sin loot.
