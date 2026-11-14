# Iconos de inventario

46 iconos de 256×256 (PNG con transparencia) para HUD/inventario: las 35 piezas de construcción de `JamBuildPart.cpp`,
armas, materiales de fusión y pickups. Nombre `T_Icon_<id>.png` = asset sin `SM_` (p. ej. `T_Icon_PART_PLANK.png`),
como pide [PROMPTS-ASSETS.md](../../docs/assets/PROMPTS-ASSETS.md) §7.

![todos](_icon_sheet.png)

- Misma cámara 3/4, luz y paleta para todos, contorno oscuro.
- Importar a UE como textura de UI: `Texture Group = UI`, `Compression = UserInterface2D`, sin mipmaps.
- Regenerar: `blender --background --factory-startup --python SourceArt/Building/scripts/gen_icons.py -- SourceArt/Icons`
  y la lámina con `icon_sheet.py`.
