---
title: 05 — RE: interior del motor del juego de referencia
date: 2026-11-13
tags:
  - research
  - re
  - motor
status: listo
---

# 05 — RE: interior del motor del juego de referencia

> [!info]
> Investigación deep-research con fuentes (2026-11-13). Todo conocimiento público: repos
> open source, wikis técnicas y charlas GDC. No incluye ROMs/keys/emuladores.
> "La referencia" = el juego fuente (2023); "el predecesor" = su entrega anterior (2017).

## 1. Panorama: "KingSystem" (ksys) + uking + Phive/Havok

- La referencia y su predecesor corren sobre el **motor in-house de la desarrolladora**. El
  ejecutable interno se llama `uking` ("U-King"), de ahí `KingSystem` (`ksys`) y el nombre
  del mod manager "UKMM". *(repo del mod manager · proyecto de decomp del predecesor)*
- La decompilación del predecesor (v1.5.0 Switch) estructura el código en dos capas:
  - `ksys` (framework del motor: recursos, física, eventos, química, save)
  - `uking` (lógica de juego: actores, AI, UI)
  - Librerías base: `sead` (contenedores/hebras/heap), `agl` (gráficos/parámetros), Havok.
  *(docs navegables del proyecto de decomp)*
- **Phive**: librería de física propia construida **encima** de Havok (una rama PhysX 3.4
  existe en otros juegos). Provee a cada actor un `ControllerSet` de controladores de
  física. *(wiki técnica de la generación reciente)*
- El predecesor integra **Havok Physics, Havok AI (NavMesh) y Havok Cloth**; hubo evidencia
  de Havok Script pero eligieron su EventFlow in-house. *(wiki del proyecto de decomp)*
- La referencia renombra los archivos de física con extensiones Phive: `.bphcl` (cloth),
  `.bphhb` (helper bones), `.bphnm` (nav mesh), `.bphsc` (static compound), `.bphsh`
  (shapes/rigid bodies). *(repositorio comunitario de formatos)*
- GDC 2024 (Takayama): Havok como base + capa in-house; **todo physics-driven sin
  excepción**. (ver [04](04-quimica-fisica-gdc.md))
- La **recompilación estática de la referencia → x86-64** (7k★, proyecto comunitario)
  confirma a nivel de código: módulos `main`, `sdk`, `subsdk0`, `rtld`; 68,968 sitios de
  frontera OS (SVCs, TLS, `x18`, jump tables) en `main` 1.4.3; gráficos NVN; RomFS con zstd;
  análisis con **Ghidra + Switch loader, Capstone, llvm-objdump**. *(su docs/architecture.md)*

**Qué revela:** la desarrolladora no reescribió la física — envolvió Havok
(`ksys::phys::System` → `hkpRigidBody`) con tablas de materiales, controladores Phive por
actor y una capa de química propia. Masa/volumen autocalculados de forma+material con
correcciones manuales donde el gameplay lo pedía.

## 2. Física como wrapper (decompilado, predecesor)

- `ksys::phys::System` orquesta; `ksys::phys::RigidBody` decide el tipo por dato: FromShape,
  Ragdoll, TerrainHeightField, StaticCompoundBody, CharacterController, TeraMesh.
  `ContactMgr` y `MaterialTable` completan el sistema. Job threads `PreCalc/Calc1/Calc2`
  sincronizados con `HavokWorkerMgr` y `BaseProcMgr` (todos los actores son
  `ksys::act::BaseProc`). *(docs del proyecto de decomp)*
- Método para leer Havok sin símbolos: cruzar los **datos de reflexión** del binario con
  headers públicos de Havok 2014 (los nombres de clase casi coinciden). *(wiki del proyecto
  de decomp · repo de notas RE del predecesor)*

## 3. Chemical engine (`ksys::chm`) — el sistema mejor documentado

*(código decompilado navegable · datos convertidos a YAML · wiki técnica de modding —
enlaces en [FUENTES.md](FUENTES.md))*

### 3.1 Elementos (`SystemConfig::Element`)
`temperature`, `effect_type`, `attribute` (bitflags), `life_frames`, `wetting_rate`,
`wind_speed`, `electric_charge`, `shock_wave_speed`, `fire_level`, `moisture`.

### 3.2 Materiales (`SystemConfig::Material`) — unidades físicas reales
`heat_capacity [J/gK]`, `thermal_conductivity [W/m·K]`, `electrical_resistivity [Ω·m]`,
`electrical_capacitance [F]`, `electromotive_force [V]`, `ignition_point [°C]`,
`burn_speed`, `wetting_amount [kg/m²]`, `fire_proof_level` + nombre de elemento-respuesta
(`burn_res_element_name: chemi_fire`, `exp_res_element_name: bomb_exp`,
`electricity_res_element_name: electrification`, `ice_magic_res`, `water_res`, `warm_res`).

Materiales definidos en datos: `player, iron, tree_leaf, grass, stone, wood, enemy, bomb,
fuse, cloth, ceramic, ice, weapon_fire, oil, electric_generator (electromotive_force 4.0),
lava, ivy_burn, polymer, terrain_grass_mesh, ice_hull, weapon_wood, weapon_metal, Horse,
beam, fruit, IceMagic, wood_fuel, NPC, Meat...`

### 3.3 Reglas de mundo (`SystemConfig::World`)
`heat_transfer_speed_rate 24.0` · `fire_heat_transfer_speed_rate 2.0` ·
`conduction_speed_rate_log -5.0` · `burn_speed_rate 0.4` · `lightning_distance 25-35 m` ·
`electric_water_extra_radius_rate 500.0` (el agua "fuga" electricidad 5× más lejos) ·
`min_ice_size 0.15 m` · ratios de derretimiento de hielo vs temperatura ambiente/aire
cálido/fuego · `wind_lift_rate` (sustentación/arrastre) · `character_ignition_count 90`
frames para prender un personaje.

### 3.4 Terreno y clima
- El material de superficie (hierba/arena/nieve/roca) define el chem material aplicado;
  `terrain_grass_mesh` (ignition 250°, burn_speed 1.0) enciende la hierba. Formatos de
  terreno: `.tscb`, `.bhtmp` (heightmaps), `.beco` (ecosistemas), `.vsts`.
  *(repositorio comunitario de formatos)*
- El save guarda el clima por área (próximos 3 días, 6 periodos de 4 h empaquetados en
  nibbles). *(wiki técnica de modding)*
- La referencia: parámetros chem por actor en `ChemicalParam` (ej.: `DirectMass: 100`,
  `IsSplitIce`, `BoilingDuration`, `IsCreateTemperatureRB` — ¡el objeto caliente crea un
  rigid body de temperatura!). *(sitio de datos crudos de la comunidad)*

## 4. EventFlow / AINB (lógica de gameplay)

- **EventFlow** (`ksys::evt`): flowcharts `.bfevfl` con `evt::Manager`, `OrderParam`
  (Int/Float/String/Bool/Actor), actor bindings; recursos `.bdemo` para cutscenes con
  overrides de clima/hora y **`mIsStopChemical`** (pausa la química en diálogos).
  *(docs del proyecto de decomp · lib de la comunidad)*
- **La referencia migró gran parte a AINB (AI Node Binary)**: behavior trees con nodos,
  queries, blackboard y expresiones (formato compartido con otro título de la misma
  desarrolladora). *(wiki técnica · parser de la comunidad)*
- Animaciones: **ASB/BAEV** (slots, selectores, blenders 1D/2D). *(wiki técnica)* ·
  Sonido/efectos: **XLINK2** (`.bslnk`/`.belnk`). *(sitio de formatos de la
  desarrolladora)*

## 5. GameDataMgr / estado del mundo

- Flags del juego con `ResetType`: 0=no reset, 1=evento de reinicio nocturno,
  2=regeneración de zona, 3=medianoche, 4=aparición de una criatura legendaria;
  `GameDataMgr::calc` resetea hasta 1024 flags por tick. *(wiki técnica de modding)*
- Mods de armas en save: `AttackUp 0x1, DurabilityUp 0x2, CriticalHit 0x4, LongThrow 0x8,
  MultiShot 0x10, Zoom 0x20, QuickShot 0x40, GuardUp 0x100`; durabilidad guardada como
  `PorchItem_Value1 = durabilidad*100`. *(wiki técnica de modding)*

## 6. Charlas GDC / investigación académica

| Fuente | Contenido |
|---|---|
| [GDC 2024: "Evolving Physics and Sounds"](https://gdcvault.com/play/1034667/) · [YouTube](https://www.youtube.com/watch?v=N-dPDsLTrTE) | Physics-driven total, masa autocalculada, ruedas de 3 cuerpos, cadenas, agua por área proyectada, rebobinar físico, audio físico |
| GDC 2017: "Change and Constant" (el predecesor) | Origen de "multiplicative gameplay" |
| [SSRN — análisis académico de la habilidad de construcción](https://papers.ssrn.com/sol3/papers.cfm?abstract_id=5001142) (U. de Gotemburgo, dic 2024) | Análisis MDA de la construcción; fusión/autoconstrucción comparten el núcleo de attachment |
| ASEE 2024 — el juego de referencia en educación de ingeniería | La construcción como **interfaz CAD simplificada** (≈ módulo Assembly de SolidWorks con snapping); los alumnos miden fuerza, par, empuje en el juego |
| Análisis de diseño (canal GMTK) sobre la construcción | Simplicidad; dispositivos con formas del mundo real para transmitir función |

## → Aplicable a GameJamm

1. **Chaos = tu Havok**: wrapper fino (subsystem) + DataAssets de material físico + tablas
   de interacción. No reimplementar la emergencia; ordenar los datos como la
   desarrolladora: Element/Material/World.
2. Los valores con unidades reales del chem engine son **balance listo para copiar** en un
   `UJamChemMaterial` DataAsset (ver [08](08-mapeo-ue5-impl.md) §4).
3. DataAssets divididos por "componente" (PhysicsParam / BowParam / ChemicalParam /
   AttachmentParam / LifeParam) = el split que usa el motor y el que nuestro GDD replica.
4. "mIsStopChemical": pausa la química en diálogos/cinemáticas — detalle de robustez barato.
5. El enfoque "construcción como CAD simplificado" (ASEE) valida el diseño del asistente IA
   del GDD: la receta JSON ≈ un assembly CAD.
