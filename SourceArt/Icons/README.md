# Iconos de inventario

48 iconos de 256×256 (PNG con transparencia) para HUD/inventario: las 35 piezas de construcción de `JamBuildPart.cpp`,
armas, materiales de fusión y pickups. Nombre `T_Icon_<id>.png` = asset sin `SM_` (p. ej. `T_Icon_PART_PLANK.png`),
como pide [PROMPTS-ASSETS.md](../../docs/assets/PROMPTS-ASSETS.md) §7.

![todos](_icon_sheet.png)

## Iconos de HUD (`UI/`)

14 iconos `T_UI_*` para lo que pide el GDD (HUD y pantallas): `Tower_Red/Blue/Off` y `Base_Red/Blue/Off` (arriba al
centro; la versión `Off` es la estructura caída en gris), `Energy`, `Power_On/Off` (construcción encendida/apagada),
`Health`, `Backpack` (mochila), `Chest`, `ChestRare` (aviso de cofre raro) y `EnergyOrb`.

![hud](UI/_ui_sheet.png)

- Misma cámara 3/4, luz y paleta para todos, contorno oscuro.
- Importar a UE como textura de UI: `Texture Group = UI`, `Compression = UserInterface2D`, sin mipmaps.
- Regenerar: `blender --background --factory-startup --python SourceArt/Building/scripts/gen_icons.py -- SourceArt/Icons`
  y la lámina con `icon_sheet.py`.
