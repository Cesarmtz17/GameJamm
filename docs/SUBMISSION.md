# Checklist de entrega

Deadline: **15 nov 2026, 8:59 PM EST (7:59 PM CDMX)**. Tarde = descalificado.

## Build
- [ ] Empaquetado Windows (Shipping o Development), probado en una PC **distinta** a la que lo construyó.
- [ ] Zip `cavinos_PromptAndConquer.zip`, **< 1 GB**.
- [ ] Subido al itch.io de un integrante y enviado a la jam.
- [ ] Se puede jugar **solo** (vs bots) sin configurar nada; si el asistente IA no tiene red, el fallback funciona.

## Página / formulario
- [x] Nombre del equipo: **cavinos** (Javier, 15 nov; minúsculas).
- [ ] Nombre de cada integrante + **email de su cuenta de Epic Games**
- [x] Nombre del juego: **Prompt & Conquer** (Javier, 15 nov).
- [ ] Link a **video de gameplay de 30–60 s**
- [ ] Herramientas usadas (motores, modelos IA, generadores de assets — Rodin/Meshy/Tripo/etc. si se usan, aplica a premios de partners)
- [ ] Assets de terceros con su licencia (Fab, sonidos, etc.)
- [ ] Declarar cualquier cosa hecha antes del 12 nov (si aplica)
- [ ] Link al devlog **público** (obligatorio para Glass Box). El repo está público hoy, así que `docs/DEVLOG.md` sirve de respaldo, pero el link principal debe ser un devlog legible (itch.io); ver [DEVLOG.md](DEVLOG.md)
- [ ] Marcar en el formulario los Modifiers que perseguimos (ver "Metas de premios")
- [ ] Controles explicados en la página
- [x] Inglés obligatorio verificado en las reglas oficiales, §8(i): texto del juego, audio, video e información de entrega.

## Metas de premios

Fuente: [página de la jam](https://itch.io/jam/2026-epic-ai-megajam), leída el 13 nov 2026 (resumen automático: **confirmar premios y requisitos en la página antes de enviar**).
Los **Modifiers** son categorías bonus con premio propio, juzgadas aparte de las categorías principales.

> **Verificado (15 nov):** las [reglas oficiales](https://epicgames.ent.box.com/shared/static/a7xcgl07fqoieeb99sjtd3juhj4agc4z.pdf),
> §10 y §12, limitan los premios a uno principal y un Modifier por entrega. §10 contempla una excepción si no quedan otros
> ganadores elegibles para categorías adicionales. El límite es sobre ganar premios; falta revisar las casillas del formulario.

| Categoría | Premio | Meta | Qué exige |
|---|---|---|---|
| Best Overall — Unreal Engine (principal) | $7,500 / $5,000 / $2,500 | **Sí** | Juego completo de punta a punta |
| Best Student Project (principal) | $7,500 / $5,000 / $2,500 | Por confirmar | **Todos** los integrantes inscritos actualmente en educación superior (reglas §1(f), §4) |
| **Best Team** (Modifier) | $5,000 + premios de partners | **Sí** | 2+ integrantes (somos 5); sin trabajo extra |
| **Glass Box** (Modifier) | $2,500 + una ASUS ROG Ally por integrante | **Sí, prioridad** | Link a un devlog **público** de cómo construimos el juego |
| Render Royalty (Modifier) | $2,500 + premios de partners | Stretch | Mejores visuales; solo si el look está decente en el paquete final |
| ↳ OpenAI Award | $2,500 en créditos de API | Stretch | Entrar a Render Royalty y usar modelos o productos de OpenAI en desarrollo, gameplay o ambos |
| ↳ NVIDIA RTX Award | Una RTX 5090 | Solo si da tiempo | Entrar a Render Royalty y usar al menos una feature de NvRTX (la página cita DLSS 4.5 y NVIDIA ACE); responder las preguntas de NVIDIA al enviar |
| Solo Driver, Pocket Powerhouse, Walkie-Talkie | — | No aplica | Desarrollador solo / juego móvil / UEFN |

Premios de partners (Hyper3D Rodin, Tripo, Meshy): aplican a todas las categorías; si usamos Rodin, listarlo en "Herramientas usadas".
Aparte, Best Overall — Unreal Engine tiene un OpenAI Award ($5,000 en créditos) por uso de modelos o productos de OpenAI;
no exige entrar a Render Royalty. Hay que responder la pregunta de OpenAI en el formulario.

### Pendientes por Modifier
- [x] Reglas de premios verificadas: límite y excepción descritos arriba. Falta comprobar si el formulario permite marcar varias candidaturas.
- [ ] **Glass Box:** devlog público con link probado antes de enviar (15 nov, 19:59 CDMX). Plan en [DEVLOG.md](DEVLOG.md).
- [x] **OpenAI (15 nov, Javier):** sí. **Gameplay:** la OpenAI Decisions API (`gpt-6-luna`) decide las piezas de construcciones y armas (GDD §5.3). **Desarrollo:** Codex y su generación de imágenes (íconos de loot), más gpt-image-2 para conceptos. Best Overall OpenAI no exige Render Royalty. Falta contestar el campo opcional de OpenAI del formulario.
- [ ] **NVIDIA:** decidir si hay tiempo para una feature NvRTX; si no, no aplicar.
- [ ] **Render Royalty:** decidir con el equipo de arte si el look llega; si no, quitarlo de la lista antes de enviar.

## Assets de terceros / herramientas (ir llenando)
Prellenado desde el historial del repo el 13 nov: **confirmar licencias y completar**.

| Asset / herramienta | Fuente | Licencia | Quién lo agregó |
|---|---|---|---|
| Claude Code (agentes del equipo: código, docs, research) | Anthropic | Servicio; términos del proveedor | prellenado |
| gpt-image-2 vía OpenClaw (conceptos de arte en `docs/concepts/`) | OpenAI | Servicio; por confirmar términos de uso de las imágenes | prellenado |
| Unreal MCP (plugin de UE 5.8) | Epic Games | Por confirmar | prellenado |
| Blender 5.3 (arte 3D por scripts en `SourceArt/**/scripts/`: 104 modelos, mapa, iconos, hoja de estilo; no se usó Blender MCP) | Blender Foundation | GPL (la herramienta; los assets generados son nuestros) | Emilio |
| Audio original conservado en `SourceArt/Audio/` (16 SFX + 3 loops de música; los otros 7 SFX se reemplazaron con Stable Audio 3) | Sintetizado por código propio (numpy) | Original del equipo, sin samples de terceros | Emilio |
| Claude (agente de Emilio vía OpenClaw: scripts de Blender y audio, docs de arte) | Anthropic | Servicio; términos del proveedor | Emilio |
| Jev / Decisions API (`api.typesafe.ai`), OpenRouter, fal (asistente de IA en gameplay) | Typesafe, OpenRouter, fal | Servicios; términos del proveedor | prellenado |
| OpenAI Decisions API, modelo `gpt-6-luna` (capa de decisión en gameplay: elige las piezas de construcciones y armas) | OpenAI | Servicio de API; términos del proveedor; clave con tope solo en el zip | Javier (15 nov) |
| Codex CLI y su generación de imágenes (25 íconos de loot `SourceArt/UI/Icons`, UI del HUD) | OpenAI | Servicio (plan ChatGPT Plus de Javier); imágenes nuestras según términos | Javier (15 nov) |
| Generación de imágenes integrada de Codex (`image_gen`): 6 íconos adicionales de viga, bola, vela, hielo, luz y estabilizador; 31 íconos en total | OpenAI | Servicio; términos del proveedor | Javier (15 nov) |
| Stable Audio 3 small-sfx, local: 7 efectos aprobados e importados (`SFX_Build_Pop` v1, `SFX_Hit` v3, `SFX_Sword_Swing` v1, `SFX_Explosion` v2, `SFX_Tower_Shot` v1, `SFX_Tower_Destroyed` v1 y `SFX_Chest_Open` v4). Crédito: "Sound effects made with Stable Audio 3 (local). Powered by Stability AI." | Stability AI | Stability AI Community License | Javier (15 nov) |
| Hyper3D Rodin Gen-2.5 (piezas de arma de mano y props) | Deemos / Hyper3D | Servicio; términos del proveedor | Javier (14–15 nov) |

**Selección de audio (15 nov):** Javier conservó la música de partida, `SFX_Loot_Pickup` y `SFX_Fanfare` originales. Las otras músicas y efectos todavía conservan sus originales. Para el cofre eligió `SFX_Chest_Open` v4 (1.05 s), ya importado sobre el asset existente con lock; es el séptimo reemplazo.

**Idioma (15 nov):** Javier pidió la interfaz completa en inglés. Traducción de código y datos lista; compilación y prueba de esa localización todavía pendientes. Ejemplos previstos para la página y el video: `an ice rifle` y `a car that burns everything`. La elección del inglés viene de Javier; no se presenta como requisito de la jam.
