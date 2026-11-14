---
title: FUENTES — Lista maestra de toda la investigación
date: 2026-11-13
tags:
  - research
  - fuentes
status: listo
---

# FUENTES — Lista maestra de toda la investigación

> [!note] Sin enlaces a propósito (2026-11-13)
> Este repo es público (jam): las URLs del ecosistema del juego fuente contienen su nombre,
> así que se quitaron. Cada fuente queda descrita por tipo + qué aportó; solo se conservan
> enlaces a recursos **neutros** (GDC Vault, REA, papers, repos de herramientas sin marcas
> del juego).

## Charlas GDC (fuentes primarias de la desarrolladora)

1. **GDC 2024 — "Evolving Physics and Sounds"** (Dohta, Takayama, Osada) — fuente primaria
   de: mundo 100% physics-driven, ruedas de 3 cuerpos rígidos, cadenas como
   cápsulas+ball-sockets, resistencia al agua por área proyectada, masa autocalculada,
   puertas=slider+motor, rebobinar físico, "physics engine for sound", multiplicative
   gameplay. En GDC Vault (gratis) y con transcripción en YouTube:
   [gdcvault.com/play/1034667/](https://gdcvault.com/play/1034667/) ·
   [youtube.com/watch?v=N-dPDsLTrTE](https://www.youtube.com/watch?v=N-dPDsLTrTE)
2. **GDC 2017 — "Change and Constant"** (el predecesor) — origen de "multiplicative
   gameplay" y el chem engine. Referenciado dentro de la charla 2024.

## Prensa técnica (cobertura de las charlas)

3. The Verge — "how the team made physics feel like magic" — "mundo sin autodestrucción",
   sin programa de vehículo, definición de multiplicative gameplay.
4. GameDeveloper — "how Nintendo did the impossible with [the] physics system" — Havok +
   capa in-house, cálculo de peso por forma, "gut feeling" del gameplay.
5. Digital Trends — las dos capas de física.
6. Portal de una wiki de la franquicia — física: desafíos y triunfos; el conflicto
   fusión↔kinematics.
7. Den of Geek — "create a system that makes fun things happen".
8. TechSpot — filosofía del equipo (GDC).
9. Portal de fan-news — "physics engine for sound" (y el mismo video de la charla).

## Wikis/portales de gameplay (mecánicas y valores)

10. Enciclopedia de la franquicia — **límite de 21 piezas**, pegamento que aguanta agua,
    activación grupal de dispositivos, dispositivos (los 27), rugido de élite.
11. Página oficial de la desarrolladora — las 4 habilidades nuevas: attach points como
    "pegamento verde", despegue por wiggle, activar dispositivos golpeándolos.
12. Portal de guías (Dot Esports) — flujo visual naranja→verde→A, rotación, despegue.
13. Portal de guías (DualShockers) — "bright dot + slime" del attach point, 45°.
14. Portal de guías (Game8) — tabla de controles (apoyar plano, wiggle), guía de fusión,
    armas prístinas, escudo-cohete, moto flotante.
15. Wiki de Fextralife — "un imán sin límite de metal", glow verde.
16. Portal de guías (QuestFern) — límite 21 citado del tip in-game, "build first, carry
    later", guía de fusión (+25/+10/+5/+3 durabilidad, distorsión 95%/132.68%, tabla de 40+
    materiales, multiplicadores por familia), dispositivos, mejoras de batería
    (3→48 pozos, 100 cargas/pozo), autoconstrucción (30+8), esquemas (46).
17. Wiki de la franquicia (Dungeon) — fusión: durabilidad por familia, ×2 colmillos/garras
    en flechas, escudos sin durabilidad, Fuse Attack Power.
18. Enciclopedia de la franquicia (Fandom) — 148 materiales con Fuse AP y efectos, surf de
    escudo/grind, atravesar techos (check rojo/verde, 30 s), rebobinar (congela tiempo,
    línea amarilla, rango casi infinito).
19. Otro portal de guías de la franquicia — consumos en pozos/seg, daño de cañón/bomba de tiempo,
    30 dispensadoras con odds, habilidades.
20. Portal de guías (Legends of Z) — guía de dispositivos.

## Datamines / foros / simuladores

21. Sitio de datos crudos de la comunidad (Kiranico) — attachments, dispositivos, armas con
    BGYMLs expandidos (`AttachmentParam`, `SpObj_*`, `MaxLife 54000`).
22. Reddit (r/ de la referencia) — consumo de baterías por minuto, pegamento anisótropo
    (ley de Hooke), valores mostrados 95%/132.68%, spreadsheet de ~94k fusiones, pesos
    relativos de dispositivos.
23. Canal de datamining en YouTube — costes en frames, multiplicadores por familia,
    durabilidades de dispositivos, brazos mecánicos (+13/nivel).
24. Simulador web de cocina — lógica real recuperada por un modder de la escena.

## Wikis técnicas de RE

25. Wiki técnica de modding — predecesor: BYML, Havok, gamedata, saves, GameDataMgr;
    `ksys::chm` con unidades físicas; Chemical.
26. Wiki técnica de la generación reciente — AINB, ASB, Phive.
27. [nintendo-formats.com](https://nintendo-formats.com/) — formatos de la desarrolladora
    (XLINK2 y familia).
28. Visor de código decompilado navegable (`uking`) — ej. `chmSystemConfigObjs.cpp`.
29. Wiki del proyecto de decomp del predecesor — Havok por reflexión + headers 2014.
30. Docs navegables del proyecto de decomp (DeepWiki) — ksys/uking, física, EventFlow.
31. Repositorio comunitario de formatos — ~50 formatos documentados (BYML v7/BGYML,
    Phive, RESTBL, MeshCodec, ZSTD+diccionarios...).

## Papers / educación

32. [SSRN — análisis MDA de la habilidad de construcción](https://papers.ssrn.com/sol3/papers.cfm?abstract_id=5001142)
    (U. de Gotemburgo, dic 2024) — fusión/autoconstrucción comparten el núcleo de
    attachment.
33. ASEE 2024 — el juego de referencia en educación de ingeniería: la construcción como
    interfaz CAD simplificada (≈ módulo Assembly de SolidWorks con snapping).
34. Medium — exploración del chemistry engine del juego de referencia.
35. CheatCC — chemistry engine del predecesor explicado.
36. Reddit r/howdidtheycodeit — spec/comunidad del chem engine.
37. Game Maker's Toolkit (GMTK) — análisis de diseño de la construcción.
38. Merry-Go-Round Magazine — la construcción y el diseño del juego fuente.

## Repos GitHub (detalle por repo en [06-re-formatos-herramientas.md](06-re-formatos-herramientas.md))

- **Decomp**: proyecto de decomp del predecesor · decomp experimental de la referencia ·
  recompilación estática de la referencia → x86-64 (+ su docs/architecture.md).
- **Formatos/herramientas**: repositorio comunitario de formatos · tool ZSTD con
  diccionarios · suite de análisis de filesystem · TKMM · editor VSCode de formatos ·
  parser ainb · PhysicsTool · HavokLib · hkxcmd.
- **Predecesor**: byml-v2 · aamp · oead · sarc · evfl · BCML/UKMM · repo de notas RE · repo
  de datos en YAML.
- **Datos**: CSVs comunitarios de armas/materiales · repo de gamedata del save · API del
  compendio de items.
- **Implementación UE de referencia**:
  [EvelynSchwab/ComponentFuseMechanic](https://github.com/EvelynSchwab/ComponentFuseMechanic)
  — sistema de fusión/attach inspirado en la construcción del juego fuente, para UE 5.2+
  (Chaos).

## REA (herramienta de análisis)

39. [morluto/rea](https://github.com/morluto/rea) ·
    [rea.tools](https://rea.tools) —
    [CLI guide](https://github.com/morluto/rea/blob/main/docs/getting-started/cli.md) ·
    [native investigation](https://github.com/morluto/rea/blob/main/docs/guides/native/native-investigation.md) ·
    [showcases](https://rea.tools/showcase/)

## Auditoría open-source y validación (docs 09-11)

40. [arXiv 2510.16952](https://arxiv.org/html/2510.16952v1) — LLM genera un JSON DSL que
    un backend determinista valida (sintaxis/tipos/rangos) y ejecuta; defaults para
    parámetros faltantes y fallback inofensivo. Advierte que su error handling "salvó la
    mayoría de las salidas malas" (métricas pass/fail esconden errores) y que no probó
    inputs adversarios. Cita en [09](09-auditoria-fuentes-publicas.md) §6 y
    [11](11-diseno-ensamblado-open-source.md) §4.6.
41. [Waterloo CS-2009-23A](https://cs.uwaterloo.ca/research/tr/2009/CS-2009-23A.pdf) —
    shape grammars: referencia académica de construcción por reglas (no aplicada a
    builds con física; no fetchado).
42. [Spreadsheet de reglas de torneos de Besiege](https://docs.google.com/spreadsheets/d/19loFSAuqFSxgLp28y1vSREPj58C0LOPUbf1n_6Un-6w/edit)
    — precedente débil de caps de presupuesto/piezas (documento comunitario).
43. [OSI — texto de la licencia MIT](https://opensource.org/license/mit) — única
    condición: conservar aviso de copyright y permiso.
44. Derecho de EE.UU. (nota legal de [10](10-repo-open-source-fusion.md) §5):
    §102(b) ideas/procesos fuera de copyright
    ([Berkeley Law](https://www.law.berkeley.edu/center-article/why-copyright-law-excludes-systems-and-processes-from-the-scope-of-its-protection/) ·
    [Baker v. Selden](https://www.lexplug.com/casebrief/baker_v_selden_65bd562dabbe1c5b9f8a38dd)) ·
    §1201 anti-circumvención ([Cornell LII](https://www.law.cornell.edu/uscode/text/17/1201)).
45. Foros y tracker de Unreal Engine — comportamiento de Chaos constraints (evento
    "broken" que no dispara en 5.0.2, [UE-368639](https://issues.unrealengine.com/issue/UE-368639)
    plasticity/drive, snap-back al reactivar simulación),
    [release notes 5.8](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-5-8-release-notes)
    y el assembly toolkit cerrado de Team Expotential. URLs completas en
    [10](10-repo-open-source-fusion.md) §1 y §3.
46. [godot-proposals #1423](https://github.com/godotengine/godot-proposals/issues/1423) —
    el motor no proveerá weld joint nativo.
47. [Nintendo Life (2025)](https://www.nintendolife.com/news/2025/05/nintendo-updates-its-user-agreement-to-crack-down-on-emulation)
    — actualización de términos de cuenta de Nintendo (RE/decomp de servicios
    prohibidos).
48. Prensa y guías de la auditoría (sin URL a propósito: el slug lleva el nombre del
    juego) — Shacknews (recap del panel GDC 2024), GameSpot (guía de autoconstrucción),
    Pocket Tactics, Prima Games, GamesRadar (×2), Twinfinite, GameSkinny, ggrecon,
    GameWith, Game Guides, gamertagmythras, wiki de la franquicia (páginas de esquemas
    preconstruidos), página comunitaria de speedruns (entrelazamiento de fusión), otra
    wiki de la franquicia (glitches) y la entrevista oficial "Ask the Developer" de
    Nintendo (ya listada en #11).

## Nota legal

> [!warning] Solo conocimiento público
> Toda la lista documenta conocimiento público: repos open source (que no incluyen assets
> del juego), wikis técnicas, datamines ya publicados por la comunidad, prensa y charlas
> GDC. Ninguna fuente conduce a ROMs, keys o emuladores; los proyectos de decomp/ports
> exigen una copia propia y legal del juego. Nada de Nintendo assets se redistribuye.
