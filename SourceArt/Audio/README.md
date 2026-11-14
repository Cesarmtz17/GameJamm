# Audio

Dueño: Emilio (área 5). Todo **sintetizado por código** (numpy, sin samples de terceros): original y sin licencias que
declarar. Cubre la tabla de feedback y audio del GDD §17. WAV 44.1 kHz, 16-bit.

## Efectos (`SFX/`, mono)

| Archivo | Cuándo suena (GDD §17) |
|---|---|
| `SFX_Build_Pop` | Cada pieza que aparece al armar el asistente |
| `SFX_Build_Attach` | Unir piezas / golpe en madera |
| `SFX_Power_On` / `SFX_Power_Off` | Encender / apagar la construcción (C) |
| `SFX_Motor_Loop` | Zumbido de motor mientras está encendida (loop) |
| `SFX_Energy_Empty` | Sin energía (bajón eléctrico) |
| `SFX_Fire_Loop` | Fuego / lanzallamas (loop) |
| `SFX_Ice` | Hielo / congelar |
| `SFX_Shock` | Rayo / chispa |
| `SFX_Explosion` | Bomba, destrucción |
| `SFX_Cannon` | Disparo de cañón |
| `SFX_Hit` | Golpe con arma |
| `SFX_Sword_Swing` | Swing de espada |
| `SFX_Tower_Shot` | Disparo del cristal de la atalaya |
| `SFX_Tower_Destroyed` | Torre destruida (derrumbe + explosión) |
| `SFX_Fanfare` | Fanfarria corta (torre caída, victoria) |
| `SFX_Loot_Pickup` | Recoger loot ("tin") |
| `SFX_Backpack_Full` | Mochila llena (error suave) |
| `SFX_Death` / `SFX_Respawn` | Muerte / reaparecer |
| `SFX_Chest_Open` | Abrir cofre |
| `SFX_UI_Click` | Botones de menú |
| `SFX_Amb_Wind_Loop` | Ambiente de viento en las islas (loop) |

## Música (`Music/`, estéreo, loops exactos)

| Archivo | Uso | Tempo |
|---|---|---|
| `MUS_Menu_Loop` | Menú y pantalla final | 84 bpm |
| `MUS_Match_Loop` | Partida | 112 bpm |
| `MUS_Match_Intense_Loop` | Muerte súbita (sube la intensidad) | 140 bpm |

## En Unreal

- Importar los WAV a `Content/Game/Audio/`. En los `_Loop`: **Looping = true**.
- Sugerido: Sound Class `Music` y `SFX` para el volumen de Opciones; atenuación 3D en los SFX del mundo (fuego, motor, torre).
- Regenerar: `blender --background --factory-startup --python SourceArt/Audio/scripts/gen_sfx.py -- SourceArt/Audio/SFX`
  y `gen_music.py -- SourceArt/Audio/Music`. Para cambiar un sonido, editar su función y volver a correr.
