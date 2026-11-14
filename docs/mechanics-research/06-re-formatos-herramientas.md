---
title: 06 — RE: formatos de datos, herramientas y repos
date: 2026-11-13
tags:
  - research
  - re
  - formatos
  - herramientas
status: listo
---

# 06 — RE: formatos de datos, herramientas y repos

> [!info]
> Investigación deep-research con fuentes (2026-11-13). Fuente principal de formatos: el
> **repositorio comunitario de formatos** (~50 formatos documentados). Solo conocimiento
> público; los repos citados no incluyen assets del juego.
> Nota: los nombres/URLs exactos del ecosistema se quitaron a propósito (repo público);
> ver [FUENTES.md](FUENTES.md) para el mapa de fuentes.

## 1. Formatos de datos del juego de referencia

### 1.1 BYML / BGYML (Binary YAML)
- El predecesor: BYML v2 (little endian Switch, big endian WiiU). **La referencia: v4 y
  v7**; el `.bgyml` es BYML v7 ("Binary Generated YAML") con nodos nuevos:
  `HashArrayU32_*`, `HashArrayWithRemapU32_*`, `DictionaryWithRemap (0xc4)`,
  `RelocatedKeyTable (0xc5)`, `MonoTypedArray (0xc8)` — el `0xc8` (arrays mono-tipo) es la
  gran novedad para arrays densos. *(repositorio de formatos · wiki técnica de modding)*
- En el predecesor: `.byml`, `.mubin` (map units), `.bgdata`, `.baischedule`. En la
  referencia se añade `.bgyml`.
- **Qué revela:** todos los parámetros del juego (actores, mapas, quests, UI) son YAML
  binario jerárquico → diseño data-driven extremo.

### 1.2 AAMP (parameter archive binario)
- Parámetros con "XML interno" binario
  (`ParameterIO → ParameterList → ParameterObject → Parameter`). En el predecesor: actor
  params, world config (clima/luz), **`.bphysics`** (sidecar de física de actor). En la
  referencia es raro como extensión propia, mayormente embebido en archivos Phive.
  *(repositorio de formatos · lib aamp de la comunidad)*

### 1.3 Phive/Havok (física por actor)
- **Predecesor**: `.hkrb` (rigid bodies), `.hkcl` (cloth), `.hkrg` (ragdoll), `.hksc`
  (static compound), `.hknm2` (navmesh), `.hktmrb` (terreno) dentro de cada actor pack.
  *(wiki técnica de modding)*
- **Referencia**: mismos conceptos reenvasados por Phive:
  `Phive/ControllerSetParam/*.bgyml` → `Phive/ShapeParam/*__Physical.bgyml` → archivo de
  colisión ("polytopes"). *(tutorial de modding en GameBanana)*
- Herramientas: [PhysicsTool](https://github.com/KB0MB/PhysicsTool) (edita hkcl/bphcl,
  masas de partículas, colliders, genera polytopes) ·
  [Havok Actor Tool](https://gamebanana.com/tools/10303) ·
  [hkxcmd](https://github.com/figment/hkxcmd) (hkx→XML) ·
  [HavokLib](https://github.com/PredatorCZ/HavokLib) (serialize Havok 5.0–2017, exporta
  GLTF) · [HKX2-Enhanced-Library](https://github.com/Monitor221hz/HKX2-Enhanced-Library) (C#)
- **Qué revela:** masa, inercia, colliders y constraints de *cada actor* están en datos
  editables.

### 1.4 RSTB / RESTBL (Resource Size Table)
- Tabla de tamaños precalculados por archivo; si un mod agranda un archivo hay que
  actualizar la tabla o el juego crashea. La referencia usa `.rsizetable` con hashes CRC32.
  Spec: en el suite de análisis del filesystem de la comunidad.
- **Qué revela:** presupuesto de streaming declarado por recurso (consola de 3 GB). En UE5
  World Partition lo reemplaza, pero el *concepto* de budget-check por recurso es copiable
  para CI.

### 1.5 SARC, MSBT, BFRES/BNTX, ModelCodec
- **SARC**: contenedor omnipresente (`.pack`, `.pack.zs`); la referencia **anida SARCs
  dentro de SARCs**. *(lib sarc de la comunidad)*
- **MSBT**: texto localizado con tags de función. *(lib msbt de la comunidad)*
- **BFRES v10** + **BNTX**, y el **MeshCodec (`.mc`)** de la referencia: envoltorio de
  compresión sobre BFRES. *(repositorio de formatos)*
- La referencia usa **ZSTD** (no Yaz0 como el predecesor): herramienta de la comunidad con
  los diccionarios del juego (`Pack/ZsDic.pack.zs`).

### 1.6 ActorInfo y packs por actor (la gran diferencia referencia vs predecesor)
- **Predecesor**: un único `ActorInfo.product.sbyml` (BYML gigante) + `gamedata.ssarc` con
  flags del juego. *(tooling de la comunidad)*
- **La referencia cambió a per-actor**: cada actor es un `.pack.zs` en `Pack/Actor/` con
  BGYMLs internos, ej. `Actor/Weapon_Bow_028.engine__actor__ActorParam.bgyml` que
  referencia componentes por ruta:
  - `Component/ModelInfo`, `Component/Physics/*.bgyml` (→ ControllerSet)
  - `Component/BowParam/*.bgyml` (**BaseAttack** vive aquí)
  - `Component/ChemicalParam/*.bgyml` (→ `Work/Chemical/MaterialParam/stone.gyml`)
  - `Component/AttachmentParam/*.bgyml` (attach points — ver [01](01-mecanicas-construccion.md) §7)
  - `Component/LifeParam`, `Component/CaptureParam` (iconos), `Blackboard/BlackboardParamTable`
  *(tutoriales de modding en GameBanana)*
- **Detalle de diseño: herencia con `$parent`** — casi todo BGYML de la referencia tiene
  `"$parent": "Work/Component/…/Base.gyml"`: los parámetros se heredan en árbol (ej.
  `BowParam de Weapon_Bow_129 → Weapon_Bow_028`); editar un hijo afecta solo sus overrides.
  *(sitio de datos crudos de la comunidad)*
- Save data de la referencia: estructura documentada en un repo comunitario de gamedata.

## 2. Data mining: dónde viven los valores

- **Attach/fusión**: tablas `AttachmentParam` por objeto (el sitio de datos crudos las
  publica expandidas).
- **Dispositivos**: `SpObj_*` en el sitio de datos crudos (ej. `MaxLife 54000` de la rueda
  chica).
- **Armas/materiales**: CSVs listos (`weapons.csv`, `materials.csv`) en un repo comunitario
  de datos de armas; spreadsheet de ~94k fusiones (foro); hoja de datos de lanzamiento de un
  portal de gear.
- **Cocina**: simulador web con la lógica real recuperada por un modder conocido de la
  escena.
- **Chem**: `system.chmres.yml` con unidades físicas reales (repo de datos del predecesor
  convertidos a YAML).

## 3. Repos maestros (tipo → aporte)

### Decompilación / motor
| Proyecto | Aporte |
|---|---|
| Proyecto de decomp del predecesor | Decomp matching v1.5.0: ksys/uking, física, química, eventos; web pública de progreso |
| Wiki del proyecto de decomp: Havok | Método para leer Havok con reflexión + headers 2014 |
| Docs navegables del proyecto de decomp | Documentación de los sistemas |
| Repo de notas RE del predecesor | Notas + índice de libs byml/sarc/rstb/aamp/evfl |
| Decomp de otros títulos de la franquicia | Tres títulos adicionales (uno al 100%) |
| Decomp experimental de la referencia | Matching limpio-room de v1.0.0 |
| Recompilación estática de la referencia → x86-64 (7k★) | Su docs/architecture.md describe el binario a fondo |

### Formatos / herramientas de la referencia
| Proyecto | Aporte |
|---|---|
| Repositorio comunitario de formatos | Documento maestro de TODOS los formatos (~50) |
| Tool ZSTD de la comunidad | ZSTD con los diccionarios del juego |
| Suite de análisis de filesystem | Spec RESTBL |
| Libs de formatos (fork de oead + wrapper C#) | BYML/AAMP/SARC read/write |
| Parser de behavior trees AINB | Parser de la comunidad |
| Lib XLINK2 (+ GUI alternativa) | Sonido/efectos |
| [Mod manager/merger de la referencia](https://tkmm.org) | Gestión y merge de mods |
| Editor de VSCode para BYML/BGYML/AAMP/AINB/ASB/BAEV/XLINK/RESTBL/BNTX | Editor de la comunidad |
| Editor de EventFlow de la referencia | Editor de flowcharts |
| Repack batch de BGYML de actores + parcheo RESTBL | Tool de la comunidad |
| Repo de estructura del save data | Gamedata documentada |
| Inyector runtime de la comunidad (1.5k★) | Freecam, DFPS por cámara de prueba, actor spawner con JSON |
| Buscador de recetas | ~230 entradas del Recipe Book |

### Predecesor (base del ecosistema)
| Proyecto | Aporte |
|---|---|
| Lib byml-v2 | Parser/writer BYML |
| Lib aamp | AAMP con API estilo interno |
| Libs sarc · oead | SARC read/write · lib C++ multi-formato |
| Libs evfl (+ GUI event-editor) | EventFlow |
| BCML (histórico) · UKMM · Wild-Bits · roead | Mod managers y GUI de formatos |
| tooling CLI de la comunidad | yaz0/aamp/byml/sarc/actorinfo |
| Repo de datos del predecesor | Dumps de datos convertidos a YAML |

### Havok / física
[PhysicsTool](https://github.com/KB0MB/PhysicsTool) ·
[HavokLib](https://github.com/PredatorCZ/HavokLib) ·
[hkxcmd](https://github.com/figment/hkxcmd) ·
[Dexesttp/hkxpack](https://github.com/Dexesttp/hkxpack) ·
[HKX2-Enhanced-Library](https://github.com/Monitor221hz/HKX2-Enhanced-Library)

### Datasets / implementaciones replicables
CSVs de armas/materiales de la comunidad ·
[EvelynSchwab/ComponentFuseMechanic](https://github.com/EvelynSchwab/ComponentFuseMechanic)
(**construcción/fusión en UE 5.2** — ver [08](08-mapeo-ue5-impl.md) §7) · API del compendio
de items (predecesor + referencia, de la comunidad).

### Sitios de datos (no GitHub, esenciales)
- Sitio de datos crudos de la comunidad (Kiranico) — datos con BGYMLs expandidos
  (attachments, dispositivos, armas).
- Wiki técnica de modding del predecesor · wiki técnica de la generación reciente (AINB,
  ASB, Phive) · [nintendo-formats.com](https://nintendo-formats.com/)
- Visor de código decompilado navegable del ejecutable `uking`.
- [GDC Vault, charla 2024](https://gdcvault.com/play/1034667/) ·
  [paper SSRN sobre la construcción](https://papers.ssrn.com/sol3/papers.cfm?abstract_id=5001142)
- GameBanana: [tuts/19424](https://gamebanana.com/tuts/19424) (actores),
  [tuts/17803](https://gamebanana.com/tuts/17803) (armas),
  [tools/23430](https://gamebanana.com/tools/23430) (PhysicsTool)

## → Aplicable a GameJamm

1. **La estructura "per-actor con componentes + herencia $parent" es la que ya tenemos**
   (DataAssets por componente en UE5) — validar que cada `UBuildPartData`/
   `UJamFusionMaterialData` siga ese split.
2. **El sitio de datos crudos es la fuente de verdad de attach params y valores de
   dispositivos**: copiar la *forma* de las tablas (bone + offsets + subtype tags), no los
   valores originales.
3. Los CSVs comunitarios = base de nuestra tabla de FuseAP para el MVP (retocando valores
   propios).
4. Si algún día se analiza el binario: Ghidra + Switch loader (el mismo stack que usa la
   recompilación estática de la referencia) — y REA automatiza ese flujo
   ([07-guia-rea.md](07-guia-rea.md)).
