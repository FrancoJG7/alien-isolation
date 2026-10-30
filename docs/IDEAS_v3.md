# 50 ideas fáciles nuevas

26/10/2026. Todas usan piezas que ya conocemos (ver `ANALISIS_GHIDRA.md` y `ANALISIS_GHIDRA_2.md`) y no repiten las de `IDEAS.md`, `IDEAS_v2.md` ni `DETALLE.md`. Nada implementado ni probado.

Las piezas, para leer la columna "Con qué":
- **Pose:** el envoltorio de la cabeza de MotherVR (altura y posición).
- **Vibra:** vibración de los mandos por OpenVR (falta la prueba de que llega).
- **Audio:** sonidos propios (.wav) y volumen del juego desde nuestro DLL.
- **NPC:** lista de personajes, su posición y su tipo (1 alien, 2 androide, 3-4 humanos, 5 facehugger).
- **Ripley:** vida (`+0x33c`), linterna (`+0x2ba`), corriendo (`+0x2b8`), agachada (`+0x2af`), escondida (`+0x2b4`), objeto en la mano y balas (0x8177b0).
- **Sentidos:** enganche de "por qué me vio" (0x729120).
- **Tecla:** la tecla virtual. Fácil **si** la prueba de diagnóstico sale bien.
- **OpenVR:** datos del visor y los mandos (batería, sensor de cara, tracking).

## Sentir lo que pasa alrededor (1-12)

| # | Idea | Cómo se juega | Con qué |
|---|---|---|---|
| 1 | Pasos del alien | Cuando el alien camina a menos de 10 m, sentís cada paso suyo como un golpe sordo en los mandos, más fuerte cuanto más cerca | NPC (velocidad del alien) + Vibra |
| 2 | Alien por arriba | Si el alien está en el ducto justo encima tuyo, un retumbo grave distinto: sabés que está arriba sin verlo | NPC (altura del alien vs la tuya) + Vibra |
| 3 | Androides cerca | Un zumbido mecánico, parejo, cuando hay un androide a menos de 6 m. Distinto del latido del alien | NPC tipo 2 + Vibra |
| 4 | Humanos cerca | Pulsos cortos e irregulares si hay humanos hostiles cerca | NPC tipo 3-4 + Vibra |
| 5 | Facehugger | Vibración rápida y nerviosa cuando hay un facehugger cerca | NPC tipo 5 + Vibra |
| 6 | Cada sentido, su patrón | Si el alien te está escuchando, pulsos cortos; si te está viendo, uno largo. Aprendés qué te delata | Sentidos + Vibra |
| 7 | Modo entrenamiento | Opción para las primeras horas: un toque cada vez que cualquier enemigo empieza a notarte. Después se apaga | Sentidos + Vibra |
| 8 | Silencio en el locker | Escondida y con el alien cerca, nuestro latido se corta de golpe: el silencio da más miedo que el ruido | Ripley (escondida) + NPC |
| 9 | El encierro del locker | Dentro del locker el juego suena un 20 % más bajo, como amortiguado por la puerta | Ripley (escondida) + Audio |
| 10 | Te encontró | Si el alien te saca del escondite o te mata, vibración larga en los dos mandos y el volumen baja de golpe | Ripley (vida en 0) + Vibra + Audio |
| 11 | Volver a respirar | Cuando el alien se aleja más de 20 m después de haber estado cerca, un suspiro propio bajito | NPC + Audio |
| 12 | El alien se fue a los ductos | Pulso doble cuando el alien pasa del piso a los ductos (cambia de altura de golpe) | NPC + Vibra |

## Tu estado, al tacto (13-24)

| # | Idea | Cómo se juega | Con qué |
|---|---|---|---|
| 13 | Linterna al tacto | Un clic suave en el mando al prender o apagar la linterna, por si no estás segura de cómo quedó | Ripley (linterna) + Vibra |
| 14 | Correr al tacto | Un pulso al empezar a correr y otro al dejar: sabés cuándo estás haciendo ruido | Ripley (corriendo) + Vibra |
| 15 | Entrar al escondite | Un pulso suave al quedar escondida y otro al salir | Ripley (escondida) + Vibra |
| 16 | ¿Qué tengo en la mano? | Al cambiar de objeto, una cantidad de pulsos según cuál: 1 revólver, 2 pistola de pernos, 3 escopeta... Sin mirar sabés qué sacaste | Ripley (objeto) + Vibra |
| 17 | Disparo con peso | Cada disparo vibra según el arma: revólver seco, escopeta fuerte, pernos largo | Ripley (balas bajan) + Vibra |
| 18 | Recogiste algo | Pulso corto cuando sube tu munición de reserva: agarraste balas | Ripley (balas) + Vibra |
| 19 | Curada | Pulso cálido y lento cuando te sube la vida | Ripley (vida) + Vibra |
| 20 | Cansancio de correr | Si corrés más de 8 segundos seguidos, escuchás a "Ripley" jadear bajito unos segundos | Ripley (corriendo) + Audio |
| 21 | Vida baja, sonido apagado | Con menos de 25 % de vida, el juego suena un 15 % más bajo, como aturdida | Ripley (vida) + Audio |
| 22 | Linterna olvidada | Si te escondés con la linterna prendida, un aviso suave (no la apaga sola, solo avisa) | Ripley (linterna + escondida) + Vibra |
| 23 | Tracker con el alien en el piso de arriba | Opción para que el latido ignore al alien si está a más de 4 m de altura de diferencia (en otro piso) | NPC |
| 24 | Últimas balas | Con 1 bala en el arma, cada vez que apuntes sentís un pulso de aviso | Ripley (balas) + Vibra |

## Comodidad en VR (25-36)

| # | Idea | Cómo se juega | Con qué |
|---|---|---|---|
| 25 | Techo de altura | Si te parás en puntas de pie o sos más alta que Ripley, la cámara no sube más que la cabeza de Ripley parada | Pose |
| 26 | Tu altura es la de Ripley | El mod compensa solo la diferencia entre tu altura y la de Ripley, sin tocar el .ini | Pose |
| 27 | Detecta que te sentaste | Si pasás 30 segundos con la cabeza mucho más baja y quieta, pasa solo al modo sentado (y vuelve si te parás) | Pose |
| 28 | Recentrar con gesto | Las dos manos arriba de la cabeza 2 segundos: recentra, sin buscar el teclado | Pose + OpenVR |
| 29 | Calibrar con gesto | Parada, los dos gatillos apretados 2 segundos: el mod toma tu altura parada | Pose + OpenVR |
| 30 | Mando sin tracking, sin gestos | Si un mando pierde el seguimiento, sus gestos se apagan hasta que vuelva (evita disparos de gestos locos) | OpenVR |
| 31 | Modo zurdo | Una línea del .ini invierte los mandos para todos nuestros gestos | OpenVR |
| 32 | Batería baja | Si un mando o el visor bajan del 15 % de batería, un aviso de vibración (una vez) y una línea en el log | OpenVR |
| 33 | Perfiles | Varias personas en la misma PC: `Perfil=franco` carga sus alturas y preferencias | .ini |
| 34 | Ajustar en vivo | `Ctrl + flechas` sube y baja el umbral de agacharse mientras jugás, con un pulso que confirma | Teclado + Vibra |
| 35 | Pausa con gesto | Brazos cruzados sobre el pecho 2 segundos: pausa | OpenVR + Tecla |
| 36 | Pausa al perder tracking | Si el visor pierde el seguimiento (se tapó la cámara, te fuiste del área), el juego se pausa | OpenVR + Tecla |

## Botones con el cuerpo, si anda la tecla virtual (37-44)

| # | Idea | Cómo se juega | Con qué |
|---|---|---|---|
| 37 | Correr inclinándote | Inclinarte hacia adelante (como arrancando una carrera) con el stick adelante te hace correr. Alternativa al trote | Pose + Tecla |
| 38 | Sacar el tracker levantando el brazo | Levantás el antebrazo izquierdo delante tuyo y Ripley saca el tracker; lo bajás y lo guarda | OpenVR + Tecla |
| 39 | Mochila en el hombro | Llevar la mano por arriba del hombro (como a una mochila) abre el menú de crafteo o el radial | OpenVR + Tecla |
| 40 | Linterna en la sien | Tocarte la sien con dos dedos prende o apaga la linterna, como una de casco | OpenVR + Tecla |
| 41 | Siguiente arma con la muñeca | Girar rápido la muñeca derecha hacia afuera pasa al siguiente objeto | OpenVR + Tecla |
| 42 | Recarga solo si hace falta | El gesto de recarga no hace nada si el arma está llena (no perdés tiempo ni hacés ruido) | Ripley (balas) + Tecla |
| 43 | Apagar la linterna al agacharte de golpe | Opción: si te agachás muy rápido (reflejo de esconderse), también apaga la linterna | Agachar + Tecla |
| 44 | Usar con la palma | Empujar con la palma abierta hacia adelante = "usar" (abre puertas, toma objetos donde el juego te muestra la indicación) | OpenVR + Tecla |

## Recuerdos y desafíos (45-50)

| # | Idea | Cómo se juega | Con qué |
|---|---|---|---|
| 45 | Foto del susto | Cuando das un salto con el alien cerca, el mod saca una captura (la tecla de captura de Steam). Al final tenés tu álbum de sustos | Pose + NPC + Tecla de Steam |
| 46 | Foto de la muerte | Igual, en el momento en que el alien te mata | Ripley (vida) |
| 47 | Marcas para el video | Si grabás la partida, el log anota la hora de cada susto y cada muerte para encontrarlos en el video | Log |
| 48 | Racha sin ser vista | El resumen de la partida dice tu mejor racha de minutos sin que nadie te note | Sentidos + Log |
| 49 | Desafíos | El resumen marca si cumpliste retos: "sin correr", "sin linterna", "sin que te vean" en una sesión | Ripley + Sentidos + Log |
| 50 | Registro de cuerpo | Un archivo con tu altura, cuántas veces te agachaste y cuánto te moviste por minuto, para ver cuánto ejercicio hiciste jugando | Pose + Log |

## Cuáles haría primero

1. **13, 14, 15 y 16** (tu estado al tacto): son unas líneas cada una sobre lo que ya leemos, y sirven para probar la vibración.
2. **1, 2 y 8** (pasos del alien, alien por arriba y silencio en el locker): son las que más miedo suman.
3. **25, 27, 28 y 30** (comodidad): hacen que todo lo demás se sienta bien.
4. **45** (foto del susto), porque es divertida y casi gratis.
