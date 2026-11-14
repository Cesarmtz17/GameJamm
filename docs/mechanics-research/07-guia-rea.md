---
title: 07 — Guía REA para el binario del juego de referencia
date: 2026-11-13
tags:
  - research
  - re
  - herramientas
  - rea
status: listo
---

# 07 — Guía REA (Reverse Engineer Anything) para el binario del juego de referencia

> [!info]
> Repo: <https://github.com/morluto/rea> · Web: <https://rea.tools> · MIT license.
> "One MCP for reverse engineering across binaries, applications, and runtime behavior."
> "La referencia" = el juego fuente (2023).

## 1. Qué es REA

REA es un **MCP server + CLI** que conecta un agente (nosotros) con herramientas de
ingeniería inversa: binarios nativos (pseudocódigo, ensamblado, strings, símbolos, llamadas
y referencias), apps JS/Electron, ensamblados .NET, APKs, artefactos de Godot/Unity, WASM,
SQLite, capturas de red, etc. Cada resultado viene con **Evidence**: evidencia +
limitaciones de cada conclusión, no solo respuestas.

Lo que REA **no** trae de fábrica para la referencia: los formatos propietarios de la
desarrolladora (BYML, AAMP, SARC, Havok .hkx). Para eso se usan las herramientas open source
de la comunidad (doc 06). REA es la pieza correcta para analizar **el código ejecutable del
juego** y para inspeccionar/extraer artefactos genéricos.

## 2. Instalación y arranque

Requisitos: Node.js 22.x (≥22.23.4), 24.x (≥24.22.1) o 26.12.1+, y npm.

> [!note] Estado en esta máquina (2026-11-13): instalado y registrado
> - Node.js actualizado a **v26.12.1** (winget; el v24.15.0 previo no cumplía el requisito).
> - Registro MCP en `C:\Users\javie\.config\opencode\opencode.jsonc`
>   (backup: `opencode.jsonc.rea.backup`) — *reiniciar OpenCode para que el agente lo cargue*.
> - Skill instalada: `C:\Users\javie\.agents\skills\reverse-engineer-anything`.
> - **Pendiente**: no hay proveedor de análisis nativo configurado (Ghidra). Para analizar el
>   binario de la referencia algún día: descargar Ghidra 12.x (≥12.1) y apuntar
>   `GHIDRA_INSTALL_DIR`. Sin proveedor, REA sigue funcionando para: artefactos estáticos,
>   JS/Electron, browser observation, SQLite, etc.

```bash
# Registrar REA en el agente (Claude Code / Codex / Cursor / Gemini CLI...)
npx rea-agents setup

# O instalar el CLI global para uso manual
npm install --global rea-agents
rea --help

# Actualizar después
rea update
```

`npx rea-agents setup` añade el MCP server y las instrucciones de workflow al agente
(con backup de la config previa). Tras instalar: reiniciar el agente.

Verificar proveedores de análisis nativo disponibles:

```bash
rea providers --json      # proveedores y operaciones soportadas
rea capabilities --json   # superficie MCP completa
rea doctor --provider ghidra --json   # diagnóstico de un motor concreto
```

Proveedores de análisis nativo (se configuran aparte, REA no los instala salvo que se
apruebe):

| Proveedor | Notas |
|---|---|
| **Ghidra** | Gratis, soporta **AArch64** (lo que usa la Switch). En Windows es soporte **experimental** (`docs/providers/windows-ghidra-p0.md`) — nuestro caso (Win64). |
| Hopper | macOS-first; REA puede instalarlo con aprobación durante setup. |
| IDA / Hex-Rays | Comercial: hay que tener licencia legal propia. |
| Kuna | Decompilador vía MCP (REA_JEB_MCP_URL para JEB). |

Para binarios ELF/PE/Mach-O con arranque lento (el binario de la referencia es grande),
subir el deadline del proveedor: `REA_GHIDRA_STARTUP_TIMEOUT_MS`, y para decompilar:
`REA_GHIDRA_DECOMPILE_TIMEOUT_SECONDS`.

## 3. Cómo atacar el binario con REA (flujo recomendado)

> [!warning] Regla de oro
> Solo con una copia del juego **legalmente adquirida y de la que seas dueño**; con fines de
> investigación/educación/interoperabilidad. No redistribuir binarios, assets ni
> pseudocódigo. REA tiene el mismo disclaimer (DISCLAIMER.md).

La referencia ejecuta un binario **AArch64** (módulo principal `U-King`, herencia directa
del predecesor, cuyo ejecutable era `U-King.elf`). Los data files (SARC/BYML/AAMP/Havok) se
estudian con las tools de la comunidad (doc 06); el *cómo el motor los usa* se ve en el
binario.

### 3.1 Inspección inicial (sin proveedor, instantáneo)

```bash
# Overview estático: formato, secciones, arquitectura, mitigaciones
rea analyze /ruta/al/extraido/U-King --json > uking-overview.json
```

`rea analyze` en un binario nativo selecciona el workflow nativo (necesita proveedor) y da
runtime candidates/component markers para apps de escritorio. Para artefactos empaquetados:

```bash
rea inspect-artifact /ruta/al/archivo --detail summary --json   # inventario
rea extract-artifact /ruta/al/archivo --json                    # extracción
```

El contenedor SARC de la desarrolladora no está soportado de fábrica: desempaquetar primero
con las tools de la comunidad (doc 06) y usar REA para lo que produzca (binarios, SQLite,
etc.).

### 3.2 Análisis nativo con Ghidra (AArch64)

```bash
# 1) Inventario de funciones + strings
rea inspect /ruta/al/U-King --provider ghidra --json
rea search /ruta/al/U-King "Attach" --kind procedures --provider ghidra --json
rea search /ruta/al/U-King "Fuse" --provider ghidra --json

# 2) Dossier de una función específica
rea function /ruta/al/U-King 0x00900000 --provider ghidra --json
rea function /ruta/al/U-King ksys::act::... --provider ghidra --json

# 3) Pseudocódigo / instrucciones / referencias
rea decompile /ruta/al/U-King 0x00900000 --provider ghidra --json
rea instructions /ruta/al/U-King 0x00900000 --provider ghidra --json
rea xrefs /ruta/al/U-King 0x00900000 --provider ghidra --json

# 4) Trace acotado: cómo una feature llama a otra
rea trace /ruta/al/U-King "attach" --provider ghidra --json

# 5) Grafo def-use de valores dentro de una función (p-code)
rea trace-native-values /ruta/al/U-King <procedure> --provider ghidra --json

# 6) Exportar pseudocódigo de TODO el binario (trabajo pesado, un archivo por función)
rea decompile-all /ruta/al/U-King --output /ruta/al/salida/ --provider ghidra
```

Consejos REA para este binario:
- Es **sin símbolos** (stripped). Buscar por **strings**: los nombres de las habilidades y
  dispositivos del juego, más términos genéricos ("Attach", "Fuse", "Chemical", "Weld",
  "Cooking"...), y por nombres que la RE del predecesor ya catalogó (`ksys::`,
  `KingSystem`, EventFlow...) — la referencia comparte linaje con el predecesor, por lo que
  la tabla de nombres del proyecto de decomp es un mapa útil.
- Usar **snapshots** para no repetir el análisis de importación de Ghidra (caro en un
  binario de este tamaño):

  ```bash
  rea analyze /ruta/al/U-King --provider ghidra --snapshot C:/rea-snapshots/uking.json
  # La siguiente query con el mismo snapshot reusa el resultado exacto si coincide
  ```

- Pseudocódigos grandes: `detail: "summary"` y paginar con `inspect-analysis-view`.
- Si Ghidra en Windows falla: leer `docs/providers/windows-ghidra-p0.md` del repo;
  alternativa es Ghidra headless en WSL/Linux.

### 3.3 Vía MCP (la recomendada para trabajar con agentes)

Tras `npx rea-agents setup`, el agente usa directamente:

```
open_binary { path, provider_id: "ghidra", snapshot_path }
list_procedures / search_procedures { pattern: "Attach" }
analyze_function { address, detail: "summary" }   // dossier; el cuerpo se pagina con inspect_analysis_view
decompile { procedure_pseudo_code }
procedure_references { direction: "incoming"|"outgoing" }
```

El flujo del agente: **pregunta → REA inspecciona y traza → Evidence con código,
referencias y unknowns → el agente explica/implementa/prueba**. Es exactamente el loop que
queremos: "¿cómo detecta el modo construcción los attach points?" → trace → pseudocódigo →
spec para UE5.

### 3.4 Qué preguntarle a REA sobre la referencia (agenda de investigación)

Preguntas que mapean 1:1 a nuestro juego (GameJamm) y que el binario puede responder:

1. **Attach points / pegamento**: ¿cómo se selecciona el punto de anclaje al pegar dos
   cuerpos? (buscar strings `Attach`, `Rigid`, `Weld`). Esperado: constraint rígida entre
   los 2 cuerpos + snap a la superficie más cercana.
2. **Despegue por wiggle**: ¿qué umbral de input/tiempo detecta el "shake"?
3. **Fusión**: ¿la tabla de valores material→daño está en el binario o en BYML de gamedata?
   (casi seguro en data files: doc 06). En el binario: la lógica de por qué no se pueden
   fusionar dos armas.
4. **Dispositivos**: nombres internos de los devices (`Fan`, `Rocket`, `Steering...`) y su
   integración con el sistema de energía.
5. **Chemistry**: entrada `Chemical`/`Burnable`/`Conductor` — los flags por-material que
   sabemos por la RE del predecesor (doc 05) y que en la referencia se amplían.
6. **Límite de piezas por creación**: buscar el contador/constante que rechaza la n-ésima
   pieza.

### 3.5 Límites y expectativas realistas

- El binario de la referencia es ~decenas de MB AArch64 sin símbolos: el análisis completo
  es inviable en un gamejam; solo valen **preguntas quirúrgicas**.
- Ghidra en Windows es soporte P0 experimental; si da problemas, Ghidra headless bajo WSL.
- La "verdad" del gameplay (valores de daño, costes de batería) casi seguro vive en los
  **data files** (BYML/AAMP), no en el código: ahí las tools de la comunidad son más
  rápidas que decompilar (doc 06).
- REA exige análisis local; no sube tu app/binario a ningún sitio (FAQ del README).

## 4. Script de arranque "REA + referencia" (plantilla)

```powershell
# 1) Setup (una vez)
npx rea-agents setup
rea providers --json   # verificar proveedor

# 2) Preparar target (solo desde TU copia legal; extracción con tools de la comunidad, doc 06)
$UK = "C:\rea-work\U-King"          # módulo principal extraído
rea analyze $UK --provider ghidra --snapshot C:\rea-work\uking.json --json

# 3) Sesión de preguntas quirúrgicas
rea search $UK "Attach" --provider ghidra --json
rea trace $UK "Weld" --provider ghidra --json
rea decompile $UK 0x... --provider ghidra --json
```

## 5. Fuentes REA

- Repo: <https://github.com/morluto/rea> (README, docs/getting-started/cli.md,
  docs/guides/native/native-investigation.md, DISCLAIMER.md, SECURITY.md)
- Web + guías ilustradas: <https://rea.tools> y <https://rea.tools/guides/>
- Casos showcase (metodología a copiar):
  [DX-Ball sound-pan](https://rea.tools/showcase/dx-ball/), Notion clipboard bridge, TH04
  bullet-ring — patrón: *preguntar → trazar a la función → reconstruir el cálculo en código
  propio → validar contra el original*.
