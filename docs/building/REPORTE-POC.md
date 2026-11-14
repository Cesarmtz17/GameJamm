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
> Fecha: 2026-11-14 01:21. Cada caso se construye delante del jugador y se mide durante 10 s.

**Resultado: 5 de 8 pruebas aisladas y 40 de 42 frases pasan.**

## 1. Mecanicas verificadas aisladas

Una mecanica por prueba, con receta fija (sin modelo de decision) y una metrica propia.

| Mecanica | Prueba | Piezas | Medido | Veredicto |
|---|---|---|---|---|
| `Roller` | Rueda sola (raiz): debe rodar de pie | 1 | rodo 829 cm (max 329 cm/s), inclinacion final del eje 0.00 (0 = de pie) | PASA |
| `head` | Cabeza + rayo: debe girar hacia un objetivo que se mueve de lado | 3 | error medio de punteria 82 grados; la cabeza barrio 6 grados siguiendo al objetivo | FALLA |
| `hinge` | Bisagra + hoja: al empujarla debe girar y seguir unida | 3 | la hoja giro hasta 38 grados; separacion maxima de la union 0.2 cm | PASA |
| `stake` | Estaca: un bloque con ventilador encendido no debe moverse | 3 | con el ventilador encendido se movio 2.6 cm | PASA |
| `light` | Foco: debe encender una luz | 2 | foco encendido (componente de luz visible con intensidad > 0) | PASA |
| `stabilizer` | Estabilizador: un dron golpeado de lado debe enderezarse | 4 | tras el golpe se inclino hasta up.z=0.99 y termino en up.z=1.00 (1 = derecho) | FALLA |
| `cannon` | Cañon: la bola debe explotar al impactar y alcanzar al objetivo | 2 | 4 bolas; explosiones que alcanzaron al objetivo: 0; objetivo empujado 20 cm | FALLA |
| `battery` | Bateria grande: el lanzallamas gasta la bateria (80) antes que la barra del jugador | 3 | bateria de la construccion 80.0 -> 64.0; barra del jugador minima 100.0 | PASA |

## 2. Frases completas

Camino completo: frase -> modelo de decision -> receta -> ensamblador, con un cubo-objetivo de 20 kg a 3.5 m al frente. "PASA" = se construyo y se midio al menos una mecanica (o es una estructura estatica sin dispositivos). No juzga si la plantilla elegida es la que una persona esperaria: eso se lee en la columna de decision.

| # | Frase | Backend | Decision y receta (estado del spawner) | Piezas | Encendidas | Mecanica observada | Mecanica verificada aislada | Veredicto |
|---|---|---|---|---|---|---|---|---|
| 1 | un carro con cohete | Jev | `OK 8 parts (jev pieces=[Plank x1, Seat x1, Wheel x4, Rocket x1, Cylinder x1]) 'Construccion'` | 8 | 5 | se movio 1582 cm (max 1073 cm/s); subio 131 cm; algo gira a 14.0 rad/s; objetivo empujado 374 cm | - | PASA |
| 2 | un tanque | Jev | `OK 7 parts (jev pieces=[Plank x1, Seat x1, Wheel x4, Cannon x1]) 'Construccion'` | 7 | 5 | se movio 720 cm (max 287 cm/s); algo gira a 11.9 rad/s; disparo/lanzo 4 bolas | cannon: FALLA | PASA |
| 3 | un cohete | Jev | `OK 4 parts (jev pieces=[Cylinder x1, Seat x1, Rocket x1, Stabilizer x1]) 'Construccion'` | 4 | 2 | se movio 793 cm (max 1106 cm/s); subio 149 cm; algo gira a 22.4 rad/s | stabilizer: FALLA | PASA |
| 4 | un avion con alas y ventilador | Jev | `OK 5 parts (jev pieces=[Plank x1, Seat x1, Fan x1, Wing x2]) 'Construccion'` | 5 | 3 | se movio 456 cm (max 500 cm/s); subio 303 cm; algo gira a 7.1 rad/s; objetivo empujado 75 cm | - | PASA |
| 5 | una torreta con laser | Jev | `OK 4 parts (jev pieces=[Block x1, Head x1, Cannon x1, Beam x1]) 'Construccion'` | 4 | 3 | disparo/lanzo 4 bolas | head: FALLA; cannon: FALLA | PASA |
| 6 | un globo aerostatico con fuego debajo | Jev | `OK 5 parts (jev pieces=[Plank x1, Seat x1, Fan x1, Flamethrower x1, Balloon x1]) 'Construccion'` | 5 | 3 | se movio 897 cm (max 431 cm/s); subio 1035 cm; algo gira a 4.1 rad/s; su efecto toco al objetivo (18) | - | PASA |
| 7 | un carro con lanzallamas | Jev | `OK 7 parts (jev pieces=[Plank x1, Seat x1, Wheel x4, Flamethrower x1]) 'Construccion'` | 7 | 5 | se movio 959 cm (max 283 cm/s); algo gira a 11.8 rad/s; su efecto toco al objetivo (24) | - | PASA |
| 8 | un hovercraft con vela y emisor de hielo | Jev | `OK 7 parts (jev pieces=[Plank x1, Seat x1, Fan x1, Sail x1, HoverStone x2, Frost x1]) 'Construccion'` | 7 | 5 | se movio 325 cm (max 346 cm/s); subio 207 cm; algo gira a 12.0 rad/s; su efecto toco al objetivo (10); objetivo congelado | - | PASA |
| 9 | un carro con una bomba | Jev | `OK 7 parts (jev pieces=[Plank x1, Seat x1, Wheel x4, Bomb x1]) 'Construccion'` | 7 | 6 | se movio 1190 cm (max 1588 cm/s); subio 64 cm; algo gira a 22.7 rad/s; objetivo empujado 545 cm; la bomba exploto | - | PASA |
| 10 | un puente largo de tablas | Jev | `OK 1 parts (jev pieces=[Plank x1]) 'Construccion'` | 1 | 0 | estructura estatica, se mantuvo en pie | - | PASA |
| 11 | un trampolin con resorte que lance cosas | Jev | `OK 3 parts (jev pieces=[Plank x1, Spring x1, Girder x1]) 'Construccion'` | 3 | 1 | algo gira a 2.5 rad/s; disparo/lanzo 4 bolas; objetivo empujado 62 cm | - | PASA |
| 12 | un velero con ruedas | Jev | `OK 7 parts (jev pieces=[Plank x1, Seat x1, Wheel x4, Sail x1]) 'Construccion'` | 7 | 5 | se movio 145 cm (max 125 cm/s); algo gira a 11.5 rad/s | - | PASA |
| 13 | una fortaleza con cañones | Jev | `OK 3 parts (jev pieces=[Block x1, Cannon x1, Wall x1]) 'Construccion'` | 3 | 1 | se movio 174 cm (max 210 cm/s); algo gira a 2.8 rad/s; disparo/lanzo 3 bolas | cannon: FALLA | PASA |
| 14 | una moto | Jev | `OK 10 parts (jev pieces=[Plank x2, Seat x1, Wheel x4, BigWheel x2, Stabilizer x1]) 'Construccion'` | 10 | 7 | se movio 1170 cm (max 314 cm/s); algo gira a 11.8 rad/s | stabilizer: FALLA | PASA |
| 15 | un trineo con ventilador | Jev | `OK 4 parts (jev pieces=[Sled x1, Seat x1, Fan x1, Plank x1]) 'Construccion'` | 4 | 1 | se movio 579 cm (max 272 cm/s); objetivo empujado 527 cm | - | PASA |
| 16 | un dron con un faro | Jev | `OK 6 parts (jev pieces=[Plank x1, Seat x1, Fan x2, Stabilizer x1, Light x1]) 'Construccion'` | 6 | 4 | se movio 113 cm (max 330 cm/s); subio 631 cm | light: PASA; stabilizer: FALLA | PASA |
| 17 | una catapulta | Jev | `OK 5 parts (jev pieces=[Plank x1, Spring x1, Ball x1, Girder x1, Block x1]) 'Construccion'` | 5 | 1 | disparo/lanzo 4 bolas | - | PASA |
| 18 | un aspersor de fuego que gire | Jev | `OK 3 parts (jev pieces=[Block x1, Turntable x1, Flamethrower x1]) 'Construccion'` | 3 | 2 | algo gira a 4.2 rad/s | - | PASA |
| 19 | una rueda sola que ruede quemando todo | Jev | `OK 8 parts (jev pieces=[Plank x1, Seat x1, Wheel x4, Turntable x1, Flamethrower x1]) 'Construccion'` | 8 | 6 | se movio 1001 cm (max 286 cm/s); algo gira a 12.2 rad/s; su efecto toco al objetivo (15) | - | PASA |
| 20 | un planeador | Jev | `OK 6 parts (jev pieces=[Plank x1, Seat x1, Rocket x1, Wing x2, Stabilizer x1]) 'Construccion'` | 6 | 4 | se movio 2431 cm (max 2107 cm/s); subio 5056 cm | stabilizer: FALLA | PASA |
| 21 | una torreta con cabeza que apunte sola | Jev | `OK 3 parts (jev pieces=[Block x1, Head x1, Cannon x1]) 'Construccion'` | 3 | 2 | disparo/lanzo 1 bolas | head: FALLA; cannon: FALLA | PASA |
| 22 | un carro con manguera de agua | Jev | `OK 7 parts (jev pieces=[Plank x1, Seat x1, Wheel x4, Hydrant x1]) 'Construccion'` | 7 | 5 | se movio 901 cm (max 292 cm/s); algo gira a 12.1 rad/s; su efecto toco al objetivo (6) | - | PASA |
| 23 | un tanque electrico que de toques | Jev | `OK 9 parts (jev pieces=[Plank x1, Seat x1, Wheel x4, Cannon x1, Shock x1, Battery x1]) 'Construccion'` | 9 | 8 | se movio 379 cm (max 221 cm/s); algo gira a 12.9 rad/s; disparo/lanzo 2 bolas; su efecto toco al objetivo (2) | cannon: FALLA; battery: PASA | PASA |
| 24 | un carro con turbina y bateria | Jev | `OK 9 parts (jev pieces=[Plank x1, Seat x1, Wheel x4, Turbine x1, Battery x1, BigBattery x1]) 'Construccion'` | 9 | 5 | se movio 660 cm (max 477 cm/s); subio 143 cm; algo gira a 53.1 rad/s; objetivo empujado 525 cm | battery: PASA | PASA |
| 25 | un carro levitante con piedras antigravedad | Jev | `OK 6 parts (jev pieces=[Plank x1, Seat x1, HoverStone x2, Stabilizer x1, Ball x1]) 'Construccion'` | 6 | 3 | nada medible | stabilizer: FALLA | FALLA |
| 26 | un dirigible con cañon | Jev | `OK 4 parts (jev pieces=[Plank x1, Seat x1, Cannon x1, Balloon x1]) 'Construccion'` | 4 | 2 | se movio 508 cm (max 558 cm/s); subio 1280 cm; disparo/lanzo 1 bolas; su efecto toco al objetivo (1); objetivo empujado 63 cm | cannon: FALLA | PASA |
| 27 | una torreta anclada al suelo con una bisagra | Jev | `OK 5 parts (jev pieces=[Block x1, Cannon x1, Stake x1, Hinge x1, Plank x1]) 'Construccion'` | 5 | 3 | disparo/lanzo 4 bolas; su efecto toco al objetivo (1); objetivo empujado 88 cm | hinge: PASA; stake: PASA; cannon: FALLA | PASA |
| 28 | algo que ruede y queme todo | Jev | `OK 8 parts (jev pieces=[Plank x1, Seat x1, Wheel x4, Flamethrower x1, Bomb x1]) 'Construccion'` | 8 | 7 | se movio 552 cm (max 1525 cm/s); algo gira a 23.4 rad/s; su efecto toco al objetivo (23); objetivo empujado 1089 cm; la bomba exploto | - | PASA |
| 29 | hazme un carrito bien loco con lanzallamas, un globo, cohete, luces y una bateria extra | Jev | `OK 11 parts (jev pieces=[Plank x1, Seat x1, Wheel x4, Rocket x1, Flamethrower x1, Balloon x1, Battery x1, Light x1] skipped=[BigBattery x1, Cylinder x1]) 'Construccion'` | 11 | 8 | se movio 1488 cm (max 888 cm/s); subio 1151 cm; algo gira a 11.3 rad/s; su efecto toco al objetivo (7) | light: PASA; battery: PASA | PASA |
| 30 | arma un carro con 25 llantas | Jev | `OK 31 parts (jev pieces=[Plank x5, Seat x1, Wheel x25]) 'Construccion'` | 31 | 25 | se movio 947 cm (max 8606 cm/s); subio 2010 cm; algo gira a 204.3 rad/s; objetivo empujado 11825 cm; piezas pedidas: Wheel 25/25 | - | PASA |
| 31 | una catapulta | Jev | `OK 5 parts (jev pieces=[Plank x1, Spring x1, Ball x1, Girder x1, Block x1]) 'Construccion'` | 5 | 1 | disparo/lanzo 4 bolas; piezas pedidas: Spring 1/1 | - | PASA |
| 32 | un puente con 10 tablas | Jev | `OK 10 parts (jev pieces=[Plank x10]) 'Construccion'` | 10 | 0 | estructura estatica, se mantuvo en pie; piezas pedidas: Plank 10/10 | - | PASA |
| 33 | un cohete con 3 propulsores | Jev | `OK 6 parts (jev pieces=[Cylinder x1, Seat x1, Rocket x3, Stabilizer x1]) 'Construccion'` | 6 | 4 | se movio 2105 cm (max 2518 cm/s); subio 4395 cm; piezas pedidas: Rocket 3/3 | stabilizer: FALLA | PASA |
| 34 | un tanque con 4 cañones | Jev | `OK 10 parts (jev pieces=[Plank x1, Seat x1, Wheel x4, Cannon x4]) 'Construccion'` | 10 | 10 | se movio 291 cm (max 300 cm/s); subio 63 cm; algo gira a 13.9 rad/s; disparo/lanzo 11 bolas; su efecto toco al objetivo (5); objetivo empujado 409 cm; piezas pedidas: Cannon 4/4 | cannon: FALLA | PASA |
| 35 | una torre de 12 bloques | Jev | `OK 12 parts (jev pieces=[Block x12]) 'Construccion'` | 12 | 0 | estructura estatica, se mantuvo en pie; piezas pedidas: Block 12/12 | - | PASA |
| 36 | un carro con seis ruedas y dos lanzallamas | Jev | `OK 10 parts (jev pieces=[Plank x1, Seat x1, Wheel x6, Flamethrower x2]) 'Construccion'` | 10 | 8 | se movio 1103 cm (max 273 cm/s); algo gira a 12.1 rad/s; su efecto toco al objetivo (44); piezas pedidas: Wheel 6/6, Flamethrower 2/2 | - | PASA |
| 37 | un carro con una viga larga al frente como lanza | Jev | `OK 8 parts (jev pieces=[Plank x1, Seat x1, Wheel x4, Panel x1, Girder x1]) 'Construccion'` | 8 | 4 | se movio 130 cm (max 146 cm/s); algo gira a 13.4 rad/s | - | PASA |
| 38 | un tanque con plataforma giratoria | Jev | `OK 9 parts (jev pieces=[Plank x1, Seat x1, Wheel x4, Turntable x1, Cannon x1, Block x1]) 'Construccion'` | 9 | 9 | se movio 131 cm (max 382 cm/s); subio 101 cm; algo gira a 12.9 rad/s; disparo/lanzo 3 bolas | cannon: FALLA | PASA |
| 39 | un trineo con vela | Jev | `OK 4 parts (jev pieces=[Sled x1, Seat x1, Sail x1, Plank x1]) 'Construccion'` | 4 | 1 | se movio 119 cm (max 154 cm/s); objetivo empujado 81 cm | - | PASA |
| 40 | un carro con ventilador y una bateria | Jev | `OK 9 parts (jev pieces=[Plank x1, Seat x1, Wheel x4, Fan x1, Battery x1, BigBattery x1]) 'Construccion'` | 9 | 5 | se movio 893 cm (max 353 cm/s); subio 119 cm; algo gira a 11.7 rad/s; objetivo empujado 610 cm | battery: PASA | PASA |
| 41 | un muro defensivo con escudo | Jev | `OK 3 parts (jev pieces=[Wall x1, Panel x1, Stake x1]) 'Construccion'` | 3 | 1 | nada medible | stake: PASA | FALLA |
| 42 | un carro con resorte que salte | Jev | `OK 7 parts (jev pieces=[Plank x1, Seat x1, Wheel x4, Spring x1]) 'Construccion'` | 7 | 5 | se movio 1044 cm (max 444 cm/s); subio 47 cm; algo gira a 13.1 rad/s | - | PASA |

## Como leerlo

- **Backend**: `Jev` = decidio el modelo de decision; `respaldo` = fallo la llamada y se uso el generador por palabras clave.
- **Decision**: plantilla (`archetype`), `skipped=[...]` = extras pedidos que no cupieron o no aplican, `extras=[...]` = extras que el modelo marco.
- **Mecanica observada**: medido en el juego (posicion, velocidad, giro, proyectiles, efecto sobre el cubo-objetivo).
- **Mecanica verificada aislada**: resultado de la seccion 1 para las mecanicas que lleva esa construccion (por extra elegido o por pieza de la plantilla).
