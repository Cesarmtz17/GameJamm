---
title: Catálogo de elementos para generar assets (construcción, vehículos, dispositivos)
date: 2026-11-13
tags:
  - assets
  - catalogo
  - construccion
  - gamejamm
status: borrador-v1
---

# Catálogo de elementos del juego → lista de assets

> [!info] Para quién es
> Para **Emilio** (generación de assets) y para cualquier agente que arme piezas. Cada fila es un
> elemento jugable con lo que hay que producir. Basado en la research de
> [03-mecanicas-vehiculos-dispositivos.md](../mechanics-research/03-mecanicas-vehiculos-dispositivos.md),
> [01-mecanicas-construccion.md](../mechanics-research/01-mecanicas-construccion.md) y
> [02-mecanicas-fusion.md](../mechanics-research/02-mecanicas-fusion.md).

> [!warning] Reglas de contenido (LEER)
> - **Todos los assets son ORIGINALES** (generados por IA o hechos por nosotros). **No** usar modelos,
>   texturas, sonidos ni nombres propietarios del juego de referencia ni de ningún juego comercial.
> - Los nombres de este catálogo son **nuestros IDs genéricos** (`DEV_FAN`, `PART_PLANK`...), no los del juego de referencia.
> - Los números (masa, consumo, tamaño) son **valores iniciales de diseño** para tunear en DataAssets; no son datos oficiales.
> - Cambios a assets existentes: `git lfs lock` primero. Carpeta sugerida: `Content/Game/Building/`.

> [!note] Verificación de la lista de dispositivos (2026-11-13)
> Cotejada con 2 guías públicas: una lista **27** dispositivos (incluye cohete, piedra flotante, emisor de rayo,
> vagón teledirigido, cabeza objetivo, trineo y batería grande) y otra, más antigua ("los encontrados hasta ahora"),
> lista 21, todos contenidos en los 27. No hay contradicciones. Los demás sitios consultados bloquearon el acceso (403), así que
> la lista completa se apoya en esas dos fuentes más la research de Javier (doc 03). Las piezas estructurales (tablas, vigas, bloques)
> y las uniones son **diseño nuestro**, no parte de esa lista.

## 0. Convenciones para TODAS las piezas

| Tema | Regla |
|---|---|
| Escala | 1 unidad UE = 1 cm. Pieza chica ≈ 50 cm, mediana ≈ 100 cm, grande ≈ 200 cm. Pivote en el centro de masa |
| Mesh | Low-poly estilizado (≤ 5k tris/pieza), 1 material (instancia de `M_Part_Master`), UV limpio |
| Colisión | Colisión **simple** convexa por pieza (sin complex-as-simple). Nombrar `UCX_<id>_NN` |
| Sockets de unión | Nombre exacto `Attach_<cara>` en el mesh: `Attach_Top`, `Attach_Bottom`, `Attach_Front`, `Attach_Back`, `Attach_Left`, `Attach_Right`. Orientados con +X saliendo de la superficie. Ver **§0.1 Contrato de uniones** (cómo se relacionan con la receta JSON) |
| Sockets de efecto | `Fx_Out` (salida de empuje/fuego/proyectil), `Fx_Hit` (punto de impacto) cuando aplique |
| Nombres de asset | `SM_<id>`, `M_<id>`, `MI_<id>_<variante>`, `NS_<id>_<efecto>`, `SFX_<id>_<evento>` |
| Variantes | Material por estado: normal / activo / sin energía / pre-expiración (destello) |
| Límite de ensamblado | 21 piezas por construcción (valor de diseño, ajustable en DataAsset) |

## 0.1 Contrato de uniones y tipos (receta JSON ↔ ensamblador ↔ mesh)

> [!important] Un solo vocabulario
> Antes había tres formas de nombrar las uniones: `Attach_Left` (este catálogo), `Side` (ejemplo del GDD) y `Top`/`Side`/`AxleL`/`AxleR`
> (research 12). Desde ahora queda así:

| Capa | Qué se escribe | Ejemplo |
|---|---|---|
| **Receta JSON** (`"socket"`, lo que escribe la IA) | La cara a secas: `Top`, `Bottom`, `Front`, `Back`, `Left`, `Right`. Opcional: una segunda cara para correrse hacia un borde, separada por `_` | `"Left"`, `"Left_Front"` (lado izquierdo, pegado al frente) |
| **Mesh** (Socket Manager, lo que hace arte) | `Attach_` + la cara | `Attach_Left` |
| **Ejes de rueda** | No hay nombre aparte (`AxleL`/`AxleR` ya no existen): la rueda se une por `Left`/`Right` | Rueda en `"Left_Front"` del chasis |
| Alias heredado | `Side` = `Right` (el código lo acepta, pero no lo usen en recetas nuevas) | — |
| Sin socket | `Top` | — |

**Ejes en el marco del vehículo:** Front = +X, Right = +Y, Top = +Z (convención de UE).

> [!warning] Cómo coloca las piezas el ensamblador (`JamLayout.cpp`)
> El código **todavía no lee los sockets del mesh**: coloca cada pieza junto a la cara del padre como una **caja de tamaño
> `Size`** (tabla de abajo) y luego **escala el mesh a su bounding box** para que llene esa caja. Por eso lo que importa son
> las **proporciones** y el **pivote al centro**, no los cm exactos. Si dos piezas caen en la misma cara, la segunda se
> empuja hacia afuera para que no se encimen. Los sockets `Attach_*` se ponen igual, porque se usarán después.
> El parser repara recetas mal escritas por la IA (alias de tipo, sinónimos de cara, padres inexistentes, etc.): ver tests en
> `Source/GameJamm/Building/Tests/JamRecipeTests.cpp`.

**Tipo en la receta ↔ ID del catálogo** (el `"type"` del JSON es el nombre corto; el ID es el del asset):

| `"type"` en receta | ID / asset | `Size` X×Y×Z (cm) | Masa (kg) |
|---|---|---|---|
| `Plank` | `PART_PLANK` → `SM_PART_PLANK` | 200 × 60 × 10 | 5 |
| `Block` | `PART_BLOCK` | 60 × 60 × 60 | 15 |
| `Cylinder` | `PART_CYLINDER` | 150 × 50 × 50 | 10 |
| `Wheel` | `DEV_WHEEL_S` | 50 × 20 × 50 | 4 |
| `BigWheel` | `DEV_WHEEL_L` | 80 × 30 × 80 | 8 |
| `Fan` | `DEV_FAN` | 15 × 50 × 50 | 3 |
| `Rocket` | `DEV_ROCKET` | 60 × 20 × 20 | 4 |
| `Balloon` | `DEV_BALLOON` | 90 × 90 × 90 | 1 |
| `Cannon` | `DEV_CANNON` | 70 × 20 × 20 | 8 |
| `Seat` | `CTRL_SEAT` | 40 × 40 × 30 | 5 |
| `Battery` | `PWR_BATTERY_S` | 20 × 20 × 20 | 3 |
| `Flamethrower` | `DEV_FLAME` | 40 × 20 × 20 | 3 |

La fuente de verdad de `Size` y masa es la tabla de `Source/GameJamm/Building/JamBuildPart.cpp` (luego, DataAssets). Si alguno
cambia ahí, actualicen esta tabla.

Cada pieza necesita mínimo: **mesh + colisión + sockets + 1 icono** (256×256, para el menú). Los dispositivos
añaden **VFX (Niagara)** y **SFX** de sus estados.

## 1. Piezas estructurales (sin energía)

| ID | Qué es | Sockets | Masa inicial (kg) | Notas para el asset |
|---|---|---|---|---|
| `PART_PLANK` | Tabla/plancha plana larga | Top, Bottom, Front, Back, Left, Right | 5 | Base de plataformas, alas y puentes |
| `PART_PANEL` | Panel cuadrado (pared/escudo) | las 6 caras | 8 | Variante "ventana" opcional |
| `PART_WALL` | Muro grueso grande | las 6 caras | 30 | Cuidar que la colisión sea una caja simple |
| `PART_BEAM` | Viga larga y delgada | extremos + 4 laterales | 6 | Para chasis |
| `PART_BLOCK` | Cubo macizo | las 6 caras | 15 | Pieza comodín |
| `PART_CYLINDER` | Cilindro/poste | Top, Bottom + laterales | 10 | Ejes y torres |
| `PART_BALL` | Esfera | 6 puntos cardinales | 10 | Rueda improvisada, junta |
| `PART_RAMP` | Cuña/rampa | Bottom, Back, Left, Right | 10 | |
| `PART_ARCH` | Arco/curva | extremos | 12 | Opcional (post-MVP) |
| `PART_ROD` | Varilla fina | extremos | 2 | Para ensamblar sin peso |

## 2. Uniones y anclajes (conectan piezas)

| ID | Qué es | Función | Asset |
|---|---|---|---|
| `JOINT_GLUE` | Pegamento/soldadura | Une dos piezas rígidamente (fixed) | Solo VFX/SFX de pegado y un material de "costura" |
| `JOINT_HINGE` | Bisagra | Gira en un eje (puertas, brazos) | Mesh con 2 mitades que giran + límite angular |
| `JOINT_SLIDER` | Corredera | Desliza en un eje (pistón) | Mesh de riel + carro |
| `JOINT_BALL` | Rótula | Giro libre limitado | Mesh de bola + copa |
| `JOINT_MOTOR` | Motor de eje | Gira con velocidad/torque (base de ruedas y hélices) | Mesh + animación de giro |
| `ANCHOR_STAKE` | Estaca/ancla | Se clava en el suelo y fija la construcción | Mesh + VFX de clavado |
| `ANCHOR_ROPE` | Cuerda/cadena | Une dos puntos con holgura | Mesh tileable + spline |

## 3. Movilidad (ruedas y bases)

| ID | Qué es | Energía (consumo/seg) | Sockets | Notas |
|---|---|---|---|---|
| `DEV_WHEEL_S` | Rueda chica motorizada | 0.015 | Attach_Left/Right (eje) | Rápida, mala en cuestas. Física: 3 cuerpos (rueda+motor+eje) |
| `DEV_WHEEL_L` | Rueda grande todoterreno | 0.015 | Attach_Left/Right | Cruza agua somera |
| `DEV_CART` | Vagón sin motor | 0 | Top + laterales | Fricción baja; necesita empuje externo |
| `DEV_SLED` | Trineo | 0 | Top | Se desliza en nieve/arena/pasto |
| `DEV_TRACK` | Oruga | 0.03 | laterales | Post-MVP |
| `DEV_SPRING` | Resorte | 0 | Top, Bottom | Se comprime y lanza lo de encima |

## 4. Propulsión y vuelo

| ID | Qué es | Energía | Sockets | Notas |
|---|---|---|---|---|
| `DEV_FAN` | Ventilador/hélice | **0.156** | Attach_Back, `Fx_Out` | Empuje; hacia arriba = sustentación. Apilables |
| `DEV_ROCKET` | Cohete de un uso | uso único (~3 s) | Attach_Back, `Fx_Out` | Thrust fuerte, se consume. VFX de llama y humo |
| `DEV_WING` | Ala/planeador | 0 | Attach lateral | Planea; centro de masa importa |
| `DEV_BALLOON` | Globo | 0 | Bottom | Sube con calor; emisor de fuego debajo = más rápido |
| `DEV_HOVER` | Piedra flotante | 0.03 | Attach_Bottom | Ignora gravedad al activarse |
| `DEV_STABILIZER` | Estabilizador | 0.03 | Attach_Bottom | Endereza la construcción |

## 5. Control y energía

| ID | Qué es | Función | Notas |
|---|---|---|---|
| `CTRL_SEAT` | Asiento de dirección | Se monta; dirige y **activa todos los dispositivos conectados** | Mesh con sockets de montura (`Seat_Rider`) y animación de sentado |
| `CTRL_HEAD` | Cabeza sensor | Gira hacia enemigos; con cañón/emisor = torreta autónoma | Mesh con cuello giratorio; ojo/lente que brilla al detectar |
| `CTRL_TIMEBOMB` | Bomba de tiempo | Estalla; su onda activa dispositivos cercanos | Mesh + VFX de cuenta atrás |
| `PWR_BATTERY_S` | Batería chica desechable | Alimenta el build **antes** que la reserva del jugador | Mesh + 4 estados de carga |
| `PWR_BATTERY_L` | Batería grande desechable | Igual, con más capacidad | Variante de escala |
| `PWR_CELL` | Celda de energía (HUD del jugador) | Reserva global del jugador | Solo UI (medidor de segmentos) |

## 6. Emisores y armas montables

| ID | Qué es | Energía | Efecto | Asset de efecto |
|---|---|---|---|---|
| `DEV_FLAME` | Emisor de fuego | 0.18 | Fuego continuo; calienta globos | `NS_flame` (cono), SFX loop |
| `DEV_FROST` | Emisor de hielo | 0.18 | Congela agua y enemigos | `NS_frost`, plataforma de hielo (mesh+material) |
| `DEV_SHOCK` | Emisor de rayo | 0.18 | Rayo desde la punta | `NS_shock` (arcos), SFX chisporroteo |
| `DEV_BEAM` | Emisor de haz | 0.18 | Haz de largo alcance (torreta) | `NS_beam` (cinta) + decal de impacto |
| `DEV_CANNON` | Cañón | 0.30 | Proyectil explosivo a intervalos | Mesh de proyectil + `NS_blast` |
| `DEV_HYDRANT` | Hidrante | 0.03 | Escupe agua, apaga fuego | `NS_water` |
| `DEV_LIGHT` | Luz/foco | 0.03 | Luz direccional | Luz + cono volumétrico |
| `DEV_MIRROR` | Espejo | 0 | Refleja luz/haz | Material reflectivo |

## 7. Materiales de fusión (para armas, escudos y flechas)

Mecánica: `Ataque = Base + Bono(material)`; la durabilidad sube una vez; cada material añade un efecto. Ver doc 02.

| ID de material | Tipo | Efecto al fusionar | Asset |
|---|---|---|---|
| `FUSE_FIRE` | Elemental | Quema | Icono + VFX de llama en el arma |
| `FUSE_ICE` | Elemental | Congela | Icono + VFX de hielo |
| `FUSE_SHOCK` | Elemental | Electrocuta | Icono + VFX de arcos |
| `FUSE_BOMB` | Explosivo | Explota al golpear | Icono + VFX de explosión |
| `FUSE_HOMING` | Guiado | Flechas/proyectiles con búsqueda | Icono + estela |
| `FUSE_SPIKE` | Cuerpo | Más daño (colmillo/púa) | Mesh que se pega al arma |
| `FUSE_ROCK` | Cuerpo | Daño pesado, lento | Mesh que se pega al arma |

Cada material necesita: **mesh pegable** (con socket `Attach_*`), **icono**, y **VFX/SFX del efecto** sobre el arma.

## 8. Interacciones entre elementos (para animaciones y VFX cruzados)

| Elemento A | + Elemento B | Resultado esperado |
|---|---|---|
| `DEV_FLAME` | `DEV_BALLOON` | El globo sube más rápido |
| `DEV_FROST` | agua | Plataforma de hielo |
| `DEV_FLAME` | `DEV_HYDRANT` | El fuego se apaga |
| `DEV_SHOCK` | agua / metal | Descarga se propaga |
| `CTRL_HEAD` | `DEV_CANNON` / `DEV_BEAM` | Torreta autónoma |
| `CTRL_SEAT` | cualquier `DEV_*` conectado | Se activan al subirse |
| `PWR_BATTERY_*` | cualquier `DEV_*` | Consumen primero la batería desechable |
| `CTRL_TIMEBOMB` | `DEV_*` cercano | La onda lo activa |

## 9. Prioridad de producción (jam: entrega dom 15, 19:59 CDMX)

1. **Imprescindibles (MVP):** `PART_PLANK`, `PART_BLOCK`, `PART_CYLINDER`, `JOINT_GLUE`, `DEV_WHEEL_S`, `DEV_FAN`, `CTRL_SEAT`, `PWR_BATTERY_S`, `DEV_CANNON`, `FUSE_FIRE`.
2. **Alta:** `PART_PANEL`, `PART_WALL`, `PART_BALL`, `DEV_WHEEL_L`, `DEV_ROCKET`, `DEV_WING`, `DEV_FLAME`, `DEV_FROST`, `JOINT_HINGE`, `ANCHOR_STAKE`, `FUSE_ICE`, `FUSE_SHOCK`, `FUSE_BOMB`.
3. **Si sobra tiempo:** el resto (`DEV_HOVER`, `DEV_BALLOON`, `CTRL_HEAD`, `DEV_BEAM`, `DEV_TRACK`, uniones avanzadas).

## 10. Entrega por pieza (checklist para Emilio)

- [ ] `SM_<id>` con pivote y escala correctos
- [ ] Colisión `UCX_<id>_NN` convexa
- [ ] Sockets `Attach_*` (y `Fx_*` si aplica) con nombres exactos
- [ ] `MI_<id>` instancia de `M_Part_Master`
- [ ] Icono 256×256
- [ ] (Dispositivos) VFX Niagara + SFX por estado: activo / sin energía / pre-expiración
- [ ] `git lfs lock` antes de editar; commit con `art: <qué>`; pull y push al terminar
