---
title: 14 — Handoff del solver por capacidades (contexto completo para la sesión con Unreal)
date: 2026-11-13
tags:
  - research
  - construccion
  - ia
  - handoff
  - pruebas
status: borrador
---

# 14 — Handoff del solver por capacidades (contexto completo para la sesión con Unreal)

> [!info]
> Todo lo que hace falta para **compilar, probar y ver** el solver de construcción sin hablar con quien lo escribió.
> Se escribió en una máquina **sin Unreal**: rama `build/poc-solver` (base `build/poc-integracion`). Diseño de origen:
> [13-conocimiento-ia-construccion.md](13-conocimiento-ia-construccion.md) §3.3 y
> [12-modelos-de-decision-jev.md](12-modelos-de-decision-jev.md).
> Etiquetas: **[ejecutado]** = corrido de verdad en la máquina de origen · **[no verificado en editor]** = falta compilarlo o
> verlo en Unreal · **[supuesto]** = API de UE 5.8 asumida, la decide el compilador.

> [!tip] Veredicto central
> El modelo de decisión solo **elige** (arquetipo, extras, piezas deseadas). `UJamSolver::Solve` decide lo posible: llena los slots
> de la plantilla con el inventario, sustituye, reduce en pares, cambia de forma, recorta a 21 piezas, comprueba que la
> construcción se mueva o ataque y lista los materiales que faltan. La lógica está **probada de forma ejecutable** (Python y el
> C++ real contra un mock), pero **nada se ha compilado ni visto en el editor**. Empieza por el runbook de la §6.

> [!warning]
> **No verificado en editor:** compilación con UnrealBuildTool/UHT en macOS, las pruebas de Automation dentro del editor, los
> comandos `Jam.*` en Play, la física de lo construido y la llamada real al modelo de decisión. Trátalo como pendiente hasta
> correr la §6 y anotar el resultado.

## 1. Qué se leyó y qué hallazgos importan

| Fuente | Hallazgo que condiciona el diseño |
|---|---|
| `CLAUDE.md`, `docs/GDD.md` | Las mecánicas salen de las propiedades de las piezas; el asistente nunca bloquea el juego (siempre hay respaldo); la receta JSON y `UInventoryComponent` son contratos compartidos |
| Doc 13 §1, §3.3 | Capacidades (`provides`), slots por rol, sustitutos en orden, reducir en pares, degradar forma, pedir materiales, Seat gratis, energía tipo maná |
| `BuildKnowledge.json` v2 | Tenía incoherencias que impedían usarlo tal cual (tabla de la §2.3) |
| `JamAssembler.cpp` | `ExtraTable` ya define sockets y arquetipos permitidos por extra: el solver la reutiliza en vez de duplicarla. `ComposeRecipe` es el punto único decisiones → receta |
| `JamLayout.cpp` | Dos piezas en la misma cara se empujan hacia afuera (no falla): útil, pero un asiento y un cañón ambos en `Top` quedan apilados, así que las plantillas usan `Top_Front` / `Top_Back` |
| `JamBuildPart.cpp` | El catálogo (23 tipos) manda sobre qué se puede spawnear. La rueda tiene su eje en Y: sus caras `Left`/`Right` son las tapas y `Front`/`Back`/`Top`/`Bottom` el canto |
| `BuildRecipe.cpp` | `ValidateRecipe` exige una sola raíz, ids únicos, tipos del catálogo y ≤ 21 piezas. `GetRequiredItems` cuenta el Seat como `Part.Seat` (ver §5, riesgo 9) |
| `GameJamm.Build.cs` | `Json` y `JsonUtilities` ya estaban: **no se tocó** |

## 2. Decisiones de diseño y por qué

### 2.1 Algoritmo (`UJamSolver::Solve`)

1. **Plantilla**: se busca por `id` y, si no, por `code_archetype` (`Fortress` → plantilla `Wall`). Orden de intento: la pedida y luego
   sus `fallbacks` (un solo nivel). Gana la primera que se pueda armar.
2. **Slots** (raíz primero): candidatos = piezas deseadas (si el slot es opcional o `prefer_wanted`) → `part_pref` → piezas que dan
   la capacidad en el orden del JSON → sus `substitutes` (un nivel). Solo entran piezas que existen en `UJamPartCatalog`.
3. **Llenado**: se toma del inventario de uno en uno, o de dos en dos del mismo tipo si `pairs`. Si no llega a `min`, la plantilla
   falla y se apunta lo que falta. Si llega a `min` pero no a `count`, es una **reducción**.
4. **Slots opcionales**: solo se llenan si son `control` (Seat, gratis), si el jugador pidió una pieza con esa capacidad, o si es
   `anchor` en una plantilla anclada y hay una pieza que ancle de verdad.
5. **`requires`**: `move` exige alguna pieza con `ground_drive`, `spin`, `thrust`, `burst_thrust` o `lift`; si no la hay (sustitutos
   pasivos), agrega un empuje en `Back`, y si tampoco hay, la plantilla falla. `attack` exige un arma. Así **nunca sale una receta
   que no pueda moverse o atacar cuando el arquetipo lo exige**.
6. **Extras** (`ExtraTable`, en su orden = prioridad): una pieza por socket; si falta el tipo, prueba sus sustitutos; si no hay,
   lo apunta como faltante. Lo que pasa de 21 piezas va a `Trimmed`.
7. **Piezas deseadas sueltas** (`FJamBuildChoice::WantTypes`): en la primera cara libre de su lista `faces`.
8. **Energía**: suma `energy_cost_per_s` de lo que se enciende; disponible = barra + `energy_capacity` de las baterías pegadas;
   avisa si dura menos de `energy.warn_duration_s`.

### 2.2 Decisiones

| Decisión | Por qué |
|---|---|
| El solver vive en archivos nuevos y `ComposeRecipe` lo llama primero | Un solo punto de integración; si el JSON no carga, sigue el código de siempre sin tocarlo |
| `ComposeRecipe` usa **inventario ilimitado** | Su firma no recibe inventario y el spawner actual no tiene uno. Con inventario real se llama a `UJamSolver::Solve` directo |
| "Usable" = existe en `UJamPartCatalog` (no el campo `in_code`) | El catálogo lo edita otra rama: en cuanto agregue `Head`, `Sled`, etc., el solver los usa sin cambiar código |
| Lo pedido que no existe va a `Unsupported`, no a `Missing` | `Missing` es "ve a lootear esto"; no se puede lootear lo que aún no está en el juego |
| Sustituto pasivo (no da la capacidad) también se apunta en `Missing` | El carro con trineos funciona, pero lo ideal siguen siendo las ruedas |
| Orden estable en todo (arreglos, no mapas, en la salida) | Determinismo: misma entrada, misma receta; las pruebas comparan texto exacto |
| Campo nuevo `FJamBuildChoice::WantTypes` (opcional, vacío por defecto) | Es la única forma de que "que apunte sola" llegue del texto al solver sin inventar un extra que el camino por tabla no sabría armar |
| `RefineChoiceFromText` no hace nada si el JSON no cargó | El camino por tabla no conoce `Spinner`/`Roller`/`Airship` y fallaría |
| Inventario de depuración del PoC manda sobre el del jugador si tiene algo | Un personaje que aparece sin loot haría fallar toda prueba; `Jam.InvClear` devuelve el control al inventario real |

### 2.3 Cambios a `BuildKnowledge.json` (v2 → v3)

| Cambio | Motivo |
|---|---|
| Campos nuevos por plantilla: `requires`, `fallbacks`; por slot: `prefer_wanted` | Datos que el solver necesita y no existían |
| Campo nuevo por elemento: `lift_face` (Fan, Turbine, Jet, Rocket, HoverStone = `Bottom`; Balloon = `Top`) | Un globo que sustituye a un ventilador va arriba, no abajo |
| `energy.warn_duration_s` = 15 | Umbral del aviso de poca energía (doc 13 §6) |
| Wheel / BigWheel: `faces` + `Front`, `Back` | La plantilla `Bike` las pone ahí y el elemento no lo permitía |
| Turntable: `provides` + `spin` | `Spinner` la listaba en `part_pref` sin que diera la capacidad |
| Tank: cañón en `Top_Front`, asiento en `Top_Back` | Ambos en `Top` quedaban apilados |
| Flyer: sustento `count` 2 en `Bottom_Front` / `Bottom_Back` | `Bottom_Left` / `Bottom_Right` no caben en una tabla de 60 cm de ancho y se encimaban |
| Airship: slot opcional `heat` bajo el globo (`on: heat`), asiento en `Top_Back` | "Globo con fuego debajo": lanzallamas apuntando al globo (doc 13 §7) |
| Wall: raíz + 2 muros laterales + bloque, todos `min` 0 | Igual que el arquetipo `Fortress` del código |
| Spinner: armas en `Front` / `Back` | Es el canto de la rueda; `Left`/`Right` son las tapas del eje |
| Turret: `part_pref` Cannon + `prefer_wanted` | Sin eso la torreta por defecto salía con lanzallamas |
| Masas de Panel, Spring, HoverStone = las del catálogo | El código manda |

## 3. Archivos y contratos

| Archivo | Estado | Contenido |
|---|---|---|
| `Source/GameJamm/Building/JamSolver.h/.cpp` | nuevo | `UJamSolver` y las estructuras de entrada y salida |
| `Source/GameJamm/Building/JamPoc.h/.cpp` | nuevo | Comandos `Jam.*` y `UJamPoc` (mismas acciones para Blueprint / Python) |
| `Source/GameJamm/Building/Tests/JamSolverTests.cpp` | nuevo | 4 pruebas de Automation `GameJamm.Building.Solver.*` |
| `Source/GameJamm/Building/Tests/JamSolverCases.inl` | nuevo, **generado** | 64 casos espejo. No editar: `python3 -I Content/Game/Building/Data/solver_proto.py --emit-cpp` |
| `Source/GameJamm/Building/JamAssembler.h` | **cambio de header** | `FJamBuildChoice::WantTypes` (`TArray<FName>`, opcional). Obliga a recompilar |
| `Source/GameJamm/Building/JamAssembler.cpp` | cambio | `ComposeRecipe` llama primero al solver; `ChoiceFromKeywords` termina con `RefineChoiceFromText` |
| `Content/Game/Building/Data/BuildKnowledge.json` | cambio | v3 (§2.3) |
| `Content/Game/Building/Data/solver_proto.py` | nuevo | Prototipo + 70 casos + espejo de `JamLayout` |
| `Content/Game/Building/Data/validate_knowledge.py` | nuevo | Validador del JSON contra sí mismo y contra las tablas del C++ |

No se tocó `JamBuildPart.*`, `Core/`, `Config/`, `GameJamm.Build.cs` ni ningún asset.

### 3.1 `UJamSolver`

| Función | Contrato |
|---|---|
| `LoadKnowledge(bForceReload)` | Lee `<Proyecto>/Content/Game/Building/Data/BuildKnowledge.json` una vez. `false` si falta o es inválido |
| `LoadKnowledgeFromString(Json, OutError)` | Igual desde texto (pruebas). Si es inválido el solver queda sin conocimiento |
| `UnloadKnowledge()` | Pruebas: simula que falta el JSON hasta `LoadKnowledge(true)` |
| `HasTemplate(Archetype)` | Hay plantilla con ese `id` o `code_archetype` |
| `IsInfiniteType(Type)` | `true` para Seat: no se descuenta del inventario |
| `Solve(Request, OutResult)` | Devuelve `OutResult.bBuildable`. Si el JSON no cargó: `false` y `bKnowledgeLoaded = false` |
| `InventoryFromComponent(Inventory)` | `{"Part.Wheel": 3, "Mat.Fire": 1}` → `{"Wheel": 3}`. Solo lee `Items` |
| `RefineChoiceFromText(Prompt, Choice)` | Rueda acostada → `Spinner`, rueda infernal → `Roller`, globo → `Airship`, "que apunte sola" → `WantTypes` + `Head` |

`FJamSolverRequest`: `Choice`, `Orientation` (vacío = la de la plantilla), `Inventory` (tipo → cantidad), `bUnlimitedInventory`,
`Energy` (< 0 = `energy.start`).

`FJamSolverResult`: `bKnowledgeLoaded`, `bBuildable`, `Recipe`, `Template`, `RequestedTemplate`, `bDegraded`, `Substitutions`
(`Role`, `Wanted`, `Used`, `Count`), `Reductions` (`Role`, `Wanted`, `Got`), `Missing` (`Type`, `Count`), `Unsupported`, `Trimmed`,
`Added`, `Orientation`, `bAnchored`, `EnergyCostPerSecond`, `EnergyAvailable`, `DurationSeconds` (−1 = no gasta), `bLowEnergy`,
`MassKg`, `Message` (texto para el jugador), `Note` (línea de log).

Uso con el inventario real del jugador:

```cpp
FJamSolverRequest Req;
Req.Choice = Choice;                                              // del modelo de decisión o de ChoiceFromKeywords
Req.Inventory = UJamSolver::InventoryFromComponent(InventoryComponent);
FJamSolverResult Res;
if (UJamSolver::Solve(Req, Res)) { /* ensamblar Res.Recipe, mostrar Res.Message */ }
else if (!Res.bKnowledgeLoaded) { /* UJamBuildGenerator::ComposeRecipe: camino por tabla */ }
else { /* no se arma: mostrar Res.Message y Res.Missing */ }
```

### 3.2 Comandos del PoC (`JamPoc.cpp`)

| Comando | Qué hace |
|---|---|
| `Jam.Build <pedido libre>` | Decide (modelo si hay API key; si no, palabras clave), resuelve, arma ~500 cm delante de la cámara, enciende a los 1.5 s y muestra el resumen |
| `Jam.Inv <Tipo> <n>` | Pone `n` piezas de ese tipo en el inventario de depuración (acepta alias: `Rueda`, `DEV_FAN`) |
| `Jam.InvAll [n]` | `n` (10) de cada pieza del catálogo |
| `Jam.InvClear` | Vacía el inventario de depuración: vuelve a mandar el del jugador o, si no hay, ilimitado |
| `Jam.InvShow` | Lista el inventario de depuración |
| `Jam.Clear` | Destruye lo construido y devuelve las piezas |
| `Jam.Prompts` | Imprime 12 pedidos de ejemplo |
| `Jam.Reload` | Vuelve a leer el JSON sin recompilar |

`Jam.Dummy`, `Jam.Energy` y `Jam.Seat` pertenecen a otra rama: aquí no se definen.

## 4. Pruebas y resultado real

| Prueba | Cómo se corrió | Resultado |
|---|---|---|
| `python3 -I Content/Game/Building/Data/solver_proto.py` | **[ejecutado]** | `73 casos, 0 fallas`: 70 casos, barrido de 398 combinaciones sin piezas encimadas, espejo C++ al día, JSON ausente |
| `python3 -I Content/Game/Building/Data/validate_knowledge.py` | **[ejecutado]** | `41 elementos (23 en codigo), 16 plantillas, 7 arquetipos, 16 extras -> 0 errores, 0 avisos`. El JSON v2 original daba 68 errores |
| C++ real contra un mock mínimo de Unreal, con `clang++` | **[ejecutado]**, compilación normal y en un solo archivo (unity) | Las 4 pruebas de Automation: 664 comprobaciones, 0 errores. `ComposeAllCombos` por el solver: 126 combinaciones, 0 malas. Guion de 36 comandos `Jam.*`: sin errores |
| Compilar con UnrealBuildTool / UHT | **[no verificado en editor]** | pendiente |
| `Automation RunTests GameJamm.Building` en el editor | **[no verificado en editor]** | pendiente |
| Comandos `Jam.*` en Play | **[no verificado en editor]** | pendiente |

> [!note]
> El mock (no está en el repo) imita `FString`, `FName`, `TArray`, `TMap`, JSON, la consola y Automation lo justo para compilar y
> **ejecutar** `JamSolver.cpp`, `JamLayout.cpp`, `JamPoc.cpp`, `JamSolverTests.cpp`, el generador de `JamAssembler.cpp` y el
> catálogo de `JamBuildPart.cpp` sin modificarlos. Prueba la lógica y la sintaxis de C++; **no** prueba las firmas reales de la
> API del motor ni UHT (§5).

Casos que cubren los 70 de Python (64 pasan idénticos a C++; los 6 restantes simulan piezas que aún no están en el catálogo):

| Grupo | Casos |
|---|---|
| Carro | básico; con cohete; cohete → ventilador; sin sustituto → pide; 2 y 3 ruedas → par centrado; BigWheel por Wheel; mezcla por pares; ruedas grandes; chasis sustituto; sin ruedas → trineo / pelota + empuje; sin empuje → no se arma; degrada a moto flotante; inventario vacío |
| Cohete | básico; ventilador abajo; otro chasis; sin empuje → no se arma |
| Volador | básico; con empuje extra; 1 ventilador → moto flotante; globos arriba; sin sustento → carro; cañón abajo |
| Torreta | básica; de fuego; que apunta sola (con y sin `Head` en el catálogo, y sin `Head` en inventario); láser por cañón; sin armas → no se arma; con estaca |
| Tanque | básico; con muro; panel por muro; ruedas chicas + lanzallamas; sin arma → carro; sin ruedas → torreta; no duplica el cañón |
| Rueda acostada / infernal | 2 lanzallamas; 1 lanzallamas → reduce; rueda infernal; de hielo; sin armas → pide; sin rueda → torreta |
| Globo | con fuego debajo; simple; sin lanzallamas → pide; ventilador por globo; sustituto de sustento va abajo |
| Otros | hover y reducción; fortaleza completa / pobre / de panel; 30 baterías → recorta a 21; todos los extras → 21; Seat gratis e infinito; poca energía; batería como reserva; ejemplo del doc (2.3/s, 43 s); libre; pieza inexistente; planeador; arquetipo desconocido; inventario ilimitado |

Pruebas de Automation (`Building/Tests/JamSolverTests.cpp`):

| Prueba | Qué comprueba |
|---|---|
| `GameJamm.Building.Solver.KnowledgeLoads` | El JSON real carga y hay plantilla para cada arquetipo de `ArchetypeTable` |
| `GameJamm.Building.Solver.MirrorPythonCases` | Los 64 casos, campo por campo (piezas, padre, cara, activación, sustituciones, reducciones, faltantes, recortes, energía), receta válida, sin piezas encimadas, sin usar más que el inventario, determinismo |
| `GameJamm.Building.Solver.HeadlineBuilds` | Rueda acostada con dos lanzallamas, carro sin ruedas, torreta sin armas, Seat gratis, recorte a 21, energía |
| `GameJamm.Building.Solver.FallbackAndIntegration` | Sin JSON o con JSON inválido el generador sigue por la tabla; con JSON `GenerateRecipe` pasa por el solver; palabras clave → `Spinner` / `Airship` / `Head`; inventario desde `UInventoryComponent` sin modificarlo |

## 5. Riesgos de compilación en macOS y supuestos de API (UE 5.8)

No hay nada exclusivo de Windows: rutas con `FPaths` y `/`, sin WinAPI, sin dependencias nuevas de módulo.

| # | Supuesto o riesgo | Dónde | Si falla |
|---|---|---|---|
| 1 | `FMath::RoundToInt32` y `FMath::FloorToInt32` existen (se evitó `RoundToInt(double)`, que devuelve `int64`) | `JamSolver.cpp`, `JamPoc.cpp` | Cambiar por `static_cast<int32>(FMath::RoundToInt(...))` |
| 2 | `FJsonObject::TryGetStringField` / `TryGetBoolField` / `TryGetNumberField(double&)` / `TryGetArrayField` / `TryGetObjectField` aceptan `const TCHAR*` como nombre; `FJsonValue::TryGetObject(const TSharedPtr<FJsonObject>*&)` y `TryGetString(FString&)` | `JamSolver.cpp` (lectura del JSON) | Ajustar al tipo que pida el compilador. `BuildRecipe.cpp` ya usa `TryGetStringField` igual |
| 3 | `TArray<FString>::Contains(TEXT("x"))` y `FString == TEXT("x")` | `JamSolver.cpp` | Envolver en `FString(TEXT("x"))` |
| 4 | `FAutoConsoleCommandWithWorldAndArgs` + `FConsoleCommandWithWorldAndArgsDelegate::CreateStatic` con firma `(const TArray<FString>&, UWorld*)` | `JamPoc.cpp` | Revisar `HAL/IConsoleManager.h` |
| 5 | `UWorld::LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic))`; encabezado `Engine/HitResult.h` | `JamPoc.cpp` | Cambiar el include o usar `LineTraceSingleByChannel` |
| 6 | `UE_HALF_PI` y `UE_PI` | `JamPoc.cpp` | Usar `HALF_PI` / `PI` |
| 7 | `TestTrue(FString, bool)`, `TestEqual(FString, int32, int32)`, `TestEqual(const TCHAR*, FString, FString)`, `AddWarning(FString)` | `JamSolverTests.cpp` | Convertir el primer argumento con `*Texto` |
| 8 | `#include "Building/Tests/JamSolverCases.inl"` dentro de un inicializador de arreglo | `JamSolverTests.cpp` | Comprobar que el `.inl` está en el proyecto generado |
| 9 | `UBuildRecipeLibrary::GetRequiredItems` cuenta `Part.Seat`, así que `ValidateRecipe` **con inventario** rechazaría recetas con asiento gratis | `BuildRecipe.cpp` (no se cambió) | El solver valida con `nullptr`. Quien consuma inventario debe saltarse `UJamSolver::IsInfiniteType` como hace el PoC |
| 10 | Texto con acentos dentro de `TEXT("...")` en archivos UTF-8 sin BOM | todos los `.cpp` nuevos | Mismo caso que `JamAssembler.cpp`, que ya compila así |
| 11 | UHT: `USTRUCT` con un miembro `FJamBuildChoice` de otro header, `UPARAM(ref)`, `BlueprintPure` que devuelve `TMap<FName, int32>` | `JamSolver.h`, `JamPoc.h` | `BuildRecipe.h` ya devuelve un `TMap` igual |
| 12 | Si otra rama agrega piezas al catálogo (`Head`, `Sled`...), algunos casos espejo dejan de valer | `JamSolverCases.inl` | La prueba salta sola los que esperaban la pieza ausente; para el resto, actualizar `in_code` en el JSON y regenerar con `--emit-cpp` |
| 13 | El JSON no viaja en una build empaquetada | Ajustes de empaquetado | Agregar `Content/Game/Building/Data` a *Additional Non-Asset Directories to Package*. Sin eso el juego usa el respaldo por tabla (no se rompe) |
| 14 | Rueda como raíz no gira (bug conocido del doc 13 §4) | Otra rama | `Spinner` y `Roller` se arman y encienden las armas, pero no giran ni ruedan hasta que exista ese motor |

## 6. Runbook para la sesión con Unreal

### 6.1 Antes de compilar

1. Traer la rama `build/poc-solver` (o la rama donde se haya integrado). Hay un **cambio de header** (`JamAssembler.h`): recompilar todo el módulo.
2. Desde la raíz del repo:
   - `python3 -I Content/Game/Building/Data/validate_knowledge.py` → debe terminar en `0 errores`.
   - `python3 -I Content/Game/Building/Data/solver_proto.py` → debe terminar en `73 casos, 0 fallas`.
   - Si la primera marca `in_code` distinto del catálogo (otra rama agregó piezas): corregir `in_code` en el JSON, correr
     `python3 -I Content/Game/Building/Data/solver_proto.py --emit-cpp` y revisar los casos que cambien.

### 6.2 Compilar

1. Cerrar el editor. Regenerar los archivos de proyecto (clic derecho en `GameJamm.uproject` → *Generate Xcode Project*, o la opción
   equivalente del menú del editor) porque hay archivos nuevos.
2. Compilar el target `GameJammEditor` (Development Editor) desde Xcode, o abrir `GameJamm.uproject` y aceptar la recompilación.
3. Si hay errores: casi seguro están en la tabla de la §5. Corregir el mínimo, sin cambiar la lógica, y volver a correr 6.1.

### 6.3 Pruebas automáticas

En el editor: *Tools → Session Frontend → Automation*, filtrar `GameJamm.Building` y correr todo; o en la consola del editor:

```
Automation RunTests GameJamm.Building
```

Deben pasar las 5 pruebas anteriores (`GameJamm.Building.Recipe.*`) y las 4 nuevas (`GameJamm.Building.Solver.*`).

| Si falla | Qué significa |
|---|---|
| `Solver.KnowledgeLoads` | El JSON no está en `<Proyecto>/Content/Game/Building/Data/` o es inválido. El mensaje trae la ruta que buscó |
| `Solver.MirrorPythonCases`, un campo de un caso | El C++ difiere del prototipo, o cambió el catálogo o el JSON. El error trae el caso, el campo, el valor obtenido y el esperado |
| `Recipe.ComposeAllCombos` con "overlaps" | Una plantilla del JSON coloca dos piezas encimadas con los tamaños actuales del catálogo |

### 6.4 Verlo en el editor

1. Abrir cualquier mapa con suelo (uno vacío con un piso basta). **Play**.
2. Abrir la consola con `~` y escribir los comandos. El resumen aparece arriba a la izquierda en pantalla durante 20 s y en el
   *Output Log* (categoría `LogJamPoc`).
3. Guion sugerido y qué debe verse (texto **[ejecutado]** contra el mock; la parte visual es **[no verificado en editor]**):

| Comando | Resumen esperado | En el mundo |
|---|---|---|
| `Jam.Prompts` | 12 ejemplos | — |
| `Jam.Build un carro` | `plantilla Car`, `Plank x1, Wheel x4, Seat x1`, `gasta 0.8/s ... dura 75 s` | Tabla con 4 ruedas y asiento a ~5 m; a los 1.5 s avanza |
| `Jam.Build un carro con cohete` | 7 piezas, `Rocket x1` | Igual, con cohete atrás: acelera fuerte 3 s |
| `Jam.Build un cohete` | `plantilla Rocket`, `Cylinder x1, Rocket x1`, `no gasta` | Cilindro que sale hacia arriba |
| `Jam.Build un avión con ventilador` | `plantilla Flyer`, `Fan x3`, `POCA ENERGÍA` | Tabla con 2 ventiladores abajo y 1 atrás: sube |
| `Jam.Build una torreta que apunte sola` | `quiere=[Head]`, `Block x1, Cannon x1`, `Aún no existe en el catálogo: Head` | Bloque con cañón que dispara. Cuando `Head` exista saldrá con cabeza y el cañón sobre ella |
| `Jam.Build un tanque con lanzallamas` | `plantilla Tank`, `BigWheel x4, Cannon x1, Seat x1, Flamethrower x1` | Tanque de ruedas grandes |
| `Jam.Build una rueda acostada con dos lanzallamas que gire y queme todo` | `arquetipo=Spinner`, `orientación flat`, `Wheel x1, Flamethrower x2` | Rueda acostada con un lanzallamas a cada lado del canto. Girar depende del riesgo 14 |
| `Jam.Build una rueda infernal` | `arquetipo=Roller`, `BigWheel x1, Flamethrower x1` | Rueda parada con lanzallamas de lado |
| `Jam.Build un globo con fuego debajo` | `arquetipo=Airship`, `Plank x1, Flamethrower x1, Balloon x1, Seat x1` | Tabla con lanzallamas apuntando arriba y el globo encima |
| `Jam.Clear` | `N construcciones destruidas` | Todo desaparece |
| `Jam.Inv Plank 1`, `Jam.Inv Fan 2`, `Jam.Build un carro` | `plantilla Hoverbike (pedida Car) DEGRADADA`, `Materiales faltantes: 2 Wheel` (amarillo) | Moto flotante en vez de carro |
| `Jam.Build un carro` otra vez | `Solver: NO se arma`, `Te falta: 1 Plank` (rojo) | Nada nuevo: el inventario ya se gastó |
| `Jam.Clear`, `Jam.InvShow` | `Plank x1, Fan x2` | Las piezas regresaron |
| `Jam.InvClear`, `Jam.Inv Plank 1`, `Jam.Inv Wheel 2`, `Jam.Build un carro con cohete` | `Reducción: ground_drive de 4 a 2`, `Materiales faltantes: 2 Wheel, 1 Rocket` | Carro de 2 ruedas centradas |
| `Jam.InvClear`, `Jam.Inv Block 1`, `Jam.Build una torreta` | `NO se arma`, `Te falta: 1 Cannon` | Nada: una torreta sin arma no se entrega |

4. **Con modelo de decisión:** si hay clave en `Config/DefaultSecrets.ini` o en las variables de entorno que lee
   `FJamDecisionClient`, la línea `Decisión:` dice `modelo de decisión (...)`. Sin clave o si la llamada falla dice
   `RESPALDO por palabras clave`. Nunca escribir claves en el repo.
5. **Sin el JSON** (renombrarlo un momento y `Jam.Reload`): `Jam.Build un carro` debe decir `Respaldo por tabla (sin BuildKnowledge.json)`
   y armar igual. Restaurar el archivo y `Jam.Reload`.
6. **Desde el MCP o Python del editor** (con Play activo): `UJamPoc::Build(WorldContext, "un carro")`, `UJamPoc::GetLastSummary()`,
   `UJamPoc::FillDebugInventory(10)`, `UJamPoc::ClearBuilds()`.

### 6.5 Si algo falla, cómo reportarlo

| Qué pasó | Qué anotar |
|---|---|
| No compila | Archivo, línea y el error completo; qué fila de la §5 era y qué se cambió |
| Falla una prueba | Nombre de la prueba y el mensaje (trae caso, campo, obtenido y esperado) |
| `Jam.Build` no hace nada | Si había Play activo, la línea `[Jam...]` del *Output Log*, y la salida de `Jam.Reload` |
| Se arma pero se ve mal o no se mueve | El pedido, `LastSummary`, captura, y si el problema es de receta (cara o padre equivocado) o de dispositivo (la pieza no empuja / no gira: otra rama) |

Anotar el resultado real de la §6 en este doc (tabla de la §4) y cambiar `status` a `listo`.

## → Aplicable a GameJamm

1. **Compilar y correr la §6** en la sesión con Unreal; es lo único que falta para dar el solver por probado.
2. **Conectar el inventario real**: en el flujo de juego llamar a `UJamSolver::Solve` con `InventoryFromComponent` (hoy solo el PoC
   lo hace), descontar con `UInventoryComponent::RemoveItem` saltando `IsInfiniteType`, y devolver al desarmar.
3. **Mostrar `FJamSolverResult::Message`** en la UI del asistente y ofrecer las dos opciones del doc 13 §3.3 paso 7 (versión
   reducida ahora, o ir por el material) usando `Missing`.
4. **`AJamBuildSpawner`**: aplicar `Orientation` al `Origin` y llamar a `RefineChoiceFromText` tras la respuesta del modelo (hoy
   solo el PoC y el camino por palabras clave lo hacen).
5. **Preguntas dinámicas al modelo** (doc 13 §3.1–3.2): ofrecer también las plantillas solo-JSON (`Spinner`, `Roller`, `Airship`...)
   y `want_<Tipo>` → `FJamBuildChoice::WantTypes`.
6. **Pendiente del solver**: chequeo de empuje contra peso (doc 13 §3.4), dónde conseguir cada material, petición pendiente.
7. **Constantes tunables** (todas en el JSON): `energy.warn_duration_s`, `energy.start`, `energy_cost_per_s`, `fallbacks`,
   `part_pref`, `min` / `count` por slot. Tras cambiarlas: validar, correr el prototipo y regenerar el espejo.
8. **Empaquetado**: agregar `Content/Game/Building/Data` a los directorios no-asset.
