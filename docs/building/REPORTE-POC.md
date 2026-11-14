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
> Fecha: 2026-11-14 00:09. Cada frase se construye delante del jugador con un cubo-objetivo de 20 kg a 3.5 m al frente y se mide durante 10 s.

**Resultado: 29 de 29 frases pasan.** "PASA" = se construyo y se midio al menos una mecanica (o es una estructura estatica sin dispositivos). No juzga si el arquetipo elegido es el que una persona esperaria: eso se lee en la columna de decision.

| # | Frase | Backend | Decision y receta (estado del spawner) | Piezas | Encendidas | Mecanica observada | Veredicto |
|---|---|---|---|---|---|---|---|
| 1 | un carro con cohete | Jev | `OK 7 parts (jev conf=1.00 archetype=Car extras=[rocket]) 'Car'` | 7 | 5 | se movio 3458 cm (max 1652 cm/s); subio 75 cm; algo gira a 41.9 rad/s; objetivo empujado 1962 cm | PASA |
| 2 | un tanque | Jev | `OK 7 parts (jev conf=1.00 archetype=Tank extras=[]) 'Tank'` | 7 | 5 | se movio 1522 cm (max 335 cm/s); algo gira a 11.0 rad/s; disparo/lanzo 4 bolas; objetivo empujado 633 cm | PASA |
| 3 | un cohete | Jev | `OK 4 parts (jev conf=1.00 archetype=Rocket skipped=[rocket] extras=[rocket]) 'Rocket'` | 4 | 3 | se movio 205 cm (max 17768 cm/s); subio 92740 cm | PASA |
| 4 | un avion con alas y ventilador | Jev | `OK 7 parts (jev conf=0.68 archetype=Glider skipped=[wing] extras=[fan,wing]) 'Glider'` | 7 | 5 | se movio 1909 cm (max 3411 cm/s); subio 3914 cm; algo gira a 6.4 rad/s | PASA |
| 5 | una torreta con laser | Jev | `OK 4 parts (jev conf=1.00 archetype=Turret extras=[beam]) 'Turret'` | 4 | 2 | disparo/lanzo 4 bolas | PASA |
| 6 | un globo aerostatico con fuego debajo | Jev | `OK 7 parts (jev conf=1.00 archetype=Airship skipped=[balloon] extras=[flame,balloon]) 'Airship'` | 7 | 5 | se movio 1206 cm (max 402 cm/s); subio 2210 cm; su efecto toco al objetivo (10) | PASA |
| 7 | un carro con lanzallamas | Jev | `OK 7 parts (jev conf=1.00 archetype=Car extras=[flame]) 'Car'` | 7 | 5 | se movio 529 cm (max 252 cm/s); algo gira a 11.3 rad/s; su efecto toco al objetivo (91); objetivo empujado 228 cm | PASA |
| 8 | un hovercraft con vela y emisor de hielo | Jev | `OK 7 parts (jev conf=1.00 archetype=Hover extras=[sail,frost]) 'Hover'` | 7 | 5 | se movio 1987 cm (max 518 cm/s); subio 64 cm; su efecto toco al objetivo (23); objetivo congelado | PASA |
| 9 | un carro con una bomba | Jev | `OK 7 parts (jev conf=0.99 archetype=Car extras=[bomb]) 'Car'` | 7 | 6 | se movio 892 cm (max 818 cm/s); algo gira a 13.0 rad/s; objetivo empujado 1481 cm; la bomba exploto | PASA |
| 10 | un puente largo de tablas | Jev | `OK 5 parts (jev conf=1.00 archetype=Bridge extras=[]) 'Bridge'` | 5 | 0 | estructura estatica, se mantuvo en pie | PASA |
| 11 | un trampolin con resorte que lance cosas | Jev | `OK 3 parts (jev conf=1.00 archetype=Catapult skipped=[spring] extras=[spring]) 'Catapult'` | 3 | 1 | disparo/lanzo 4 bolas | PASA |
| 12 | un velero con ruedas | Jev | `OK 7 parts (jev conf=0.99 archetype=Car extras=[sail]) 'Car'` | 7 | 5 | se movio 805 cm (max 259 cm/s); algo gira a 11.9 rad/s; objetivo empujado 589 cm | PASA |
| 13 | una fortaleza con cañones | Jev | `OK 5 parts (jev conf=1.00 archetype=Fortress extras=[cannon]) 'Fortress'` | 5 | 1 | disparo/lanzo 4 bolas | PASA |
| 14 | una moto | Jev | `OK 5 parts (jev conf=1.00 archetype=Bike extras=[]) 'Bike'` | 5 | 3 | se movio 1426 cm (max 412 cm/s); algo gira a 9.1 rad/s; objetivo empujado 95 cm | PASA |
| 15 | un trineo con ventilador | Jev | `OK 4 parts (jev conf=1.00 archetype=Sled extras=[fan]) 'Sled'` | 4 | 2 | se movio 339 cm (max 616 cm/s); algo gira a 6.9 rad/s; objetivo empujado 97 cm | PASA |
| 16 | un dron con un faro | Jev | `OK 6 parts (jev conf=1.00 archetype=Flyer extras=[light]) 'Flyer'` | 6 | 4 | se movio 328 cm (max 499 cm/s); subio 2114 cm | PASA |
| 17 | una catapulta | Jev | `OK 3 parts (jev conf=1.00 archetype=Catapult extras=[]) 'Catapult'` | 3 | 1 | disparo/lanzo 4 bolas | PASA |
| 18 | un aspersor de fuego que gire | Jev | `OK 5 parts (jev conf=1.00 archetype=Spinner skipped=[spin] extras=[spin,flame]) 'Spinner'` | 5 | 4 | algo gira a 4.2 rad/s | PASA |
| 19 | una rueda sola que ruede quemando todo | Jev | `OK 4 parts (jev conf=1.00 archetype=Roller extras=[flame]) 'Roller'` | 4 | 4 | su efecto toco al objetivo (84) | PASA |
| 20 | un planeador | Jev | `OK 6 parts (jev conf=1.00 archetype=Glider skipped=[wing] extras=[wing]) 'Glider'` | 6 | 4 | se movio 4987 cm (max 3825 cm/s); subio 7438 cm; algo gira a 4.3 rad/s | PASA |
| 21 | una torreta con cabeza que apunte sola | Jev | `OK 4 parts (jev conf=1.00 archetype=Turret extras=[head]) 'Turret'` | 4 | 2 | disparo/lanzo 4 bolas | PASA |
| 22 | un carro con manguera de agua | Jev | `OK 7 parts (jev conf=0.99 archetype=Car extras=[hydrant]) 'Car'` | 7 | 5 | se movio 944 cm (max 253 cm/s); algo gira a 11.7 rad/s; su efecto toco al objetivo (37); objetivo empujado 769 cm | PASA |
| 23 | un tanque electrico que de toques | Jev | `OK 8 parts (jev conf=1.00 archetype=Tank extras=[shock]) 'Tank'` | 8 | 6 | se movio 794 cm (max 324 cm/s); algo gira a 10.8 rad/s; disparo/lanzo 4 bolas; su efecto toco al objetivo (8); objetivo empujado 440 cm | PASA |
| 24 | un carro con turbina y bateria | Jev | `OK 8 parts (jev conf=1.00 archetype=Car extras=[turbine,battery]) 'Car'` | 8 | 5 | se movio 2424 cm (max 648 cm/s); algo gira a 20.5 rad/s; objetivo empujado 2173 cm | PASA |
| 25 | un carro levitante con piedras antigravedad | Jev | `OK 5 parts (jev conf=0.59 archetype=Hover skipped=[hover] extras=[hover]) 'Hover'` | 5 | 3 | subio 43 cm | PASA |
| 26 | un dirigible con cañon | Jev | `OK 7 parts (jev conf=1.00 archetype=Airship extras=[cannon]) 'Airship'` | 7 | 5 | se movio 1127 cm (max 361 cm/s); subio 1647 cm; disparo/lanzo 4 bolas | PASA |
| 27 | una torreta anclada al suelo con una bisagra | Jev | `OK 6 parts (jev conf=1.00 archetype=Turret extras=[stake,hinge]) 'Turret'` | 6 | 2 | disparo/lanzo 4 bolas | PASA |
| 28 | algo que ruede y queme todo | Jev | `OK 6 parts (jev conf=0.90 archetype=Roller skipped=[spin] extras=[spin,flame,bomb,ball]) 'Roller'` | 6 | 5 | se movio 101 cm (max 481 cm/s); algo gira a 10.5 rad/s; su efecto toco al objetivo (41); objetivo empujado 55 cm; la bomba exploto | PASA |
| 29 | hazme un carrito bien loco con lanzallamas, un globo, cohete, luces y una bateria extra | Jev | `OK 11 parts (jev conf=1.00 archetype=Car extras=[rocket,flame,balloon,battery,light]) 'Car'` | 11 | 8 | se movio 1484 cm (max 1039 cm/s); subio 71 cm; algo gira a 25.0 rad/s; su efecto toco al objetivo (15); objetivo empujado 397 cm | PASA |

## Como leerlo

- **Backend**: `Jev` = decidio el modelo de decision; `respaldo` = fallo la llamada y se uso el generador por palabras clave.
- **Decision**: arquetipo (plantilla de piezas), `skipped=[...]` = extras pedidos que no cupieron o no aplican, `extras=[...]` = extras que el modelo marco.
- **Mecanica observada**: medido en el juego (posicion, velocidad, giro, proyectiles, efecto sobre el cubo-objetivo).
