# Ideas nuevas de gameplay para MotherVR + GRAND

26/10/2026. Lluvia de ideas, no evaluación a fondo. No repite lo que ya está en `ideas_jayp/EVALUACION.md`, `agachar/` ni `microfono/` (dedos, fundas, cambio de mano, recarga manual, melee físico, agacharse real, micrófono). La idea guía: que tu cuerpo real importe, pero que el juego siga decidiendo como en el original (el alien no tiene que volverse más tonto ni más injusto).

Todas se harían con el mismo truco del prototipo de agacharse: nuestro DLL entre GRAND y el juego, que lee visor y mandos por OpenVR y llama funciones que ya existen en AI.exe o simula un botón. Las estimaciones son a ojo: nada de esto está buscado en Ghidra todavía, salvo donde lo digo.

## Resumen

| # | Idea | Factible | Esfuerzo | Por qué suma al horror |
|---|---|---|---|---|
| 1 | Latido en el mando cuando el alien está cerca o te está por ver | Sí | Bajo a medio | Sentís el peligro sin mirar el tracker |
| 2 | Contener la respiración en el locker inclinándote hacia atrás y callándote | Sí | Bajo (con micrófono hecho) | Es el momento más tenso del juego, ahora con el cuerpo |
| 3 | Taparse la boca con la mano baja el ruido del micrófono | Sí | Bajo | Gesto instintivo de miedo que tiene un efecto real |
| 4 | Acercar el tracker a la cara para enfocarlo | Sí | Bajo | Elegir entre mirar el tracker o mirar el pasillo |
| 5 | Prender/apagar la linterna tocándote el hombro o la cabeza | Sí | Bajo | La linterna te delata (el alien tiene un sentido para eso) |
| 6 | Tirar bengalas y ruidosos con el gesto de tirar | Sí, versión simple | Medio | Lanzar un ruidoso de verdad para distraerlo |
| 7 | Manivela física para puertas y palancas | Probable | Medio | Girar la manivela con el alien atrás es pánico puro |
| 8 | Ajustar la dificultad para VR con datos, sin parches | Sí | Bajo | Compensa lo lento que se gira y apunta en VR |
| 9 | Asomarse de verdad: que el alien vea tu cabeza y no tu cuerpo | Difícil | Alto | Esconderse "honesto", sin trampa ni castigo injusto |

## 1. Latido en el mando

**Qué es.** Un pulso suave en el mando (o en los dos) tipo latido, que se acelera cuando el alien está cerca, y un pulso fuerte cuando está a punto de verte.

**Cómo.** Ya sabemos recorrer la lista de NPC (`[[0x16f0c88]+8]+0xd0`) y la posición del jugador. Con eso sale la distancia. Mejor todavía: ya tenemos ubicada la función que suma activación a un sentido (0x729120), así que se puede leer "cuánto le falta al alien para detectarte" y usar eso en vez de la distancia. La vibración se manda con OpenVR (`TriggerHapticPulse`), que nuestro DLL ya tiene abierto.

**Ojo.** Que no reemplace al tracker: solo cerca y suave, con un .ini para la intensidad o apagarlo. Hay que separar al alien de los androides y humanos (o que cada uno tenga su "latido").

## 2. Contener la respiración en el locker

**Qué es.** En el original, cuando el alien revisa el locker, tenés que tirarte hacia atrás con el stick y apretar un botón. En VR: te inclinás de verdad hacia atrás, adentro del locker, y te quedás callado. Si hablás o respirás fuerte (micrófono), te encuentra.

**Cómo.** Reusa lo del micrófono y lo de agacharse: leer la posición del visor relativa al cuerpo y, si estás escondido (`+0x2b4` y escondite activo en `+0x31c`→`+0x48`, ya anotados en `agachar/NOTAS.md`), simular la acción de "contener la respiración". Durante ese momento, el umbral del micrófono baja.

**Ojo.** Hay que encontrar la acción de contener la respiración en el juego. La pista es que es una acción de input como la de agacharse (`0x5a8790`).

## 3. Taparse la boca

**Qué es.** Si la mano libre está pegada a la cara (cerca del visor, abajo), el ruido que detecta el micrófono se multiplica por, digamos, 0.3. Es lo que hace cualquiera cuando se asusta.

**Cómo.** Unas líneas en el DLL del micrófono: distancia entre el mando y el visor. Es la idea más barata de la lista.

## 4. Enfocar el tracker acercándolo a la cara

**Qué es.** En el original, mantener un botón hace que el tracker se vea nítido y el fondo borroso. En VR: cuando levantás el tracker a la altura de los ojos, se activa solo ese enfoque. Si lo bajás, ves el pasillo pero no los puntitos.

**Cómo.** Mano izquierda cerca del visor y delante de la cara → simular el botón de enfoque (el DLL ya sabe hacer OR en los botones del mando, como hace GRAND con el IED).

**Ojo.** Fijarse si GRAND ya hace algo parecido para no duplicar.

## 5. Linterna con gesto

**Qué es.** Tocarte el hombro (o la sien, como si estuviera en el casco) prende y apaga la linterna. Apagar la linterna rápido cuando oís al alien se vuelve un reflejo.

**Cómo.** Zona alrededor del visor + grip → simular el botón de la linterna. Es la misma técnica de las fundas, así que sale casi gratis si se hacen esas. El alien ya tiene un sentido para ver la linterna (`SEE_FLASH_LIGHT`, tipo 6), así que el gesto tiene consecuencias reales.

## 6. Tirar con el gesto

**Qué es.** Con un ruidoso o una bengala en la mano, hacés el movimiento de tirar y sale volando al soltar el gatillo.

**Cómo.** Versión simple: detectar el gesto (mano rápida hacia adelante) y simular el botón de tirar del juego, que lo tira a donde apuntás. Versión mejor: que la fuerza del gesto cambie la distancia. Eso depende de si el juego tiene un parámetro de "fuerza de tiro" (hay que buscarlo; en el original el arco depende de cuánto mantenés el botón, así que puede que sí).

**Ojo.** Se cruza con la detección del melee por gesto: hay que distinguir "estoy pegando" de "estoy tirando" según lo que tengas equipado.

## 7. Manivela física

**Qué es.** Para las puertas que se abren con la llave de mantenimiento o las palancas que hay que girar, hacer el movimiento circular con la mano en vez de machacar un botón. Si parás, la puerta se frena.

**Cómo.** Medir el ángulo que gira la mano alrededor de un centro y convertir cada media vuelta en un "botón apretado". Hay que ver si esas interacciones son "machacar botón" o "mantener botón" en AI.exe.

**Ojo.** Puede cansar en las puertas largas. Opción en el .ini para apagarlo.

## 8. Dificultad ajustada para VR

**Qué es.** En VR girás, apuntás y leés el tracker más lento que con mando. Eso hace que el juego sea más difícil de lo pensado, no más tenso. Un ajuste chico de cuánto tarda el alien en verte o escucharte lo compensa.

**Cómo.** Ya sabemos que la dificultad carga multiplicadores por sentido (0x6f8b00, `max_hearing_distance_modifier`, `movement_sound_sense_activation_modifier`). Se puede tocar el archivo de configuración con OpenCAGE, sin parchear el exe, o leer los valores desde nuestro DLL. Bonus: un preset "VR difícil" que sea más duro en la vista y más blando en lo físico (porque ya agregamos el micrófono).

**Ojo.** Es lo más fácil de pasarse. Cambios del 10 al 20 %, no más.

## 9. Asomarse de verdad

**Qué es.** Hoy el alien mira dónde está el personaje, no tu cabeza. Si asomás la cabeza por la esquina de la mesa, el juego te sigue teniendo escondido; si te inclinás detrás de una columna pero el cuerpo quedó afuera, te ve. La idea es que lo que importa sea dónde está tu cabeza.

**Cómo.** Encontrar el punto que el alien usa para ver al jugador (en la función de visión, 0x72e1f0, o antes) y moverlo a la posición real del visor, con un límite para que no se aleje demasiado del cuerpo.

**Ojo.** Toca la visión del alien, que es lo más delicado del juego. Lo dejaría para cuando agacharse esté probado, porque comparte el problema.

## Descartadas rápido

- **HUD en la muñeca o mapa en la mano:** hay que dibujar cosas nuevas, y eso vive adentro de GRAND o de Scaleform. Mucho trabajo sin el código.
- **Que el alien escuche cuando chocás el mando con algo:** no hay forma confiable de saber que chocaste.
- **Pulsómetro real:** necesita hardware extra.

## Orden que propongo

1. Taparse la boca (3): casi gratis encima del micrófono.
2. Latido en el mando (1): independiente, mucho efecto.
3. Contener la respiración (2) y enfocar el tracker (4).
4. Linterna (5) y tirar (6), junto con las fundas.
5. Dificultad VR (8) cuando ya hayas jugado un rato con todo lo anterior.
6. Manivela (7) y asomarse (9) más adelante.

*Lluvia de ideas. Nada está implementado ni probado.*

# Segunda tanda

| # | Idea | Factible | Esfuerzo | Por qué suma al horror |
|---|---|---|---|---|
| 10 | Moverte rápido en tu cuarto hace ruido | Sí | Bajo (con micrófono hecho) | Un paso apurado de verdad te delata |
| 11 | Inclinarte de verdad activa el asomarse del juego | Probable | Medio | Versión barata de la 9, usando lo que el alien ya entiende |
| 12 | Golpear con la llave inglesa una pared para distraerlo | Sí | Medio | Hacer ruido a propósito y salir corriendo para el otro lado |
| 13 | Botiquín: llevártelo al cuello | Sí | Bajo | Curarte con el alien cerca se vuelve un gesto nervioso |
| 14 | Sintonizador de accesos girando la muñeca | Probable | Medio | Hackear una puerta con las manos mientras escuchás pasos |
| 15 | Guardar sosteniendo la tarjeta en la terminal | Sí | Bajo | Esos segundos de guardado ahora los hacés con el cuerpo |
| 16 | Pegar la oreja a una puerta para escuchar del otro lado | Difícil | Alto | Saber si abrir o no, como harías en la vida real |
| 17 | Temblor en el mando al apuntar con el alien cerca | Sí | Bajo (con el latido hecho) | Tus manos "tiemblan" sin arruinar la puntería |

## 10. Moverte rápido hace ruido

**Qué es.** Si en tu cuarto das un paso rápido o te movés de golpe (el visor se desplaza rápido), el juego lo cuenta como ruido de movimiento, igual que correr con el stick.

**Cómo.** Velocidad del visor por encima de un umbral → la misma llamada que ya armamos para el micrófono (0x6e9f10 y 0x72f080, tipo MOVEMENT_SOUND), con un volumen según la velocidad. Es reusar todo lo del micrófono con otra fuente.

**Ojo.** Que girar la cabeza rápido no cuente: solo desplazamiento, no rotación. Umbral alto, para que solo lo dispare un movimiento brusco de verdad.

## 11. Inclinarte activa el asomarse del juego

**Qué es.** El juego original deja asomarse por las esquinas y por encima de las mesas con un botón. Si te inclinás de verdad, el DLL aprieta ese botón por vos. Así el alien te ve asomado, porque el propio juego sabe que lo estás, sin tocar su visión como pide la idea 9.

**Cómo.** Desplazamiento del visor de costado (o hacia arriba estando agachado) respecto del centro del cuerpo → simular el botón de asomarse (y la dirección con el stick). Mismo truco que agacharse.

**Ojo.** Hay que ver cómo choca con la cámara de MotherVR: el juego mueve la cámara al asomarse y vos ya moviste la cabeza, así que podría sumarse dos veces. Puede que haga falta anular el desplazamiento de cámara del juego.

## 12. Golpear para distraer

**Qué es.** Con la llave inglesa en la mano, das dos o tres golpes cortos en el aire (como golpeando una cañería) y eso hace ruido en ese lugar. Es como el ruidoso pero gratis y peligroso, porque el ruido sale de donde estás vos.

**Cómo.** Gesto de golpecitos repetidos (distinto del golpe de melee: cortos y seguidos) → llamada de ruido en la posición de la mano, con tipo WEAPON_SOUND o MOVEMENT_SOUND.

**Ojo.** Es una mecánica nueva que el original no tiene. Como el ruido sale de donde estás, el alien viene a donde estás: no rompe el balance, te expone.

## 13. Botiquín al cuello

**Qué es.** Para usar el botiquín, lo llevás al cuello o al brazo, como una inyección. La curación sigue siendo la del juego.

**Cómo.** Mano con el botiquín cerca del cuello (debajo del visor) → simular el botón de curar. Mismas zonas que las fundas.

## 14. Sintonizador de accesos con la muñeca

**Qué es.** El minijuego del sintonizador (y el de recablear) se juega con el stick. La idea es que lo hagas girando la muñeca, como si giraras una perilla.

**Cómo.** Leer la rotación del mando y convertirla en el valor del stick que espera el minijuego (el DLL ya escribe en el mando: sticks en `+0x268..0x274`).

**Ojo.** Hay que ver qué espera exactamente cada minijuego. Si espera un ángulo del stick es directo; si espera "izquierda/derecha" también, pero se siente menos.

## 15. Guardar con la tarjeta

**Qué es.** En las terminales de guardado tenés que mantener un botón mientras suena la terminal. En VR: acercás la mano a la terminal y la mantenés ahí. Si sacás la mano, se corta.

**Cómo.** Mano hacia adelante y abajo, mientras el juego muestra la indicación de guardar → mantener el botón de usar. Lo difícil es saber que estás frente a una terminal: si no se encuentra, alcanza con "mano extendida + grip".

**Ojo.** Es chica, pero suma porque guardar es uno de los momentos más tensos.

## 16. Oreja contra la puerta

**Qué es.** Acercás la cabeza a una puerta cerrada y se escucha más fuerte lo que hay del otro lado.

**Cómo.** Habría que tocar el audio (Wwise) para subir el volumen de lo que está detrás. Wwise tiene parámetros que se pueden cambiar en tiempo real, pero encontrar el correcto en AI.exe y saber qué es "una puerta delante tuyo" es mucho trabajo.

**Ojo.** Es la que más me gusta en lo conceptual, pero la más difícil de esta tanda. La dejaría para el final.

## 17. Temblor al apuntar

**Qué es.** Si tenés un arma levantada y el alien está cerca, el mando tiembla un poco. Es solo vibración, no mueve la mira, así que no te castiga de más.

**Cómo.** Lo mismo que el latido (idea 1), con otro patrón cuando hay un arma equipada y apuntando.

## Orden que propongo para la segunda tanda

1. Moverte rápido hace ruido (10) y temblor (17): salen casi gratis con lo que ya hay.
2. Botiquín (13) y guardar (15), junto con las fundas.
3. Inclinarte para asomarte (11), cuando agacharse esté probado.
4. Golpear para distraer (12) y sintonizador (14).
5. Oreja contra la puerta (16) si un día sobra tiempo.

*Lluvia de ideas. Nada está implementado ni probado.*
