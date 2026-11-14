# Guía de arte — GameJamm (título por definir)

> Dirección de arte, UI y audio del juego. Dueño: área 5 (Mundo / Arte / Audio / UI), ver [TEAM.md](TEAM.md).
> El tema está en [GDD.md](GDD.md#tema-directors-cut). Las convenciones técnicas de cada pieza
> (escala, colisión `UCX_`, sockets `Attach_*`, nombres `SM_/MI_/NS_`) están en
> [assets/CATALOGO-ELEMENTOS.md](assets/CATALOGO-ELEMENTOS.md). Esta guía no las repite: define **cómo se ve**.
>
> Referencias visuales: [concepts/](concepts/) (conceptos 01-A, 01-B y 01-C).

## 1. El look en una frase

**Un archipiélago de islas flotantes con islas grandes de campo abierto, en low-poly estilizado y luz de día cálida.**
Las islas chicas son puntos de interés (torres, loot, ruinas); las grandes son praderas abiertas donde se maneja y se pelea.
Las creaciones del jugador son **artefactos improvisados** de madera, hierro y cuerda, armados con lo que encuentra.

Tomado de los conceptos:
- **01-A (cielo):** cadena de islas conectadas por puentes de cuerda y tabla; bases de cada equipo en los extremos.
- **01-B (campo abierto):** valles de pasto, río, caminos de tierra y colinas para manejar a toda velocidad.
- **01-C (islas):** cascadas que caen de las orillas al mar de nubes, molinos, ruinas de piedra y chatarra para lootear.

> [!warning] Sin ficción de cine
> Una versión anterior de esta guía describía un estudio de cine (utilería, reflectores, claqueta). Se retiró: no es el
> tema del jam ni la dirección del equipo. No usar reflectores, cámaras, claquetas ni sets de filmación en el mundo.

## 2. Tres pilares (para decidir cuando haya duda)

| Pilar | Qué significa | Ejemplo sí | Ejemplo no |
|---|---|---|---|
| **Hecho con lo que hay** | Madera, hierro, cuerda, lona y piedra; tornillos y herrajes grandes y visibles | Rueda de madera con aro de hierro y cuatro pernos | Metal pulido realista o sci-fi brillante |
| **Legible de lejos** | Siluetas gruesas, formas exageradas y el color de equipo en bloques grandes | Torre con bandera enorme y franjas rojas | Detalles finos que se pierden a 20 m |
| **Aire y escala** | Cielo abierto, horizonte lejano, islas a distintas alturas | Ver la isla enemiga a lo lejos desde tu base | Espacios cerrados u oscuros |

## 3. Paleta

| Uso | Color | Hex |
|---|---|---|
| Madera (base de casi todas las piezas) | madera clara | `#C9A06B` |
| Madera sombra / vetas | madera oscura | `#8A6A43` |
| Hierro, herrajes, pernos | gris hierro | `#5A5D63` |
| Pasto de islas | verde lima | `#A8CC4A` |
| Pradera / campo abierto | verde pradera | `#77984A` |
| Roca de la base de las islas | umbra | `#78604B` |
| Piedra de ruinas | arenisca | `#D8B989` |
| Agua y cascadas | turquesa | `#408F94` |
| Cielo | azul cielo | `#58AFE0` |
| Nubes | blanco nube | `#F6F4ED` |
| Luz del sol | dorado | `#E7B65A` |
| **Equipo Rojo** | carmesí | `#C94248` |
| **Equipo Azul** | cobalto | `#355DC6` |
| Neutral (campamentos de loot) | amarillo advertencia | `#F2C230` + franjas `#1A1A1A` |
| FX fuego | naranja | `#FF7A1A` |
| FX hielo | cian | `#8FE3FF` |
| FX rayo | amarillo chispa | `#F5F06A` |
| FX bomba / explosión | blanco + humo `#2B2B2B` | `#FFFFFF` |

Reglas:
- El color de equipo **solo** va en máscaras grandes (banderas, franjas, techos de torre, ropa del personaje), nunca como
  tinte de todo el modelo. La madera sigue siendo madera.
- Además del color, cada equipo tiene una **forma** para daltonismo: Rojo = rombo, Azul = círculo. Van en banderas, torres y base.
- Lo neutral (loot, enemigos de campamento) usa amarillo con franjas: así "esto se puede lootear" se lee al instante.

## 4. Materiales

Un solo material maestro, `M_Part_Master`, en `Content/Game/Art/Materials/`, y una instancia por pieza (`MI_<id>`).

| Parámetro | Tipo | Para qué |
|---|---|---|
| **`Color`** | Vector | **Obligatorio con este nombre exacto.** El código de construcción (`JamBuildPart.cpp`) ya pinta las piezas con `SetVectorParameterValue("Color", …)` |
| `TeamColor` | Vector | Lo que va pintado con la máscara de equipo (canal R de `T_<id>_Mask`) |
| `Wear` | Scalar 0–1 | Bordes despintados y raspones (tinte de borde por curvatura o textura) |
| `Emissive` | Scalar | Encendido (dispositivo activo), apagado (sin energía) o parpadeo (pre-expiración), según los estados del catálogo |
| `WoodGrain` | Texture | Textura de veta tileable compartida por todas las piezas de madera |

- Rugosidad alta (0.7–0.9) en todo. Lo único brillante son el agua, los cristales de energía y los FX.
- Las texturas por pieza son opcionales. Con el maestro y la veta compartida, la mayoría de las piezas basta con `Color` y `Wear`.

## 5. Modelado (piezas, armas, props)

- **Formas gruesas y biseladas**, con proporciones de juguete: ruedas un poco más grandes y tablas un poco más gruesas de lo real.
- Los detalles finos (vetas, letras, desgaste) van **en textura o decal**; los herrajes grandes (pernos, aros, esquineros) sí van
  en geometría porque dan la silueta. Presupuesto = 5k tris (ver catálogo).
- **Cada pieza tiene que "decir" su función de lejos:** el ventilador con aspas enormes, el cohete con franjas y aletas, el cañón
  con una boca exagerada y la batería con un rayo pintado.
- **Armas:** espada y arco de aventurero, sencillos y robustos. Los materiales de fusión se pegan encima del arma (ver §8 sobre FX).

## 6. Personajes (recorte de alcance)

No hay tiempo para personajes originales completos. Usamos el **maniquí de UE (Manny/Quinn) con una instancia de material de
"ropa de explorador"** con color de equipo, más 1–2 accesorios estáticos pegados a sockets:

| Personaje | Accesorios | Color |
|---|---|---|
| Jugador | Gorro + bufanda | Color del equipo + chaleco café |
| Bots héroe | Casco con gogles | Color del equipo |
| Minions | Ninguno | Color del equipo, más apagado (`Wear` alto) |
| Enemigos de campamento | Casco de chatarra | Amarillo neutral |

Si alguien tiene tiempo el viernes, el primer upgrade es el gorro y la bufanda del jugador, porque salen en todas las tomas del video.

## 7. Mundo: el archipiélago

El mapa es una **cadena de islas flotantes** sobre un mar de nubes. El carril las conecta de base a base.

| Zona | Qué es | Qué se ve |
|---|---|---|
| Base Roja + torres rojas | Isla fortaleza roja | Castillo de piedra y madera con banderas rojas, en un extremo |
| Centro del carril | **Islas de campo abierto** | Praderas grandes con colinas, río y caminos de tierra: aquí se maneja y se pelea |
| Base Azul + torres azules | Isla fortaleza azul | Castillo con banderas azules, en el otro extremo |
| Campamentos de loot (3–4) | Islas chicas de ruinas y chatarra | Arcos de piedra rotos, pilas de piezas, cofres con franjas amarillas |

- **Torre** = atalaya de madera sobre su propia isla chica, con bandera de equipo y un cristal arriba que es el que dispara.
- **Base** = torre del homenaje de la fortaleza, con el estandarte grande del equipo. Si la destruyes, ganas.
- **Conexiones:** puentes de cuerda y tabla, rampas de tierra y huecos para cruzar volando con un artefacto.
- **Límites del mapa:** el borde de las islas y el mar de nubes. Caerse = respawn.
- **Detalles baratos que dan vida:** cascadas en las orillas, molinos, árboles low-poly, rocas flotando sueltas.

> [!warning] Originalidad
> Las islas flotantes también existen en el juego de referencia de la research. Nuestro diseño tiene que ser propio: nada de
> sus formas, arquitectura, símbolos ni nombres.

## 8. Iluminación y FX

**Luz:**
- Lumen. Sol direccional cálido (`#E7B65A`), cielo azul con nubes volumétricas y niebla de altura suave para dar profundidad
  a las islas lejanas.
- Presupuesto: una sola luz direccional con sombras; las luces locales (cristales, fuego) sin sombras o con emisivos.

**FX** estilizados y con trazo grueso:

| FX | Cómo se ve |
|---|---|
| Fuego (`FUSE_FIRE`, `DEV_FLAME`) | Llamas naranjas gruesas con bocanadas de humo oscuro |
| Hielo (`FUSE_ICE`, `DEV_FROST`) | Neblina fría a ras de suelo y cristales cian facetados |
| Rayo (`FUSE_SHOCK`) | Chispas amarillas en zigzag |
| Explosión (`FUSE_BOMB`, `DEV_CANNON`) | Destello blanco, bola de humo redonda y astillas de madera volando |
| Golpe | Astillas de madera y polvo, sin sangre |
| Destrucción de torre o base | La estructura se desarma en tablas y la bandera cae |

## 9. UI y presentación

**Tipografías** (gratuitas, de Google Fonts, licencia OFL; anotarlas en [SUBMISSION.md](SUBMISSION.md)):
- **Bebas Neue**: títulos y números grandes.
- **Courier Prime**: todo lo que "escribe" el asistente de IA.

**Elementos (en orden de prioridad):**

| # | Elemento | Cuándo | Costo |
|---|---|---|---|
| 1 | Panel del **asistente de IA** con texto que aparece letra por letra y la receta que va a armar | Al pedir una construcción | Bajo |
| 2 | **Modo construcción**: puntos de unión que brillan en amarillo y confirmación de la creación con su nombre | Al construir | Bajo |
| 3 | Aviso grande al activar una creación | Al activar | Bajo |
| 4 | Timer de la partida y estado de torres en la parte de arriba | Siempre | Muy bajo |
| 5 | Pantalla de **victoria / derrota** | Al destruir una base | Muy bajo |
| 6 | Resumen final con stats: "Torres derribadas", "Mejor creación: Rueda Infernal" | Pantalla final | Bajo |
| 7 | Menú principal con "Jugar" y "Salir" | Inicio | Bajo |

HUD de juego: vida, inventario y estado de torres en paneles de madera con esquineros de hierro. Cada torre se muestra como un
icono de atalaya y se apaga cuando cae.

## 10. Audio (dirección breve)

- **Música:** aventura ligera (cuerdas, percusión, flauta), intensa en combate y tranquila en las islas de loot.
- **SFX:** viento en las islas, golpe de madera al unir piezas, motor y hélices de los artefactos, cristal de la torre al disparar.
- **Golpes:** madera y piedra (*thunk*), no metal brillante.

## 11. Generar assets con IA

**Prefijo de estilo** (va al inicio de todo prompt de Meshy/Tripo/Rodin):

> `stylized low-poly game prop, handcrafted from wood planks, iron brackets, bolts and rope, chunky beveled shapes, toy-like proportions, matte finish, single object, centered, neutral background`

**Negativo:** `realistic, photoreal, glossy metal, sci-fi chrome, text, logo, brand, character face, multiple objects`

**Prompts por asset, tamaños, orientación y qué hace cada uno:** [assets/PROMPTS-ASSETS.md](assets/PROMPTS-ASSETS.md).

**Proceso por pieza:** generar → en Blender ajustar escala (cm), pivote en el centro de masa, colisión `UCX_`, sockets `Attach_*`
→ importar a `Content/Game/Building/` (o `Art/` si es un prop de mundo) → `MI_<id>` del maestro → checklist del catálogo §10.
Anotar la herramienta usada en [SUBMISSION.md](SUBMISSION.md) (cuenta para los premios de partners).

## 12. Prohibido

- Nada de películas, personajes, logos ni marcas reales.
- Nada de texto generado por IA dentro de las texturas (sale ilegible). El texto va como decal o en UI.
- Nada de assets ni nombres del juego de referencia de la research (ver reglas del catálogo).
