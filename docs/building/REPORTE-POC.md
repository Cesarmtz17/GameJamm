---
title: Reporte del POC de construccion por lenguaje natural
tags:
  - building
  - pruebas
status: generado
---

# Reporte del POC: frase -> modelo de decision -> construccion

> [!info]
> Archivo GENERADO por el comando de consola `Jam.Test` (en PIE, `Source/GameJamm/Building/JamPlayConsole.cpp`). No editar a mano: volver a correr el comando.
> Fecha: 2026-11-14 02:52. Bateria: interna (aisladas + frases del POC). Cada caso se construye delante del jugador y se mide durante 12 s.

**Resultado: 7 de 8 pruebas aisladas pasan. Frases: 30 de 42 pasan TODO (construida + decision + coherencia + utilizable).**

| Medida (frases) | Resultado |
|---|---|
| Decision correcta (piezas pedidas, prohibidas y cantidades) | 42 de 42 |
| Coherencia media (0-100) | 97.5 |
| Coherencia 85 o mas | 39 de 42 |
| Utilizable (todos sus canales responden a la entrada simulada) | 32 de 42 |

## 1. Mecanicas verificadas aisladas

Una mecanica por prueba, con receta fija (sin modelo de decision) y una metrica propia.

| Mecanica | Prueba | Piezas | Medido | Veredicto |
|---|---|---|---|---|
| `Roller` | Rueda sola (raiz): debe rodar de pie | 1 | rodo 539 cm (max 328 cm/s), inclinacion final del eje 0.00 (0 = de pie) | PASA |
| `head` | Cabeza + rayo: debe girar hacia un objetivo que se mueve de lado | 3 | error medio de punteria 12 grados; la cabeza barrio 74 grados siguiendo al objetivo | PASA |
| `hinge` | Bisagra + hoja: al empujarla debe girar y seguir unida | 3 | la hoja giro hasta 38 grados; separacion maxima de la union 0.2 cm | PASA |
| `stake` | Estaca: un bloque con ventilador encendido no debe moverse | 3 | con el ventilador encendido se movio 2.9 cm | PASA |
| `light` | Foco: debe encender una luz | 2 | foco encendido (componente de luz visible con intensidad > 0) | PASA |
| `stabilizer` | Estabilizador: un dron golpeado de lado debe enderezarse | 4 | tras el golpe se inclino hasta up.z=0.94 y termino en up.z=1.00 (1 = derecho) | PASA |
| `cannon` | Cañon: la bola debe explotar al impactar y alcanzar al objetivo | 2 | 3 bolas; explosiones que alcanzaron al objetivo: 0; objetivo empujado 20 cm | FALLA |
| `battery` | Bateria grande: el lanzallamas gasta la bateria (80) antes que la barra del jugador | 3 | bateria de la construccion 80.0 -> 59.1; barra del jugador minima 100.0 | PASA |

## 2. Frases completas

Camino completo: frase -> modelo de decision (piezas y cantidades) -> compositor con sentido -> validador de coherencia -> ensamblador, con un cubo-objetivo de 20 kg a 3.5 m al frente.

| # | Frase | Backend | Decision y receta (estado del spawner) | Piezas | Decision | Coherencia | Utilizable (canales) | Veredicto |
|---|---|---|---|---|---|---|---|---|
| 1 | un carro con cohete | Jev | `OK 8 parts (jev pieces=[Plank x1, Wheel x4, Rocket x1, Seat x1, Cylinder x1] coh=100) 'Construccion'` | 8 | ok  | 100 | NO: MOVER NO (W: 97 cm al frente); GIRAR ok (D: 30 grados); TURBO ok (Espacio: cohete/bomba). Visto: termino volcada | FALLA |
| 2 | un tanque | Jev | `OK 8 parts (jev pieces=[Plank x1, Wheel x4, Seat x1, Cannon x1, Panel x1] aviso=[el escudo va atras o a los lados para no tapar el arma] coh=100) 'Construccion'` | 8 | ok  | 100 (el escudo va atras o a los lados para no tapar el arma) | NO: MOVER NO (W: 53 cm al frente); GIRAR ok (D: 32 grados); FUEGO ok (clic: 1 armas; su efecto toco al objetivo; al soltar 0). Visto: 1 bolas | FALLA |
| 3 | un cohete | Jev | `OK 7 parts (jev pieces=[Cylinder x1, Plank x3, Rocket x1, Seat x1, Stabilizer x1] aviso=[chasis mas largo: +2 tablas para que todo quepa con sentido] coh=98 centro de masa al borde (largo);) 'Construccion'` | 7 | ok  | 98 (chasis mas largo: +2 tablas para que todo quepa con sentido) | SI: TURBO ok (Espacio: cohete/bomba); ALTURA ok (subio 344 cm). Visto: termino volcada | PASA |
| 4 | un avion con alas y ventilador | Jev | `OK 5 parts (jev pieces=[Plank x1, Fan x1, Seat x1, Wing x2] coh=100) 'Construccion'` | 5 | ok  | 100 | SI: MOVER ok (W: 461 cm al frente); GIRAR ok (D: 37 grados). Visto: efecto sobre el objetivo x1; termino volcada | PASA |
| 5 | una torreta con laser | Jev | `OK 5 parts (jev pieces=[Block x3, Head x1, Beam x1] coh=100) 'Construccion'` | 5 | ok  | 100 | SI: FUEGO ok (clic: 1 armas; no toco al objetivo; al soltar 0); sin asiento: mando desde la pieza | PASA |
| 6 | un globo aerostatico con fuego debajo | Jev | `OK 9 parts (jev pieces=[Plank x3, Balloon x3, Fan x1, Seat x1, Flamethrower x1] aviso=[agregue 2 Balloon para que despegue; chasis mas largo: +2 tablas para que todo quepa con sentido] coh=98) 'Construccion'` | 9 | ok  | 98 (agregue 2 Balloon para que despegue; chasis mas largo: +2 tablas para que todo quepa con sentido) | SI: MOVER ok (W: 279 cm al frente); GIRAR ok (D: 39 grados); FUEGO ok (clic: 1 armas; no toco al objetivo; al soltar 0); ALTURA ok (subio 6441 cm) | PASA |
| 7 | un carro con lanzallamas | Jev | `OK 10 parts (jev pieces=[Plank x3, Wheel x4, Seat x1, Flamethrower x1, Cylinder x1] aviso=[chasis mas largo: +2 tablas para que todo quepa con sentido] coh=100) 'Construccion'` | 10 | ok  | 100 (chasis mas largo: +2 tablas para que todo quepa con sentido) | SI: MOVER ok (W: 204 cm al frente); GIRAR ok (D: 24 grados); FUEGO ok (clic: 1 armas; su efecto toco al objetivo; al soltar 0). Visto: efecto sobre el objetivo x25; termino volcada | PASA |
| 8 | un hovercraft con vela y emisor de hielo | Jev | `OK 7 parts (jev pieces=[Plank x1, HoverStone x2, Seat x1, Frost x1, Sail x1, Girder x1] coh=69 GRAVE sustentacion descentrada (53 cm del centro de masa);) 'Construccion'` | 7 | ok  | 69 (No quedo bien: sustentacion descentrada (53 cm del centro de masa)) | SI: FUEGO ok (clic: 1 armas; su efecto toco al objetivo; al soltar 0); ALTURA ok (subio 37 cm). Visto: efecto sobre el objetivo x1; objetivo congelado; objetivo empujado 31 cm | FALLA |
| 9 | un carro con una bomba | Jev | `OK 7 parts (jev pieces=[Plank x1, Wheel x4, Seat x1, Bomb x1] coh=100) 'Construccion'` | 7 | ok  | 100 | SI: MOVER ok (W: 171 cm al frente); GIRAR ok (D: 88 grados); TURBO ok (Espacio: cohete/bomba) | PASA |
| 10 | un puente largo de tablas | Jev | `OK 2 parts (jev pieces=[Plank x1, Girder x1] coh=100) 'Construccion'` | 2 | ok  | 100 | NO: estructura: se movio 52 cm, se cayo. Visto: objetivo empujado 75 cm | FALLA |
| 11 | un trampolin con resorte que lance cosas | Jev | `OK 3 parts (jev pieces=[Plank x1, Spring x1, Launcher x1] coh=100) 'Construccion'` | 3 | ok  | 100 | SI: C enciende 2 dispositivos. Visto: 8 bolas | PASA |
| 12 | un velero con ruedas | Jev | `OK 11 parts (jev pieces=[Plank x3, Wheel x4, Seat x1, Sail x1, Girder x1, Rod x1] aviso=[chasis mas largo: +2 tablas para que todo quepa con sentido] coh=100) 'Construccion'` | 11 | ok  | 100 (chasis mas largo: +2 tablas para que todo quepa con sentido) | SI: MOVER ok (W: 264 cm al frente); GIRAR ok (D: 82 grados). Visto: termino volcada | PASA |
| 13 | una fortaleza con cañones | Jev | `OK 3 parts (jev pieces=[Wall x1, Cannon x1, Block x1] coh=96 alto, puede caerse: centro de masa a 118 cm sobre 60 de ancho;) 'Construccion'` | 3 | ok  | 96 | SI: FUEGO ok (clic: 1 armas; su efecto toco al objetivo; al soltar 0); sin asiento: mando desde la pieza. Visto: 1 bolas | PASA |
| 14 | una moto | Jev | `OK 5 parts (jev pieces=[Plank x1, Wheel x2, Seat x1, Stabilizer x1] coh=100) 'Construccion'` | 5 | ok  | 100 | SI: MOVER ok (W: 243 cm al frente); GIRAR ok (D: 39 grados). Visto: objetivo empujado 31 cm | PASA |
| 15 | un trineo con ventilador | Jev | `OK 8 parts (jev pieces=[Sled x1, Plank x3, RailSkid x2, Fan x1, Seat x1] aviso=[chasis mas largo: +2 tablas para que todo quepa con sentido] coh=100) 'Construccion'` | 8 | ok  | 100 (chasis mas largo: +2 tablas para que todo quepa con sentido) | SI: MOVER ok (W: 484 cm al frente); GIRAR ok (D: 23 grados). Visto: objetivo empujado 789 cm | PASA |
| 16 | un dron con un faro | Jev | `OK 6 parts (jev pieces=[Plank x1, Fan x2, Seat x1, Light x1, Stabilizer x1] coh=100) 'Construccion'` | 6 | ok  | 100 | SI: ALTURA ok (subio 631 cm) | PASA |
| 17 | una catapulta | Jev | `OK 4 parts (jev pieces=[Block x1, Spring x1, Ball x1, Girder x1] coh=100) 'Construccion'` | 4 | ok  | 100 | SI: C enciende 1 dispositivos. Visto: efecto sobre el objetivo x1; termino volcada | PASA |
| 18 | un aspersor de fuego que gire | Jev | `OK 4 parts (jev pieces=[Block x1, Turntable x1, Flamethrower x1, Hydrant x1] coh=100) 'Construccion'` | 4 | ok  | 100 | SI: FUEGO ok (clic: 2 armas; su efecto toco al objetivo; al soltar 0); sin asiento: mando desde la pieza. Visto: efecto sobre el objetivo x4 | PASA |
| 19 | una rueda sola que ruede quemando todo | Jev | `OK 7 parts (jev pieces=[Plank x1, Wheel x4, Seat x1, Flamethrower x1] coh=100) 'Construccion'` | 7 | ok  | 100 | SI: MOVER ok (W: 292 cm al frente); GIRAR ok (D: 29 grados); FUEGO ok (clic: 1 armas; su efecto toco al objetivo; al soltar 0). Visto: efecto sobre el objetivo x24; termino volcada | PASA |
| 20 | un planeador | Jev | `OK 6 parts (jev pieces=[Plank x1, Rocket x1, Seat x1, Wing x2, Girder x1] coh=100) 'Construccion'` | 6 | ok  | 100 | SI: TURBO ok (Espacio: cohete/bomba). Visto: objetivo empujado 107 cm | PASA |
| 21 | una torreta con cabeza que apunte sola | Jev | `OK 3 parts (jev pieces=[Block x1, Head x1, Cannon x1] skipped=[Plank x1] coh=100) 'Construccion'` | 3 | ok no todo cupo | 100 | SI: FUEGO ok (clic: 1 armas; su efecto toco al objetivo; al soltar 0); sin asiento: mando desde la pieza. Visto: efecto sobre el objetivo x1; objetivo empujado 177 cm; 1 bolas | PASA |
| 22 | un carro con manguera de agua | Jev | `OK 7 parts (jev pieces=[Plank x1, Wheel x4, Seat x1, Hydrant x1] coh=100) 'Construccion'` | 7 | ok  | 100 | SI: MOVER ok (W: 280 cm al frente); GIRAR ok (D: 65 grados); FUEGO ok (clic: 1 armas; su efecto toco al objetivo; al soltar 0). Visto: efecto sobre el objetivo x25; objetivo empujado 64 cm | PASA |
| 23 | un tanque electrico que de toques | Jev | `OK 12 parts (jev pieces=[Plank x3, Wheel x4, Seat x1, Cannon x1, Shock x1, Panel x1, Battery x1] skipped=[Spikes x1] aviso=[chasis mas largo: +2 tablas para que todo quepa con sentido; el escudo va atras o a los lados para no tapar el arma] coh=100) 'Construccion'` | 12 | ok no todo cupo | 100 (chasis mas largo: +2 tablas para que todo quepa con sentido; el escudo va atras o a los lados para no tapar el arma) | NO: MOVER NO (W: 28 cm al frente); GIRAR ok (D: 64 grados); FUEGO ok (clic: 2 armas; su efecto toco al objetivo; al soltar 0). Visto: 1 bolas; termino volcada | FALLA |
| 24 | un carro con turbina y bateria | Jev | `OK 8 parts (jev pieces=[Plank x1, Wheel x4, Turbine x1, Seat x1, Battery x1] coh=100) 'Construccion'` | 8 | ok  | 100 | NO: MOVER ok (W: 218 cm al frente); GIRAR NO (D: 9 grados). Visto: objetivo empujado 35 cm | FALLA |
| 25 | un carro levitante con piedras antigravedad | Jev | `OK 6 parts (jev pieces=[Plank x1, HoverStone x2, Seat x1, Ball x1, Stabilizer x1] coh=69 GRAVE sustentacion descentrada (36 cm del centro de masa);) 'Construccion'` | 6 | ok  | 69 (No quedo bien: sustentacion descentrada (36 cm del centro de masa)) | NO: ALTURA NO (subio 1 cm) | FALLA |
| 26 | un dirigible con cañon | Jev | `OK 8 parts (jev pieces=[Plank x3, Balloon x3, Seat x1, Cannon x1] aviso=[agregue 2 Balloon para que despegue; chasis mas largo: +2 tablas para que todo quepa con sentido] coh=69 GRAVE sustentacion descentrada (74 cm del centro de masa);) 'Construccion'` | 8 | ok  | 69 (agregue 2 Balloon para que despegue; chasis mas largo: +2 tablas para que todo quepa con sentido. No quedo bien: sustentacion descentrada (74 cm del centro de masa)) | SI: FUEGO ok (clic: 1 armas; su efecto toco al objetivo; al soltar 0); ALTURA ok (subio 6548 cm). Visto: 1 bolas | FALLA |
| 27 | una torreta anclada al suelo con una bisagra | Jev | `OK 3 parts (jev pieces=[Block x1, Cannon x1, Stake x1] skipped=[Wall x1] coh=100) 'Construccion'` | 3 | ok no todo cupo | 100 | SI: FUEGO ok (clic: 1 armas; su efecto toco al objetivo; al soltar 0); sin asiento: mando desde la pieza. Visto: 1 bolas | PASA |
| 28 | algo que ruede y queme todo | Jev | `OK 8 parts (jev pieces=[Plank x1, Wheel x4, Seat x1, Flamethrower x1, Bomb x1] coh=100) 'Construccion'` | 8 | ok  | 100 | SI: MOVER ok (W: 235 cm al frente); GIRAR ok (D: 73 grados); FUEGO ok (clic: 1 armas; su efecto toco al objetivo; al soltar 0); TURBO ok (Espacio: cohete/bomba). Visto: efecto sobre el objetivo x21 | PASA |
| 29 | hazme un carrito bien loco con lanzallamas, un globo, cohete, luces y una bateria extra | Jev | `OK 14 parts (jev pieces=[Plank x3, Wheel x4, Balloon x1, Rocket x1, Seat x1, Flamethrower x1, Light x1, Cylinder x1, Battery x1] aviso=[chasis mas largo: +2 tablas para que todo quepa con sentido] coh=99) 'Construccion'` | 14 | ok  | 99 (chasis mas largo: +2 tablas para que todo quepa con sentido) | NO: MOVER NO (W: 6 cm al frente); GIRAR NO (D: 10 grados); FUEGO ok (clic: 1 armas; su efecto toco al objetivo; al soltar 0); TURBO ok (Espacio: cohete/bomba); ALTURA ok (subio 814 cm). Visto: efecto sobre el objetivo x7 | FALLA |
| 30 | arma un carro con 25 llantas | Jev | `OK 33 parts (jev pieces=[Plank x7, Wheel x25, Seat x1] aviso=[chasis mas largo: +6 tablas para que todo quepa con sentido] coh=100) 'Construccion'` | 33 | ok Wheel 25/25 | 100 (chasis mas largo: +6 tablas para que todo quepa con sentido) | NO: MOVER ok (W: 387 cm al frente); GIRAR ok (D: 139 grados); sin entrada se fue 155 cm. Visto: efecto sobre el objetivo x2; objetivo empujado 19178 cm; 33 piezas a 20 fps | FALLA |
| 31 | una catapulta | Jev | `OK 4 parts (jev pieces=[Block x1, Spring x1, Ball x1, Girder x1] coh=100) 'Construccion'` | 4 | ok Spring 1/1 | 100 | SI: C enciende 1 dispositivos. Visto: efecto sobre el objetivo x1; termino volcada | PASA |
| 32 | un puente con 10 tablas | Jev | `OK 10 parts (jev pieces=[Plank x10] coh=100) 'Construccion'` | 10 | ok Plank 10/10 | 100 | SI: estructura: se movio 10 cm, en pie | PASA |
| 33 | un cohete con 3 propulsores | Jev | `OK 9 parts (jev pieces=[Cylinder x1, Plank x3, Rocket x3, Seat x1, Stabilizer x1] aviso=[chasis mas largo: +2 tablas para que todo quepa con sentido] coh=99) 'Construccion'` | 9 | ok Rocket 3/3 | 99 (chasis mas largo: +2 tablas para que todo quepa con sentido) | SI: TURBO ok (Espacio: cohete/bomba); ALTURA ok (subio 496 cm). Visto: termino volcada | PASA |
| 34 | un tanque con 4 cañones | Jev | `OK 13 parts (jev pieces=[Plank x3, Wheel x4, Seat x1, Cannon x4, Panel x1] aviso=[chasis mas largo: +2 tablas para que todo quepa con sentido; el escudo va atras o a los lados para no tapar el arma] coh=100) 'Construccion'` | 13 | ok Cannon 4/4 | 100 (chasis mas largo: +2 tablas para que todo quepa con sentido; el escudo va atras o a los lados para no tapar el arma) | NO: MOVER NO (W: 81 cm al frente); GIRAR ok (D: 53 grados); FUEGO ok (clic: 4 armas; su efecto toco al objetivo; al soltar 0). Visto: 4 bolas | FALLA |
| 35 | una torre de 12 bloques | Jev | `OK 12 parts (jev pieces=[Block x12] coh=96 alto, puede caerse: centro de masa a 360 cm sobre 60 de ancho;) 'Construccion'` | 12 | ok Block 12/12 | 96 | SI: estructura: se movio 0 cm, en pie | PASA |
| 36 | un carro con seis ruedas y dos lanzallamas | Jev | `OK 10 parts (jev pieces=[Plank x1, Wheel x6, Seat x1, Flamethrower x2] coh=100) 'Construccion'` | 10 | ok Wheel 6/6, Flamethrower 2/2 | 100 | SI: MOVER ok (W: 260 cm al frente); GIRAR ok (D: 50 grados); FUEGO ok (clic: 2 armas; su efecto toco al objetivo; al soltar 0). Visto: efecto sobre el objetivo x48; objetivo empujado 146 cm; termino volcada | PASA |
| 37 | un carro con una viga larga al frente como lanza | Jev | `OK 10 parts (jev pieces=[Plank x3, Wheel x4, Seat x1, Spikes x1, Girder x1] aviso=[chasis mas largo: +2 tablas para que todo quepa con sentido] coh=100) 'Construccion'` | 10 | ok  | 100 (chasis mas largo: +2 tablas para que todo quepa con sentido) | NO: MOVER NO (W: 90 cm al frente); GIRAR ok (D: 52 grados). Visto: termino volcada | FALLA |
| 38 | un tanque con plataforma giratoria | Jev | `OK 8 parts (jev pieces=[Plank x1, Wheel x4, Seat x1, Turntable x1, Cannon x1] coh=100) 'Construccion'` | 8 | ok  | 100 | SI: MOVER ok (W: 277 cm al frente); GIRAR ok (D: 74 grados); FUEGO ok (clic: 1 armas; su efecto toco al objetivo; al soltar 0). Visto: 1 bolas | PASA |
| 39 | un trineo con vela | Jev | `OK 10 parts (jev pieces=[Sled x1, Plank x3, RailSkid x2, Seat x1, Sail x1, Girder x1, Rod x1] aviso=[chasis mas largo: +2 tablas para que todo quepa con sentido] coh=100) 'Construccion'` | 10 | ok  | 100 (chasis mas largo: +2 tablas para que todo quepa con sentido) | SI: C enciende 1 dispositivos. Visto: objetivo empujado 232 cm | PASA |
| 40 | un carro con ventilador y una bateria | Jev | `OK 8 parts (jev pieces=[Plank x1, Wheel x4, Fan x1, Seat x1, Battery x1] coh=100) 'Construccion'` | 8 | ok  | 100 | SI: MOVER ok (W: 369 cm al frente); GIRAR ok (D: 61 grados). Visto: objetivo empujado 357 cm; termino volcada | PASA |
| 41 | un muro defensivo con escudo | Jev | `OK 3 parts (jev pieces=[Wall x1, Panel x1, Block x1] coh=100) 'Construccion'` | 3 | ok  | 100 | SI: estructura: se movio 0 cm, en pie | PASA |
| 42 | un carro con resorte que salte | Jev | `OK 7 parts (jev pieces=[Plank x1, Wheel x4, Spring x1, Seat x1] coh=100) 'Construccion'` | 7 | ok  | 100 | SI: MOVER ok (W: 123 cm al frente); GIRAR ok (D: 87 grados). Visto: 1 bolas | PASA |

## Como leerlo

- **Backend**: `Jev` = decidio el modelo de decision; `respaldo` = fallo la llamada y se uso el generador por palabras clave.
- **Decision**: `ok` si estan las piezas exigidas (`must`), no estan las prohibidas (`forbid`) y las cantidades pedidas coinciden (`tipo tengo/pedido`).
- **Coherencia**: puntaje 0-100 de `UJamAssembler::Coherence` (conexion, solapes, simetria, apoyo, empuje, tiro y aire libres, centro de masa, potencia). 85 o mas pasa; 70-84 se construye con aviso; menos de 70 se rearma. Entre parentesis, lo que se le dijo al jugador.
- **Utilizable**: el jugador toma el mando (asiento) y se simula la entrada por el mismo camino que el teclado. Fases: 2-3.5 s sin entrada (las armas deben estar quietas), 3.5-6 s clic (FUEGO), 6.5-9 s W (MOVER: mas de 100 cm al frente), 9-10.5 s W+D (GIRAR: mas de 15 grados), 10.5 s Espacio (TURBO: cohete o bomba). ALTURA = subio mas de 30 cm. Solo se exigen los canales que la construccion tiene por sus piezas (o los que la frase pide).
- **Veredicto**: PASA = construida + decision + coherencia 85 o mas + utilizable.
