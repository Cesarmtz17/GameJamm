# Tareas para el Claude de la PC de escritorio de Cesar

> **Solo para la sesión de Claude Code en la PC de escritorio de Cesar** (la que tiene Unreal y compila). Los agentes de
> los demás integrantes no toman estas tareas. Las escribe la sesión de la Mac de Cesar a partir de [GDD.md](GDD.md);
> si una tarea choca con lo que ya hizo alguien, avisa a Cesar en vez de decidir solo.

## Cómo trabajar esta lista

1. `git pull --rebase --autostash`. Lee `CLAUDE.md`, `docs/TEAM.md` y `docs/GDD.md` (Jugabilidad).
2. Toma la **primera tarea sin marcar**, en orden. Al empezar, cámbiala a `[~] (PC, <hora>)` y haz push de este archivo, para que
   no se repita.
3. No toques `Source/GameJamm/Building/` ni `Content/Game/Building/` (son de Jesús Daniel): usa sus APIs públicas. Si hace falta
   un cambio ahí, escríbelo en *Pendientes para otras áreas* del GDD.
4. Haz commits chicos `<area>: <qué>`, con `git add` solo de tus rutas. Antes de cada push: compilar, probar en PIE,
   `git pull --rebase --autostash` y luego `git push`.
5. Nunca subas `Saved/`, `Intermediate/`, `Binaries/`, claves ni la sección `AndroidFileServer` de `Config/DefaultEngine.ini`.
   `git lfs lock` antes de editar un `.uasset` que ya exista.
6. Al terminar, marca `[x]`, pon el hash del commit y una línea de cómo probarlo. Si algo no se pudo hacer, pon `[!]` y el motivo.
7. Freeze de features: **dom 15 nov, 12:00 CDMX**. Después de esa hora, solo arreglos.

## Tareas (en orden de prioridad)

> **Cesar (14 nov):** sigue con las tareas en orden **sin parar entre una y otra**; solo detente si algo no compila o choca
> con lo de otro. Antes de cada tarea haz pull: Jesús sube cambios grandes de Building seguido (recompilar si trae headers).
> El tope de piezas queda en **60** por ahora (pruebas); no lo cambies.

### [x] 0. Probar el brillo al prender
`git checkout build/brillo-encendido`, compilar y en PIE correr `Jam.Say un carro` y luego **C**: la malla debe brillar
(parámetro `Emissive` de `M_Part_Master`; si el material no lo tiene, agrégalo: Scalar 0–1 × `#E7B65A` a Emissive Color).
Si funciona, haz merge a `main` y push. Si no compila, pon aquí el primer error.

> **Hecho (PC, 14 nov 01:10):** en main como `3bb11e6`. Probado en `-game`: `Jam.Say un carro` → apagado sin brillo, **C** → las llantas brillan cálido (#E7B65A); **C** otra vez lo apaga.
> **Ojo, no compila en Windows/MSVC ni `main` ni la rama:** `JamPlayConsole.cpp(1005): error C4458: la declaración de 'Slot' oculta el miembro de clase`
> (`UCanvasPanelSlot* Slot = Root->AddChildToCanvas(Box)` choca con `UWidget::Slot`). Arreglo: renombrar a `CanvasSlot` en las líneas 1005–1010.
> Lo probé con ese cambio solo local, sin subirlo, porque el archivo es de Jesús Daniel. **Pendiente para Building.**

### [x] 1. Partida jugable (Core + mapa)
Core no tiene dueño: anota "Cesar" en la fila Core de `docs/TEAM.md` y haz push.
1. Importa `SourceArt/World/FBX/SM_MAP_Dressed.fbx` (o el blockout) y arma `Content/Game/Maps/L_Main` con World Partition + OFPA.
   Las posiciones están en `SourceArt/World/map_layout.json` (bases en ±12400 X, torres en ±8200 y ±4400).
2. Coloca `ATeamStructure` (`Source/GameJamm/Core/TeamStructure`) con `SM_TOWER_WATCH` y `SM_BASE_KEEP`: 2 torres y 1 base por
   equipo, con los valores de GDD §9 (torre 1500 HP, 15 m, 20 de daño cada 1.5 s; base 3000 HP, que solo recibe daño al caer
   sus 2 torres).
3. Personaje `AJamCharacter` en Core, a partir de la tercera persona de `AJamTestCharacter` (Building/JamPlayConsole), con
   `UHealthComponent` (150 HP, Team), `UInventoryComponent` y respawn en la base (5 s + 1 s por minuto, tope 15 s). Todas las
   teclas de construcción de Jesús tienen que seguir funcionando con este personaje.
4. `AJamGameMode`: Rojo contra Azul; gana quien destruya la base. Muerte súbita a las 12:00 y a las 15:00 gana la base con más
   HP (§10). Pantalla simple de victoria o derrota.
5. Prueba: Play en L_Main, caminar a la torre enemiga, recibir daño, morir y reaparecer.

> **Hecho (PC, 14 nov 01:45):** `19aa557`. Mapa por defecto = `/Game/Game/Maps/L_Main` (World Partition + OFPA, todo
> siempre cargado). Torres y bases son `ATeamStructure` reales; las figuras del FBX se quitaron. `AJamCharacter` hereda de
> `AJamTestCharacter`, así que todas las teclas de construcción siguen igual.
> **Cómo probarlo:** Play en L_Main. Consola: `Jam.Goto 3300 0` te pone a 11 m de la torre azul 1: dispara un rayo, mueres en
> ~12 s y reapareces en la base roja a los 5 s. `Jam.Time 715` → muerte súbita a 12:00. `Jam.Time 890` → a las 15:00
> gana quien tenga más vida en torres y base (EMPATE si nadie hizo daño). `Jam.Hurt 200` = morir al instante. `Jam.Pos` = dónde estás.
> Probado en `-game` con los tres casos. **Sin probar:** caminar todo el carril con teclado (desde aquí no pude simular WASD),
> ni derribar una torre (falta combate, tarea 2). `bUseUnity=false` en `GameJamm.Build.cs`: en un build limpio chocaban `K` de
> `JamAssembler.cpp` y `JamBuildPart.cpp`.

### [x] (PC, 14 nov 18:20: armas y fusión hechas) 2. Combate y fusión (GDD §8)
**Cesar (14 nov):** si en `main` todavía no existe `Source/GameJamm/Combat/`, hazla tú sin preguntar y anota "Cesar" en la fila
Combate de `TEAM.md`. Si ya existe código de Jesús ahí, no lo dupliques: avisa en el Registro y pasa a la tarea 3.
1. Espada (20 de daño cada 0.6 s, 2 m) y arco (15 de daño cada 0.8 s, 40 m, flecha física) con `SM_WPN_SWORD`, `SM_WPN_BOW` y
   `SM_WPN_ARROW`. Clic izquierdo ataca. No uses las teclas de Building (1-9, C, E, X, V, H, G, T); ver la tabla §7.
2. `UFusionComponent`: pega 1 material al arma por 20 golpes. Valores en DataAsset o JSON:
   - Roca: +12 de daño, empuje ×2, ataque 30 % más lento.
   - Fuego: 5 de daño por segundo durante 3 s.
   - Hielo: ralentiza 50 % durante 2 s.
   - Rayo: aturde 0.5 s.
   - Bomba: 35 de daño en 2.5 m y se gasta.

   Usa los meshes `SM_FUSE_*` pegados a la punta del arma.
3. Los materiales salen de `UInventoryComponent` (`Mat.Rock`, `Mat.Fire`, `Mat.Ice`, `Mat.Shock`, `Mat.Bomb`).
4. Todo el daño va por `ApplyDamage` a `UHealthComponent`, y nunca a aliados (`UJamStatics::AreEnemies`).
5. Prueba: pegarle a un dummy con cada material.

### [~] (PC, 01:55 — PAUSADA 02:20, casi lista) 3. Minions y bots (GDD §3 y §11)
IA no tiene dueño: anota "Cesar" en la fila IA de `TEAM.md`. Por tiempo, nada de StateTree: máquina de estados en C++ en
`Source/GameJamm/AI/`.
1. `AJamMinion`, con el maniquí en el color de su equipo pero más apagado (ART.md §6):
   - Salen en oleadas cada 30 s desde cada base: 3 melee y 1 ranged, y un heavy a partir del minuto 8. Vida: 60 / 40 / 200.
   - Recorren el carril (puntos de `map_layout.json`) y atacan al enemigo más cercano o a la torre.
   - Las torres apuntan primero a los minions. Sin minions aliados cerca, se le hace 50 % de daño a las torres.
   - Matar un minion da +3 de energía (`AJamBuildPart::SetPlayerEnergy`).
2. Bot héroe: 2 por equipo para completar el 3v3, con los roles Constructor, Peleador y Tirador.
   - Estados: ir al carril, pelear, retirarse con poca vida y lootear.
   - El Constructor arma con el asistente de Jesús usando frases fijas ("un carro con lanzallamas"), vía
     `AJamBuildSpawner::SetPromptAndBuild`.
3. Prueba: una partida de 5 minutos sin tocar nada; los minions chocan en el centro y los bots pelean.

### [x] 4. Loot y energía (GDD §4, §5 y §12, en Core) (PC, 14 nov 18:35: hecho, guardias incluidos)
1. Coloca en las 4 islas de `map_layout.json` el `ALootChest` (`SM_PROP_CHEST`), el cofre raro (con tapa animable) y las pilas
   de chatarra, más el cofre raro central a partir del minuto 5.
   - Se abren con E y llenan `UInventoryComponent`.
   - Topes de la mochila (§4): 40 en total; 8 por tipo común, 4 por dispositivo y 2 por raro.
2. Pickups: `SM_PICKUP_PART`, `SM_PICKUP_MATERIAL` y `SM_PICKUP_ENERGY_ORB`.
   - Cristal chico +15 y grande +40 (el mismo orbe, escalado y en cian).
   - Al morir sueltas una bolsa con el 50 % de tus piezas y un orbe con el 50 % de la energía que tenías arriba del mínimo.
3. Cofre garantizado junto al spawn (§12): 1 Plank, 4 Wheel, 1 Flamethrower, 1 Fan y 1 Fuego. Va con el letrero
   "Prueba: un carro que queme todo".
4. Guardias de campamento (§11): Guardia de 80 HP y Lanzador de 50 HP, usando `AJamMinion` con equipo Neutral.

### [ ] 5. Menú, pantalla final y audio (GDD §13 y §17)
Avísale a Emilio (dueño de Audio/UI) antes de empezar.
1. Importa `SourceArt/Audio` (23 SFX y 3 loops) a `Content/Game/Audio/` y conéctalos a los eventos de §17: armar, encender, sin
   energía, golpes, torre destruida, loot, mochila llena, muerte y respawn. Música: `MUS_Match_Loop`, y
   `MUS_Match_Intense_Loop` en la muerte súbita.
2. Importa `SourceArt/Icons` (`T_Icon_*` y `T_UI_*`) a `Content/Game/UI/`.
3. Menú de inicio (Jugar Asalto, Controles, Salir) y pantalla final "tu corte final", con la construcción de la partida y el
   ganador.

## Contexto de la sesión Mac (14 nov, ~02:00) — para seguir desde la PC

Todo está en `main`. Lo que hizo la sesión de la Mac y lo que quedó abierto:

| Tema | Estado | Dónde |
|---|---|---|
| GDD completo (Jugabilidad §1–§18: modos, minions, loot, energía, construir, controles, combate y fusión, balance, partida minuto a minuto, bots, primeros 2 min, HUD, PvP de construcciones, interacciones, feedback y audio, glosario) | Hecho | `docs/GDD.md` |
| Pendientes de otras áreas (Construcción, Arte) con estado `[ ]/[~]/[x]` | Al día | GDD → *Pendientes para otras áreas* |
| Brillo al prender (`Emissive` de `M_Part_Master`) | En `main` (`3bb11e6`) | — |
| Fix MSVC C4458 `Slot` → `CanvasSlot` | En `main` (`2c2b196`) | `JamPlayConsole.cpp` |
| Tope de piezas | **60 temporal** (código de Jesús). Revisar antes del freeze: la propuesta es "lo que cabe en la mochila, máx. 40" en partida | GDD §4 |
| Valores de piezas distintos a §9 (Bomba, Beam, Head, Frost) | Abierto: para el MVP manda el código | GDD → Pendientes |
| Solver con inventario en `main` | Abierto (está en `build/poc-final`, sin compilar) | Jesús Daniel |
| Arte que falta | Niagara (fuego, hielo, rayo, agua, humo), cristales de energía (orbe escalado en cian), ícono de `Ball`, ícono del asistente/micrófono, SFX de arco, cohete y rayo | Emilio |
| Título del juego | Para después (decisión de Cesar) | — |

## Registro
| Tarea | Estado | Commit | Cómo probar |
|---|---|---|---|
| 2 Fusión (14 nov 18:20) | **Hecha (MVP 5 materiales).** F (o cruceta arriba) pega a la espada un `Mat.*` del inventario: dura 20 golpes (Bomba 1), reemplaza al anterior; en el arco elige la punta y cada flecha gasta 1. Fuego quema 5/s por 3 s; Hielo ralentiza 50 % y 3 golpes seguidos congelan (flecha congela 1.5 s); Rayo aturde 0.5 s (flecha: área de 2 m); Roca +12, ×2 empuje, 30 % más lenta; Bomba explota 35 en 2.5 m. El `SM_FUSE_*` se ve en la punta. Valores en `Content/Game/Combat/Data/Fusion.json`; estados en `UJamStatusComponent` (reusable por construcciones). | (este) | `Jam.Give Mat.Fire 1` + `Jam.Dummy blue 150` + `Jam.Fuse Fire` + `Jam.Attack 3`; `Jam.Weapon bow` + `Jam.Fuse Ice` |
| 4.4 Guardias (14 nov 18:35) | **Hecho.** `AJamCampGuard` (AI, hereda de `AJamMinion`, equipo Neutral): Guardia 80 HP (7 cada 1.2 s) y Lanzador 50 HP (5 cada 2 s desde 15 m, suelta un `Mat.*`); persiguen solo dentro de 20 m de su campamento, no se tiran de la orilla, vuelven y se curan; +10 de energía al jugador que los mata. 10 en las 4 islas (`camps` en Loot.json), reaparecen a los 90 s. Los minions del carril los ignoran. Parado en el campamento mueres en ~12 s. | (este) | `Jam.Goto -8600 -3000 400` |
| 4 Loot | **Hecho 1–3, falta 4 (guardias).** `AJamLootDirector` (lo crea el GameMode) lee `Content/Game/Core/Data/Loot.json`: cofre inicial junto al spawn con el kit del GDD y el letrero, 2 cofres comunes y 2 de dispositivos (reponen a los 60/90 s), 4 montones de chatarra en el carril (45 s) y cofre raro central al min 5 y cada 3 min. Se abren por **cercanía** (la E ya es montar). Topes de mochila del §4; lo que no cabe queda en el suelo. Al morir: bolsa con 50 % de piezas + orbe con 50 % de la energía arriba de 50 (dura 30 s). El kit inicial fijo del personaje se quitó (ahora sale del cofre). | (este) | `Jam.Rare`, `Jam.Inv`, `Jam.Give Wheel 6`, `Jam.Energy 120` + `Jam.Hurt 200` |
| 3 Minions y bots | **Pausada (14 nov 02:20), casi lista.** Hecho: minions 3 melee + 1 ranged (+pesado min 8) cada 30 s, bots 2 rojo / 3 azul con retirarse/defender/empujar, el Constructor arma ("un carro con lanzallamas", 7 piezas), torres priorizan minions y 50 % sin escolta, islas con colisión de doble cara (ya no se atoran: stuck=0). **Pendiente:** (a) la 2.ª oleada reporta "0 vivos antes": revisar si la 1.ª muere/cae antes de 30 s o si el conteo falla (`Jam.Agents` a los 20 s); (b) confirmar que chocan en el centro y que los bots pelean; (c) partida de 5 min sin tocar nada. Luego marcar [x]. | b6657d6, 6c2e8f3 | `Jam.Wave`, `Jam.Agents`, `Jam.Profile`, `Jam.Goto 0 -1500 400` |
| 2 Combate y fusión | **Sin empezar (pausada).** Revisado: no existe `Source/GameJamm/Combat/`; `SM_FUSE_*` solo están como FBX en `SourceArt/Fusion/FBX/` (hay que importarlos); `SM_WPN_SWORD/BOW/ARROW` **no están en el repo** (solo iconos de BOW/ARROW): preguntar a Emilio/Javier o usar un mesh provisional. Ojo: el clic izq. ya lo usa `JamPlayConsole.cpp:1247` (disparo de la construcción manejada); el ataque debe funcionar solo cuando no vas sentado. | — | — |
| 3 Minions y bots (14 nov tarde) | **Casi lista.** Hecho y subido: suelo de islas volteado (se veía por debajo), huecos de los puentes de base cerrados (ahí caían todos los minions), minions rodean sus torres. Medido: oleadas chocan en el centro, torres matan minions, bots pelean. **Pendiente:** partida de 5 min sin tocar y marcar [x]; aún cae alguno en el puente torre-prado rojo (X≈-7050). | 6106c47, 5e1a664 | `Jam.Profile X0 X1 paso Y`, log `Vivos`/`Muere` en cada oleada |
| Vehículos (pedido de Cesar, con OK de Jesús) | **PAUSADA, sigue con Codex.** Hecho: C ya no dispara cohetes con asiento (Espacio sí); empuje del cohete por el centro de masa; asistencia de estabilidad en suelo + auto-enderezar (`JamPlayConsole.cpp`, `DriveMount`). Carro con lanzallamas 3/3 PASA. **Pendiente:** (a) *carro cohete* aún se vuelca: medido con log, al apretar W la trompa sube poco a poco (fwdZ 0.3→0.5) y se ladea; si después se enciende el cohete estando ladeado, sale disparado de lado. Causa raíz: par de reacción del motor de rueda (`SetAngularDriveParams(0, 5000*Scale, 2000000*Scale)` en `JamAssembler.cpp`) contra un chasis ligero con el cohete de 4 kg atrás. Ideas: limitar el par del motor según masa/largo del chasis, o bajar el centro de masa (`SetCenterOfMass` en la raíz); (b) la batería `Jam.Test` es muy ruidosa (la misma frase pasa y falla); la fila 1 de cada corrida arma cerca del origen (puente del río); (c) **construir no usa el inventario**: `ValidateRecipe(..., nullptr, ...)` en `JamAssembler.cpp:604/1248/2267`, por eso \"un carro con solo 3 llantas\" no se puede probar; hay que pasar el `UInventoryComponent` del jugador y que el código sustituya/reduzca (ley de Construcción §2); (d) carro de 2 ruedas se balancea sin estabilizador. Ojo: `Jam.Test` sobrescribe `docs/building/REPORTE-BATERIA.md` (de Jesús): restaurar con `git checkout` si no es para él. | (este commit) | `Jam.Test 12 <json>` con frases propias; `Jam.Say`, `Jam.Seat`, `Jam.Drive 1 0 3`, `Jam.Fire boost` |
| Assets | Megascans es gratis en Fab desde el 12 nov (23 mil assets); falta revisar la página de la jam para ver qué herramientas da Epic. En `SourceArt/` hay sin importar: `SM_WPN_SWORD/BOW/ARROW` (destraban la tarea 2), pickups, cascos y FX. | — | — |
| 2 Combate (14 nov 17:50) | **Armas hechas, fusión pendiente.** `Source/GameJamm/Combat/`: `UJamWeaponComponent` (espada 20 cada 0.6 s, 2 m, empuje; arco 15 por flecha cada 0.8 s, `AJamArrow` física a la mira hasta 40 m), `AJamTrainingDummy`. A pie: clic izq./RT ataca, rueda/Q/Y cambia de arma; montado o escribiendo no. FBX `SM_WPN_*` y `SM_FUSE_*` importados a `Content/Game/Combat/`. Torres: 10 de daño a héroes (+10 %/disparo seguido, tope ×1.5), minions igual. | ver git log | `Jam.Dummy blue 150` + `Jam.Attack 5` → 4-5 golpes de 20; `Jam.Weapon bow` + `Jam.Dummy blue 1500` → flechas de 15; `Jam.Dummy red` → 0 daño |
