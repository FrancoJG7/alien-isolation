# Detalle de las ideas fáciles y moderadas

26/10/2026. Basado en `PRIORIDADES.md` y `ANALISIS_GHIDRA.md`. Para cada idea: **cómo se juega** (lo que sentís vos), **cómo funciona** (lo que hace el DLL), **ajustes** (lo que va en `agachar.ini`) y **ojo** (lo que puede salir mal). Nada está implementado ni probado.

Todas viven en el mismo `xinput_ag.dll` de agacharse, corren una vez por cuadro en el hilo del juego y se pueden apagar una por una con `Activado=0` en su sección del .ini.

Índice:
- A. Fáciles ya: A1 a A17
- B. Fáciles o moderadas si la prueba de la tabla de botones sale bien: B0 (la prueba) y B1 a B12
- C. Moderadas: C1 a C9

---

# A. Fáciles ya

## A1. Techo del ducto (F1)
**Cómo se juega.** En un ducto, un hueco bajo o cualquier lugar donde el juego te obliga a ir agachada, aunque te pares en tu cuarto la cámara no sube más que el techo. Ya no se puede "sacar la cabeza" por arriba del ducto y ver el pasillo de al lado.

**Cómo funciona.** En el envoltorio de pose de la v0.1.7 (donde ya bajamos la altura): si `pj+0x2b4` está prendido, la altura de la cabeza que recibe el juego se recorta a "altura de agachado + margen". Al salir del ducto se suelta despacio en 0,3 s para que no pegue un salto.

**Ajustes.** `[Techo] Activado=1`, `Margen=0.05` (metros por encima de la altura de agachado).

**Ojo.** Hay que confirmar con el log que `+0x2b4` también se prende en los ductos y no solo en lockers. Si no, hace falta otra bandera (sale del log de estados de la versión de diagnóstico).

## A2. Correa invisible (F2)
**Cómo se juega.** Tu cabeza puede moverse unos 25 cm alrededor del cuerpo de Ripley. Si te pasás (para meter la cabeza en una pared, en la puerta del locker o asomarte por arriba de una mesa con el cuerpo escondido), la imagen se frena en el borde como si chocaras con algo blando. Esconderse se vuelve honesto: si el alien no te ve, es porque de verdad no te asomaste.

**Cómo funciona.** MotherVR nos da dos poses: cabeza (+0x14) y "cuerpo" (+0x18, solo giro y posición). En la salida de la cabeza limitamos la distancia horizontal entre las dos. Los últimos 5 cm del límite son un resorte suave, no una pared dura.

**Ajustes.** `[Correa] Activado=1`, `Radio=0.25`, `Resorte=0.05`, `RadioEscondida=0.10` (más corto dentro del locker).

**Ojo.** Lo primero es confirmar que las dos poses se miden igual (punto A9 del análisis). Si el borde se siente duro, marea: por eso el resorte. Si te movés mucho por el cuarto sin mover a Ripley, la correa te va a frenar seguido; en ese caso conviene recentrar (A4).

## A3. Aviso de pared (E4)
**Cómo se juega.** Cuando la correa te frena, sentís un toquecito en los dos mandos. Tu cuerpo entiende "ahí hay algo" en vez de que la imagen se pegue sin explicación.

**Cómo funciona.** Si la correa recortó más de 2 cm en este cuadro y no lo hizo en el anterior, pulso corto (TriggerHapticPulse de OpenVR). Como mucho uno cada 0,5 s.

**Ajustes.** `[Correa] Vibrar=1`, `FuerzaVibracion=0.3`.

**Ojo.** Depende de que la vibración llegue (prueba de la versión de diagnóstico).

## A4. Recentrar al salir del escondite (E6)
**Cómo se juega.** Saliste del locker o del ducto y quedaste corrida de Ripley: en medio segundo la imagen se acomoda sola, sin apretar Ctrl + Alt.

**Cómo funciona.** Cuando `pj+0x2b4` pasa de 1 a 0, guardamos la diferencia horizontal entre cabeza y cuerpo y la vamos restando de a poco en la pose durante 0,5 s.

**Ajustes.** `[Recentrar] AlSalir=1`, `Duracion=0.5`.

**Ojo.** Si el corrimiento es grande, deslizarse medio segundo puede marear; probar 0,3 y 1,0.

## A5. Vibración al agacharte o pararte (F3)
**Cómo se juega.** Cuando el juego confirma que ya estás agachada (o parada), un toquecito en el mando. Sabés sin mirar que el alien te ve como agachada.

**Cómo funciona.** El DLL ya vigila `pj+0x2af`. Cuando cambia, un pulso corto. Si el juego deshace el cambio y reintentamos, no vuelve a vibrar.

**Ajustes.** `[Agachar] Vibrar=1`, `FuerzaVibracion=0.4`.

**Ojo.** Es la prueba de vibración: si esto no se siente, todo lo que vibra (A3, A6, A8, A9, A10) necesita otro camino.

## A6. Latido por distancia al alien (F4)
**Cómo se juega.** Un latido suave en el mando izquierdo (el del tracker) que aparece cuando el alien está a menos de 15 m y se acelera al acercarse: 50 por minuto lejos, 120 a 3 m.

**Cómo funciona.** Cada cuadro recorremos la lista de NPC (`[[0x16f0c88]+8]+0xd0`), elegimos los de tipo alien (`[npc+0x64]+4 == 1`) y medimos la distancia a Ripley. Cada latido son dos pulsos cortos (pum-pum).

**Ajustes.** `[Latido] Activado=1`, `Modo=distancia`, `DistanciaMax=15`, `Fuerza=0.3`, `Mando=izquierdo`.

**Ojo.** Confirmar con el log que 1 = alien. Es información que el original no da, así que la fuerza tiene que ser baja: una sensación, no un radar. En ductos el alien puede estar "cerca" en línea recta pero lejos de verdad; la versión A12 lo resuelve.

## A7. Latido del lado del alien (E3)
**Cómo se juega.** Igual que A6, pero el latido es más fuerte en el mando del lado donde está el alien.

**Cómo funciona.** Ángulo entre hacia dónde mira tu cabeza y la dirección al alien: reparte la fuerza entre los dos mandos.

**Ajustes.** `[Latido] Direccional=1`.

**Ojo.** Esto ya es casi un radar. Lo dejaría apagado por defecto.

## A8. Vibración por daño y latido con poca vida (N9)
**Cómo se juega.** Cuando te pegan, un pulso fuerte y corto en los dos mandos. Con menos de 30 % de vida, el latido de A6 aparece aunque el alien esté lejos, más rápido cuanto menos vida tengas.

**Cómo funciona.** Leemos la vida en `pj+0x33c` y el máximo en `pj+0x33e` × `pj+0x340` (la misma cuenta que usa la barra de vida). Si baja de un cuadro al otro, pulso proporcional a cuánto bajó.

**Ajustes.** `[Danio] Activado=1`, `Fuerza=0.8`, `VidaBaja=0.3`.

**Ojo.** Ninguno grande. Verificar que la vida no se lea como cero en cargas y cinemáticas (para no vibrar por error).

## A9. Tus pasos en el mando (E5)
**Cómo se juega.** Un pulso mínimo en cada paso de Ripley. Caminar agachada casi no se siente; correr se siente claro. Sentís cuánto ruido estás haciendo.

**Cómo funciona.** Velocidad de Ripley con la posición `pj+0x70..0x78` entre cuadros. Un "paso" cada cierta distancia recorrida (0,7 m caminando, 1 m corriendo, con `pj+0x2b8` = corriendo). Fuerza según velocidad y si estás agachada.

**Ajustes.** `[Pasos] Activado=0` (apagado por defecto), `Fuerza=0.15`.

**Ojo.** Puede cansar. Con la vibración de otras ideas encima, mezclar con cuidado (prioridad: daño > latido > pasos).

## A10. Temblor al apuntar con el alien cerca (17)
**Cómo se juega.** Con un arma levantada y el alien a menos de 8 m, el mando del arma tiembla apenas. No mueve la mira: solo es la sensación de manos nerviosas.

**Cómo funciona.** Apuntando = acción de apuntar mantenida (relativo 0x2bc) o, si GRAND no la usa, "objeto equipado es arma" (gestor de equipo `+0x3bc8`). Distancia al alien como en A6. Vibración continua muy suave.

**Ajustes.** `[Temblor] Activado=1`, `Distancia=8`, `Fuerza=0.15`.

**Ojo.** Ver qué hace GRAND con la acción de apuntar: si siempre está "apuntando", usar solo lo del arma equipada.

## A11. Log de "por qué me vio" (F8)
**Cómo se juega.** No se ve en el juego. Después de jugar, `agachar.log` dice cosas como "14:32:05 alien: vista 0.42 → 0.78 (agachada, linterna prendida, a 6.1 m)". Sirve para saber si te vio por la linterna, por ruido o porque te asomaste, y para ajustar todo lo demás con datos.

**Cómo funciona.** Enganchamos 0x729120 (la función por donde pasa cualquier aumento de un sentido). Si el objetivo (`+0x42c`) es Ripley, anotamos tipo de sentido (`+0x64`: 0 vista, 1 ruido de arma, 2 ruido de movimiento, 6 linterna...), cuánto quedó (`+0x5c`), postura, linterna (`pj+0x2ba`) y distancia. Solo cuando cruza un nivel (0,25 / 0,5 / 0,75 / 1), para no llenar el log.

**Ajustes.** `[Log] Sentidos=1`.

**Ojo.** Enganchar una función que se llama muchas veces por cuadro: el código tiene que ser mínimo. Se prueba primero en el emulador.

## A12. Latido según cuánto le falta para verte (M7)
**Cómo se juega.** El latido ya no depende de la distancia sino de cuánto se está llenando el sentido del alien hacia vos. Detrás de una pared, aunque esté a 2 m, el corazón va tranquilo; en un pasillo a 15 m mirándote, se dispara. Es la versión "buena" de A6.

**Cómo funciona.** Con lo de A11 sabemos dónde está el objeto sentido de vista del alien hacia Ripley: leemos `+0x5c` (0 a 1) cada cuadro. Ritmo = 50 + 90 × activación.

**Ajustes.** `[Latido] Modo=activacion`.

**Ojo.** Es información que el juego normalmente da por sonido y por el tracker. Fuerza baja, como A6.

## A13. Altura de referencia que se aprende sola (F5)
**Cómo se juega.** No tenés que preocuparte por la calibración: si te sacás el visor, cambiás de lugar o recentrás, el mod vuelve a aprender tu altura parada en unos segundos.

**Cómo funciona.** Guardamos las alturas de los últimos 2 minutos cuando estás quieta y tomamos la del 90 % más alto. Si MotherVR recentra (salto brusco de posición), arrancamos de cero.

**Ajustes.** `[Agachar] AlturaAuto=1`.

**Ojo.** Si pasás mucho tiempo agachada seguido (más de 2 minutos), la referencia podría bajar; por eso solo cuenta cuando estás "parada" según el juego.

## A14. Modo sentado (F6)
**Cómo se juega.** Para jugar en silla: inclinarte hacia adelante y abajo unos 15 cm agacha a Ripley; volver a la postura normal la para.

**Cómo funciona.** Los mismos umbrales por velocidad de la v0.1.9, con referencia sentada y bajadas más chicas.

**Ajustes.** `[Agachar] Modo=sentado`, `BajadaAgachar=0.15`, `BajadaParar=0.08`.

**Ojo.** Sentado es más fácil disparar el agachado sin querer al mirar el tracker. Probar con los valores.

## A15. Recargar el .ini en vivo y tecla de pánico (F7)
**Cómo se juega.** Cambiás un valor en `agachar.ini`, guardás, y el juego lo toma al instante. `Ctrl + F12` apaga todo el mod (y lo vuelve a prender).

**Cómo funciona.** Cada segundo miramos la fecha del archivo. La tecla se lee con GetAsyncKeyState en el bucle.

**Ajustes.** `[General] TeclaPanico=ctrl+f12`.

**Ojo.** Nada; es la que más tiempo ahorra para ajustar todo lo demás con el visor puesto.

## A16. Pausa al sacarte el visor (E1)
**Cómo se juega.** Te sacás el visor para atender la puerta: el juego se pausa solo. Te lo ponés: vuelve a la pausa en la misma posición, recentrado.

**Cómo funciona.** OpenVR avisa si el sensor del visor detecta tu cara (nivel de actividad del dispositivo). Si deja de detectarla 2 segundos, mandamos la tecla de pausa (Esc). Al ponértelo, recentramos (A4).

**Ajustes.** `[Pausa] Activado=1`, `Segundos=2`.

**Ojo.** Que no se dispare en menús o cinemáticas. Con Steam Link, ver que el sensor llegue igual.

## A17. Sonidos y volumen propios: latido audible, mano en la oreja, taparte los oídos, jadeo (E2, E7, E8, E11)
**Cómo se juega.**
- Latido audible: además (o en vez) de vibrar, escuchás un latido grave muy bajito.
- Mano en la oreja: ahuecás la mano al lado de la oreja y el juego suena un 50 % más fuerte mientras la mantengas. Sirve para escuchar pasos detrás de una pared.
- Taparte los oídos: las dos manos en las orejas bajan el juego al 30 %.
- Jadeo: si te moviste mucho de verdad (agacharte y pararte seguido), escuchás un jadeo propio que se calma solo.

**Cómo funciona.** Nuestro DLL reproduce sus propios .wav por Windows (llegan al visor por Steam Link igual que el juego) y puede cambiar el volumen de la sesión de audio del propio juego (ISimpleAudioVolume). Las manos se detectan por distancia entre mando y visor, a la altura de la oreja.

**Ajustes.** `[Audio] Latido=1`, `VolumenLatido=0.2`, `ManoOreja=1`, `Amplificar=1.5`, `TaparOidos=1`, `Jadeo=0`.

**Ojo.** Subir el volumen puede saturar si el juego ya está fuerte: usar con el volumen del juego por debajo del máximo. Los .wav van en la carpeta del juego.

---

# B. Si la prueba de la tabla de botones sale bien

## B0. La prueba (versión de diagnóstico)
**Qué es.** Ripley lee sus botones de una tabla (`[0x1759b44]`) en cada cuadro. Si nuestro DLL puede "apretar" un botón ahí, todas las ideas de abajo se reducen a: detectar el gesto + apretar ese botón. El juego sigue aplicando sus reglas (tiempos, munición, animaciones).

**Cómo se prueba.** Una versión con tres teclas:
- F9: aprieta la linterna (un cuadro, bit "recién apretado").
- F10: mantiene correr mientras está apretada.
- F11: aprieta recargar.

El log anota si `pj+0x2ba` (linterna) o `pj+0x2b8` (corriendo) cambiaron. Si funcionan, se abre todo el grupo B. Si no, hay que escribir justo después de que el juego llena la tabla (candidatos 0x5a9380 y 0x5a8160).

## B1. Tapar la linterna con la mano (M8 y vieja 5)
**Cómo se juega.** Ponés la mano libre delante de la linterna (o de la cara, según dónde la ponga GRAND) y se apaga; la sacás y se prende. Cuando oís al alien, el reflejo es taparla.

**Cómo funciona.** Mano a menos de 10 cm del punto de la linterna durante 0,2 s → aprieta "linterna" (índice 29) si `pj+0x2ba` está prendida. Al sacar la mano, la aprieta otra vez si estaba prendida antes de taparla.

**Ajustes.** `[Linterna] Tapar=1`, `Distancia=0.10`.

**Ojo.** El alien ve la linterna (`SEE_FLASH_LIGHT`), así que esto tiene efecto real. Que no se apague sin querer al mirar el tracker.

## B2. Trotar en el lugar para correr (M4)
**Cómo se juega.** Con el stick hacia adelante, si trotás suave en tu lugar (la cabeza rebota 2 o 3 veces por segundo), Ripley corre. Si parás de trotar, vuelve a caminar.

**Cómo funciona.** Buscamos oscilación en la altura de la cabeza (misma velocidad suavizada de agacharse). Mientras dure: mantener "correr" (índice 8).

**Ajustes.** `[Correr] Trotar=0` (apagado por defecto), `Frecuencia=1.5..3.5`, `Amplitud=0.03`.

**Ojo.** No tiene que disparar el agachado ni confundirse con acomodarte. Cansa y a algunos les marea: por eso arranca apagado.

## B3. Enfocar el tracker acercándolo a la cara (vieja 4)
**Cómo se juega.** Levantás el tracker a la altura de los ojos y se enfoca (pantalla nítida, fondo borroso, como en el original al mantener el botón). Lo bajás y ves el pasillo.

**Cómo funciona.** Mando izquierdo a menos de 25 cm del visor y delante de la cara → mantener "tracker" (índice 28).

**Ajustes.** `[Tracker] EnfocarCerca=1`, `Distancia=0.25`.

**Ojo.** Ver primero si GRAND ya hace algo así, para no duplicar.

## B4. Agarrar el arma con las dos manos (N6)
**Cómo se juega.** Con la mano libre agarrando el arma (como un revólver a dos manos), Ripley "apunta" y el arma se estabiliza. Soltás y vuelve al modo normal.

**Cómo funciona.** Mando izquierdo a menos de 12 cm del derecho, con grip → mantener "apuntar" (índice 10).

**Ajustes.** `[DosManos] Activado=1`, `Distancia=0.12`.

**Ojo.** Depende de qué hace GRAND con apuntar; si ya usa la mano real para la mira, el efecto puede ser solo el zoom o la estabilidad.

## B5. Recarga con gesto (D8)
**Cómo se juega.** Llevás la mano libre a la cadera (como buscando munición) y después al arma: recarga.

**Cómo funciona.** Mano izquierda en la zona de la cadera izquierda con grip, después a menos de 15 cm del arma en menos de 1,5 s → aprieta "recargar" (índice 16). El juego decide si hay balas y hace su animación.

**Ajustes.** `[Recarga] Gesto=1`, `Tiempo=1.5`.

**Ojo.** La zona de la cadera depende de hacia dónde mira tu cuerpo, no tu cabeza (pose de cuerpo de MotherVR).

## B6. Fundas en el cuerpo (D7)
**Cómo se juega.** Mano a la cadera derecha + grip = revólver. Hombro derecho = arma grande (escopeta, lanzallamas). Pecho = arrojable. Cadera izquierda = tracker. Hombro izquierdo = botiquín.

**Cómo funciona.** Zonas relativas al cuerpo y a tu altura. Cada zona aprieta su botón: armas 1 a 5 (índices 19-23), objetos (31-36), tracker (28). Un pulso corto confirma (A5).

**Ajustes.** `[Fundas] Activado=1` y una línea por zona con su botón, para que puedas reasignarlas.

**Ojo.** Chocar con el grip de GRAND. No se dibuja ninguna funda: solo cambia cómo elegís. Hay que ver qué arma corresponde a cada número en cada parte del juego.

## B7. Tirar con fuerza (D5)
**Cómo se juega.** Con un ruidoso, bengala o molotov en la mano, hacés el movimiento de tirar: rápido llega lejos, suave cae cerca.

**Cómo funciona.** En el original el arco depende de cuánto mantenés el botón. Detectamos el gesto y mantenemos "disparar/tirar" (índice 11) un tiempo proporcional a la velocidad de la mano; al terminar el gesto, lo soltamos.

**Ajustes.** `[Tirar] Gesto=1`, `TiempoMax=1.0`.

**Ojo.** Hay que medir cuánto tiempo mantenido da cada distancia. Que no se mezcle con el melee (B12).

## B8. Botiquín al cuello (vieja 13)
**Cómo se juega.** Con el botiquín en la mano, lo llevás al cuello como una inyección: te curás.

**Cómo funciona.** Mano a menos de 15 cm debajo del visor → aprieta el objeto botiquín (índices 31-36, a ver cuál).

**Ajustes.** `[Botiquin] Gesto=1`.

**Ojo.** Ver si curarse es "apretar el objeto" o "usar".

## B9. Manivela y soplete (vieja 7 y N8)
**Cómo se juega.**
- Manivela: en las puertas que hay que forzar machacando un botón, girás la mano en círculo. Si parás, la puerta se frena.
- Soplete: para cortar un panel, la mano tiene que moverse despacio sobre la zona; si la sacás, se pausa.

**Cómo funciona.** Cada media vuelta de la mano = un toque de "machacar" (índice 15). El soplete mantiene "usar" mientras la mano se mueve despacio.

**Ajustes.** `[Manivela] Activado=1`, `[Soplete] Activado=1`.

**Ojo.** Saber cuándo hay una puerta para forzar: se puede usar el estado de Ripley (C1). Las puertas largas cansan: opción para apagar.

## B10. Salir del locker empujando (N14)
**Cómo se juega.** Dentro del locker, te inclinás hacia adelante rápido y salís.

**Cómo funciona.** Escondida (`pj+0x2b4`) + cabeza hacia adelante a más de 0,6 m/s → aprieta la acción que saca del locker (usar o atrás, a confirmar).

**Ajustes.** `[Locker] SalirEmpujando=1`.

**Ojo.** Que no salgas al acercarte a la rendija para espiar (C8): por eso velocidad y no distancia.

## B11. Asomarse con el cuerpo (M2 y M10)
**Cómo se juega.** Te inclinás de costado detrás de una esquina, o te levantás un poco agachada detrás de una caja: Ripley se asoma de verdad, y el alien te ve asomada porque el juego sabe que lo estás.

**Cómo funciona.** Cabeza corrida de costado (o hacia arriba estando agachada) respecto del cuerpo, por velocidad + recorrido mínimo, como agacharse → mantener "asomarse" (índice 18) y el stick hacia ese lado. Como el juego también mueve la cámara al asomarse, en la pose restamos ese desplazamiento para que no se sume dos veces.

**Ajustes.** `[Asomarse] Activado=1`, `Lateral=0.15`, `Arriba=0.12`.

**Ojo.** Es la más delicada del grupo B: depende de cómo combina el stick con el botón y de medir el desplazamiento que mete el juego.

## B12. Melee por gesto (de la lista de JayP)
**Cómo se juega.** Con la llave inglesa en la mano, das un golpe de verdad y Ripley pega. El juego sigue decidiendo si pega o si el androide lo bloquea.

**Cómo funciona.** Mano derecha a más de 2 m/s hacia adelante con la llave equipada → aprieta "disparar" (índice 11).

**Ajustes.** `[Melee] Gesto=1`, `Velocidad=2.0`.

**Ojo.** Que no se dispare al mover la mano para otra cosa. Ver qué hace GRAND con la llave hoy.

---

# C. Moderadas

## C1. Cámara en muertes y animaciones (M1)
**Cómo se juega.** Cuando el alien te mata desde el ducto (o en escaleras, palancas, ascensores), la cámara queda donde está Ripley y ves la escena como corresponde. Al terminar, vuelve suave.

**Cómo funciona.** Durante ciertos estados de Ripley (`[pj+0x388]+0x1e`), en la pose de la cabeza dejamos solo hacia dónde mirás y anulamos cuánto te moviste en el cuarto.

**Qué falta.** Saber qué número es "animación con cámara". Sale del log de estados de la versión de diagnóstico: hacés una escalera, una muerte y una palanca, y vemos los números.

**Ajustes.** `[Animaciones] FijarPosicion=1`, `EstadosExtra=` (para agregar números a mano).

## C2. Contener la respiración y AireSegundos (M3)
**Cómo se juega.** En el locker, te inclinás hacia atrás y aguantás el aire de verdad. El juego usa el tiempo que vos pusiste en `AireSegundos`.

**Cómo funciona.** El juego del aire es un script del nivel (Custom_Hiding_Controller, con Start_Breathing_Game y End_Breathing_Game). El tiempo probablemente se cambia con OpenCAGE en los datos del nivel, sin tocar el exe. El gesto (inclinarte atrás) aprieta la acción que usa el juego para contener, vía la tabla (B0).

**Qué falta.** Encontrar el parámetro de duración con OpenCAGE y ver qué botón usa el locker para contener.

## C3. Cosas que hacen ruido: sobresalto, chocar los mandos, jadeo, temblar en el locker, moverse rápido, golpear para distraer (M5, N2, N3, N15, 10, 12)
**Cómo se juega.**
- Sobresalto: el alien aparece y vos das un salto para atrás en tu cuarto: hace ruido.
- Chocar los mandos: aplaudís con los dos mandos y hacés un ruido fuerte para llamarlo (por ejemplo hacia los humanos).
- Jadeo con ruido: si te agitaste, durante unos segundos hacés ruido chico.
- Temblar en el locker: escondida y con el alien a menos de 3 m, si movés mucho la cabeza, hacés ruido chico.
- Golpear con la llave: golpecitos cortos en el aire hacen ruido donde estás.

**Cómo funciona.** Todas usan la misma llamada de ruido del micrófono (0x6e9f10 y 0x72f080) con un volumen según el gesto.

**Qué falta.** Probar esa llamada en el juego, que nunca se probó. Lo más directo es juntar el micrófono con la v0.1.9 y probarlas de a una.

## C4. Tracker pegado al pecho y bips que vibran (N1, N10)
**Cómo se juega.**
- Tracker al pecho: pegás el tracker al pecho y el alien deja de oír sus bips (pero vos no lo ves bien).
- Bips que vibran: cada bip del tracker vibra en el mando izquierdo, más fuerte cuanto más cerca está el contacto.

**Cómo funciona.** Engancharse al evento del tracker (tipo 2) que reciben los NPC: bajar su volumen (`+0x30`) si la mano está en el pecho, y mandar el pulso.

**Qué falta.** Encontrar dónde se crea ese evento (hoy solo sabemos dónde se recibe).

## C5. Balance con el cuerpo: inmovilidad, cuclillas profundas, mirada fija (M6, N4, N5)
**Cómo se juega.**
- Inmovilidad: si te quedás quieta del todo unos segundos, el alien te detecta un poco más lento.
- Cuclillas profundas: agachada y además muy abajo, tus pasos suenan 20 % menos.
- Mirada fija: si desde el escondite mirás fijo al alien más de 2 segundos, te nota un poco antes.

**Cómo funciona.** En el mismo enganche de A11 (0x729120), multiplicar el aumento del sentido solo cuando el objetivo es Ripley y se cumple la condición.

**Qué falta.** Nada técnico grande; es balance. Máximo 15-20 % y con A11 midiendo.

## C6. Rendija del locker (M9)
**Cómo se juega.** Dentro del locker, acercás la cara a la rendija para ver más; alejarte te protege. La cabeza no atraviesa la puerta.

**Cómo funciona.** Correa (A2) especial dentro del locker, centrada en la cámara que pone el juego al entrar.

**Qué falta.** Saber dónde guarda el juego esa cámara (o tomar la posición al entrar como centro).

## C7. Mapa girando la muñeca (E10)
**Cómo se juega.** Girás la muñeca izquierda hacia la cara como para ver la hora: se abre el mapa. La bajás: se cierra.

**Cómo funciona.** Rotación del mando izquierdo + posición delante de la cara → aprieta el botón del mapa.

**Qué falta.** Ver si el mapa es "menú de HUD" (índice 38) o tiene su propia acción, y que la tabla (B0) funcione con menús.
