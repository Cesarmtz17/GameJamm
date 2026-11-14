---
title: 09 — Auditoría de fuentes públicas: qué se sabe (y qué no) de construcción y fusión
date: 2026-11-13
tags:
  - research
  - construccion
  - fusion
  - dsl
status: listo
---

# 09 — Auditoría de fuentes públicas: construcción y fusión

> [!info]
> Auditoría de todo lo publicado sobre las mecánicas de construcción y fusión del juego
> de referencia (2026-11-13): qué está documentado, qué descansa en una sola fuente y
> qué falta por completo. Dirigida al diseño del DSL (lenguaje natural → estructura).
> Ver [FUENTES.md](FUENTES.md). Etiquetas: **[fuente única]**, **[solo snippet]** (visto
> solo en resumen de búsqueda, página no leída), **[inferencia]**.

> [!tip] Hallazgo central
> **Las fuentes públicas NO documentan las reglas que deciden si dos piezas se unen,
> cuántas pueden unirse, ni los límites de peso/estabilidad.** Lo que los desarrolladores
> sí publicaron es la arquitectura. Las reglas del builder las definimos nosotros, y se
> calibran con playtests (§8).

## 1. Lo que los desarrolladores sí publicaron (arquitectura)

- **Todo objeto es physics-driven, cada objeto lleva sus propias propiedades**, y el
  movimiento sale de masa, velocidad y constraints — no de código por vehículo.
  *(GameDeveloper — write-up del panel de física de GDC 2024, FUENTES #4 · Shacknews —
  recap del panel, FUENTES #48). Detalle completo en [04-quimica-fisica-gdc.md](04-quimica-fisica-gdc.md).*
- **Los joints son objetos reales del diseño**: la suspensión usa *"a range limit
  constraint and a clean constraint"*; la olla portátil lleva un joint en su base para
  quedarse nivelada en suelo irregular. *(transcripción GDC, ver [04](04-quimica-fisica-gdc.md))*
- La rueda son **3 cuerpos rígidos** (rueda, motor, eje); la masa se calcula sola de
  material+forma; la resistencia al agua usa el área proyectada. *(GDC)*
- **Cue visual del pegamento:** el equipo habló del "glue bulging out" entre objetos
  pegados y lo defendió como la forma más clara de mostrar qué está unido; el sonido de
  pegado es el mismo sea cual sea la unión. *(entrevista oficial de la desarrolladora,
  FUENTES #11 · GamesRadar, [solo snippet], FUENTES #48)*

## 2. Durabilidad de dispositivos (MaxLife) — la mejor tabla disponible

La mejor fuente de catálogo es la base de datos de dispositivos de Kiranico (datamine
comunitario, FUENTES #21): ~27 dispositivos con función de una línea y un valor
**MaxLife**. Leer MaxLife como durabilidad es **[inferencia]**. El patrón es claro:
casi todo comparte **54,000** y las piezas frágiles bajan muchísimo. *(fuente única —
toda esta tabla)*

| Dispositivo | Función (según la fuente) | MaxLife |
|---|---|---|
| Ventilador (Fan) | Hélice: mueve objetos y empuja | **54,000** |
| Ala (Wing) | Sustentación; balance centrado = más distancia | 2,250 |
| Cohete (Rocket) | Thrust fuerte, se quema rápido | **90** |
| Ruedas chica/grande | Tracción; la grande cruza terreno difícil y agua somera | 54,000 c/u |
| Batería / Batería grande | Energía desechable / de un solo uso | 4,500 / 63,000 |
| Asiento de dirección | Dirige el vehículo y activa todos los dispositivos conectados | 54,000 |
| Estabilizador | Se mantiene vertical al activarse | 54,000 |
| Globo (Balloon) | Sube con aire caliente (más caliente = más rápido) | 3,600 |
| Resorte (Spring) | Se comprime y dispara lo que tiene encima | **500** |
| Bomba de tiempo | Explota con timer; su onda activa otros dispositivos | 240 |
| Cañón (Cannon) | Disparos explosivos por intervalos | 54,000 |
| Olla portátil | Un uso | no listado |

La misma tabla cubre piedra flotante, vagón, trineo, estaca, espejo y los emisores.
**La energía cruza todo el catálogo**: ciertos enemigos (los constructos) sueltan
cargas; un resumen de búsqueda dice que **cada dispositivo cuesta 3 cargas
cristalizadas** (baterías grandes: 30), pero es **[solo snippet]** y no verificado.
Que un ventilador drene mucho más rápido que una rueda y que los dispositivos
activos duren ~30 min viene de guías de poca autoridad, sin confirmar.

> [!note] Para GameJamm
> Si usamos durabilidad por pieza, la forma del dato es **"vida larga para piezas
> estructurales, vida corta para consumibles"** (90 vs 54,000 = 600× de separación),
> no gradaciones finas. Tabla completa de dispositivos con consumos:
> [03-mecanicas-vehiculos-dispositivos.md](03-mecanicas-vehiculos-dispositivos.md).

## 3. Esquemas preconstruidos (recetas oficiales) — [solo snippet]

- **Excavadora** = Tabla + 3 Ruedas grandes + Asiento de dirección + Estabilizador +
  Púas.
- **Balsa multiusos** = 2 Tablas + 6 Troncos + Vela + Olla portátil.

Ambos son stubs vistos solo como resúmenes: las listas pueden estar incompletas.
Tabla/Tronco/Vela son **materiales**, no dispositivos — solo los dispositivos llevan
la mecánica de energía.

## 4. Lo que NO está publicado (y hay que definir nosotros)

Ninguna fuente legible describe: condiciones de attach/detach, reglas de conexión por
superficie, taxonomía de tipos de joint, umbrales de peso ni límites de estabilidad
del modo construcción. La guía de construcción de GameSpot no era legible.

Sobre fusión hay poco más: el híbrido **conserva rasgos de ambas partes** (piedra +
rama de árbol = martillo más fuerte); despegar una parte fusionada permite fusionar
otra nueva; el "reciclado de fusión" aplica solo a armas específicas
**[fuente única]**; y los devs dijeron que hicieron *"special adjustments to each
item"* para las armas fusionadas → **tuning individual, no una regla genérica**.

## 5. Dos capas: sistema diseñado vs máquinas de los jugadores

- **Capa diseñada:** el modelo físico, el tuning por objeto, los joints, el cue del
  pegamento, los ajustes por arma, y la decisión de que "todo" sea physics-driven —
  el director técnico lo enmarca como *multiplicative gameplay*; el programador lead:
  *"everything, without exception being physics-driven, is necessary to make
  multiplicative gameplay a reality"*. La frase del equipo — *"a system that makes fun
  things happen"* en vez de "something fun" — es lo más cerca que quedó un
  desarrollador de hablar de emergencia; **la palabra "emergent" no sale en ninguna
  cita de desarrollador**. *(The Verge, cobertura de GDC 2024 [fuente única], FUENTES
  #3 · Digital Trends, FUENTES #5)*
- **Capa emergente (los builds de la comunidad):** los prototipos internos usaban solo
  piezas del predecesor (carro con llantas de engranaje, vapor de paletas, tanque con
  cañón); el prototipo del director técnico era un tronco con llantas que "se movía
  hacia adelante"; la comunidad reutilizó la olla portátil (y su ball-socket) como
  pivote de vehículos y robots; los leads "quedaron blown away" por los builds.
  **Qué builds fueron intencionados y cuáles no no lo dicen las fuentes**
  **[inferencia]**.
- **El límite difuso es el scripting por dispositivo:** el asiento de dirección
  "activa todos los dispositivos conectados a la vez" y el estabilizador "se mantiene
  vertical" — triggers específicos por dispositivo *encima* de la física. El caso de
  los engranajes muestra dónde se rompía: puertas y engranajes eran objetos no-física
  y **atravesaban cajas metálicas** hasta que la caja se hizo physics-driven; el
  choque entre la no-física y la libertad del modo construcción causaba "problemas
  diarios", y el fix fue **física uniforme, no una regla por exploit**. La
  **[inferencia]**: el trigger del dispositivo es script, mientras que su movimiento,
  masa y colisión son física — las fuentes no lo dicen explícitamente.
  *(recap Shacknews [solo snippet] · The Verge [fuente única])*

## 6. Lecciones para un DSL (lenguaje natural → estructura)

1. **Una capa física genérica + un conjunto pequeño de tipos de componentes
   componibles**, no scripts por ítem. Cada tipo declara un contrato corto de
   comportamiento (masa, tipos de joint, interacciones) que el motor aplica igual.
   El equipo "creó un sistema en el que los vehículos se pueden hacer" en vez de un
   programa de vehículo: puentes, vagones y sonido salieron de sistemas de
   componentes. *(GDC · The Verge)*
2. **La validación tiene mejor precedente en generación de contenido con LLM:** un
   estudio de arXiv hace que el modelo emita un JSON DSL propio que un backend
   determinista ejecuta tras validar sintaxis, tipos de componente y rangos, con
   defaults para parámetros faltantes y fallback inofensivo ante salida mala
   ([arXiv 2510.16952](https://arxiv.org/html/2510.16952v1)). El mismo paper advierte
   que su error handling "salvó la mayoría de las salidas malas" → las métricas
   pass/fail **esconden errores**, y no probaron inputs adversarios.
3. Stack razonable **[inferencia]**: catálogo cerrado de componentes + grafo tipado de
   piezas/conexiones con checks de integridad referencial + parámetros clamped +
   default seguro.
4. **Caps de presupuesto/piezas tienen precedente débil**: reglas de torneos
   comunitarios de Besiege y Stormworks
   ([spreadsheet de Besiege Bots](https://docs.google.com/spreadsheets/d/19loFSAuqFSxgLp28y1vSREPj58C0LOPUbf1n_6Un-6w/edit),
   documento comunitario).
5. **Gramáticas de forma** son la referencia académica de construcción por reglas,
   pero ninguna fuente las aplica a builds simulados con física
   ([Waterloo CS-2009-23A](https://cs.uwaterloo.ca/research/tr/2009/CS-2009-23A.pdf),
   no fetchado).
6. Ninguna fuente documenta **schemas de sockets** en el juego de referencia, Besiege,
   Scrap Mechanic, Stormworks ni KSP → los sockets con metadata tipada son propuesta
   nuestra (ver [11-diseno-ensamblado-open-source.md](11-diseno-ensamblado-open-source.md)).

## 7. Edge cases documentados (lo que se rompió)

- **"Entrelazamiento de fusión"** (el nombre comunitario del glitch): un ítem actúa como si estuviera
  fusionado al escudo sin estarlo visualmente; se configura presionando el botón de
  escudo tarde. Habilita exploits de cohete y **duplicación de equipo**; en 1.1.x el
  cohete suelto puede consumirse si se deja mucho tiempo (a diferencia de 1.0); una
  wiki dice que no está parcheado y que sustenta muchos glitches, con uno relacionado
  fixeado en 1.2.0. La lección **[inferencia]**: **el flag "adjunto" y el estado
  físico pueden divergir** — en nuestro ensamblado, el grafo y la física deben
  re-sincronizarse explícitamente. *(página comunitaria de speedruns [fuente única] ·
  otra wiki de la franquicia [solo snippet], FUENTES #48)*
- Pesos calculados resultaron "demasiado pesados" para algunas formas y se ajustaron a
  mano; los objetos kinematic se rompían bajo la física. *(GDC, ver
  [04](04-quimica-fisica-gdc.md))*
- Guía de comunidad: cada pieza extra pesa a los ventiladores y drena la celda; los
  builds chicos rinden mejor **[solo snippet]**.

## 8. Plan de pruebas con jugadores (para fijar los números que faltan)

Cada hueco del §4 se cierra **jugando** (playtest, no extracción de archivos):

| Pregunta | Test |
|---|---|
| Reglas de attach | Fusionar cada par de un set chico de piezas y registrar qué pares aguantan y si importa la superficie de contacto |
| Límite de fusión/piezas | Apilar piezas hasta que el build falle |
| Límite de peso | Añadir tablas hasta que un carro con ruedas deje de moverse a la misma velocidad |
| Estabilidad | Torres altas con y sin estabilizador |
| Consumo de energía | Arrancar ventilador y rueda con batería llena e idéntica carga; contar cargas hasta parar (fuentes contradictorias) |
| ¿Autoconstrucción repara? | Dañar un vehículo construido y correr la autoconstrucción: ¿vuelven las piezas o se reconstruye todo? |
| Emergencia vs script | Construir lo mismo en otro orden y comparar el resultado: si coincide, es composición física pura; la diferencia delata script por dispositivo |

## → Aplicable a GameJamm

1. Las reglas del builder (attach, límites, pesos, consumos) son **decisiones
   nuestras** — no hay dato oficial que copiar; calibrar con el plan de pruebas §8.
2. Durabilidad por pieza con la forma "estructural 54,000 vs consumible 90": una sola
   brecha de orden de magnitud, no gradaciones finas.
3. El grafo de ensamblado y la física deben re-sincronizarse explícitamente (lección
   del entrelazamiento de fusión).
4. DSL validado contra schema antes de spawn (arXiv) + caps de presupuesto (torneos
   Besiege) — implementación concreta en
   [11-diseno-ensamblado-open-source.md](11-diseno-ensamblado-open-source.md) §6.
