# Lluvia de ideas v2: gameplay VR con lo que aprendimos agachándonos

26/10/2026. Para AI.exe Steam (md5 bb0d932f…), MotherVR 0.8.1 y GRAND, sobre la base de `xinput_ag.dll` v0.1.9 (agacharse, "funciona absolutamente perfecto").

Esta tanda no repite `IDEAS.md` ni `ideas_jayp/EVALUACION.md`: al final hay una tabla que las **re-evalúa** con lo que sabemos ahora. Las ideas nuevas están ordenadas en tres niveles:

- **Fácil:** usa piezas que ya funcionan en el juego. Días, no semanas.
- **Moderado:** hay que encontrar uno o dos datos nuevos en AI.exe (un campo, una función), con el mismo método que usamos para agacharse.
- **Factible:** difícil pero se puede. Toca algo delicado (la visión del alien, el audio, las armas) o necesita varias piezas nuevas.

Nada de esto está implementado. Donde digo una dirección es porque ya la tenemos anotada en `agachar/NOTAS.md` o `microfono/NOTAS.md`; lo demás está por buscar.

---

## Lo que aprendimos y cambia las cuentas

1. **Tenemos un reloj propio.** El DLL corre una vez por cuadro en el hilo del juego (enganche a PeekMessageA en el bucle principal). Todo lo que sea "mirar algo en cada cuadro y reaccionar" ahora es barato.
2. **Tenemos la cabeza y las manos.** OpenVR sale de MotherVR (IVRSystem_017). `GetDeviceToAbsoluteTrackingPose` devuelve todos los dispositivos, no solo el visor, así que los mandos ya están a mano.
3. **Podemos editar lo que el juego ve de tu cabeza.** Envolvemos las dos entradas de pose de MotherVR (cabeza y cuerpo) y hoy solo tocamos la altura (`out[7]`). Ahí mismo están la posición de costado y adelante. Esa es la palanca más poderosa que tenemos y la mayoría de las ideas nuevas salen de ahí.
4. **Simular un botón no es lo mismo que pedirle algo al personaje.** El "botón" que tocamos en la v0.1.4 era una entidad de script (MonitorActionMap), no el control de Ripley. Lo que anduvo fue escribir el **pedido interno** del personaje (`+0x3e3a`, vía Crouch/Uncrouch). Regla nueva: para cada acción, buscar primero el pedido interno; el botón (hacer OR en el estado del mando en `+0x1c`, como hace GRAND con el IED) queda como plan B.
5. **El juego a veces deshace lo que pedís.** Vigilar unos segundos después de cada cambio y reintentar si se revierte. GRAND además llama Crouch/Uncrouch solo (a los ~18 s y ~48 s de arrancar). Cualquier idea que escriba estado tiene que convivir con eso.
6. **Velocidad mejor que distancia.** Los gestos se sienten bien cuando se detectan por velocidad (con un mínimo de recorrido) y se bloquean si te movés en sentido contrario. Sirve para todos los gestos de esta lista.
7. **El log manda.** Cada idea tiene que anotar sus cambios de estado con hora, y tener un `Activado=0` para hacer pruebas A/B. Así encontramos que el "mirar para abajo" era de MotherVR.
8. **Ya tenemos la IA a mano:** lista de NPC (`[[0x16f0c88]+8]+0xd0`), la llamada de ruido (0x6e9f10 → 0x72f080) y la función que suma activación a cada sentido (0x729120).

---

## Fácil

| # | Idea | Qué reusa |
|---|---|---|
| F1 | Techo del ducto: no se puede asomar la cabeza por el techo | Pose (altura) + `+0x2b4` |
| F2 | Correa invisible: la cabeza no se aleja más de X cm del cuerpo | Pose (posición) |
| F3 | Vibración corta que confirma "ya estás agachada / parada" | `+0x2af` + háptica |
| F4 | Latido por distancia al alien | Lista de NPC + háptica |
| F5 | Altura de referencia que se aprende sola | Lógica de agacharse |
| F6 | Modo sentado para agacharse | Lógica de agacharse |
| F7 | Recargar el .ini sin reiniciar el juego y tecla para apagar todo | Bucle |
| F8 | Log de "por qué me vio" | 0x729120 + log |

### F1. Techo del ducto
**Qué es.** En los ductos y en lugares bajos el juego te fuerza agachada (`+0x2b4`). Si te parás en tu cuarto, hoy la cámara puede subir más de lo que el ducto permite y ver a través del techo. Con esto, mientras el juego te tenga forzada, la altura de la cabeza tiene tope.

**Cómo.** En el mismo envoltorio de pose de la v0.1.7, si `+0x2b4` está prendido, recortar `out[7]` a la altura de agachado + un margen. Son unas líneas.

**Ojo.** Verificar en el log que `+0x2b4` se prende en ductos y no solo en lockers.

### F2. Correa invisible
**Qué es.** Hoy podés meter la cabeza a través de una pared, de la puerta del locker o asomarte por encima de una mesa con el cuerpo escondido. La idea es una correa: la cámara puede alejarse del cuerpo hasta, digamos, 25 cm. Si te pasás, la imagen se queda en el borde (como chocar contra algo).

**Cómo.** Con la pose del cuerpo (que MotherVR ya calcula, entrada +0x18) y la de la cabeza, limitar la diferencia horizontal en la salida de la cabeza. Mismo truco que la altura fija.

**Por qué suma.** Esconderse vuelve a ser honesto sin tocar la visión del alien: si no te ve, es porque de verdad no te asomaste. Es la mitad de la vieja idea 9 por una fracción del costo.

**Ojo.** Si el borde es duro marea. Mejor un "resorte" de 5 cm al llegar al límite. Valor en el .ini y 0 = apagado.

### F3. Vibración de confirmación
**Qué es.** Un toquecito en el mando cuando el juego confirma el cambio de postura (`+0x2af` pasa de 0 a 1 o al revés). Así sabés sin mirar que el juego te tiene agachada.

**Cómo.** El DLL ya vigila `+0x2af`. Falta mandar el pulso por OpenVR (`TriggerHapticPulse`).

**Ojo.** Es la primera vez que usamos la háptica. En juegos viejos con SteamVR Input nuevo a veces el pulso no llega: es la prueba de 5 minutos que destraba F4 y varias más.

### F4. Latido por distancia
**Qué es.** Re-evaluación de la vieja idea 1, ahora con todo a mano. Un latido suave que se acelera cuando el alien está cerca.

**Cómo.** Cada cuadro: recorrer la lista de NPC, quedarse con el alien, distancia a Ripley, pulso. La versión "le falta poco para verte" (leer la activación del sentido) es moderada y queda para después.

**Ojo.** Hay que distinguir al alien de los androides y humanos. Lo más simple es por el tipo de personaje; si no aparece, por el nombre de la entidad.

### F5. Altura de referencia que se aprende sola
**Qué es.** Hoy la referencia de "parada" sale de un valor fijo o del arranque. Si te sacás el visor, cambiás de lugar o recentrás, puede quedar corrida. Con esto el DLL aprende tu altura parada mirando la parte más alta de los últimos minutos, y la reajusta despacio.

**Cómo.** Un percentil móvil (el 90 % más alto de los últimos 2 minutos, solo cuando estás quieta). Al detectar un recentrado de MotherVR (salto brusco de posición), reiniciar.

### F6. Modo sentado
**Qué es.** Para jugar en silla: agacharse = inclinarte hacia adelante y abajo unos 15 a 20 cm desde tu postura sentada.

**Cómo.** Los mismos umbrales por velocidad de la v0.1.8/0.1.9, con una referencia sentada y otros valores en el .ini (`Modo=sentado`).

### F7. Recargar el .ini en vivo y tecla de pánico
**Qué es.** Cambiás `agachar.ini`, guardás, y el juego toma los valores al instante. Y una tecla (o combinación de botones) que apaga todo el mod de golpe.

**Cómo.** Cada segundo mirar la fecha del archivo. La tecla con `GetAsyncKeyState` en el bucle.

**Por qué.** No es gameplay, pero cada idea de abajo va a necesitar ajustar umbrales con el visor puesto. Esto ahorra decenas de reinicios.

### F8. Log de "por qué me vio"
**Qué es.** Cuando el alien te encuentra, que el log diga qué sentido lo activó (vista, ruido de movimiento, linterna...), cuánto, y en qué postura estabas.

**Cómo.** Enganchar 0x729120 (suma activación a un sentido) y anotar solo cuando el objetivo es Ripley.

**Por qué.** Sirve para saber si F2, el micrófono o las ideas de balance están haciendo lo que creemos. Sin esto, "me vio igual" no se puede diagnosticar.

---

## Moderado

| # | Idea | Qué hay que buscar |
|---|---|---|
| M1 | Arreglar la cámara en las muertes y animaciones (el bug del ducto) | Bandera de "cámara de animación activa" |
| M2 | Inclinarte de verdad activa el asomarse del juego | Pedido interno de asomarse |
| M3 | Contener la respiración en el locker inclinándote hacia atrás | Pedido interno del locker |
| M4 | Trotar en el lugar para correr | Pedido interno de correr |
| M5 | Sobresaltarte hace ruido | Ya está (llamada de ruido), falta probarla |
| M6 | Quedarte inmóvil te hace un poco más difícil de ver | Multiplicador del sentido visual |
| M7 | Latido según cuánto le falta al alien para verte | Activación del sentido visual |
| M8 | Linterna: apagarla tapándola con la mano | Estado interno de la linterna |
| M9 | Mirar por la rendija del locker | Posición de la rendija |
| M10 | Agacharse detrás de una cobertura baja te esconde mejor | Cómo el juego marca "en cobertura" |

### M1. Cámara en muertes y animaciones
**Qué es.** El primer bug que reportaste: cuando el alien te mata desde un ducto, la cámara no queda donde está el personaje. Lo mismo puede pasar en escaleras, palancas y cualquier animación que mueva a Ripley. Mientras dure la animación, ignorar cuánto te moviste en el cuarto y dejar solo hacia dónde mirás.

**Cómo.** Ahora sabemos dónde tocar: en la salida de la pose de cabeza de MotherVR. Falta la bandera de "animación o cinemática con cámara" en AI.exe. Candidatos: el flag de "jugador con control" (`+0x64`→`+0x1f`, anotado sin verificar) y los flags de sentidos pausados que usa 0x729120 durante cinemáticas.

**Ojo.** Que la vuelta sea suave (medio segundo), si no la cámara salta al terminar.

### M2. Asomarse con el cuerpo
**Qué es.** Re-evaluación de la vieja idea 11. Te inclinás de costado (o te levantás un poco estando agachada detrás de algo) y el juego entra en su asomarse, así el alien te ve asomada porque el juego sabe que lo estás.

**Cómo, ahora.** No simular el botón (ya vimos que eso puede ser un evento de script). Buscar el pedido de asomarse en el personaje, probablemente cerca del bloque de agachado (`+0x3e2c`). Y el problema del doble movimiento que había quedado abierto ya tiene solución: el juego mueve la cámara al asomarse y nosotros restamos ese desplazamiento en la pose, igual que hicimos con la altura.

**Gesto.** Desplazamiento de costado de la cabeza respecto del cuerpo, por velocidad + mínimo de recorrido, con bloqueo en sentido contrario. La misma receta de la v0.1.9.

### M3. Contener la respiración en el locker
**Qué es.** Re-evaluación de la vieja idea 2. Cuando el alien revisa el locker, inclinarte hacia atrás de verdad hace lo que en el original es tirar el stick para atrás.

**Cómo, ahora.** Ya sabemos cuándo estás escondida (`+0x2b4` y `+0x31c`→`+0x48`). Falta el estado interno de "inclinado atrás en el escondite". Gesto: la cabeza se va hacia atrás respecto de la puerta del locker.

**Ojo.** En el original esto consume aire. Si el gesto es fácil de sostener, hay que respetar que el aire se acabe igual: solo cambiamos cómo se pide, no las reglas.

**Variante de Franco: contener la respiración de verdad.** No se puede detectar de forma confiable que *dejaste* de respirar: respirar tranquilo por la nariz es casi silencio para el micrófono del Quest, igual que no respirar. Lo que sí se detecta es lo contrario: un resoplido, un jadeo o soltar el aire de golpe (ruido fuerte y corto pegado al micrófono). Propuesta:
- Contener la respiración se pide con el gesto (inclinarte atrás en el locker), sistema de honor: vos aguantás el aire de verdad mientras lo mantenés.
- `LimiteAire=juego` (por defecto) deja el límite del original; `LimiteAire=0` lo saca y el límite sos vos. Sacarlo requiere encontrar el medidor de aire, que es la misma búsqueda que el estado del locker.
- Con el micrófono: mientras contenés, cualquier soplido o jadeo corta la contención y hace ruido. Si soltás el aire de golpe porque no aguantás más, el alien lo escucha. Eso reemplaza a "taparse la boca" (vieja idea 3) en el locker.

**Versión 2 (Franco): detectar los dos sonidos.** La bocanada al tomar aire empieza la contención, y el resoplido y los jadeos al soltarlo la terminan (y hacen ruido). Es posible: la respiración es un soplido sin tono (ruido ancho, 0,3 a 1,5 s), distinto de la voz que ya detectamos por tono. Separar inhalar de exhalar por el sonido es difícil, pero no hace falta: en el locker el primer soplido largo empieza, el siguiente termina, y los jadeos seguidos (varios soplidos por segundo) confirman. El gesto de inclinarse queda de respaldo.
- Riesgo principal: Steam Link o el Quest pueden filtrar la respiración como ruido de fondo. Primera prueba: grabar desde "Steam Streaming Microphone" (Grabadora de Windows) una bocanada, 10 s de silencio y el resoplido con jadeos, y analizar el audio.
- Dificultad: moderado si la grabación muestra los soplidos claros; factible si hay que pelear con el filtro.
- **Descartada por Franco (26/10).** Queda la versión simple, elegida por él: `AireSegundos=N` en el .ini (cuánto aguantás vos de verdad). El juego usa ese tiempo en lugar del suyo y Franco aguanta el aire a la par. El gesto de inclinarse atrás sigue pidiendo la contención. Hace falta encontrar el medidor de aire del locker (o su duración en la config, que se podría cambiar con OpenCAGE sin parche).

### M4. Trotar para correr
**Qué es.** Hacer un trote suave en el lugar (la cabeza rebota 2 a 3 veces por segundo) activa el correr del juego mientras empujás el stick. Correr ahora cansa de verdad, y lo vas a hacer solo cuando haga falta.

**Cómo.** Detectar el rebote con la misma velocidad vertical suavizada de agacharse, pero buscando oscilación en vez de bajada. Pedido interno de correr: por buscar.

**Ojo.** No tiene que confundirse con agacharse ni disparar agachados en falso. Y tiene que poder apagarse: a mucha gente le marea o la cansa.

### M5. Sobresaltarte hace ruido
**Qué es.** Si el alien aparece y vos das un salto o un paso brusco para atrás en tu cuarto, eso hace ruido en el juego. Es una variante con más personalidad de la vieja idea 10.

**Cómo.** Pico de velocidad del visor (traslación, no giro) → la llamada de ruido ya anotada (0x6e9f10 y 0x72f080) con un volumen según la velocidad. Depende de juntar el código del micrófono con la v0.1.9, que igual está pendiente.

### M6. Inmovilidad
**Qué es.** Si te quedás quieta del todo (cabeza y manos casi sin moverse) unos segundos, el sentido visual del alien te detecta un poco más lento. Recompensa el "congelarse" que te sale natural.

**Cómo.** Medir quietud con OpenVR. Escalar la activación visual cuando el objetivo es Ripley (en 0x729120 o en el multiplicador de dificultad 0x6e16c0).

**Ojo.** Toca el balance. Máximo un 15 %, y que el bonus desaparezca al mirar al alien de frente (si no, es una trampa).

### M7. Latido por "le falta poco para verte"
**Qué es.** La versión buena de F4: el latido no depende de la distancia sino de cuánto se está llenando el sentido del alien hacia vos. Detrás de una pared, aunque esté a 2 metros, el corazón va tranquilo; en un pasillo a 15 metros mirándote, se dispara.

**Cómo.** Leer la activación del sentido visual del alien para el objetivo Ripley (lo que guarda 0x729120). Con F8 hecho, ya sabemos dónde está.

### M8. Tapar la linterna con la mano
**Qué es.** Poner la mano libre delante de la linterna la apaga (o la atenúa); sacarla la prende. Es más rápido e instintivo que un botón cuando oís al alien.

**Cómo.** Mano delante de la cara o del arma (según dónde GRAND ponga la linterna) → escribir el estado interno de la linterna. El alien ya tiene un sentido para ella (`SEE_FLASH_LIGHT`).

**Ojo.** Reemplaza a la vieja idea 5 (tocarse el hombro). Mismo pedido interno, gesto más natural.

### M9. Mirar por la rendija del locker
**Qué es.** Dentro del locker, acercar la cara a la rendija te deja ver más, y alejarte te protege. Con la correa de F2 especial para el locker: la cabeza puede ir hacia la rendija pero no atravesarla.

**Cómo.** Con F2 hecho, en el escondite usar como centro la posición de la rendija (la cámara que pone el juego al entrar) y un límite más chico.

### M10. Cobertura baja honesta
**Qué es.** Hoy, agachada detrás de una caja, el juego decide si te ve con la postura del personaje. Con agacharse real y la correa (F2) eso ya es casi honesto. Lo que falta es que levantar la cabeza por encima de la caja cuente: si sacás la cabeza, que el juego pase a "asomada" (M2) en vez de dejarte "escondida mirando".

**Cómo.** Es la combinación de F2 + M2 para el caso vertical. Lo pongo aparte porque es el bug número 2 que reportaste al principio (debajo de la mesa).

---

## Factible (difícil pero se puede)

| # | Idea | Por qué es difícil |
|---|---|---|
| D1 | El alien ve tu cabeza, no al personaje | Toca la visión del alien |
| D2 | Abrir un locker o una puerta empujando con la mano | Saber qué tenés delante |
| D3 | Botones y terminales que se aprietan con el dedo | Saber qué tenés delante |
| D4 | Pegar la oreja a una puerta (Wwise) | Audio sin código |
| D5 | Tirar con la fuerza del gesto | Parámetro de tiro |
| D6 | Esquivar al androide con el cuerpo | Balance y tiempos |
| D7 | Fundas "de verdad" con el pedido interno de equipar | Inventario por dentro |
| D8 | Recarga con gesto usando el pedido interno | Estado del arma |

### D1. La cabeza es lo que se ve
**Qué es.** La vieja idea 9 sigue siendo la versión definitiva: el punto que el alien usa para verte pasa a ser tu cabeza real. Con F2 y M2 la necesidad baja mucho; queda para los casos finos (asomar un ojo por el borde de una columna).

**Cómo.** Encontrar en la visión (0x72e1f0 o antes) el punto que se toma del jugador y reemplazarlo por la posición de la cámara, con la correa como límite.

**Por qué factible y no imposible.** Ahora sabemos sacar la posición real de la cabeza en el mismo espacio que el juego (la pose de MotherVR) y tenemos el log de sentidos (F8) para ver si funciona.

### D2. Empujar para abrir
**Qué es.** Para abrir un locker, un cajón o una puerta manual, extender la mano y empujar.

**Cómo.** Lo difícil no es el gesto sino saber qué objeto tenés delante y llamar a su "usar". Pista: el juego ya resuelve "qué tengo enfrente" para mostrar la indicación de interactuar; si encontramos dónde guarda ese objeto, el gesto solo pide usarlo.

### D3. Botones con el dedo
**Qué es.** Ascensores, terminales, paneles: acercar la punta del mando al botón lo aprieta.

**Cómo.** Mismo problema que D2 (qué hay delante) más la posición del botón en el mundo. Lo pondría después de D2, que usa la misma búsqueda.

### D4. Oreja contra la puerta
**Qué es.** Sigue siendo la vieja 16. Lo nuevo es que ya sabemos detectar "cabeza muy cerca de algo" con la correa (F2): si la cabeza toca el límite contra una puerta, subir el volumen del otro lado.

**Por qué difícil.** Wwise 2013 sin símbolos. Hay que encontrar un parámetro en tiempo real (RTPC) o el volumen del bus de efectos.

### D5. Tirar con fuerza
**Qué es.** La vieja 6, versión buena: la velocidad de la mano al soltar decide la distancia del ruidoso o la bengala.

**Cómo.** Buscar el pedido interno de tirar y, si existe, la fuerza (en el original crece mientras mantenés el botón, así que hay un valor). La lección de agacharse aplica directo: el botón es plan B.

### D6. Esquivar con el cuerpo
**Qué es.** Cuando un androide te agarra o tira un golpe, moverte de verdad para un costado lo esquiva.

**Por qué difícil.** El juego no tiene esquive para el jugador: sería una mecánica nueva. Tiempos y balance delicados. La pongo porque se puede, no porque sea prioridad.

### D7 y D8. Fundas y recarga con el pedido interno
Re-evaluación de las ideas de JayP. Antes las veíamos como "simular la tecla de arma o de recargar". Con lo aprendido, eso puede fallar igual que el botón de agacharse. La versión sólida es encontrar el pedido interno de equipar un arma concreta y el de recargar (`CMD_HolsterWeapon`, `CMD_ForceReloadWeapon` son las pistas). Las subo de moderado a factible por eso, pero quedan más confiables.

---

## Re-evaluación de las ideas anteriores

| Idea vieja | Antes | Ahora | Por qué |
|---|---|---|---|
| 1 Latido | Bajo a medio | **Fácil** (F4), moderado la versión buena (M7) | Lista de NPC y OpenVR ya andan en el juego |
| 2 Contener la respiración | Bajo | **Moderado** (M3) | Simular el botón no es confiable; hay que buscar el estado interno |
| 3 Taparse la boca | Bajo | Fácil, pero espera al micrófono | Igual que antes |
| 4 Enfocar el tracker | Bajo | Moderado | Mismo motivo que la 2 |
| 5 Linterna con gesto | Bajo | Moderado, reemplazada por M8 | Mismo pedido interno, mejor gesto |
| 6 Tirar | Medio | Factible (D5) | Mismo motivo que la 2 |
| 7 Manivela | Medio | Moderado | Machacar se puede hacer en el estado del mando, como GRAND |
| 8 Dificultad VR | Bajo | Fácil, mejor después de F8 | Con el log de sentidos se ajusta con datos |
| 9 Asomarse de verdad | Alto | Factible (D1), pero F2 + M2 cubren casi todo | La pose ya es nuestra |
| 10 Moverte rápido hace ruido | Bajo | Moderado (M5) | Falta juntar micrófono y agacharse |
| 11 Inclinarte para asomarte | Medio | Moderado (M2), y el doble movimiento ya tiene solución | Restamos el desplazamiento en la pose |
| 12 Golpear para distraer | Medio | Moderado | La llamada de ruido ya está; falta el gesto |
| 13 Botiquín al cuello | Bajo | Moderado | Pedido interno de curar, por buscar |
| 14 Sintonizador con la muñeca | Medio | Moderado | GRAND ya escribe sticks en `+0x268`; podemos hacer lo mismo |
| 15 Guardar con la tarjeta | Bajo | Moderado | Pedido interno de usar, por buscar |
| 16 Oreja contra la puerta | Alto | Factible (D4) | Igual de difícil; F2 resuelve el gesto |
| 17 Temblor al apuntar | Bajo | Fácil, con F3/F4 | Solo háptica |
| JayP: micrófono | Medio | En curso (hilo propio) | Hay que pasarlo a las fuentes v0.1.9 |
| JayP: fundas, recarga | Medio | Factible (D7, D8) | Pedido interno en vez de botón |
| JayP: melee por gesto | Medio | Moderado-factible | `CMD_LaunchMeleeAttack` como pista |
| JayP: cambio de mano, recarga completa | Alto | Sigue siendo para JayP | Vive adentro de GRAND |

---

## Orden que propongo

1. **F7 y F8** primero: no se ven, pero hacen que todo lo demás se pruebe más rápido y con datos.
2. **F1, F2 y F3**: salen casi gratis con el envoltorio de pose y cambian mucho la sensación de estar escondida.
3. **F4** si la háptica de F3 funciona.
4. **M1**: es el bug que te trajo hasta acá.
5. **M2 y M10** juntas (asomarse), después **M3**.
6. Juntar el micrófono con la v0.1.9 y sumar **M5**.
7. El resto según ganas.

*Lluvia de ideas. Nada está implementado ni probado.*

---

# Tercera tanda: 33 ideas más

26/10/2026. Pedido de Franco: "30 o 40 ideas factibles buenas". Todas se apoyan en las piezas que ya andan (bucle por cuadro, OpenVR con cabeza y mandos, envoltorio de pose, estado de Ripley, lista de NPC) o en la llamada de ruido del micrófono. Dos palancas nuevas que no habíamos usado:

- **Audio propio.** El DLL puede reproducir sus propios sonidos (un .wav) por Windows, sin tocar Wwise. Con Steam Link llegan al visor igual que el juego.
- **Volumen del juego.** Windows deja que un programa cambie el volumen de su propia sesión de audio (ISimpleAudioVolume). Nuestro DLL vive adentro del juego, así que puede subir o bajar todo el sonido del juego al instante.

## Fácil (11)

| # | Idea |
|---|---|
| E1 | Pausa sola al sacarte el visor, y recentrado al ponértelo |
| E2 | Latido que se escucha, no solo se siente |
| E3 | Latido del lado del alien |
| E4 | Vibración al chocar la correa: aviso de pared |
| E5 | Tus pasos en el mando |
| E6 | Recentrado automático al salir de un escondite |
| E7 | Mano en la oreja: escuchás más |
| E8 | Taparte los oídos: escuchás menos |
| E9 | Resumen de la partida al cerrar el juego |
| E10 | Mapa girando la muñeca como si miraras un reloj |
| E11 | Ripley jadea si te moviste mucho en tu cuarto |

**E1. Pausa al sacarte el visor.** OpenVR avisa cuando el sensor del visor deja de detectar tu cara. Si pasa, el DLL manda la tecla de pausa. Al ponértelo de nuevo, recentra. Evita volver y encontrarte muerta porque el alien pasó mientras atendías la puerta.

**E2. Latido audible.** Un .wav de latido grave, muy bajo, que se acelera con la distancia al alien (o con M7 cuando esté). Se suma a la vibración de F4 o la reemplaza si la háptica no llega. Volumen en el .ini.

**E3. Latido del lado del alien.** El pulso sale más fuerte en el mando del lado donde está el alien respecto de tu cabeza. Sin mirar el tracker sabés para dónde no ir. Ojo: es información que el original no da; con intensidad baja es una sensación, no un radar.

**E4. Aviso de pared.** Cuando la correa de F2 frena tu cabeza, un pulso cortito. Así tu cuerpo entiende que ahí hay algo, en vez de que la imagen "se pegue" sin explicación.

**E5. Pasos en el mando.** Un pulso mínimo por cada paso de Ripley, calculado por la velocidad de su posición (`+0x70..0x78`). Caminar despacio casi no vibra, correr sí: sentís cuánto ruido estás haciendo.

**E6. Recentrado al salir del escondite.** Cuando `+0x2b4` pasa de 1 a 0 (saliste del locker o del ducto), recentrar la posición horizontal suavemente. Arregla el desfasaje que queda después de esconderse.

**E7. Mano en la oreja.** Mano ahuecada al costado de la cabeza → el volumen del juego sube un 50 % mientras la mantengas. Escuchar pasos detrás de una pared con un gesto natural. Es la versión barata de "oreja contra la puerta" (D4), sin tocar Wwise.

**E8. Taparte los oídos.** Las dos manos en las orejas → el juego baja al 30 %. No te salva de nada, pero es lo que hace cualquiera cuando el alien grita al lado tuyo.

**E9. Resumen de la partida.** Al cerrar, el log escribe: cuántas veces te agachaste, tiempo escondida, cuántas veces el alien te vio (con F8), y cuántos "sustos" tuviste (picos bruscos de la cabeza con el alien cerca). Divertido de leer y útil para ajustar.

**E10. Mapa en la muñeca.** Girar la muñeca izquierda hacia la cara, como mirando la hora, abre el mapa; bajarla lo cierra. El mapa es una pantalla, no una acción del personaje, así que hacer OR en el botón del mando (como GRAND con el IED) debería alcanzar. Si no anda, pasa a moderado.

**E11. Ripley jadea.** Si en el último minuto te moviste mucho de verdad (agacharte y pararte seguido, trotar), suena un jadeo propio, bajito, que se calma solo. Solo ambiente; la versión con ruido es N3.

## Moderado (15)

| # | Idea |
|---|---|
| N1 | Tracker pegado al pecho: el alien no oye sus bips |
| N2 | Chocar los mandos: ruido fuerte para atraerlo |
| N3 | Agitada de verdad: tu jadeo hace ruido |
| N4 | Cuclillas profundas: más silenciosa que agachada |
| N5 | Mirarlo fijo desde el escondite te delata |
| N6 | Agarrar el arma con las dos manos estabiliza |
| N7 | Prender la bengala raspándola |
| N8 | Soplete: seguir la costura con la mano |
| N9 | Vibración al recibir daño, latido con vida baja |
| N10 | Cada bip del tracker vibra |
| N11 | Escaleras: agarrar con las dos manos para subir |
| N12 | Plantar la IED o el EMP bajando la mano al piso |
| N13 | Cambiar las pilas de la linterna con la mano |
| N14 | Salir del locker empujando con el cuerpo |
| N15 | Temblar en el locker te delata |

**N1. Tracker al pecho.** En el original los bips del tracker los oye el alien (evento tipo 2, escáner). Con la mano izquierda pegada al pecho, el volumen de ese evento (`+0x30` en el evento que procesa 0x730c30) se multiplica por 0,3. Seguís leyendo el tracker si bajás la cabeza. Gesto lógico y con costo: no lo ves bien mientras lo tapás.

**N2. Chocar los mandos.** Los dos mandos se juntan rápido y frenan de golpe → ruido fuerte en tu posición (la llamada del micrófono). Un "aplauso" para llamar al alien hacia los humanos que te persiguen, como el truco del original, pero desde donde estás vos: arriesgado.

**N3. Jadeo con ruido.** Igual que E11, pero el jadeo cuenta como ruido de movimiento chico durante unos segundos. Si te agitás, quedate quieta antes de entrar al pasillo.

**N4. Cuclillas profundas.** Si estás agachada y además tu cabeza baja de un segundo umbral (por ejemplo 0,85 m), el ruido de tus pasos baja un 20 %. Ir en cuclillas cansa de verdad, así que el bonus se paga solo. Tocar el multiplicador de MOVEMENT_SOUND solo para Ripley.

**N5. Mirada fija.** Si desde el locker o bajo una mesa tu cabeza apunta directo al alien más de 2 segundos con él cerca, la activación visual sube un poco. Mirar de reojo pasa a ser una habilidad. Usa la dirección del visor y la posición del alien; el ajuste se hace en 0x729120.

**N6. Dos manos.** Con la mano libre agarrando el arma (mando izquierdo cerca del derecho), el juego pasa a su estado de "apuntando", que reduce el bamboleo. Hay que encontrar ese estado (o el factor de dispersión) en el arma.

**N7. Bengala raspada.** Con la bengala equipada, un raspón corto con la otra mano la prende. Si raspás despacio, no prende. Mismo gesto por velocidad de agacharse, aplicado a la mano.

**N8. Soplete.** Para cortar paneles, el juego te hace mantener un botón. Con esto mantenés el botón solo mientras la mano se mueve despacio sobre la zona; si la sacás, se pausa. Mantener el botón por el estado del mando, como E10.

**N9. Daño y vida baja.** Pulso fuerte al perder vida y latido más rápido con poca vida. Falta el campo de vida de Ripley (cerca del personaje, a buscar).

**N10. Bips del tracker.** Cada bip vibra en el mando izquierdo, más fuerte cuanto más cerca está el contacto. Se engancha al evento tipo 2 que ya ubicamos.

**N11. Escaleras.** Frente a una escalera, levantar las dos manos y tirar hacia abajo como agarrando un barrote inicia la subida (el "usar" del juego). Falta saber que estás frente a una escalera: la indicación de interactuar del juego tiene que vivir en algún lado.

**N12. IED al piso.** Con la IED o el EMP en la mano, agacharte y bajar la mano casi al piso la planta ahí en vez de tirarla. Usa la postura que ya sabemos leer.

**N13. Pilas de la linterna.** Cuando la linterna titila, llevar la mano a la cadera y después a la linterna pone pilas nuevas (si tenés). Misma idea que la recarga con gesto, en algo más chico y frecuente.

**N14. Salir del locker de golpe.** Inclinarte hacia adelante rápido dentro del locker abre la puerta y salís. Escapar del locker cuando el alien se va, con el cuerpo. Falta el pedido de salir del escondite (seguramente al lado del de entrar, `+0x31c`).

**N15. Temblar en el locker.** Si estás escondida, el alien a menos de 3 m, y tu cabeza se mueve mucho (no quieta), eso es un ruido chico. Junto con contener la respiración (M3) hace que el locker sea quedarte quieta de verdad.

## Factible (7)

| # | Idea |
|---|---|
| X1 | Manos arriba: rendirte ante los humanos |
| X2 | Apuntarle a un humano lo pone nervioso |
| X3 | Agarrar objetos del piso bajando la mano |
| X4 | Entrar al locker metiéndote de verdad |
| X5 | La pantalla del tracker brilla en la oscuridad |
| X6 | La llave pega del lado hacia donde movés la mano |
| X7 | Empujar a un androide con las dos manos |

**X1. Manos arriba.** Levantar las dos manos por encima de la cabeza frente a un humano hostil hace que dude unos segundos antes de disparar. En el original los humanos tienen estados de "advertencia"; la idea es forzar ese estado. Es una mecánica nueva y toca la IA humana, por eso factible.

**X2. Apuntar a un humano.** Lo contrario: si le apuntás a la cabeza, se pone agresivo o se esconde. La dirección del arma está en el mando derecho; falta entender cómo el juego decide la reacción de los humanos.

**X3. Agarrar del piso.** Objetos en el piso: bajar la mano y apretar el grip los levanta, en vez de mirar y apretar un botón. Mismo problema que D2 (saber qué objeto tenés delante), más la posición del objeto.

**X4. Entrar al locker.** Caminar o inclinarte hacia adentro de un locker abierto te mete. Mismo problema que X3, con la ventaja de que el juego ya sabe cuándo estás frente a uno (te muestra la indicación).

**X5. Brillo del tracker.** Con el tracker levantado frente a la cara en un lugar oscuro, el alien lo puede ver como una linterna débil (`SEE_FLASH_LIGHT`). Le da peso a bajar el tracker cuando está cerca. Inyectar un sentido nuevo es más delicado que el ruido, y es fácil pasarse de castigador.

**X6. Lado del golpe.** El golpe de la llave sale hacia la izquierda o la derecha según el movimiento de la mano. Solo si el juego tiene animaciones por lado; si no, no vale la pena. Sigue resolviendo el juego si pega, como en la evaluación de JayP.

**X7. Empujar androides.** Empujar con las dos manos a un androide que te agarra lo hace trastabillar un segundo. No existe en el original: mecánica nueva, balance delicado. La dejo al final.

## Orden que propongo para esta tanda

1. **E1, E7, E8**: salen sin buscar nada en el juego y cambian mucho cómo se vive.
2. **E2, E3, E5** junto con F3/F4, cuando sepamos si la háptica llega.
3. **N1, N10, N15**: el tracker y el locker con cuerpo. Necesitan el evento tipo 2 y la llamada de ruido.
4. **N4, N14, N12**: más uso del cuerpo agachado.
5. El resto según ganas.

*Lluvia de ideas. Nada está implementado ni probado.*

---

# Qué analizar del juego para saber qué se puede hacer

26/10/2026. Ordenado por cuántas ideas destraba cada cosa.

| # | Qué averiguar | Cómo | Destraba |
|---|---|---|---|
| A1 | Qué tiene Ripley en la mano y su estado: arma equipada, munición, apuntando, vida, pilas, equipar un arma concreta, recargar | Ghidra, desde `CMD_HolsterWeapon` / `CMD_ForceReloadWeapon` y el inventario | D5, D7, D8, N6, N7, N9, N12, N13, ideas 6 y 13 |
| A2 | Los sentidos por dentro: leer la activación de cada sentido hacia Ripley (0x729120), cómo distinguir alien / androide / humano, y el evento tipo 2 (bips del tracker) | Ghidra + un log en el juego (F8) | F4, F8, M6, M7, N1, N4, N5, N10, X5, E3, idea 8 |
| A3 | Los otros pedidos del personaje, al lado del de agacharse (`+0x3e2c`): asomarse, correr, entrar/salir del escondite, inclinarse atrás en el locker, medidor de aire | Ghidra, como encontramos `+0x3e3a` | M2, M3 (con AireSegundos), M4, M10, N14 |
| A4 | Qué objeto tenés delante: dónde guarda el juego el objeto de la indicación de "interactuar" y cómo pedir "usar" sobre él | Ghidra, desde el dibujo de la indicación | D2, D3, X3, X4, N11, idea 15 |
| A5 | Si la llamada de ruido funciona en el juego (nunca se probó) | Prueba en el juego: juntar micrófono con v0.1.9 | Micrófono, M5, N2, N3, N15, ideas 10 y 12 |
| A6 | Si la vibración de los mandos llega con MotherVR + SteamVR | Prueba de 5 minutos en el juego (F3) | F3, F4, E2–E5, N9, N10, M7, idea 17 |
| A7 | Bandera de "animación o cinemática con cámara" y "jugador con control" (`+0x64`→`+0x1f`, sin verificar) | Ghidra + log | M1 (cámara del ducto), y que ningún gesto se dispare en cinemáticas |
| A8 | Qué botones funcionan haciendo OR en el estado del mando (`+0x1c`), como GRAND con la IED: mapa, soplete, linterna, enfocar tracker, manivela | Prueba en el juego con un .ini que aprieta cada botón | E10, N8, M8, ideas 4, 5 y 7 |
| A9 | Que la pose de cuerpo de MotherVR (+0x18) esté en el mismo espacio que la de cabeza | Log en el juego | F2 (correa), M2, M9, E4 |
