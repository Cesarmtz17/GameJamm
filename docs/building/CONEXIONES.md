# Conexiones y decisiones de construcci?n

El modelo elige componentes, cantidades, ra?z y modo por tipo; el c?digo resuelve las posiciones, las uniones y la viabilidad. La receta final conserva `id`, `type`, `attach_to`, `socket` y `activate`.

## Contrato f?sico

| Pieza hija | Uni?n al padre | Movimiento |
|---|---|---|
| Estructura, asiento, bater?a, pat?n | Soldadura si el padre tambi?n es inerte; uni?n fija en otro caso | Comparte el cuerpo soldado o queda fija al padre |
| Wheel / BigWheel | Eje Y con motor | Giro libre en Y; par limitado por masa y apoyo |
| WagonWheel | Eje Y sin motor | Rueda libre; necesita otra fuente de propulsi?n |
| Hinge | Bisagra Y | ?ngulo limitado por Power, sin motor |
| Turntable | Eje Z con motor | Hace girar lo conectado encima |
| Head | Eje Z | Orienta los componentes montados hacia objetivos |
| Otros dispositivos | Uni?n fija | La capacidad aporta fuerza, disparo u otro efecto |

Las uniones de una pieza colocada sobre un segmento soldado se conectan al cuerpo f?sico de la ra?z de esa soldadura. Los ejes se transforman con la orientaci?n de la construcci?n. Las mallas de arte son visuales; la f?sica usa formas b?sicas.

`socket` usa `Cara[_Borde[_Mira]]`, con prefijo `Attach_` opcional. Ejemplo: `Top_Back_Back` coloca encima, atr?s, mirando hacia atr?s. En h?lices y cohetes `Mira` indica la salida: la fuerza act?a al contrario. X es avance, Y es derecha y Z es arriba. Las ruedas conservan eje Y independientemente de la cara usada para pegarlas.

## Lo que recibe y decide el modelo

`FJamDecisionClient::BuildConnectionGuide()` genera el contexto desde el cat?logo y las reglas compartidas de `JamConnectionRules.h`: masa, potencia, uni?n, modos disponibles, caras candidatas, simetr?a y espacio libre. Se env?a como `state.connection_rules` a Jev y en el contexto del planificador alternativo.

Jev dispone de preguntas `mode_<Tipo>` para los tipos con modos especiales. Con confianza de al menos 0.6 prevalecen sobre las banderas globales anteriores; respuestas ausentes o inv?lidas conservan el respaldo. El planificador alternativo tambi?n acepta ?nicamente modos soportados por ese tipo.

- `default`: colocaci?n habitual; h?lices y cohetes empujan hacia delante. Propeller y Launcher conservan sus valores predeterminados espec?ficos.
- `lift`: h?lices/cohetes para sustentaci?n.
- `launch`: resorte que lanza una carga.
- `inline`: ruedas delante/detr?s, como una moto.

Esto permite Fan en `lift` y Rocket en `default` en la misma construcci?n. Por ahora todas las piezas de un mismo tipo comparten modo: no se pueden pedir dos Fan verticales y otro Fan horizontal mediante la bolsa. No se dan al modelo coordenadas libres ni permiso para inventar potencia.

El ensamblador comprueba solapes, apoyo, centro de masa, direcci?n del empuje, espacio de salida y sustentaci?n. `Coherence`/`Repair` realizan la evaluaci?n y reparaci?n determinista; el resultado no se devuelve al modelo para una segunda decisi?n. La integraci?n completa con inventario sigue pendiente en el flujo actual.

## Verificaci?n y l?mites

`Jam.Building.Connections` comprueba cada tipo del cat?logo a 0? y 90? sobre un segmento soldado: cuerpos conectados, soldadura, restricciones lineales, eje angular, presencia de motor y direcci?n de empuje. Son comprobaciones del ensamblado, no una simulaci?n de todas las parejas posibles.

`Jam.Building.ConnectionDecisions` comprueba el contexto enviado y una respuesta simulada con sustentaci?n y avance separados, hasta su composici?n y coherencia. No mide precisi?n del servicio Jev real.

`Jam.Building.DriveSimulation` simula carro cohete, carro con h?lice y dron en suelo plano. `DriveLimits` comprueba l?mites de tracci?n, turbo y giro; `Tables` verifica cat?logo y recetas.

Pendiente para afirmar cobertura f?sica completa: cargas extremas y roturas, armas/efectos en todas las monturas, combinaciones encadenadas de ejes, pendientes y puentes del mapa, decisi?n real del servicio e inventario. Ejecutar las cinco suites con `Automation RunTests Jam.Building`; reportes locales en `Saved/Automation/Connections`.
