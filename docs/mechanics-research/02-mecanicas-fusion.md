---
title: 02 — Fusión: material ↔ arma/escudo/flecha
date: 2026-11-13
tags:
  - research
  - fusion
  - balance
status: listo
---

# 02 — Fusión: material ↔ arma/escudo/flecha

> [!info]
> Investigación deep-research con fuentes (2026-11-13). Ver [FUENTES.md](FUENTES.md).
> "La referencia" = el juego fuente. Los nombres propios de materiales del juego fuente se
> generalizaron por tier: **la forma de la curva es lo que importa**, no el nombre.

## 1. Reglas de combinación

- Se fusiona **un solo material** con el arma equipada (botón), escudo (otro botón), o la
  flecha tensada. **No hay fusión en lote para flechas: una por una.** *(portal de guías)*
- **No se puede fusionar un arma ya fusionada** ("Fuse has already been used on this").
  Límite = base + 1 material. Deshacer: "Destroy Fused Material" (destruye el material,
  conserva la base). **Excepción que recupera el material: el servicio de desmonte de una
  tienda del juego, 20 monedas.** *(portal de guías)*
- El material **queda consumido** (salvo desfusión o el rasgo *reciclaje de fusión* en
  algunas armas). *(wiki de la franquicia)*
- También existe **fusión arma↔arma** (ej. palo largo + horca agrícola = lanza larga): el
  material aporta el **attack power base del arma**, sin bonos. *(portal de guías)*

## 2. Attack power: la fórmula y la tabla

**Fórmula base: daño final = attack power del arma + Fuse Attack Power del material.**
Ejemplo canónico: espada viajera degradada (5) + cuerno de monstruo básico (4) = **9 ATK**.
*(wiki de la franquicia · portal de guías)*

- **Distorsión de la UI ("números mentirosos"):** las espadas/escudos muestran **95%** del
  daño real, las lanzas **132.68%** del real — y aplica también al material fusionado (un
  cuerno de élite suprema de 55 aporta ~42 reales en lanza). *(datamine en foro)*
- **Flechas:** aportan su Fuse AP tal cual, pero **colmillos y garras de monstruo cuentan
  ×2** (todas las variantes y tiers). *(wiki de la franquicia)*
- **Escudos:** el Fuse AP sube el daño del shield bash; **los escudos NO ganan durabilidad
  al fusionar**. *(wiki de la franquicia)*

### Top materiales por Fuse Attack Power

*(portal de guías con tabla de 40+ · enciclopedia de la franquicia con 148 materiales ·
CSVs comunitarios)*

| Material (genérico) | Fuse AP | | Material (genérico) | Fuse AP |
|---|---|---|---|---|
| Cuerno de élite suprema (sable) | 55 | | Cuernos elementales de dragón | 30 |
| Cuerno de élite suprema (maza) | 51 | | Cuernos de dragón mayor | 26 |
| Hoja ancestral | 50 | | Diamante | 25 |
| Cuerno de élite avanzada (sable) | 44 | | Cuerno del dragón de luz | 20 |
| Hueso de no-muerto | 40 | | Cuerno de monstruo oscuro | 17 |
| Cuerno de capataz élite | 37 | | Cuerno de monstruo azul | 7 |
| Colmillo de bestia gigante / cuerno de capataz constructo | 35 | | Cuerno de monstruo básico | 4 |
| Cuerno de lagarto élite | 34 | | Frutas / material trivial | 1 |
| Mandíbula de bestia arenosa | 32 | | | |

### Multiplicadores por familia de arma (datamine confirmado)

- **Familia de artesanía del desierto** ("Strong Fusion"): **duplica el Fuse AP del
  material** (durabilidad de fusión +5).
- **Familia acuática** ("Water Warrior"): duplica el daño TOTAL (base+fusión) mientras esté
  mojada.
- **Familia de mineral**: +3/+5/+10 al fusionar dispositivos (normal/fuerte/poderosa).
- **Familia noble**: el contraataque a tiempo duplica el arma; **familia noble de guardia**
  "Breaking Point": ×2 cuando muy dañada.
- **Set de hueso**: ×1.8 con materiales de hueso. *(datamine en foro)*

## 3. Durabilidad

- **Fusionar aumenta la durabilidad: +25** con material de monstruo (un "extra allowance",
  no una reparación). Edge case: el bonus se gasta **una sola vez por arma** — si ya gastaste
  los 25, desfusión + nueva fusión no da otros 25. *(portal de guías)*
- Excepciones: noble de guardia/armas malditas **+10**; armas oxidadas y utensilios baratos
  **+10**; artesanía del desierto **+5**; hueso **+3**. *(wiki de la franquicia)*
- Fusionar "restaura parcialmente" un arma muy dañada y prolonga su vida útil.
  *(portal de guías)*

## 4. Flechas: efectos de materiales

*(portal de guías · enciclopedia de la franquicia)*

| Material (genérico) | Efecto en flecha |
|---|---|
| Fruta ígnea / gema roja | fuego (arde al impacto) |
| Fruta de hielo / gel viscoso blanco | congela al objetivo |
| Fruta eléctrica / gema ámbar | electricidad |
| Fruta acuática | moja / apaga |
| Flor explosiva | explosión |
| Ojo de bestia o murciélago nocturno | flecha teledirigida (homing) |
| Fronda vegetal | ráfaga de viento que derriba |
| Semilla luminosa | luz al clavarse |
| Hoja ancestral | desintegra al enemigo |

## 5. Escudos fusionados

- Roca + escudo = snowboard/minero (surf con poca pérdida de durabilidad).
  *(wiki de la franquicia)*
- **Cohete + escudo = impulso vertical de un uso** (drena la celda de energía).
  *(portal de guías)*
- Emisor de fuego + escudo = escupe fuego en guardia; hongo de humo = humo; barril = explota
  al golpe. Los 4 emisores añaden ataque a distancia a cambio de durabilidad.
- La referencia añade: grind sobre rieles de vagón con surf de escudo. *(wiki)*

## 6. Degradación (tres estados)

- **Decayed** (superficie del mundo, reducido), **Intact** (parcial), **Pristine**
  (completo). Ej: espada de la familia acuática degradada 6/20 vs íntegra 12/27.
  *(portal de guías)*
- **Pristine** de "soldados espectrales" en la zona subterránea: hay que romper primero la
  versión degradada para que entre al pool; cada soldado está atado a un tipo de arma; el
  evento de reinicio nocturno re-rolla.

## → Aplicable a GameJamm (valores para DataAssets)

1. `Attack = BaseArma + FuseAP(material)` — tabla de FuseAP por material en
   `UJamFusionMaterialData`.
2. `Durabilidad += 25` con material "monstruo" (una sola vez por arma). MVP del GDD:
   Piedra/Fuego/Hielo → sugerencia inicial +10/+5/+8 con efectos (knockback/DoT/slow).
3. Flecha: hereda el efecto del material; homing con "ojo".
4. Fusión arma↔arma permitida: aporta el attack base (¡combo de lanzas largas!).
5. La "distorsión de UI" de la referencia (95%/132.68%) es un detalle de sabor: skip, no
   aporta al jam.
6. Material siempre consumido; el arma se queda con 1 fusión máx — fuerza decisiones de
   build (core del PvP).
7. Existe una spreadsheet comunitaria con las ~94k fusiones posibles — referencia para
   balancear la tabla propia (fuente en [FUENTES.md](FUENTES.md)).
