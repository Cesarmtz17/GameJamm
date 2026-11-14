# Guía de arte — Backlot Brawl (título de trabajo)

> Dirección de arte, UI y audio del juego. Dueño: rol 5 (Mundo / Arte / Audio / UI).
> El tema y la ficción están en [GDD.md](GDD.md#tema-directors-cut--cerrado). Las convenciones técnicas de cada pieza
> (escala, colisión `UCX_`, sockets `Attach_*`, nombres `SM_/MI_/NS_`) están en
> [assets/CATALOGO-ELEMENTOS.md](assets/CATALOGO-ELEMENTOS.md). Esta guía no las repite: define **cómo se ve**.

## 1. El look en una frase

**Todo es utilería de un estudio de cine de bajo presupuesto.** Las piezas, las armas, los sets y hasta los enemigos
son props hechos a mano con triplay, cartón, cinta gaffer y pintura. El mapa es un set de filmación: por delante
se ve la película y por detrás, el andamio.

Esto nos conviene porque **lo imperfecto es parte del estilo**. Si un asset generado con IA sale un poco chueco, lo
vendemos como "prop hecho a mano". Y una fachada plana con soportes por detrás es más barata que un edificio
completo.

## 2. Tres pilares (para decidir cuando haya duda)

| Pilar | Qué significa | Ejemplo sí | Ejemplo no |
|---|---|---|---|
| **Hecho a mano** | Materiales de utilería visibles: veta de madera, bordes de cartón, cinta, tornillos | Rueda de triplay con cinta gris alrededor | Metal pulido realista o sci-fi brillante |
| **Legible de lejos** | Siluetas gruesas, formas exageradas y el color de equipo en bloques grandes | Torre con un reflector enorme y bandas rojas | Detalles finos que se pierden a 20 m |
| **Luz de set** | Fondo oscuro de foro y luz cálida y dura de reflectores sobre la acción | Conos de luz volumétrica sobre el carril | Día soleado y plano |

## 3. Paleta

| Uso | Color | Hex |
|---|---|---|
| Triplay (base de casi todo) | madera clara | `#C9A06B` |
| Triplay sombra / vetas | madera oscura | `#8A6A43` |
| Cartón | kraft | `#B98B5A` |
| Cinta gaffer, metal de tramoya | gris carbón | `#3A3A3C` |
| Pintura base de sets | hueso | `#EDE6D6` |
| **Equipo Rojo** | rojo claqueta | `#D7263D` |
| **Equipo Azul** | azul croma | `#1B98E0` |
| Neutral (campamentos de loot) | amarillo de advertencia | `#F2C230` + franjas negras `#1A1A1A` |
| Luz de reflector | ámbar cálido | `#FFD38A` |
| Ambiente del foro (sombras, cielo) | azul noche | `#1E2233` |
| FX fuego | naranja pirotecnia | `#FF7A1A` |
| FX hielo | cian hielo seco | `#8FE3FF` |
| FX rayo | amarillo chispa | `#F5F06A` |
| FX bomba / explosión | blanco + humo `#2B2B2B` | `#FFFFFF` |

Reglas:
- El color de equipo **solo** va en máscaras grandes (franjas, banderas, overoles y reflectores de torre), nunca como
  tinte de todo el modelo. La madera sigue siendo madera.
- Además del color, cada equipo tiene una **forma** para daltonismo: Rojo = marcas de cinta en **X**, Azul = marcas en **O**
  (círculo). Van pintadas en torres, base, suelo y en la espalda del overol.
- Lo neutral (loot, enemigos de campamento) usa amarillo con franjas de advertencia: así "esto se puede lootear" se lee
  al instante.

## 4. Materiales

Un solo material maestro, `M_Part_Master`, en `Content/Game/Art/Materials/`, y una instancia por pieza (`MI_<id>`).

| Parámetro | Tipo | Para qué |
|---|---|---|
| **`Color`** | Vector | **Obligatorio con este nombre exacto.** El código de construcción (`JamBuildPart.cpp`) ya pinta las piezas con `SetVectorParameterValue("Color", …)` |
| `TeamColor` | Vector | Lo que va pintado con la máscara de equipo (canal R de `T_<id>_Mask`) |
| `Wear` | Scalar 0–1 | Bordes despintados y raspones (tinte de borde por curvatura o textura) |
| `Emissive` | Scalar | Encendido (dispositivo activo), apagado (sin energía) o parpadeo (pre-expiración), según los estados del catálogo |
| `WoodGrain` | Texture | Textura de veta tileable compartida por todas las piezas de madera |

- Rugosidad alta (0.7–0.9) en todo. Lo único brillante son los focos, los lentes y los charcos de FX.
- Las texturas por pieza son opcionales. Con el maestro y la veta compartida, la mayoría de las piezas basta con `Color` y `Wear`.

## 5. Modelado (piezas, armas, props)

- **Formas gruesas y biseladas**, con proporciones de juguete: ruedas un poco más grandes y tablas un poco más gruesas de lo real.
- Los detalles (tornillos, cinta, letras pintadas) van **en textura o decal**, no en geometría. Presupuesto ≤ 5k tris (ver catálogo).
- **Cada pieza tiene que "decir" su función de lejos:** el ventilador con aspas enormes, el cohete con una flecha pintada, el cañón
  con una boca exagerada y la batería con un rayo pintado.
- **Armas:** la espada es una espada de utilería de madera pintada de plateado y el arco es un arco de escenografía. Los materiales
  de fusión se pegan como props encima del arma (ver §8 sobre FX).

## 6. Personajes (recorte de alcance)

No hay tiempo para personajes originales completos. Usamos el **maniquí de UE (Manny/Quinn) con una instancia de material de
"overol de tramoyista"** con color de equipo, más 1–2 accesorios estáticos pegados a sockets:

| Personaje | Accesorios | Color |
|---|---|---|
| Jugador — **el Director** | Gorra + megáfono en la espalda | Color del equipo + franja hueso |
| Bots héroe — **Dobles de riesgo** | Casco de stunt | Color del equipo |
| Minions — **Extras** | Ninguno (o claqueta en la mano) | Color del equipo, más apagado (`Wear` alto) |
| Enemigos de campamento — **Utileros** | Caja de cartón como casco | Amarillo neutral |

Si alguien tiene tiempo el viernes, el primer upgrade es la gorra del Director, porque sale en todas las tomas del video.

## 7. Mundo: el backlot

El carril es un **foro de filmación gigante**. Cada sección del carril es un set de un género distinto, así que la variedad sale
barata: son fachadas planas con soportes.

| Zona | Set | Qué se ve |
|---|---|---|
| Base Roja + torres rojas | **Western** | Fachadas de cantina y tienda de triplay, cactus de cartón, barriles |
| Centro del carril | **Castillo medieval** | Muros de cartón piedra, almenas, un dragón de utilería tirado (si da tiempo) |
| Base Azul + torres azules | **Sci-fi de serie B** | Paneles con focos de colores, platillo volador colgado de cables |
| Campamentos de loot (3–4) | **Bodegas de utilería** | Pilas de cajas, racks con piezas, cinta amarilla y negra, cofre = baúl de utilería |

- **Torre** = torre de reflector: andamio de tubo con un reflector gigante arriba, que es el que dispara.
- **Base** = **"Cámara A"**: una cámara de cine enorme sobre un dolly, con el rollo de película del equipo. Si la destruyes, se acaba su película.
- **Límites del mapa:** paredes negras del foro con pasarelas, cables y luces colgando. El cielo es un **ciclorama pintado** (atardecer
  naranja → azul noche) y no un skybox real.
- Suelo de concreto pulido con **marcas de cinta** (las X y O de cada equipo, flechas del carril) como decals.
- **Por detrás de las fachadas se ve el andamio.** Es el chiste visual del tema y no cuesta nada extra.

## 8. Iluminación y FX

**Luz:**
- Lumen. Una luz direccional débil y fría (`#1E2233`) y spotlights cálidos (`#FFD38A`) que caen sobre el carril. Niebla
  volumétrica baja para que se vean los conos.
- Presupuesto: máximo ~12 spotlights con sombra en todo el mapa. El resto, sin sombras o con emisivos falsos.

**FX** = **efectos especiales prácticos de cine**, estilizados y con trazo grueso:

| FX | Cómo se ve |
|---|---|
| Fuego (`FUSE_FIRE`, `DEV_FLAME`) | Llamas naranjas gruesas con bocanadas de humo negro, como pirotecnia de set |
| Hielo (`FUSE_ICE`, `DEV_FROST`) | Niebla de hielo seco a ras de suelo y cristales cian facetados |
| Rayo (`FUSE_SHOCK`) | Chispas amarillas en zigzag, como chispero de soldadura |
| Explosión (`FUSE_BOMB`, `DEV_CANNON`) | Destello blanco, bola de humo redonda y astillas de triplay volando |
| Golpe | Astillas de madera y polvo, sin sangre |
| Destrucción de torre o base | La estructura se desarma en tablas y el reflector cae y parpadea |

## 9. UI y presentación (aquí vive el tema)

**Tipografías** (gratuitas, de Google Fonts, licencia OFL; anotarlas en [SUBMISSION.md](SUBMISSION.md)):
- **Bebas Neue**: títulos, claqueta y números grandes.
- **Courier Prime**: todo lo que "escribe" el Asistente de Dirección, como si fuera un guion a máquina.

**Elementos (en orden de prioridad):**

| # | Elemento | Cuándo | Costo |
|---|---|---|---|
| 1 | **Claqueta** que se cierra con el texto `ESCENA <n> · TOMA <m> · <nombre de la creación>` | Al construir | Bajo (widget + animación) |
| 2 | **Barras de cine** (letterbox) que entran + texto **"¡ACCIÓN!"** | Al activar una creación | Bajo |
| 3 | **REC ● y timecode** en la esquina = timer de la partida | Siempre | Muy bajo |
| 4 | Panel del **Asistente de Dirección** con texto en Courier que aparece letra por letra | Al pedir una construcción | Bajo |
| 5 | **"¡CORTE!"** a pantalla completa | Al destruir una base (victoria o derrota) | Muy bajo |
| 6 | **Créditos finales** con stats: "Dirigida por TÚ", "Extras caídos: 37", "Mejor toma: Rueda Infernal" | Pantalla final | Bajo |
| 7 | Menú principal = **pizarra de producción** con "Rodar película" (Jugar) y "Salir" | Inicio | Bajo |

HUD de juego: vida, inventario y estado de torres en paneles con borde de cinta gaffer. Cada torre se muestra como un icono
de reflector, y se apaga cuando cae.

## 10. Audio (dirección breve)

- **Música:** surf/western de serie B con percusión, a todo volumen en combate y bajito en las bodegas.
- **SFX con firma de cine:** claqueta (*clack*) al construir, "¡Acción!" por megáfono al activar, "¡Corte!" al final, zumbido de
  reflector en las torres y rollo de película girando en la base.
- **Golpes:** madera y cartón (*thunk*), no metal.

## 11. Generar assets con IA

**Prefijo de estilo** (va al inicio de todo prompt de Meshy/Tripo/Rodin):

> `stylized low-poly game prop, handmade movie studio prop made of painted plywood, cardboard and grey gaffer tape, chunky beveled shapes, toy-like proportions, matte finish, single object, centered, neutral background`

**Negativo:** `realistic, photoreal, glossy metal, sci-fi chrome, text, logo, brand, character face, multiple objects`

**Prompts de los 10 imprescindibles del MVP** (prefijo + esto):

| ID | Prompt |
|---|---|
| `PART_PLANK` | `long flat wooden plank with visible wood grain and two screws at each end` |
| `PART_BLOCK` | `solid wooden crate-like cube with taped edges` |
| `PART_CYLINDER` | `thick cardboard tube pole with gaffer tape bands` |
| `JOINT_GLUE` | *(solo FX: costura de cinta gaffer, sin mesh)* |
| `DEV_WHEEL_S` | `small plywood wheel with grey tape tire and a round hub, side view` |
| `DEV_FAN` | `big boxy studio wind machine fan with three oversized blades in a wooden frame` |
| `CTRL_SEAT` | `folding director's chair made of wood and canvas, no text` |
| `PWR_BATTERY_S` | `chunky prop battery box with a painted lightning bolt` |
| `DEV_CANNON` | `squat prop cannon made of a cardboard tube on a wooden base, oversized muzzle` |
| `FUSE_FIRE` | `small prop fire canister with painted flames, can be attached to a sword` |

**Proceso por pieza:** generar → en Blender ajustar escala (cm), pivote en el centro de masa, colisión `UCX_`, sockets `Attach_*`
→ importar a `Content/Game/Building/` (o `Art/` si es un prop de mundo) → `MI_<id>` del maestro → checklist del catálogo §10.
Anotar la herramienta usada en [SUBMISSION.md](SUBMISSION.md) (cuenta para los premios de partners).

## 12. Prohibido

- Nada de películas, estudios, personajes, logos ni carteles reales. Los géneros (western, castillo, sci-fi) sí; títulos y franquicias, no.
- Nada de texto generado por IA dentro de las texturas (sale ilegible). El texto va como decal o en UI.
- Nada de assets ni nombres del juego de referencia de la research (ver reglas del catálogo).
