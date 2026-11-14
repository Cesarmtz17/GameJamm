---
title: 01 — Construcción: agarrar, rotar, pegar
date: 2026-11-13
tags:
  - research
  - construccion
  - ue5
status: listo
---

# 01 — Construcción: agarrar, rotar, pegar

> [!info]
> Investigación deep-research con fuentes (2026-11-13). Datos inciertos marcados.
> Ver [FUENTES.md](FUENTES.md) para la lista completa. "La referencia" = el juego fuente.

## 1. Cómo funciona el grab

- Al activar el modo construcción, los objetos agarrables se iluminan en **naranja**; al
  apuntar con la mira se vuelven **verde** — ese es el lenguaje visual de "agarrable ahora".
  Se agarra con A. *(portales de guías de la comunidad)*
- Qué es agarrable: en la práctica, **cuerpos con física dinámica** (troncos, tablones,
  piedras, cajas, barriles, dispositivos del kit, cofres sumergidos...). La habilidad se
  describe como "un imán sin límite de metal". Los objetos 100% estáticos del terreno no
  son agarrables. *(wiki de la franquicia)*
- Movimiento mientras se sostiene: el objeto sigue al jugador; D-pad arriba/abajo lo
  aleja/acercan; el right stick lo mueve vertical/lateralmente. *(portal de guías)*

**→ Patrón UE5:** raycast de mira → highlight naranja (elegible) → verde (apuntado) →
agarrar como kinematic-follow del cuerpo (ver [08-mapeo-ue5-impl.md](08-mapeo-ue5-impl.md)
§1 y la implementación de referencia ComponentFuseMechanic).

## 2. Rotación

- Mantener R + D-pad rota el objeto: arriba/abajo = vertical, izquierda/derecha =
  horizontal, en **pasos de 45°**. Otro ángulo requiere reorientar al jugador (el marco de
  referencia es el jugador). *(portal de guías)*
- **ZL = "lay object flat"**: resetea la orientación y apoya el objeto plano en el suelo.
  *(portal de guías)*

## 3. Attach points y la "masilla verde"

- Al acercar el objeto sostenido a otro agarrable aparece **un punto brillante rodeado de
  una sustancia verde translúcida** ("a bright dot surrounded by transparent slime") — el
  preview de dónde y cómo se van a pegar. *(portal de guías · página oficial de la
  desarrolladora)*
- La pegatina es por **proximidad de superficies**: el punto se calcula entre las caras más
  cercanas de ambos cuerpos, y al confirmar se genera una **unión fija (weld/fixed joint)**
  — no hay articulación flexible. Por eso las creaciones se comportan como un cuerpo rígido
  único (ver §6).

## 4. Despegar (wiggle)

- Regla exacta: agarrar la pieza pegada y **agitar el right stick de lado a lado** de forma
  sostenida hasta que se separa. Texto in-game: "You can undo [the ability]'s adhesive by
  moving [the stick] back and forth while holding the glued-together object".
  *(portal de guías · página oficial)*
- Diseño: el despegue **no es instantáneo** — requiere umbral de "agitación acumulada"
  (evita despegues accidentales durante el uso del vehículo).

## 5. Límites

- **Máximo 21 objetos pegados por creación.** Al intentar pegar el 22º:
  *"You can't attach any more"*. *(enciclopedia de la franquicia)*
- No hay límite de peso documentado para levantar; los objetos pesados arrastran su masa
  física real (masa autocalculada de material+volumen, ver [04](04-quimica-fisica-gdc.md)).

## 6. Comportamiento físico de una creación (welded joints)

Confirmado en GDC 2024 ("Evolving Physics and Sounds", Dohta/Takayama/Osada):

> "Every single thing without exception is built with dynamic rigid bodies and constraints."

- Las creaciones del modo construcción **no son un script de "vehículo"**: son cuerpos
  individuales unidos por constraints fijos. Masa = suma, inercia recalculada. La sustancia
  verde "mantiene su forma incluso en agua" (balsas funcionales). *(transcripción GDC)*
- **Truco oficial:** "construye primero, lleva después" — un conjunto pegado se agarra como
  una pieza única. *(portal de guías)*
- **Dispositivos en grupo:** al activar un dispositivo conectado a un conjunto, **se activan
  todos a la vez** ("When multiple kit devices are connected to an Object with the
  construction mode, triggering one Device will activate all of the attached Devices at
  once"). *(enciclopedia de la franquicia)*
- Alternativa de activación: **golpear un dispositivo con un arma lo activa**.
  *(página oficial de la desarrolladora)*
- Edge case: el **rugido de un monstruo élite destruye al instante los dispositivos
  cercanos** (counter anti-ingeniería). *(enciclopedia de la franquicia)*
- Las puertas del mundo son **slider constraint + motor**, los engranajes ruedas conectadas
  por constraints, la olla portátil lleva **ball-socket** en la base (la comunidad la usó
  como articulación universal). *(transcripción GDC · prensa técnica)*

## 7. Cómo lo define el juego en datos (RE, ver detalle en [06](06-re-formatos-herramientas.md))

- Cada arma define sockets nombrados por clase en su `AttachmentActor`:
  `AttachmentBindBoneSword: "Bind_A"`, `AttachmentBindBoneLargeSword`,
  `AttachmentBindBoneSpear` — el bone del modelo donde se engancha lo fusionado. Algunos
  objetos usan otro bone (el ala del kit usa `Bind_C` en espadas de una mano). *(sitio de
  datos crudos de la comunidad)*
- Cada objeto fusionable tiene `AttachmentParam.gyml` con bloques por tipo de arma
  (`AttachParamSword_`, `AttachParamLSword_`, `AttachParamSpear_`, `AttachParamShield`) con
  `BoneName`, `TransOffset`, `RotateOffset`, `Scale`, `BoundBoxLength`, `SubModel`; flechas
  con `AttachTransOffsetArrow` etc.
- Etiquetas de gameplay por subtipo: `AttachmentAdditionalSubType: ["Blunt", <tag de
  dispositivo>, "Fan", "SpWing", "ChemicalExplosionElectric", ...]` — una parte "Fan" da
  propulsión, "SpWing" sustentación, etc.
- Atributos químicos del material: `ChemicalIsFlammable`, `ChemicalIsStruckableByThunder`,
  `AttachmentAdditionalDamage`, `RequestDeathOnHit`.
- **"Attach point" = (bone del mesh + offset/rotación + bounding box) por (objeto × tipo de
  arma) + etiquetas de subtipo + referencia a material químico.** El snap elige el punto más
  cercano entre sockets compatibles.
- La "cola" física del pegamento es **anisótropa** (resistencia distinta según dirección de
  estirado), probablemente modelada con ley de Hooke (constraints de resorte).
  *(medición documentada en un foro)*

## 8. Autoconstrucción (registro automático)

- Todo lo pegado se graba solo: historial de 30 builds + **8 favoritos**. La autoconstrucción
  reconstruye gratis si las piezas están cerca; las piezas faltantes se fabrican con un
  mineral desechable y son frágiles. 46 esquemas oficiales (12 piedras de esquema + 34 del
  clan secreto). *(portal de guías)*
- El build más famoso de la comunidad: **moto flotante** = 2 ventiladores + 1 asiento de
  dirección. *(portal de guías)*

## → Aplicable a GameJamm

1. Límite de piezas: **21** (usar ese número exacto como constante tunable).
2. Pegar = weld rígido; despegar = agitación con umbral acumulado.
3. Rotación en pasos de 45° relativa al frame del jugador.
4. Activación grupal: 1 switch activa todo el ensamblado.
5. Sockets nombrados + tabla de offsets por (pieza × tipo de arma) — igual que nuestro
   `UAttachPointComponent` + `UBuildPartData`.
6. Autoregistro (historial + favoritos) es el MVP perfecto del "asistente IA" del GDD:
   guardar recetas de builds exitosos y ofrecerlas como fallback.
7. Las reglas del builder (attach, límites, pesos) no son datos públicos — auditoría y
   plan de tests en [09-auditoria-fuentes-publicas.md](09-auditoria-fuentes-publicas.md);
   análisis a fondo del repo de referencia y diseño del grafo de ensamblado en
   [10-repo-open-source-fusion.md](10-repo-open-source-fusion.md) y
   [11-diseno-ensamblado-open-source.md](11-diseno-ensamblado-open-source.md).
