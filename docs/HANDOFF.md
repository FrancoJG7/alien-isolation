# HANDOFF COMPLETO: mods de gameplay para Alien: Isolation VR

Escrito el 26/10/2026 al cerrar un hilo largo; actualizado el 29/10 con las features de mods_vr (sección 4, "Versiones vigentes"). **Para la próxima sesión:** este archivo alcanza solo, sin el historial del chat. Tiene:
- el contexto;
- cómo trabajar con Franco;
- todo lo decidido;
- el detalle de cada idea elegida;
- todo lo que sabemos del juego, de MotherVR y de GRAND;
- el estado del código, qué falta y cómo rearmar las herramientas.

Los otros .md de la carpeta quedan como respaldo: casi nunca hace falta abrirlos.

Índice:
1. Contexto y equipo de Franco
2. Cómo trabajar con Franco
3. Qué se pidió y qué se decidió en este hilo
4. Estado del código (mod de agacharse) y lecciones aprendidas
5. Las ideas elegidas, en tandas, con todo su detalle
6. Diseño propuesto de la T0 (versión de diagnóstico)
7. Todo lo que sabemos del juego (direcciones)
8. MotherVR, GRAND y OpenVR
9. La IA, los sentidos y el ruido (del mod del micrófono)
10. Qué falta averiguar (Ghidra, archivos y pruebas en el juego)
11. Herramientas: Ghidra, compilación, emuladores
12. Ideas que no se eligieron (para más adelante)
13. Mapa de archivos

---

## 1. Contexto y equipo de Franco

- Franco juega **Alien: Isolation** (Steam, PC) en VR con:
  - **MotherVR 0.8.1** de Nibre: un `dxgi.dll` de código cerrado.
  - **GRAND v0.6.3** de JayP: un `XINPUT1_3.dll` que agrega manos, gestos, IED física, etc., encima de MotherVR. Tiene su `grand.ini` y escribe `ai-grand.log` solo si el juego arranca con `-log`.
- **Visor:** Meta Quest 3 por **Steam Link** (SteamVR). **No tiene mando de Xbox**: XInputGetState devuelve 1167 (no conectado). Steam Link crea en Windows el micrófono "Steam Streaming Microphone".
- Franco mide 1,70 m. El log midió los ojos parado a 1,62 m.
- Su `grand.ini` (log del 26/10): HudType=1, CameraOffset=0.1, HideBody=0, HeightOffset=0, RecenterOnFirstLevelLoad=1, OGMotherVRMode=0, IEDPhysicalThrowVelocityModifier=1.2.
- Usa Windows. Tiene Python. Hay que decirle siempre "abrí **PowerShell**".
- El juego está en `C:\Program Files (x86)\Steam\steamapps\common\Alien Isolation\`.
- Lo que hacemos es para uso personal, no se distribuye.

## 2. Cómo trabajar con Franco

- **Idioma:** español rioplatense, tono cercano y simple ("probá", "fijate", "copiá"). Frases completas, sin jerga de assembly, respuestas cortas. Va directo a probar.
- **Nivel:** principiante en ingeniería inversa. Sigue bien instrucciones numeradas y cortas, de a un paso.
- **Reparto:** la IA hace todo lo difícil (análisis, código, pruebas en emulador) y entrega un **.zip** con:
  - el DLL;
  - el .ini comentado;
  - las fuentes;
  - un `LEEME.txt` en español ("Qué trae / Instalar / Cómo se usa / Volver atrás / Sabido").

  - **Desde el 29/10 prefiere que le pasen el DLL y el .ini sueltos** (adjuntos directos); el zip con fuentes queda guardado aparte.

  Franco instala, prueba y reporta en pocas palabras. A veces tiene errores de tipeo o se corrige en el mensaje siguiente: leer todo antes de actuar.
- **Nunca decir "probado"** de algo verificado solo en el emulador o en Wine. Siempre decir "sin probar en el juego".
- **Siempre construir sobre el último build** que Franco confirmó, para no perder arreglos.
- **Decisiones de diseño:** 2 a 4 opciones con una recomendada. Cuando decide, no se re-discute.
- **Commits** (lleva un repo propio): pide título y descripción en español, asunto corto, cuerpo breve, y "Sin probar en el juego." al final si corresponde.
- **Costos:** le preocupa gastar cuota. Por eso existe este handoff.
  - Volcados grandes de Ghidra: no.
  - Filtrar todo con grep o head.
  - No releer archivos enormes si este alcanza.
  - No usar subagentes salvo que él lo pida.
- **Memoria del proyecto apagada:** todo lo que haya que recordar va a archivos en `/mnt/project-files/ideas_gameplay/`.
- **Doc para tildar:** "Elegí qué implementar: mods de Alien Isolation VR"
  - Link: https://claude.ai/artifact/MNA3fQWBasTtZySnv9Etex
  - Claude Doc, id `a4eaf794-14de-4923-8191-70f3f0c9312d`, container `{"kind":"project","id":"a4eaf794-14de-4923-8191-70f3f0c9312d"}`, nodo del cuerpo `f02183d0-23fc`.
  - Franco tilda ahí lo que quiere. Al 26/10 ya está volcado en la sección 5. Al final del doc agregué la tabla de tandas. Si dice que marcó algo nuevo, leer el doc.

## 3. Qué se pidió y qué se decidió en este hilo (26/10)

Orden de lo que pidió Franco:
1. Lluvia de ideas gigante de gameplay usando lo aprendido con agacharse, ordenada en fácil, moderado y factible. Salió en `IDEAS_v2.md` (F, M, D).
2. Contener la respiración: se discutieron varias variantes con el micrófono (detectar la bocanada y el resoplido). **Franco las descartó.** Quedó: **`AireSegundos=N` en el .ini**. El juego usa ese tiempo y Franco aguanta el aire de verdad a la par. Sin micrófono.
3. 30-40 ideas más (E1-E11, N1-N15, X1-X7, en `IDEAS_v2.md`).
4. Qué analizar del juego (A1-A9), y analizarlo con Ghidra (`ANALISIS_GHIDRA.md`).
5. Reevaluar la dificultad (`PRIORIDADES.md`) y detallar fáciles y moderadas (`DETALLE.md`).
6. Segunda tanda de 10 áreas de Ghidra (`ANALISIS_GHIDRA_2.md`): salió la "tecla virtual", entre otras cosas.
7. 50 ideas fáciles nuevas (`IDEAS_v3.md`).
8. El doc para tildar. Franco marcó, pidió mi opinión y tandas ordenadas (`PLAN_TANDAS.md`). **Aclaró que no hay que implementar todo de una.**
9. Preguntó qué es la 30 y la aceptó (va en T4).
10. Preguntó por jugar todo con hand tracking del Quest 3. Queda como idea a futuro (`HAND_TRACKING.md`, resumen en la sección 12).
11. Preguntó qué falta analizar (sección 10). **Le pedí un `COMMANDS.PAK`** de un nivel con lockers, para el aire. Estaba por arrancar la tercera tanda de Ghidra cuando pidió este handoff.

**Siguiente paso concreto:**
- Si Franco dice "seguí": hacer la tercera tanda de Ghidra (sección 10.1) y escribir `ANALISIS_GHIDRA_3.md`.
- Después, implementar la **T0** (sección 6).
- Si subió el COMMANDS.PAK, aparece en `/mnt/project-files/uploads/hearth/` (sin extensión, con nombre de id): leer el aire (sección 10.2).

## 4. Estado del código y lecciones

### Versiones (en `../agachar/`)
- **v0.1.9:** la **estable confirmada** ("funciona absolutamente perfecto", 18:53 del 26/10). Agacha y para a Ripley con el cuerpo, por velocidad, con altura fija para MotherVR.
- **v0.1.10** (otro hilo): A15, recarga de `agachar.ini` en vivo cada 1 s y tecla de pánico (`[General] TeclaPanico`, default ctrl+f12, MessageBeep al cambiar). **Sin probar en el juego.**
- **v0.1.11** (otro hilo, **la más nueva, sin probar en el juego**): dos DLL.
  - `xinput_ag.dll` = **cargador** (`cargador.c`): XInput, PeekMessageA, vtable de MotherVR, botón, log, recarga. Queda fijo con el juego abierto.
  - `agachar_logica.dll` (`logica_juego.c` + `logica.c`): exporta `ag_iniciar/ag_tick/ag_compensar/ag_boton/ag_terminar`. Interfaz en `anfitrion.h` (ANFITRION_VERSION 1).
  - El cargador carga una copia en `%TEMP%\agachar_logica_<pid>_<n>.dll` y la recarga con `[General] TeclaRecargar` (ctrl+f11) o `RecargarSolo=1`. Usa un SRWLOCK compartido.
  - Probado en Wine con el dxgi.dll real: 20 recargas sin cuelgues.
- **v0.2.0** (hilo "Diseñar recarga en caliente de features", 28/10, **CONFIRMADA en el juego 28/10 23:56: recarga de mods_vr\\agachar.dll y baúl andan**; v0.1.11 SI se probó en el juego: la recarga anda): cargador general + carpeta `mods_vr\` con un DLL y un .ini por feature. Interfaz en `anfitrion.h` v2; ver `../agachar/NOTAS.md` (v0.2.0) y `../recarga/EVALUACION.md`. **Cada idea nueva va como su propia feature en mods_vr**, pidiendo enganches al cargador (`enganchar_codigo` / `enganchar_puntero`) en vez de cambiar el cargador. Servicios compartidos que todavía NO están en el cargador (agregarlos cuando llegue T0, un solo reinicio): tecla virtual, mezclador de vibración, volumen.
- Steam Input: si el log dice "mando 0: conectado" sin mando, es Steam Input y rompe el menú de pausa (no deja aceptar). Franco lo desactivó el 28/10.
- **Para las tandas nuevas:**
  - Partir de **v0.1.11** si Franco la confirma; si no anda, de v0.1.9.
  - Los enganches nuevos (tecla virtual, 0x729120, XInputSetState) van en el **cargador**, que es lo único que no se recarga. Exponerlos a la lógica subiendo `ANFITRION_VERSION`.
  - La lógica de cada idea va en la DLL recargable, así Franco ajusta sin cerrar el juego.
- **manos v0.6** (hilo "Evitar que las manos atraviesen paredes", 29/10, **CONFIRMADA en el juego 29/10 01:05: anda, también en marcos de puertas**): feature `mods_vr\manos.dll` + `manos.ini` en `../agachar/manos_v0.6/` (DLL, ini y fuentes). No cambia el cargador. Engancha con `enganchar_puntero` IVRCompositor::GetLastPoses (slot 3 del compositor guardado en [MotherVR+0x6af50]): de esa lista GRAND saca las manos con SteamVR. En el hilo del juego tira rayos (0x7ebab0, filtro 23) desde la cámara del jugador hacia la mano: centro, 4 puntos a `Radio` 0.05 alrededor y 1 a `Adelante` 0.12 hacia donde apunta el mando; si chocan, la mano se acerca a los ojos hasta `Margen` 0.06 antes de la pared. Detalle y historia v0.1 a v0.6 en `../agachar/NOTAS.md` y `ANALISIS_MANOS.md`.
  - Sirve para otras ideas: **rayos del juego** (0x7ebab0, mundo [0x170791c], vectores alineados a 16), **cámara del jugador** ([[0x16f0c88]+0x58]+0x1d8/+0x1dc, cuaternión +0x2c, pos +0x3c; sigue al stick y a la cabeza) y **paso visor → mapa** (ejes de la cámara × diag(1,1,-1) × visor^T). Y que **cambiar la pose de un mando en GetLastPoses mueve la mano de GRAND** (probado).
- **Versiones vigentes al 29/10 03:30** (todas features de `mods_vr\`, cargador v0.2.0 sin cambios; historia completa en `../agachar/NOTAS.md`):

| Feature | Versión | Carpeta | Estado |
|---|---|---|---|
| manos (no atravesar paredes + pared al tacto + usar al tocar) | v0.8 | `../agachar/manos_v0.8/` | v0.7 confirmada; v0.8 (usar al tocar) probada: sirve poco, muchos objetos no tienen pared o quedan altos |
| agachar (calibrar con gesto, AlturaVista) | v0.2.3 | `../agachar/tacto_calibrar_v0.3/` | sin confirmar la v0.2.3 (v0.2.2 confirmada) |
| pausa al sacarse el visor | v0.1 | `../agachar/pausa_v0.1/` | confirmada (Quieto=2, Segundos=1) |
| mapa en la muñeca + stick derecho | v0.7.1 | `../agachar/mapa_v0.7.1/` | v0.6 confirmada; v0.7.1 agrega que no abre después de un empujón (VelocidadMax 0.6, Calma 1.0) |
| usar con la palma | v0.1.5 | `../agachar/usar_v0.1.5/` | **confirmada**, también pegado a una computadora |

- **usar v0.1.5** (hilo "Usar objetos con la palma", 29/10): empujás hacia adelante con cualquier mano (>1 m/s hacia el frente del visor, mano >0.12 m delante de los ojos, desvío lateral <50°, abajo-adelante vale) y aprieta la **E con SendInput** (acción 12). No usa la tecla virtual porque mapa.dll ya engancha las vtables de los dispositivos tipo 0 y 1 y **el cargador no deja que dos features enganchen lo mismo**. Vibra solo si el empujón usó algo (aparece un objeto en `pj+0x3570`/`+0x3574` en 0.8 s). Lecciones: la E tiene que salir temprano en el empujón (con Alcance 0.22-0.30 fallaba pegado a la computadora); Franco es bajo para algunos cajones altos, el empujón los resuelve.
- **Micrófono** (`../microfono/microfono_v0.2.1.zip`): no tiene los arreglos de agachar. **Fingía un mando de Xbox y eso rompió el menú** (el stick no navegaba, solo "aceptar"): no repetir. Al juntarlo (T7), partir de las fuentes de agachar.

### Cómo funciona hoy el mod
1. **Carga:** GRAND carga el XInput real en su DllMain (0x100367b0). Lo parcheamos con `parchear_grand.py`:
   - el call en 0x100367e8 pasa a `add esp,8; xor eax,eax; nop`;
   - el string en 0x100a1fa4 pasa a `xinput_ag.dll`;
   - se anula la relocación 0x100367ea.

   Nuestro DLL reenvía las 7 funciones de XInput y carga el XInput real en su DLL_PROCESS_ATTACH.
2. **Reloj:** sin mando de Xbox, XInputGetState se llama poco y no sirve de reloj. Por eso reemplazamos **PeekMessageA en la IAT de AI.exe**.
   - Solo corremos `paso()` cuando la dirección de vuelta es **0xa13096** + delta: es la bomba de mensajes 0xa12fb0, llamada en cada vuelta del bucle 0x7a8460, en el mismo hilo que el cuadro del juego.
   - Firma verificada en 0xa13082 (39 bytes).
3. **OpenVR:** MotherVR importa `openvr_api.dll` en diferido (IAT diferida en RVA 0x6769c) y trae uno embebido. Lo buscamos en este orden:
   1. `openvr_api.dll` cargado;
   2. la IAT diferida de cualquier módulo;
   3. `vrclient.dll` → VRClientCoreFactory("IVRClientCore_003") → slot 3.

   Usamos **IVRSystem_017**, slot 11 = GetDeviceToAbsoluteTrackingPose (da visor **y mandos**). Universo 2 = crudo, sin el recentrado.
4. **Agacharse (método directo):** llama `0x7cae20` Character::Crouch / `0x7cae90` Uncrouch (thiscall sobre pj). Ponen el pedido en `pj+0x3e3a`, y 0x7d2ad0 lo copia a `pj+0x2af` en ~11 ms. Vigila 2,5 s y reintenta (0,2 s entre intentos, máximo 5) si el juego lo deshace.
5. **Altura fija para MotherVR (v0.1.7):** MotherVR inclina la mirada hacia abajo cuando bajás la cabeza. Envolvemos dos entradas de la vtable del backend SteamVR de MotherVR (RVA 0x5ddac):
   - **+0x14** pose de cabeza (0x100070e0);
   - **+0x18** pose de "cuerpo", solo giro y posición (0x10007220).

   La salida 3x4 = pose × diag(-1,-1,1,-escala). La altura está en **out[7] = -escala·y**. Hacemos `out[7] -= escala·bajada`. GRAND sigue viendo la altura real en this+0x10.
   - Firma: bytes `8d73106a006a006a10568bc8ff520c` en RVA 0x7149, con la vtable apuntando a 0x70e0/0x7220.
   - **Esta es la palanca de todo lo de pose** (techo, correa, rendija, cámara en muertes).
6. **Lógica de agacharse:**
   - velocidad vertical suavizada (~30 ms);
   - agacha si baja a más de `Velocidad` (0,8) y ya bajó `BajadaRapida` (0,12), o si baja `BajadaAgachar` (0,25);
   - se para si sube `SubidaRapida` (0,15) desde el mínimo, o si la bajada queda menor que `BajadaParar` (0,12);
   - se bloquea si te movés en sentido contrario a más de 0,1 m/s.

### `agachar.ini` actual (v0.1.11)
```
[Agachar] Activado=1, BajadaAgachar=0.25, BajadaParar=0.12, DemoraMs=5, Velocidad=0.8,
          BajadaRapida=0.12, SubidaRapida=0.15, AlturaParado=0, PararSolo=1, Metodo=0 (0 directo,
          1 tocar botón, 2 mantener), Universo=2, FijarAltura=1, Log=1
[General] TeclaPanico=ctrl+f12, TeclaRecargar=ctrl+f11, RecargarSolo=1
```
Cada idea nueva tiene su sección con `Activado=` propio. Los comentarios del .ini van en español simple.

### Lecciones (aplicar a todo)
1. **Simular el botón de script no sirve.** MonitorActionMap (0x5a8790, vtable 0x12591f8) es una entidad CATHODE: tocarla solo dispara eventos de script, y Franco "miraba un poco abajo".
   - Lo que anduvo fue el **pedido interno** del personaje.
   - Para las otras acciones ahora está la **tecla virtual** (sección 7.2), que es como apretar una tecla real.
2. **El juego deshace cosas.** Vigilar unos segundos y reintentar. GRAND además llama Crouch/Uncrouch solo a los ~18 s y ~48 s de arrancar (su arreglo de cámara, 0x10030702/0x10030826).
3. **Velocidad mejor que distancia** para los gestos, con un recorrido mínimo, y bloqueo si te movés al revés.
4. **El log manda.** Cada idea anota sus cambios con hora y tiene su `Activado=0` para pruebas A/B.
   - Así se descubrió que el "mirar abajo" era de MotherVR: con el XINPUT1_3 original también pasaba, con OGMotherVRMode=1 también, y sin XINPUT1_3 también.
   - La prueba limpia siempre es sacar capas.
5. **No fingir un mando de Xbox:** rompe el menú.
6. **ASLR:** AI.exe se carga con rebase (los logs muestran +11927552 y -2621440). Siempre sumar el delta a las direcciones de este documento (base 0x400000).
7. **Un enganche en una función caliente** (0x729120, la de los botones) tiene que ser mínimo. Probar primero en el emulador con unicorn.

## 5. Las ideas elegidas, en tandas, con todo su detalle

**Marcadas por Franco:**
- Paso cero: diagnóstico.
- Fáciles ya: A1, A2, A3, A4, A10, A11, A12, A15, A16, A17a (latido audible), A17c (taparse los oídos), N-1, N-3.
- Tecla virtual: B1, B4, B9, B10, B11, N-7.
- Moderadas: C1, C2, C3, C4, C6, C7, N-2.
- De las 50: 1, 2, 8, 9, 13, 17, 23, 29, **30** (agregada por sugerencia mía, aceptada), 34.
- Difíciles: ninguna.

**No marcadas:** A5, A6, A7, A8, A9, A13, A14, A17b (mano en la oreja), A17d (jadeo), B2, B3, B5, B6, B7, B8, B12, C5, N-4, N-5, N-6, y de las 50 todas menos las de arriba. A15 ya la hizo otro hilo (v0.1.10).

**Tandas** (cada una es una versión que Franco prueba antes de seguir):

| Tanda | Nombre | Qué trae | Depende de |
|---|---|---|---|
| T0 | Diagnóstico | Tecla virtual de prueba, prueba de vibración, ¿llega la vibración del juego?, log de estados, A11, A15 (ya está), 34 | Nada |
| T1 | Cuerpo honesto | A1, A2, A3, A4, 29, A16 | T0 (vibración para A3) |
| T2 | Sentir al alien | A12 (con distancia de respaldo), 1, 2, 23, A10, A17a | T0 |
| T3 | Tu estado al tacto | 13, 17, N-3, N-1 (si T0 muestra que llega) | T0 |
| T4 | Gestos | B1, B4, B11, 30 | T0 (tecla virtual) |
| T5 | El locker | B10, C2, C6, 8, 9, A17c | T1, T4, el aire con OpenCAGE |
| T6 | Cámara | C1, N-2 | T0 (números de estado) |
| T7 | Ruido | Juntar el micrófono + C3 + C4 | Llamada de ruido probada |
| T8 | Manos en el mundo | B9, C7, N-7 | T0, T6 |

**Por qué este orden:**
- T0 responde si andan la vibración y la tecla virtual; de eso dependen T1 a T5.
- T1 es lo más barato y cambia cómo se siente esconderse.
- T2 y T3 juntan todo lo que vibra, con un **mezclador de vibraciones** (prioridad: daño > alien > estado) para que no se pise.
- T5 junta todo lo del locker en una sola prueba.
- T7 va al final porque toca a la IA y suma el micrófono.

**Mi opinión (ya se la di):**
- La selección es coherente: casi todo apunta a esconderse y al locker.
- 8 necesita que exista un latido (A12/A17a).
- 9 y A17c tocan el mismo volumen: un solo control que tome el más bajo de los dos.
- Lo más delicado:
  - B11, porque la cámara se puede mover dos veces;
  - C3, porque cambia el balance: va apagado por defecto y con multiplicador.
- N-1 solo si el juego la manda sin mando de Xbox; no engañarlo.
- A6 no hace falta porque A12 la incluye, pero la distancia sirve de respaldo.

Abajo, cada idea elegida con **cómo se juega / cómo funciona / ajustes / ojo**.

### T0 (ver sección 6)
- **A11. Log de "por qué me vio".**
  - Cómo se juega: no se ve en el juego. Después de jugar, `agachar.log` dice por ejemplo "14:32:05 alien: vista 0.42 → 0.78 (agachada, linterna prendida, a 6.1 m)".
  - Cómo funciona: enganche a **0x729120**. Si el objetivo `+0x42c` es Ripley, anota:
    - el tipo de sentido `+0x64` (0 vista, 1 ruido de arma, 2 ruido de movimiento, 3 daño, 4 tocado, 5 lanzallamas, 6 linterna, 7 combinado);
    - la activación `+0x5c`;
    - postura, linterna y distancia.

    Solo cuando cruza 0,25 / 0,5 / 0,75 / 1.
  - Ajustes: `[Log] Sentidos=1`.
  - Ojo: es una función muy llamada, el código tiene que ser mínimo.
- **A15.** Ya está en v0.1.10/0.1.11.
- **34. Ajustar en vivo.**
  - Cómo se juega: `Ctrl + flechas` sube y baja el umbral de agacharse mientras jugás, con un pulso (o un beep) que confirma.
  - Cómo funciona: guardarlo en el .ini, para que A15 no lo pise al recargar.

### T1 Cuerpo honesto
- **A1. Techo del ducto.**
  - Cómo se juega: en ductos y huecos bajos, aunque te pares en tu cuarto, la cámara no sube más que el techo.
  - Cómo funciona: en el envoltorio de pose, si `pj+0x2b4` está prendido, recortar la altura a "agachado + margen". Al salir, soltar en 0,3 s.
  - Ajustes: `[Techo] Activado=1, Margen=0.05`.
  - Ojo: confirmar con el log que `+0x2b4` se prende en ductos y no solo en lockers. Si no, buscar otra bandera en el log de estados.
- **A2. Correa invisible.**
  - Cómo se juega: la cabeza se mueve hasta ~25 cm alrededor del cuerpo de Ripley. Más allá, la imagen se frena en un resorte suave (no se atraviesan paredes, puertas del locker ni mesas).
  - Cómo funciona: limitar la distancia horizontal entre la pose de cabeza (+0x14) y la de cuerpo (+0x18). Los últimos 5 cm son resorte.
  - Ajustes: `[Correa] Activado=1, Radio=0.25, Resorte=0.05, RadioEscondida=0.10`.
  - Ojo: primero confirmar con log que las dos poses están en el mismo espacio (pendiente A9 de IDEAS_v2). Si te movés mucho por el cuarto, conviene recentrar (A4).
- **A3. Aviso de pared.**
  - Cómo se juega: cuando la correa frena, un toquecito en los dos mandos.
  - Cómo funciona: si recortó más de 2 cm en este cuadro y no en el anterior, pulso corto. Máximo uno cada 0,5 s.
  - Ajustes: `[Correa] Vibrar=1, FuerzaVibracion=0.3`.
  - Ojo: depende de la prueba de vibración.
- **A4. Recentrar al salir del escondite.**
  - Cómo se juega: al salir del locker o del ducto, en 0,5 s la imagen se acomoda sola.
  - Cómo funciona: cuando `+0x2b4` pasa de 1 a 0, guardar la diferencia cabeza-cuerpo y restarla de a poco.
  - Ajustes: `[Recentrar] AlSalir=1, Duracion=0.5`.
  - Ojo: si marea, probar 0,3 y 1,0.
- **29. Calibrar con gesto.**
  - Cómo se juega: parado, con los dos gatillos apretados 2 s, el mod toma tu altura parada.
  - Cómo funciona: leer el estado de los botones de los mandos por OpenVR (GetControllerState de IVRSystem). Ver si llega con Steam Link y GRAND. Alternativa: un gesto de pose, sin botones.
- **A16. Pausa al sacarte el visor.**
  - Cómo se juega: te lo sacás y el juego se pausa; te lo ponés y se recentra.
  - Cómo funciona: OpenVR da el nivel de actividad del visor (sensor de cara: GetTrackedDeviceActivityLevel o el evento de "usuario presente"). Si no detecta cara por 2 s, mandar Esc (tecla virtual o PostMessage a la ventana). Al ponérselo, recentrar (A4).
  - Ajustes: `[Pausa] Activado=1, Segundos=2`.
  - Ojo: que no se dispare en menús o cinemáticas. Ver que el sensor llegue por Steam Link.

### T2 Sentir al alien
- **A12. Latido según cuánto le falta para verte.**
  - Cómo se juega: un latido suave en el mando izquierdo según cuánto se está llenando el sentido de vista del alien hacia vos, no según la distancia. Detrás de una pared a 2 m va tranquilo; en un pasillo a 15 m mirándote, se dispara.
  - Cómo funciona: con el enganche de A11, guardar el objeto sentido de vista del alien hacia Ripley y leer `+0x5c` (0 a 1) cada cuadro. Ritmo = 50 + 90 × activación por minuto. Cada latido son dos pulsos (pum-pum).
  - Respaldo por distancia: lista de NPC, los de tipo 1, menos de 15 m.
  - Ajustes: `[Latido] Activado=1, Modo=activacion, Fuerza=0.3, Mando=izquierdo, DistanciaMax=15`.
  - Ojo: fuerza baja, es una sensación y no un radar. El objeto sentido puede morir: validar el puntero en cada cuadro o volver a tomarlo en el próximo llamado.
- **1. Pasos del alien.**
  - Cómo se juega: con el alien caminando a menos de 10 m, cada paso es un golpe sordo en los mandos, más fuerte cuanto más cerca.
  - Cómo funciona: posición del alien entre cuadros. Un "paso" cada ~1 m recorrido.
- **2. Alien por arriba.**
  - Cómo se juega: si el alien está en el ducto justo encima, un retumbo grave distinto.
  - Cómo funciona: diferencia horizontal chica y altura del alien 1,5-4 m por encima.
- **23. Ignorar al alien de otro piso.**
  - Cómo se juega: opción para que el latido ignore al alien si está a más de 4 m de altura de diferencia.
- **A10. Temblor al apuntar.**
  - Cómo se juega: con un arma levantada y el alien a menos de 8 m, el mando del arma tiembla apenas. No mueve la mira.
  - Cómo funciona: apuntando = acción 10 mantenida o, si GRAND no la usa, "objeto equipado es arma" (`[[pj+0x31c]+0x3c]+0x3bc8`, vtable +0x40).
  - Ajustes: `[Temblor] Activado=1, Distancia=8, Fuerza=0.15`.
- **A17a. Latido audible.**
  - Cómo se juega: además (o en vez) de vibrar, un latido grave muy bajito.
  - Cómo funciona: un .wav propio por Windows, que llega al visor por Steam Link igual que el juego.
  - Ajustes: `[Audio] Latido=1, VolumenLatido=0.2`.
  - Ojo: es el respaldo si la vibración no llega.

### T3 Tu estado al tacto
- **13. Linterna al tacto.** Un clic suave en el mando cuando cambia `pj+0x2ba`.
- **17. Disparo con peso.** Cada disparo vibra según el arma: revólver seco, escopeta fuerte, pernos largo. Disparo = bajan las balas cargadas (0x8177b0); el arma sale del objeto en la mano (vtable +0x20 = nombre).
- **N-3. Contar balas.** Al disparar la última bala, un clic distinto. Con pocas balas, un pulso de aviso. Usa 0x8177b0.
- **N-1. Vibración del juego en los mandos VR.**
  - Cómo se juega: los golpes, explosiones y agarres que hacían vibrar el joystick ahora vibran en tus mandos.
  - Cómo funciona: nuestro DLL **es** el XInput, así que si el juego llama XInputSetState, pasamos los motores a TriggerHapticPulse.
  - Ojo: puede que el juego no la mande sin mando conectado. **No fingir un mando.**

### T4 Gestos (tecla virtual)
- **B1. Tapar la linterna con la mano.**
  - Cómo se juega: la mano libre delante de la linterna la apaga; la sacás y se prende.
  - Cómo funciona: mano a menos de 10 cm del punto de la linterna (ver dónde la pone GRAND: hombro, cabeza o arma) durante 0,2 s → apretar linterna (acción 29) si `+0x2ba` = 1. Al sacarla, si estaba prendida antes, apretar otra vez.
  - Ajustes: `[Linterna] Tapar=1, Distancia=0.10`.
  - Ojo: el alien ve la linterna (SEE_FLASH_LIGHT), así que tiene efecto real. Que no se apague al mirar el tracker.
- **B4. Dos manos.**
  - Cómo se juega: mano izquierda a menos de 12 cm de la derecha → mantener apuntar (acción 10).
  - Ajustes: `[DosManos] Activado=1, Distancia=0.12`.
  - Ojo: ver qué hace GRAND con apuntar.
- **B11. Asomarse con el cuerpo.**
  - Cómo se juega: te inclinás de costado detrás de una esquina, o te levantás un poco agachada detrás de una caja, y Ripley se asoma de verdad (el alien te ve asomada).
  - Cómo funciona: cabeza corrida de costado (o hacia arriba agachada) respecto del cuerpo, por velocidad + recorrido mínimo → mantener asomarse (acción 18) y el stick hacia ese lado. En la pose, restar el desplazamiento que mete el juego para que no se sume dos veces.
  - Ajustes: `[Asomarse] Activado=1, Lateral=0.15, Arriba=0.12`.
  - Ojo: es la más delicada. Cómo se combina el stick: el stick es analógico y la tecla virtual es digital, así que quizás haga falta Q/E (ver qué teclas trae el juego para asomarse).
- **30. Mando sin tracking, sin gestos.** Si un mando pierde el seguimiento (`bPoseIsValid` falso o `eTrackingResult` distinto de Running_OK en la pose de OpenVR), sus gestos se apagan hasta que vuelva. Evita gestos fantasma.

### T5 El locker
- **B10. Salir empujando.** Escondida (`+0x2b4`) y la cabeza hacia adelante a más de 0,6 m/s → apretar la acción que saca del locker (usar o atrás, a confirmar). Por velocidad, para no salir al acercarte a la rendija.
- **C2. Contener la respiración y AireSegundos.**
  - Cómo se juega: en el locker, te inclinás hacia atrás y aguantás el aire de verdad. El juego usa los segundos que pusiste en `AireSegundos`.
  - Cómo funciona: el juego del aire es el script **Custom_Hiding_Controller** (métodos Get_In, Add_NPC, Start_Breathing_Game, End_Breathing_Game; el despachador en el exe es 0x9105f0). La duración está en los datos del nivel (COMMANDS.PAK), no en el exe.
  - Camino A: editar con OpenCAGE (sin parche).
  - Camino B: encontrar en memoria el parámetro del script y escribirlo desde el DLL leyendo `AireSegundos`.
  - El gesto aprieta el botón que usa el locker para contener (a confirmar).
  - Ojo: ver si la duración está por nivel (habría que editar todos) o en un composite compartido.
- **C6. Rendija del locker.** Dentro del locker, acercar la cara a la rendija deja ver más, pero la cabeza no atraviesa la puerta. Es una correa especial centrada en la cámara del locker: falta saber dónde la guarda el juego, o tomar la posición al entrar como centro.
- **8. Silencio en el locker.** Escondida y con el alien cerca, el latido se corta de golpe.
- **9. Encierro del locker.** Dentro del locker, el juego suena un 20 % más bajo. Usa el volumen de la sesión de audio del juego (ISimpleAudioVolume de WASAPI).
- **A17c. Taparse los oídos.** Las dos manos en las orejas bajan el juego al 30 %. Mano cerca del visor, a la altura de la oreja. Comparte el control de volumen con 9.

### T6 Cámara
- **C1. Cámara en muertes y animaciones.**
  - Cómo se juega: cuando el alien te mata desde el ducto (y en escaleras, palancas, ascensores), la cámara queda donde está Ripley y ves la escena. Era el **bug original** que Franco reportó: en la muerte por la ventilación, la cámara no queda alineada debajo del ducto.
  - Cómo funciona: durante ciertos estados (`[pj+0x388]+0x1e`), en la pose de la cabeza dejar solo la rotación y anular el desplazamiento del cuarto. Al terminar, volver suave.
  - Falta: mapear los números con el log de T0.
  - Ajustes: `[Animaciones] FijarPosicion=1, EstadosExtra=`.
- **N-2. Sacudidas suaves.** Todas las sacudidas pasan por el efecto CameraShake: se crean en 0x4293b0 (ejemplo en 0x665260, intensidad `+0x128`, posición `+0x108`), 0x419ea0 lee los parámetros y 0x430120 los aplica. Multiplicar por 0,3 o apagar. `[Sacudidas] Factor=0.3`.

### T7 Ruido
- Juntar el micrófono (v0.2.1) con la base nueva. **Primero probar la llamada de ruido en el juego** (nunca se probó).
- **C3. Cosas que hacen ruido:**
  - sobresalto (salto para atrás con el alien cerca);
  - chocar los mandos;
  - jadeo si te agitaste;
  - temblar en el locker con el alien a menos de 3 m;
  - golpecitos con la llave.

  Todas por 0x6e9f10 → 0x72f080 (sección 9). Apagado por defecto y con multiplicador.
- **C4. Tracker al pecho y bips que vibran:**
  - tracker pegado al pecho: el alien no oye sus bips (bajar el volumen `+0x30` del evento tipo 2);
  - cada bip vibra en el mando izquierdo.

  Falta dónde se crea el evento tipo 2; hoy solo sabemos dónde se recibe (0x730c30).

### T8 Manos en el mundo
- **B9. Manivela y soplete.** Cada media vuelta de la mano = un toque de "machacar" (acción 15). El soplete mantiene "usar" mientras la mano se mueve despacio. Saber cuándo hay una puerta para forzar: por el estado de Ripley (log de T0).
- **C7. Mapa en la muñeca.** Girar la muñeca izquierda hacia la cara como para ver la hora abre el mapa; bajarla lo cierra. Falta ver si el mapa es el menú de la HUD (acción 38) o tiene su acción, y si la tabla funciona con menús.
- **N-7. Minijuegos con las manos.** UI_ReactionGame, UI_Keypad, HackingGame y RewireSystem son entidades de script que leen la tabla de botones. Con la tecla virtual: girar la muñeca = izquierda/derecha, tocar = confirmar.

## 6. Diseño propuesto de la T0 (versión de diagnóstico)

Una sola versión, una sola sesión de prueba de Franco, que responda todo:
1. **Tecla virtual:**
   - envolver la función virtual +4 (EstaAbajo) del dispositivo teclado de la tabla de botones (sección 7.2);
   - tres teclas de prueba: linterna (un toque), correr (mantener), recargar (un toque).
   - **Ojo:** F11 choca con TeclaRecargar (ctrl+f11) y F12 con pánico. Usar por ejemplo ctrl+f5 / ctrl+f6 / ctrl+f7, configurables.
   - Log: si cambiaron `pj+0x2ba` y `pj+0x2b8`.
   - Hace falta antes saber qué scan code tiene cada acción por defecto y qué índice de tipo es el teclado (sección 10.1, punto 1).
2. **Vibración:** al agacharse (cambio de `+0x2af`), un pulso por TriggerHapticPulse de IVRSystem_017 en los dos mandos. Buscar el número de slot en `openvr.h` de la versión que corresponde.
   - Ojo: con SteamVR Input nuevo, la API vieja a veces no llega. Si no llega, probar IVRInput (TriggerHapticVibrationAction, que necesita un manifiesto de acciones) o resignarse a sonido.
3. **¿El juego manda vibración?** Anotar en el log cada llamada a nuestro XInputSetState (índice de mando y motores). Que Franco reciba un golpe o tire una IED.
4. **Log de estados:** cada cambio de:
   - `[pj+0x388]+0x1e` (número de estado);
   - `+0x2af`, `+0x2b4`, `+0x2b8`, `+0x2ba`;
   - `[pj+0x31c]+0x48`;
   - la vida.

   Con hora. Pedirle a Franco: escalera, locker (entrar, contener, salir), ducto, palanca, una muerte por el alien y un minijuego, **anotando la hora de cada cosa**.
5. **Tipos de NPC:** una vez por NPC nuevo, anotar `[npc+0x64]+4` y la posición, para confirmar que 1 = alien.
6. **Poses:** cada 2 s, cabeza (+0x14) y cuerpo (+0x18) de MotherVR, para ver si están en el mismo espacio (lo necesitan la correa y la rendija).
7. **A11:** log de sentidos (sección 5).
8. **34:** Ctrl + flechas.
9. Botones de los mandos por OpenVR (GetControllerState): anotar si llegan, para la 29.

Entregar con un LEEME que diga exactamente qué hacer en la prueba y qué mandar (agachar.log).

## 7. Todo lo que sabemos del juego (AI.exe)

AI.exe Steam: md5 `bb0d932f267c0ce1b1045e68a3d12ffb`, crc32 0x17c2184b, copia en `../binarios/AI.exe`. x86 de 32 bits, VS2010, LARGEADDRESSAWARE, con ASLR (base 0x400000; sumar el delta). Middleware: Wwise 2013, Scaleform, CRI. Scripts de nivel en CATHODE. **Todo estático salvo lo marcado como confirmado en el juego.**

### 7.1 Personaje del jugador (pj)
- **GetPlayer 0x78baf0:** `[[0x16f0c88]+8]+0x80` → `[0]` (si `+0x84` > 0).
- **Lista de NPC:** `[[0x16f0c88]+8]+0xd0` = array de punteros a personaje, `+0xd4` = cantidad (iterada en 0x6bcf1b y 0x734710).
- **Posición:** `pj+0x70..0x78` (vale para cualquier personaje).
- **Tipo de personaje:** `[x+0x64]+4`. Orden de CharacterClass: 0 PLAYER, 1 ALIEN, 2 ANDROID, 3 CIVILIAN, 4 SECURITY, 5 FACEHUGGER, 6 INNOCENT, 7 MELEE_HUMAN, 8 ANDROID_HEAVY.
  - 1 = alien confirmado dos veces en estático: 0x791030 crea `%s_BREATH` solo para tipo 1, y 0x7cb370 avisa al alien.
- **Otros campos de `[pj+0x64]`:** `+0x1c` = es el jugador; `+0x1f` = posible "jugador con control" (se usa para habilitar asomarse en 0x670c30, sin verificar).
- **Banderas:**
  - `+0x2af` agachado (**confirmado en el juego**). Lo lee la condición del alien IsCrouching 0x6eaf00.
  - `+0x2b0`, `+0x2b2`, `+0x2b4`: impiden agacharse. `+0x2b4` = escondida o forzada agachada; lo limpia 0x7cb370.
  - `+0x2b8` corriendo (lo escribe 0x679d60).
  - `+0x2ba` linterna prendida. El botón (0x5974d8) llama **0x8cd440** (prender) o **0x8cd410** (apagar) según este byte. Escribir el byte solo no prende la luz.
- **Postura** (struct en `pj+0x3e2c`):
  - `+0x3e2e` permite;
  - `+0x3e34` forzado;
  - `+0x3e3a` pedido de agachado (**confirmado**);
  - `+0x3e46` último aplicado.

  0x7d2ad0 (desde 0x7d3db0 para cada personaje) copia el pedido a `+0x2af` y avisa a la animación. Escriben `+0x3e3a` solo Crouch, Uncrouch y 0x7cb370. Llaman a Crouch: 0x687b60 (acción tipo 7: 1 = parar si !+0x2b4, 2 = agachar), 0x7cfd10 y la entidad CATHODE 0x4987c0.
- **Vida:** `+0x33c` actual (ushort), `+0x33e` máximo (ushort), `+0x340` multiplicador (float). Porcentaje = actual / (máximo × multiplicador), como hace la HUD en 0x836ab0.
- **Estado:** `[pj+0x388]+0x1e` (se vieron 6, 7, 0x19, 0x1a), sin nombres en el exe. Los textos ATOMIC_* son de la IA de los NPC.
- **Escondite activo:** `[pj+0x31c]+0x48`. Siempre 1 en juego normal según el log, así que no sirve como "escondida".
- **Control:** `[pj+0x31c]+0x44` = puntero al mapa de acciones de juego.
- **Gestor de equipo:** `[[pj+0x31c]+0x3c]`:
  - `+0x3bc0` personaje;
  - `+0x3bc8` objeto equipado (`+0x54` listo, `+0x6c` otro estado; vtable `+0x40` es arma, `+0x48`, `+0x50`);
  - `+0x3bec` / `+0x3bf0` / `+0x3bf4` cambio de objeto pendiente (0x773a90, 0x773bd0). La segunda tanda usó `+0x3bec` como "objeto en la mano" (vtable `+0x20` nombre, `+0x48` es arma): verificar cuál de los dos es en el juego;
  - `+0x3be8`, `+0x3be9` banderas del tracker (0x67c5a0, 0x773bb0).
- **Balas:** **0x8177b0** (cdecl) recibe el personaje y devuelve cargadas, reserva y nombres. Lo usa el contador de la HUD (0x8433e0). Se puede llamar desde el DLL (en el hilo del juego).
- **Recargar:** 0x677df0 lee la acción y llama 0x7d7860 y 0x7ce740.
- **Asomarse:** "Peek_System" es un mensaje que se manda al entrar a ciertos estados (0x670c30). No se encontró el lector directo del botón; con la tecla virtual no hace falta.
- **Lectura del mando:** 0xa0ee30, slot 8 de la vtable 0x13e7670. Llama a XInputGetState por el thunk 0x1008a7c (`+0x290`; si da 0x48f prueba los índices 0..3). Guarda:
  - botones en `+0x1c` (mantenidos), `+0x18` (recién apretados), `+0x20` (soltados);
  - sticks en `+0x268..0x274`;
  - gatillos en `+0x278/0x27c`.

  GRAND la engancha (0x10003710) y hace OR en `+0x1c` (por ejemplo 0x2000 para la IED).

### 7.2 Tabla de botones y tecla virtual
- **Tabla global:** `[0x1759b44]`. El mapa de **juego** está en `+0x48`; también hay en `+0x2998` y `+0x3a0c` (menús, supongo). Ripley lo lee por `[pj+0x31c]+0x44` (0x7b0b10, 0x7a37e0).
- **Cada acción** ocupa 0x40 bytes desde `mapa+0x34`:
  - `+0x34` puntero a combinaciones (8 bytes c/u: `+4` código de tecla, `+6` tipo de dispositivo, `+7` banderas);
  - `+0x38` cantidad;
  - `+0x3c` estado.

  En la práctica: **dirección del estado = `[0x1759b44] + 0x48 + 0x3c + idx*0x40`**. Ejemplo: agacharse 9 → `[0x1759b44]+0x2c4`, confirmado por el código y por MonitorActionMap.
- **Bits del estado:** 1 recién apretado, 2 mantenido, 4 soltado, 0x20 inhibido hasta soltar (InhibitActionsUntilRelease), 0x80 abajo ahora.
- **Índices** (del menú de controles, 0x803d70, tabla 0x174a4e8):

| Acción | idx | Lector |
|---|---|---|
| Correr | 8 | 0x679d60 (bit 2) |
| Agacharse | 9 | 0x677c00 (bit 1) |
| Apuntar | 10 | 0x678260 |
| Disparar / tirar | 11 | 0x678260 |
| Machacar | 15 | — |
| Recargar | 16 | 0x677df0 (bit 1) |
| **Usar (E)** | **12** (y 15, también E) | confirmado con usar.dll |
| No es usar: R, igual que recargar | 17 | 0x678200 |
| Asomarse | 18 | sin lector directo |
| Armas 1-5 | 19-23 | — |
| Tracker | 28 | 0x67a590, 0x67c5a0 (bit 2) |
| Linterna | 29 | 0x5974d8 (bit 1) |
| Objetos | 31-36 | — |
| Menú HUD | 38 | — |

- **Actualización por cuadro:** **0x986d80** (fastcall, ECX = mapa), llamada desde 0x9909fa bajo el candado 0x17582e4 para los tres mapas. Para saber si una tecla está abajo, le pregunta al dispositivo: `mapa+0x103c + tipo*4` → objeto dispositivo, **vtable +4 = EstaAbajo(código)**. Los códigos de teclado son scan codes de DirectInput (DIK; ver también 0xa0ed50).
- **Tecla virtual:**
  - envolver esa +4 del dispositivo teclado y devolver "abajo" para la tecla asignada a la acción;
  - el juego calcula solo apretado, mantenido, soltado e inhibido.
  - **CONFIRMADA en el juego** (29/10, agacharse y mapa). Módulo `tecla_virtual.c/h` en `../agachar/mapa_v0.6/fuentes/`. Teclas por acción (log de tecla_prueba): correr 8 = LShift, agacharse 9 = C, usar 12 = E, recargar 16 = R, asomarse 18 = LCtrl, tracker 28 = Space, linterna 29 = F, mapa 38 = Tab. Tipo 0 = mando de GRAND (códigos 0x01xx), tipo 1 = teclado DIK, tipo 2 = mouse.
  - **Ojo:** dos features no pueden enganchar la misma vtable. Una segunda feature que necesite apretar teclas usa SendInput (scancode DIK, solo con la ventana del juego al frente), como pausa y usar.
  - Correr no anda en la Torrens ni con el botón real (lo bloquea el nivel).
- **Tareas por cuadro del motor** (0x7d6370), para elegir dónde enganchar: START_NEXT_FRAME, PLAYER_CAMERA_UPDATE, CAMERA_UPDATE (0x7b4090), CONTROLLER_UPDATE (0x783430), AI_START_THINK, CPU_LIGHT_SAMPLER_UPDATE (0x7755c0), SOUND_UPDATE (0x783650), PLATFORM_UPDATE (0x7ca180).

### 7.3 Otros
- **CameraShake:** sección 5, N-2.
- **Respiración de Ripley:** cada personaje tiene en `+0x11a0` (constructor 0x783dc0) un bloque de parámetros de sonido con BREATH_RATE (`+0xf0`), VELOCITY y MOVEMENT_SPEED, que va a Wwise. No se sabe quién lo calcula. Hay entidades CHR_ModifyBreathing y CHR_HoldBreath.
- **Luz:** muestreador por personaje (tarea CPU_LIGHT_SAMPLER_UPDATE → 0x9c6b20 sobre 0x1765c90) y la curva AILightCurveSettings (datos). No se sabe dónde se lee el valor de Ripley.
- **Alien y escondite:** condiciones del árbol de conducta ATTACK_HIDING_PLAYER (0x8b8bb0) y COULD_RESPOND_TO_HIDING_PLAYER (0x8b9610). La conducta en sí está en árboles de datos (LegendPlugin), editables con OpenCAGE.
- **Androides y humanos:** ACT_THREAT_AWARE y otras. La reacción a armas o a manos arriba no está en el exe, está en datos.
- **Minijuegos:** UI_ReactionGame, UI_Keypad, HackingGame, RewireSystem (scripts que leen la tabla).
- **Vibración del juego:** la entidad PadRumbleImpulse.
- **Interacción ("usar"):** el objeto marcado antes de usar sigue **sin encontrarse** (no está entre los punteros de pj+0..0x4000 ni de [pj+0x64]+0..0x400). Lo que sí apareció: `pj+0x3570` o `+0x3574` pasa a un objeto 0.1-0.2 s **después** de usar y vuelve a 0 al terminar (interacción en curso). El buscador de usar.dll (Buscar=1) filtra punteros por vtable dentro de AI.exe (los floats 0x3f.../0xbf... pasaban por punteros). La HUD (0x83e320, campos `+0x350`, `+0x3f4`; 0x84bbe0) solo muestra u oculta la indicación. Bloquea: empujar puertas, botones con el dedo, agarrar del piso, entrar al locker, escaleras, guardar con la tarjeta.
- **Opción de consola "detección de ruido":** `set_noise_detection` (0x1300bf4, 0x806e00 → 0x805450) es para Kinect/PS Camera. En PC no consume micrófono: no sirve.

## 8. MotherVR, GRAND y OpenVR

- **MotherVR 0.8.1 (`dxgi.dll`, 32 bits, cerrado):**
  - Proxy de DXGI.
  - Tiene una tabla de direcciones de AI.exe en 0x10066bc8 (127 entradas, offsets desde 0x400000): ahí se ve qué engancha del juego.
  - Interfaz principal: vtable 0x1005a5c0 (+0x2c cabeza, +0x54 cuerpo).
  - Backend SteamVR: vtable 0x1005dda4 / RVA 0x5ddac (+0x14 cabeza, +0x18 cuerpo).
  - Recentrar: Ctrl + Alt en el teclado (LB + RB en el joystick).
  - Copia probable en `../uploads/hearth/96964a49…` (18 MB, PE32 DLL): verificar.
- **GRAND v0.6.3 (`XINPUT1_3.dll`, md5 original 9d9baa61d34b8e3233b777dfe031014f):**
  - `FUN_100022e0` traduce direcciones Steam → Epic/GOG.
  - Lista de hooks cerca de 0x1002a2xx.
  - Base de MotherVR en [0x100b6170] (engancha por ejemplo +0x24530).
  - Puntero OpenVR en [0x100b4874].
  - Chequea el CRC de AI.exe, dxgi.dll y MOTHER.PAK, no el suyo.
  - Engancha Crouch/Uncrouch (10 y 9 bytes) solo de paso.
  - Copia original en `../agachar/grand_original/`.
- **OpenVR:**
  - IVRSystem_017. Slot 11 = GetDeviceToAbsoluteTrackingPose, que da todos los dispositivos (0 = visor; los mandos se identifican con GetTrackedDeviceClass o GetTrackedDeviceIndexForControllerRole).
  - Para T0 hacen falta además: TriggerHapticPulse, GetControllerState, GetTrackedDeviceActivityLevel / el sensor de cara, y GetTrackedDeviceClass. **Sacar los números de slot del `openvr.h` de la versión que trae MotherVR**, no adivinarlos.

## 9. La IA, los sentidos y el ruido (de `../microfono/NOTAS.md`)

- **SENSORY_TYPE:** 0 VISUAL, 1 WEAPON_SOUND, 2 MOVEMENT_SOUND, 3 DAMAGE_CAUSED, 4 TOUCHED, 5 FLAME_THROWER, 6 SEE_FLASH_LIGHT, 7 COMBINED.
- **Multiplicadores y tasa:**
  - 0x6e16c0: multiplicador de dificultad por sentido (cargado en 0x6f8b00).
  - 0x6fb2e0: tasa de activación = lerp(min, max, t) × multiplicador.
- **0x729120:** suma activación al sentido. Es la entrada única que usan todos los sentidos.
  - Objeto sentido: `+0x5c` activación 0-1, `+0x64` tipo, `+0x34..0x40` hora de cada uno de los 4 niveles, `+0x54` cuándo se oyó, `+0x68` → gestor (NPC en `+8`), `+0x42c` objetivo.
  - Corta si el sentido está pausado (flags en `[npc+0x34d4]+0xb4..`, respeta NPC_SenseLimiter y cinemáticas).
- **Por sentido** (thiscall sobre el objeto sentido): 0x72e6d0 WEAPON_SOUND, 0x72ea40 MOVEMENT_SOUND, 0x72e1f0 visual, otros 0x72eb60/0x72ed20/0x72ee30/0x72e7f0.
- **Llamada de ruido directa (nunca probada):**
  ```
  jugador = GetPlayer (0x78baf0); pos = cabeza o [jugador+0x70..0x78] + altura
  para cada npc en la lista (+0xd0/+0xd4), salvo el jugador:
      gestor = *(npc+0x50); si 0 o *(gestor+0x8c) != npc: log y seguir
      fuerza = 0x6e9f10 [ECX = npc+0x33e0](jugador, &pos, volumen, por_camino, 1.0)
               (dentro 0x6d7cd0: 1 - d/maxOído - (1 - volumen))
      si fuerza > 0: 0x72f080 [ECX = gestor](jugador, &pos, 0, -1, (float)*(double*)0x16410a8, fuerza)
  ```
  - 0x72f120 es el equivalente para WEAPON_SOUND.
  - Falta copiar los chequeos previos de 0x730c30 (0x77a800 sobre npc+0x280, bandos).
- **Eventos de sonido para la IA:**
  - 0x730c30 = slot 5 del "oyente" (vtable 0x12edf24, constructor 0x6fde90, creado en 0x733850; objeto IA del NPC en `npc+0x33e0`).
  - Evento: `+0xc` tipo (0 movimiento, 1 arma, **2 tracker**, 6 otro del jugador), `+0x18..+0x20` posición, `+0x24` float, `+0x28` handle de la fuente, `+0x30` volumen, `+0x34..` flags.
- **Mic:** "Steam Streaming Microphone" (viene al 50 %). La detección de voz usa tono de 70-400 Hz sobre 40 ms.

## 10. Qué falta averiguar

### 10.1 Con Ghidra (tercera tanda, la que estaba por arrancar)
1. **Teclas por defecto de cada acción** e índice de tipo del teclado.
   - Pistas: las combinaciones en `mapa+0x34` (código `+4`, tipo `+6`); el menú de controles 0x803d70 / tabla 0x174a4e8; 0xa0ed50 (traducción de scan codes).
   - Puede venir de un archivo de config de datos (buscar strings como "Keyboard", "KEY_", "DIK_", nombres de acciones).
   - Si no sale en estático, la T0 puede **volcar al log** las combinaciones de cada acción leyendo la tabla en el juego. Es la opción más segura.
2. **Lista de personajes:** ya está (sección 7.1). Falta la **velocidad** (diferencia de posición entre cuadros alcanza) y confirmar el tipo con el log.
3. **¿Vibra sin mando de Xbox?** Mirar quién llama a XInputSetState (importado por AI.exe; buscar xrefs al thunk) y si antes chequea "mando conectado" (el resultado 1167 de 0xa0ee30). También qué hace PadRumbleImpulse.
4. **Cámara de muertes y del locker:** PLAYER_CAMERA_UPDATE / CAMERA_UPDATE (0x7b4090), y cómo MotherVR mete la pose (su tabla de 127 direcciones en 0x10066bc8). Para C6: dónde se pone la cámara al entrar al locker (Custom_Hiding_Controller, Get_In).
5. **Ruido y tracker:** quién crea el evento tipo 2 (buscar escrituras de 2 en `+0xc` cerca de los usos de 0x730c30, o strings del tracker: "MotionTracker", "PING", "BLIP").
6. **Botones de manivela, soplete, minijuegos y mapa:** buscar los nombres de las acciones en la tabla del menú (0x174a4e8) y en las entidades UI_* (qué índice leen).

Resultado → `ANALISIS_GHIDRA_3.md` + actualizar este handoff.

### 10.2 Con archivos del juego
- **Duración del aire (C2):** le pedí a Franco `DATA\ENV\PRODUCTION\<nivel>\WORLD\COMMANDS.PAK` de un nivel con lockers (por ejemplo una carpeta `SCI_HOSPITAL…`).
  - Formato: CathodeLib de Matt Filer (C#, en GitHub, con OpenCAGE). Buscar el composite Custom_Hiding_Controller y sus parámetros (tiempos o floats relacionados con breath / hold).
  - Si no se puede parsear, explicarle a Franco cómo verlo con el Script Editor de OpenCAGE.

### 10.3 Solo jugando (T0)
- Si la tecla virtual anda.
- Si la vibración llega.
- Si el juego manda vibración.
- Los números de estado.
- Que 1 = alien.
- Si cabeza y cuerpo están en el mismo espacio.
- Si `+0x2b4` se prende en ductos.
- Si llegan los botones de los mandos por OpenVR.
- Si el sensor de cara llega por Steam Link.

### 10.4 Pendiente de las versiones de otro hilo
- Que Franco pruebe v0.1.10 / v0.1.11 (recarga del .ini, pánico, lógica recargable).

## 11. Herramientas

**El contenedor se borra: hay que rearmar todo.**
- **Ghidra 11.4.2** (Java 21). Proyecto de análisis:
  ```
  analyzeHeadless <proj_dir> AI -import /mnt/project-files/binarios/AI.exe
  ```
  - Análisis completo: tarda bastante. Correrlo en segundo plano y esperar "Save succeeded". Los errores de RTTI no importan.
  - Antes, un monitor que cortaba en "REPORT: Processing" se confundía: esperar "Save succeeded".
- **Scripts:** en `ghidra_scripts/`, con `run.sh` (ajustar las rutas). Uso: `run.sh <Script> <salida> args`. Corre con `-process AI.exe -noanalysis -postScript`.
  - `Dec.java <salida> <dir...>`: decompila las funciones que contienen esas direcciones.
  - `Sym.java <salida> <patrón...>`: símbolos y strings que contienen el patrón, con xrefs. `=texto` es igualdad exacta. Usa getSymbolIterator con patrón, porque getAllSymbols revienta con StackOverflow.
  - `Xr.java`: xrefs a una dirección.
  - `Vt.java`: vtables.
  - Python: `pe.py` (secciones, imports), `vt.py`, `scan.py`. Antes había un `text.lst` de 125 MB con el desensamblado lineal de .text (se regenera con capstone si hace falta).
- **MotherVR en Ghidra:** antes estaba en /tmp/claude-0/gmvr, decompilado entero en /tmp/claude-0/mvr/all.c. Ya no existe: reimportar el dxgi.dll.
- **Compilar el mod** (Linux, `gcc-mingw-w64-i686`), desde `fuentes/src` de v0.1.11:
  ```
  i686-w64-mingw32-gcc -O2 -Wall -Wno-cast-function-type -shared -static -static-libgcc -o ../xinput_ag.dll cargador.c diferidos.c tecla.c xinput_ag.def -Wl,--enable-stdcall-fixup -s
  i686-w64-mingw32-gcc -O2 -Wall -Wno-cast-function-type -shared -static -static-libgcc -o ../agachar_logica.dll logica_juego.c logica.c tecla.c diferidos.c -s
  ```
- **Pruebas** (en `fuentes/pruebas/`):
  - `prueba_logica.c` (agacharse);
  - `emular_grand.py` (parche de GRAND, unicorn + pefile + capstone);
  - `prueba_diferidos.c` (OpenVR dentro de MotherVR);
  - `prueba_importacion.c` (PeekMessageA en AI.exe);
  - `prueba_tecla.c`;
  - `emular_pose_mothervr.py`;
  - `prueba_recarga.c` (Wine 9 + xvfb, `WINEDLLOVERRIDES=dxgi=n`).

  Para las ideas nuevas: probar la lógica en C en Linux y los enganches con unicorn sobre el código real.

## 12. Ideas que no se eligieron (para más adelante)

- **De DETALLE (A/B/C):**
  - A5 vibración al agacharte;
  - A6 latido por distancia (queda como respaldo de A12);
  - A7 latido del lado del alien (casi un radar);
  - A8 vibración por daño y latido con poca vida;
  - A9 tus pasos en el mando;
  - A13 altura que se aprende sola;
  - A14 modo sentado;
  - A17b mano en la oreja (escuchás 50 % más);
  - A17d jadeo propio;
  - B2 trotar para correr;
  - B3 enfocar el tracker acercándolo a la cara;
  - B5 recarga con gesto (cadera y después el arma; con N-3 no hace nada si está lleno);
  - B6 fundas en el cuerpo;
  - B7 tirar con fuerza;
  - B8 botiquín al cuello;
  - B12 melee por gesto;
  - C5 balance con el cuerpo (inmovilidad, cuclillas profundas, mirada fija; máximo 15-20 % en 0x729120);
  - N-4 respiración de Ripley según tu cuerpo (BREATH_RATE);
  - N-5 aviso cuando el alien revisa tu escondite (0x8b8bb0/0x8b9610);
  - N-6 "estás iluminada".
- **De las 50 (IDEAS_v3):**
  - 3-5 androides, humanos y facehugger cerca;
  - 6 patrón por sentido;
  - 7 modo entrenamiento;
  - 10 te encontró;
  - 11 volver a respirar;
  - 12 el alien se fue a los ductos;
  - 14 correr al tacto;
  - 15 entrar al escondite;
  - 16 qué tengo en la mano (pulsos);
  - 18 recogiste algo;
  - 19 curada;
  - 20 cansancio de correr;
  - 21 vida baja, sonido apagado;
  - 22 linterna olvidada;
  - 24 últimas balas;
  - 25 techo de altura;
  - 26 tu altura es la de Ripley;
  - 27 detecta que te sentaste;
  - 28 recentrar con gesto;
  - 31 modo zurdo;
  - 32 batería baja;
  - 33 perfiles;
  - 35 pausa con gesto;
  - 36 pausa al perder tracking;
  - 37-44 botones con el cuerpo (correr inclinándote, tracker levantando el brazo, mochila en el hombro, linterna en la sien, siguiente arma con la muñeca, recarga solo si hace falta, apagar la linterna al agacharte de golpe, usar con la palma);
  - 45-50 recuerdos y desafíos (foto del susto, foto de la muerte, marcas para el video, racha sin ser vista, desafíos, registro de cuerpo).
- **Difíciles (siguen bloqueadas):**
  - Por el objeto de "usar": D2 empujar para abrir, D3 botones con el dedo, X3 agarrar del piso, X4 entrar al locker, N11 escaleras, 15 guardar con la tarjeta.
  - D1 el alien ve tu cabeza (toca la visión; la correa y asomarse cubren casi todo).
  - D4 oreja contra la puerta (Wwise; A17b lo reemplaza).
  - D6 esquivar al androide.
  - X1 manos arriba, X2 apuntar a humanos, X5 brillo del tracker, X6 lado del golpe, X7 empujar androides.
  - N12 IED al piso, N13 pilas de la linterna.
- **Descartadas por Franco:** detectar la respiración con el micrófono (cualquier variante).
- **Hand tracking del Quest 3 (a futuro):**
  - Steam Link o Virtual Desktop pueden pasar las manos como mandos virtuales (pellizco = gatillo). Esto es de memoria, **verificar**.
  - Nuestros gestos usan posición, así que andarían casi igual.
  - Faltaría caminar: pellizco y arrastre convertido en W/A/S/D con la tecla virtual, o trotar en el lugar. Y girar con el cuerpo o con pellizco.
  - Se pierde la vibración (pasarla a sonido), andan peor las manos fuera de la vista y la puntería.
  - Primer paso: probar si MotherVR responde a las manos sin mod. Si sí, una opción `ModoManos=1`.

## 13. Mapa de archivos

`/mnt/project-files/`:
- `ideas_gameplay/`:
  - **HANDOFF.md** (este);
  - `PLAN_TANDAS.md` (tandas, opinión, qué falta);
  - `DETALLE.md` (A, B, C detalladas);
  - `IDEAS_v3.md` (las 50);
  - `IDEAS.md`, `IDEAS_v2.md` (lluvias de ideas viejas, con la discusión del aire en M3);
  - `PRIORIDADES.md`;
  - `ANALISIS_GHIDRA.md`, `ANALISIS_GHIDRA_2.md`;
  - `HAND_TRACKING.md`;
  - `ghidra_scripts/`.
- `agachar/`: NOTAS.md (historia completa, hasta usar v0.1.5 y manos v0.8), los zips de cada versión, `grand_original/`, y una carpeta por feature y versión (`manos_v0.x/`, `mapa_v0.x/`, `pausa_v0.1/`, `usar_v0.1.x/`, `tacto_calibrar_v0.x/`, `tecla_prueba_v0.1/`), cada una con `mods_vr/` (DLL e .ini) y `fuentes/`.
- `microfono/`: NOTAS.md, microfono_v0.2.zip, microfono_v0.2.1.zip.
- `ideas_jayp/EVALUACION.md`: la lista de JayP (micrófono, fundas, melee, cambiar de mano, recarga manual).
- `ANALISIS_MANOS.md`, `MANOS_PAREDES.md`: manos contra paredes (29/10).
- `handoff_mothervr.md`: el handoff original del 25/10 (bugs de MotherVR, OpenIsolation, OpenCAGE, flujo de trabajo de Door Kickers).
- `binarios/AI.exe`.
- `uploads/hearth/`: lo que subió Franco, con nombre de id:
  - logs de agachar v0.1.1 a v0.1.8;
  - sus agachar.ini;
  - el ai-grand.log (`be5274a1…`);
  - capturas PNG;
  - AI.exe (`8e12ac44…`);
  - un DLL de 18 MB (`96964a49…`, probablemente dxgi.dll de MotherVR);
  - dos DLL de 1,4 MB (`c7f2ea38…`, `ccd1eeab…`, probablemente XINPUT1_3 de GRAND);
  - dos zips (`41e0d54c…` de 6 MB, `f19f2fd8…`).

  Mirar con `file` antes de usar.

## 14. Tercera tanda de Ghidra (29/10, hilo "Elegir features para el resto del juego")
Ver `ANALISIS_GHIDRA_3.md`. Resumen: el gestor de música `[0x16b1758]` ya calcula alien elegido (+0x188), distancia por camino (+0x18c), estado (+0x190: 1 presente, 2 fuera/ductos?), sigilo (+0x1d4) y peligro (+0x1d8) cada cuadro; se lee sin enganchar. Sentidos: `[npc+0x34d4]` → lista en +0xc de sensores por objetivo (+0x42c); activación del tipo t en `sensor+0x74+t*0x74`; umbrales en `[npc+0x1154]` (0x6cbfe0), modo SENSE_SET `npc+0x4274`. NPC activo: 0x78b330 == 0. Falta: log de diagnóstico con el alien.
