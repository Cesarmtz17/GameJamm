---
title: 03 — Vehículos, dispositivos, baterías y habilidades de movimiento
date: 2026-11-13
tags:
  - research
  - vehiculos
  - dispositivos
  - balance
status: listo
---

# 03 — Vehículos, dispositivos, baterías y habilidades de movimiento

> [!info]
> Investigación deep-research con fuentes (2026-11-13). Ver [FUENTES.md](FUENTES.md).
> "La referencia" = el juego fuente. Los nombres de dispositivos se traducen de forma
> genérica (ventilador, emisor de fuego...); los números son los datamineados.

## 1. Los 27 dispositivos del kit

*(lista oficial en la enciclopedia de la franquicia · consumos medidos por la comunidad ·
comportamiento documentado en portales de guías)*

| Dispositivo | Qué hace | ¿Drena energía? | Duración activa | Consumo (pozos/seg) |
|---|---|---|---|---|
| **Ventilador (Fan)** | Hélice: propulsión de vehículos; updraft si va boca arriba | Sí | 30 min | **0.156** |
| **Ala (Wing)** | Planeador de un vuelo; balancearse en el centro vuela más lejos | No | 1:15 | 0 |
| **Cohete (Rocket)** | Thrust explosivo de un uso, luego se quema | Disputado* | 3 seg | — |
| **Globo (Balloon)** | Sube con calor (emisor de fuego debajo = más rápido) | No¹ | 2 min | 0 |
| **Rueda chica (Small Wheel)** | Rueda motorizada rápida, mala en cuestas | Sí | 30 min | 0.015 |
| **Rueda grande (Big Wheel)** | Rueda para terreno difícil; cruza agua somera | Sí | 30 min | 0.015 |
| **Vagón (Cart)** | Plataforma con ruedas de baja fricción (requiere empuje externo) | No | — | 0 |
| **Trineo (Sled)** | Base deslizante (arena/hierba/nieve) | No | — | 0 |
| **Piedra flotante (Hover Stone)** | Flota inmóvil al activarse (ignora gravedad) | Sí | 30 min | 0.03 |
| **Asiento de dirección (Steering Stick)** | Montable: dirige el vehículo y **enciende todos los dispositivos conectados** | No | — | 0 |
| **Estabilizador (Stabilizer)** | Endereza y mantiene la construcción vertical (motos flotantes) | Sí | 30 min | 0.03 |
| **Resorte (Spring)** | Se comprime y dispara lo que tenga encima | No¹ | — | 0 |
| **Vagón teledirigido (Homing Cart)** | Vagón que se dirige solo hacia monstruos | Sí¹ | 30 min | 0.03 |
| **Emisor de fuego (Flame Emitter)** | Fuego continuo; calienta globos | Sí | 30 min | 0.18 |
| **Emisor de hielo (Frost Emitter)** | Congela enemigos y agua (plataformas de hielo) | Sí | 30 min | 0.18 |
| **Emisor de rayo (Shock Emitter)** | Rayo eléctrico desde su cuerno al ser golpeado | Sí¹ | 30 min | 0.18 |
| **Emisor de haz (Beam Emitter)** | Haz dañino de largo alcance (torretas) | Sí¹ | 30 min | 0.18 |
| **Cañón (Cannon)** | Proyectiles explosivos a intervalos (600 usos, daño blast 12/40) | Sí¹ | 30 min | **0.3** |
| **Cabeza objetivo (Construct Head)** | Gira hacia "enemigos" detectados; + emisor/cañón = torreta autónoma | Sí | 30 min | 0.03 |
| **Bomba de tiempo (Time Bomb)** | Bomba de tiempo; su onda activa otros dispositivos; 1 uso | No (1 uso) | — | 0 |
| **Luz (Light)** | Foco de luz direccional | Sí | 30 min | 0.03 |
| **Espejo (Mirror)** | Concentra y refleja luz | No¹ | 30 min | 0 |
| **Hidrante (Hydrant)** | Escupe agua al golpearlo (apaga fuegos, moja) | Sí¹ | 30 min | 0.03 |
| **Olla portátil (Portable Pot)** | Olla portátil de un solo uso | No (1 uso) | — | 0 |
| **Estaca (Stake)** | Se clava y queda fijado (ancla de construcciones) | No | — | 0 |
| **Batería (Battery)** | Batería desechable que alimenta el build antes que tu celda de energía | Suministra | — | 0 |
| **Batería grande (Big Battery)** | Versión grande desechable | Suministra | — | 0 |

\* Disputa documentada: un cohete suelto parece no consumir; fundido a escudo gasta ~medio
pozo de energía por uso. *(portal de guías)*
¹ Marcado "menos seguro" por la fuente. Los dispositivos sin celda de energía expiran por
uso y destellan verde antes de desaparecer.

**Datos de consumo en frames (datamine):** haz/emisores = 1 batería cada 500 frames
(16.66 s); estabilizador/piedra flotante = 3000 frames; ruedas = 6000 frames. Ventilador en
agua/riel gasta ¼. Dispositivo en escudo ×3 coste; en arma ×20; armas de mineral −25%.
*(canal de datamining en YouTube)*

**Física por dispositivo (GDC 2024):** la rueda es **3 cuerpos rígidos** (rueda, motor, eje):
el eje sostiene el motor, el motor transfiere torque a la rueda, la fricción con el suelo
crea la tracción. Las cadenas son cápsulas rígidas unidas por ball-sockets. *(transcripción
GDC)*

## 2. Celdas de energía (baterías)

- **Capacidad: 3 pozos de energía iniciales → máx 48** (45 añadibles). Cada pozo =
  **100 cargas cristalizadas**; el total son 4,500. Medidor de 8 segmentos; desde el pozo 25
  los segmentos valen doble. *(portal de guías, mejoras de batería)*
- Moneda: carga (1) / grande (20) / enorme (100). Los constructores-herrería: 3 minerales =
  1 carga; 15 minerales grandes = 1 pozo de energía.
- **Regla de consumo:** los dispositivos del build **drenan primero las baterías
  desechables pegadas al conjunto, después tu celda de energía**.
- **Recarga:** solo cuando ningún dispositivo consume; la carga de dispositivo rellena un
  poco; la carga grande evita drenaje un tiempo.
- Edge case: con batería llena (48), las tiendas de refinado venden dispositivos a elección
  (3 cargas por dispositivo; 30 por batería grande). *(portal de guías)*

## 3. Física de vehículos

- **No existe "programa de vehículo"** (Takayama, GDC 2024): *"Instead of creating a vehicle
  program, we created a system in which vehicles could be made"*. Los vehículos emergen de
  ruedas (motor+torque+fricción), asiento de dirección y masas reales. *(prensa técnica)*
- **Empuje:** ventiladores apilables ("team up to create greater force, but they'll drain
  more energy"); cohetes = thrust potente de 3 s. Los valores de newtons no están
  publicados; la comunidad midió empíricamente pesos relativos ("ventiladores necesarios
  para levantar X"): estabilizador 1, rueda grande 6, vagón/trineo 2, resorte 3, piedra
  flotante activa 15. *(medición en foro)*
- **Resistencia al agua:** se calcula con el **área proyectada en la dirección de la
  velocidad** (no decay simple) — por eso una tabla orientada "muerde" el agua como remo.
  Aplica a TODOS los objetos: el bote de remos es emergente, no dedicado. *(transcripción
  GDC)*

## 4. Atravesar techos (habilidad de movimiento)

- **Atraviesa techos desde abajo** ("rise through ceilings, hillsides, and even certain
  enemies").
- **Detección del techo:** cámara hacia arriba + rejilla; punto central **rojo = superficie
  superior no es lo bastante plana/horizontal (falla); verde = plana → asciende**. Check =
  horizontalidad de la superficie + **espacio libre encima para salir**.
- Edge cases: mundo pausado mientras eliges; **salida automática a los 30 s** de haber
  emergido. Nació como herramienta interna de testing. *(wiki de la franquicia · portal de
  guías)*

## 5. Rebobinar (habilidad de movimiento)

- **Congela el tiempo** mientras apuntas (única habilidad que lo hace), muestra la
  **trayectoria predicha como línea amarilla**, y al aplicar **invierte el movimiento del
  objeto por unos segundos**. Se puede detener en cualquier momento.
- **Rango casi infinito**. Funciona en objetos con trayectoria propia: rocas que caen,
  proyectiles (se devuelven al lanzador), armas arrojadas, plataformas móviles, vehículos.
- **Piezas pegadas:** no "deshace" el pegamento; opera sobre el conjunto como cuerpo físico
  en movimiento. La desarrolladora lo convirtió a física: "we also changed [this ability] to
  be physics driven". *(wiki de la franquicia · transcripción GDC)*
- Combo canónico: pegar ventiladores/cohetes a una roca, lanzarla, **rebobinar para
  reutilizar el build**. *(portal de guías)*

## 6. Torretas y builds famosos de la comunidad

- **Torreta autónoma** = cabeza objetivo + emisor de haz/cañón/fuego (apunta y dispara
  sola); vagón teledirigido como plataforma móvil. *(portal de guías)*
- **Escudo-cohete** = impulso vertical tipo salto del predecesor, de un uso.
  *(portal de guías)*
- **Moto flotante** = 2 ventiladores + asiento de dirección (el build más famoso).
  *(portal de guías)*
- **Grind de escudo en rieles** (trenes emergentes). *(wiki de la franquicia)*
- Otros valores datamined: emisor de haz 12 daño/tick; cañón 12 + elemento; bomba de tiempo
  30 tras 4 s (bomba normal 40); brazos mecánicos del juego 27 + fusión (+13/nivel); ala 75 s
  de vida, globo 120 s, resto 30 min; dispositivo fusionado pierde 1 durabilidad por
  activación y 1 cada 2 s. *(canal de datamining · sitio de datos crudos)*

## → Aplicable a GameJamm

1. Piezas iniciales del GDD ↔ dispositivos: Llanta (rueda chica/grande), Motor/Cohete
   (cohete/ventilador), Lanzallamas (emisor de fuego), Roca, Tabla, Asiento (asiento de
   dirección).
2. Batería global del jugador con **recarga solo en reposo** + baterías desechables en
   piezas (más baratas) que drenan primero — balance perfecto para PvP.
3. Consumos de referencia para balancear (pozos/seg): ventilador 0.156, ruedas 0.015,
   emisores 0.18, cañón 0.3.
4. **No programes "vehículo"**: programa piezas (rueda, motor, emisor, cabeza-objetivo) y
   deja que se combinen. La torreta emerge de cabeza objetivo + emisor.
5. Asiento de dirección = asiento que activa el ensamblado y da control de rotación.
6. Atravesar techos/rebobinar: fuera de alcance (ver [08](08-mapeo-ue5-impl.md) §5); si
   sobra tiempo, rebobinar de trayectoria para rocas/proyectiles.
7. Límites anti-junk: expiración por uso (destello verde) y la energía finita ya frenan el
   spam de builds — mantener ambos.
