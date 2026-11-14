# Checklist de entrega

Deadline: **15 nov 2026, 8:59 PM EST (7:59 PM CDMX)**. Tarde = descalificado.

## Build
- [ ] Empaquetado Windows (Shipping o Development), probado en una PC **distinta** a la que lo construyó.
- [ ] Zip `TeamName_GameName.zip`, **< 1 GB**.
- [ ] Subido al itch.io de un integrante y enviado a la jam.
- [ ] Se puede jugar **solo** (vs bots) sin configurar nada; si el asistente IA no tiene red, el fallback funciona.

## Página / formulario
- [ ] Nombre del equipo
- [ ] Nombre de cada integrante + **email de su cuenta de Epic Games**
- [ ] Nombre del juego
- [ ] Link a **video de gameplay de 30–60 s**
- [ ] Herramientas usadas (motores, modelos IA, generadores de assets — Rodin/Meshy/Tripo/etc. si se usan, aplica a premios de partners)
- [ ] Assets de terceros con su licencia (Fab, sonidos, etc.)
- [ ] Declarar cualquier cosa hecha antes del 12 nov (si aplica)
- [ ] Link al devlog **público** (obligatorio para Glass Box). El repo está público hoy, así que `docs/DEVLOG.md` sirve de respaldo, pero el link principal debe ser un devlog legible (itch.io); ver [DEVLOG.md](DEVLOG.md)
- [ ] Marcar en el formulario los Modifiers que perseguimos (ver "Metas de premios")
- [ ] Controles explicados en la página

## Metas de premios

Fuente: [página de la jam](https://itch.io/jam/2026-epic-ai-megajam), leída el 13 nov 2026 (resumen automático: **confirmar premios y requisitos en la página antes de enviar**).
Los **Modifiers** son categorías bonus con premio propio, juzgadas aparte de las categorías principales.

> **Ojo:** un resumen de prensa dice que cada entrega puede ganar **una** categoría principal y **un** Modifier. La página no lo
> confirmó en lo que leímos. Si es cierto, hay que elegir dónde poner el esfuerzo (ver "Pendientes").

| Categoría | Premio | Meta | Qué exige |
|---|---|---|---|
| Best Overall — Unreal Engine (principal) | $7,500 / $5,000 / $2,500 | **Sí** | Juego completo de punta a punta |
| Best Student Project (principal) | $7,500 / $5,000 / $2,500 | ¿Alguien? | Alguien inscrito hoy en una institución de educación superior |
| **Best Team** (Modifier) | $5,000 + premios de partners | **Sí** | 2+ integrantes (somos 5); sin trabajo extra |
| **Glass Box** (Modifier) | $2,500 + una ASUS ROG Ally por integrante | **Sí, prioridad** | Link a un devlog **público** de cómo construimos el juego |
| Render Royalty (Modifier) | $2,500 + premios de partners | Stretch | Mejores visuales; solo si el look está decente al feature freeze |
| ↳ OpenAI Award | $2,500 en créditos de API | Stretch | Entrar a Render Royalty y usar modelos o productos de OpenAI en desarrollo, gameplay o ambos |
| ↳ NVIDIA RTX Award | Una RTX 5090 | Solo si da tiempo | Entrar a Render Royalty y usar al menos una feature de NvRTX (la página cita DLSS 4.5 y NVIDIA ACE); responder las preguntas de NVIDIA al enviar |
| Solo Driver, Pocket Powerhouse, Walkie-Talkie | — | No aplica | Desarrollador solo / juego móvil / UEFN |

Premios de partners (Hyper3D Rodin, Tripo, Meshy): aplican a todas las categorías; si usamos Rodin, listarlo en "Herramientas usadas".
Aparte, la categoría principal Best Overall — Unreal Engine tiene un OpenAI Award ($5,000 en créditos); su criterio exacto está por confirmar en la página.

### Pendientes por Modifier
- [ ] Confirmar en las reglas/FAQ si se puede ganar más de un Modifier por entrega y si el formulario deja marcar varios.
- [ ] **Glass Box:** devlog público con link probado antes del feature freeze (15 nov, 12:00 CDMX). Plan en [DEVLOG.md](DEVLOG.md).
- [ ] **OpenAI:** decidir si lo perseguimos. Ya hay uso en desarrollo (conceptos de arte con gpt-image-2); falta decidir con el dueño de Building si el asistente usa un modelo de OpenAI en gameplay, y declararlo.
- [ ] **NVIDIA:** decidir si hay tiempo para una feature NvRTX; si no, no aplicar.
- [ ] **Render Royalty:** decidir con el equipo de arte si el look llega; si no, quitarlo de la lista antes de enviar.

## Assets de terceros / herramientas (ir llenando)
Prellenado desde el historial del repo el 13 nov: **confirmar licencias y completar**.

| Asset / herramienta | Fuente | Licencia | Quién lo agregó |
|---|---|---|---|
| Claude Code (agentes del equipo: código, docs, research) | Anthropic | Servicio; términos del proveedor | prellenado |
| gpt-image-2 vía OpenClaw (conceptos de arte en `docs/concepts/`) | OpenAI | Servicio; por confirmar términos de uso de las imágenes | prellenado |
| Unreal MCP (plugin de UE 5.8) | Epic Games | Por confirmar | prellenado |
| Blender MCP (arte 3D) | Por confirmar | Por confirmar | prellenado |
| Jev / Decisions API (`api.typesafe.ai`), OpenRouter, fal (asistente de IA en gameplay) | Typesafe, OpenRouter, fal | Servicios; términos del proveedor | prellenado |
