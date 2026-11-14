---
title: FORMATO — Convención de los MD de research
date: 2026-11-14
tags:
  - meta
  - formato
  - obsidian
status: listo
---

# FORMATO — Convención de formato de los docs de research

> [!info]
> Convención obligatoria para todo doc de `docs/mechanics-research/`. Está escrita en el
> repo para que se aplique igual **con o sin skill de Obsidian** (`.opencode/skills/`,
> solo la cargan agentes de OpenCode): el formato es Obsidian-flavored Markdown y
> **GitHub lo renderiza** sin Obsidian. Quien tenga las skills cargadas tiene además la
> referencia completa de sintaxis en `.opencode/skills/obsidian-markdown/`.

## 1. Frontmatter (YAML) — obligatorio

```yaml
---
title: NN — Título corto del doc
date: 2026-11-13        # fecha de la investigación
tags:
  - research
  - construccion        # 2-5 tags por tema
status: listo           # listo | borrador | deprecado
---
```

- `title` lleva el número del doc (`NN —`). El H1 del cuerpo repite ese título.

## 2. Callouts (bloques `> [!tipo]`)

Solo estos 4 tipos (GitHub los muestra como cajas de colores). Cada línea del callout
empieza con `> `:

| Callout | Uso |
|---|---|
| `> [!info]` | Primer bloque tras el H1: qué es el doc, alcance, etiquetas de evidencia usadas |
| `> [!tip]` | El hallazgo/veredicto central (lo primero que hay que leer si hay prisa) |
| `> [!warning]` | Riesgos, avisos legales, cosas que se rompen |
| `> [!note]` | Notas de edición, aclaraciones, datos dirigidos a GameJamm |

No usar otros tipos (`[!quote]`, `[!example]`, ...): la paleta del repo es esa.

## 3. Estructura del doc

1. `# NN — Título` (H1 = frontmatter `title`).
2. Callout `[!info]` de contexto (y `[!tip]` si hay un veredicto central).
3. Secciones `##` numeradas (1., 2., ...) — una idea por sección; **tablas** para
   cualquier dato numérico o comparativo.
4. **Última sección obligatoria: `## → Aplicable a GameJamm`** con la lista accionable
   (decisiones, constantes tunables, tests). Si el doc no produce acciones, poner
   "ninguna" y por qué.

## 4. Enlaces

- **Enlaces relativos a archivos `.md`**: `[08-mapeo-ue5-impl.md](08-mapeo-ue5-impl.md)`.
- **Nunca `[[wikilinks]]`**: GitHub no los renderiza (Obsidian sí, pero el repo es
  público en GitHub primero).
- URLs externas solo si son **neutras** (§5). Si la URL de una fuente lleva el nombre
  del juego en el slug → citar la fuente **sin URL** (tipo de fuente + qué aportó) y
  listarla en [FUENTES.md](FUENTES.md).

## 5. Reglas de contenido (repo público del jam)

- **Cero nombres del juego fuente** (títulos, personajes, lugares, objetos,
  habilidades): usar "el juego de referencia", "la referencia", "el predecesor";
  habilidades → genérico (construcción, fusión, autoconstrucción); dispositivos →
  nombre genérico con el nombre datamineado entre paréntesis si aporta
  ("Ventilador (Fan)").
- **URLs del ecosistema: prohibidas.** Solo neutras: GDC Vault corto, YouTube watch
  ID, arXiv/SSRN/papers, foros de UE, repos GitHub sin marcas del juego, REA y
  herramientas de modding sin marcas del juego.
- Términos técnicos de RE permitidos (son datos, no marcas del juego): `uking`,
  `U-King`, `ksys`, `KingSystem`, `Phive`, `Havok`, `BYML`/`AAMP`/`SARC`, claves de
  datos (`MaxLife`, `AttachmentParam`, ...). "Nintendo" como empresa sí; el nombre del
  juego, no.
- **Citas**: `(tipo de fuente · FUENTES #N)` o `*(portal de guías)*`. Etiquetas de
  evidencia: **[fuente única]**, **[solo snippet]**, **[inferencia]**, **[code]**,
  **[page]**, **[memory]**.
- Toda afirmación fuerte lleva fuente; todo dato numérico importante va a tabla.

## 6. Checklist para un doc nuevo

- [ ] Frontmatter completo (`title`/`date`/`tags`/`status`).
- [ ] Callout `[!info]` inicial + `[!tip]` con el hallazgo.
- [ ] Secciones numeradas, tablas para datos.
- [ ] Sección final `## → Aplicable a GameJamm`.
- [ ] Enlaces relativos (sin wikilinks); fuentes citadas contra [FUENTES.md](FUENTES.md).
- [ ] `git grep -i` de términos del juego fuente → 0 matches (reglas §5).
- [ ] Fila nueva en [00-INDEX.md](00-INDEX.md) y entradas nuevas en [FUENTES.md](FUENTES.md).
