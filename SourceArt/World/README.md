# Mapa — blockout de islas flotantes

Dueño: Emilio (arte / mundo). Generado por script en Blender 5.3 con la dirección de [docs/ART.md](../../docs/ART.md) §7.
Es **blockout**: geometría real para caminar y probar el layout; torres, bases y cofres aquí son solo la figura.

![planta](preview_top.png)

| Archivo | Qué es |
|---|---|
| `FBX/SM_MAP_Blockout.fbx` | Todo el mapa (islas, puentes, pradera, río, torres/bases de figura, árboles), cada pieza en su posición de mundo |
| `map_layout.json` | Posiciones en cm (marco UE) de `PlayerStart`, `Base`, `Tower`, `LootCamp`, `Chest` por equipo |
| `Blend/MAP_Blockout.blend` | Escena de Blender |
| `scripts/gen_map.py` | Generador (regenera todo en segundos) |

Layout: carril de ~250 m sobre X (Rojo en X−, Azul en X+). Base → puente → isla T2 → puente → pradera central (T1 de cada
equipo en las orillas, río con puente) → T2 → base. 4 islas de loot con puente. Todo lo caminable está en Z = 0.

## Versión vestida (`FBX/SM_MAP_Dressed.fbx`)

Mismo layout y mismas posiciones (`map_layout.json`), pero con los modelos reales de `SourceArt/World/Props`: atalayas con
barricada, fortalezas con banderas y plataforma de spawn, campamentos de loot con ruinas, cofre que se abre, chatarra,
caja, barril y fogata, árboles variados, ~70 detalles en la pradera (pasto, flores, arbustos, rocas, hongos), faroles en el
camino, mesa de construcción y tienda en cada base. Rojo/azul ya vienen pintados por equipo.

![vestido](preview_dressed_close.png)

Se importa igual que el blockout (pasos de abajo). Generador: `scripts/gen_map_dressed.py` (reutiliza `gen_map.py` y los
builders de `SourceArt/Building/scripts`).

## Probarlo caminando en UE (≈5 min)

1. Crear un nivel de prueba en tu carpeta, p. ej. `Content/Developers/<tu-nombre>/L_Test_Islands` (no tocar `L_Main`).
2. **File → Import Into Level…** → `SourceArt/World/FBX/SM_MAP_Blockout.fbx`. Destino de assets: `Content/Developers/<tu-nombre>/MapBlockout/`.
   Así cada pieza entra ya colocada en su lugar.
3. Colisión para poder caminar: selecciona todos los Static Mesh importados en el Content Browser → *Asset Actions → Bulk Edit
   via Property Matrix* → `Collision Complexity = Use Complex Collision As Simple` → guardar.
4. Poner un **Player Start** en el spawn rojo: `X = -10800, Y = 0, Z = 100` (o el azul en `X = 10800`).
5. Agregar luz rápida: *Window → Env. Light Mixer* → crear Directional Light, Sky Atmosphere, Sky Light, Volumetric Cloud.
6. **Play** (Alt+P) con el personaje de tercera persona.

Después, Core puede colocar los actores reales (`ATeamStructure`, cofres, spawns) con las posiciones de `map_layout.json`.

> [!warning] Sin probar en Unreal
> Verificado solo en Blender. Si la escala sale ×100 o el mapa sale girado, avisar a Emilio y se ajusta el export.
