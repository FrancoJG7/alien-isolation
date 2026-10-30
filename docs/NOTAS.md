# Agacharse en la vida real: notas técnicas (v0.1, 26/10/2026)

Para AI.exe Steam (crc32 0x17c2184b, md5 bb0d932f267c0ce1b1045e68a3d12ffb, copia en `binarios/AI.exe`), MotherVR 0.8.1, GRAND test (XINPUT1_3.dll md5 9d9baa61d34b8e3233b777dfe031014f). Direcciones sin rebasar (AI.exe tiene ASLR; base 0x400000).

## AI.exe
- `0x78baf0` GetPlayer: `[[0x16f0c88]+8]+0x80` → `[0]` (si `+0x84` > 0). Los NPC están en `+0xd0`/`+0xd4`.
- `0x7cae20` Character::Crouch (thiscall), `0x7cae90` Character::Uncrouch. GRAND los engancha (10 y 9 bytes) pero solo pasan de largo.
- `0x4987c0` entidad CATHODE que fuerza agachado: GetPlayer → Crouch/Uncrouch y escribe `+0x2af`.
- Character `+0x2af` = agachado. Lo lee la condición del alien IsCrouching (`0x6eaf00`). `+0x2b4` = forzado agachado (escondido); `+0x31c`→`+0x48` = escondite activo; `+0x64`→`+0x1f` parece "jugador con control" (sin verificar).
- `0xa0ee30` lectura del mando (XInputGetState vía thunk `0x1008a7c`): botones en `+0x1c` (mantenidos), `+0x18` (recién apretados), `+0x20` (soltados); sticks `+0x268..0x274`, gatillos `+0x278/0x27c`. GRAND la engancha y hace OR en `+0x1c` (así dispara el IED con 0x2000).
- `0x5a8790` reenvía acciones de input a CATHODE; acción "crouch" en `[0x1759b44]+0x2c4` (bit 1 = mantenida).

## GRAND
- `FUN_100022e0` traduce direcciones Steam → Epic/GOG. Lista de hooks en `FUN_1002b...` (~0x1002a2xx). HMD vía `GetModuleHandle("openvr_api.dll")` + `VR_GetGenericInterface`.
- Carga el XInput real en DllMain (`0x100367b0`): GetSystemDirectoryA + "\XINPUT1_3.dll". El parche v0.1 lo cambia a `xinput_ag.dll` (call en 0x100367e8 → `add esp,8; xor eax,eax; nop`, string en 0x100a1fa4, se anula la relocación 0x100367ea).

## Prototipo
`agachar/agachar_v0.1.zip`: xinput_ag.dll (C, mingw) reenvía XInput y en cada lectura del mando mide la altura del visor (IVRSystem_017, universo crudo) y llama Crouch/Uncrouch en el cambio de postura. Sin probar en el juego.

## v0.1.1 (26/10, después de la primera prueba)
- Franco probó la v0.2.1 (agachar + micrófono): en el menú el stick izquierdo no navegaba, solo andaba "aceptar". Pidió dejar el micrófono para después.
- `agachar/agachar_v0.1.1.zip`: mismo código de agacharse, compilado con `-DSIN_MICROFONO` (sin nada del micrófono).
- Hipótesis sin confirmar: GRAND original carga el XInput de Windows en su DllMain (al arrancar); con el parche se cargaba recién en la primera lectura del mando. Si Steam Input engancha el XInput al arrancar, el mando emulado no llegaba. Ahora xinput_ag.dll carga el XInput real en su DLL_PROCESS_ATTACH.
- agachar.log anota "mando N: conectado / no conectado (codigo)" cada vez que cambia, para diagnosticar.
- El juego (0xa0ee30) llama XInputGetState(+0x290) y si da 0x48f prueba los índices 0..3. El hook de GRAND sobre 0xa0ee30 (0x10003710) llama al original y solo agrega botones/sticks de gestos cuando hay una bandera puesta.

## v0.1.2 (26/10)
- agachar.log de Franco con v0.1.1: solo las 3 líneas de inicio; nunca "OpenVR listo" → `GetModuleHandle("openvr_api.dll")` daba NULL todo el tiempo.
- MotherVR 0.8.1 (dxgi.dll) importa openvr_api.dll en diferido (IAT diferida en RVA 0x6769c: VR_GetGenericInterface, VR_InitInternal2, ..., VR_GetInitToken) y trae un openvr_api embebido en .rsrc. GRAND no usa GetModuleHandle salvo como respaldo: engancha funciones de MotherVR (base en [0x100b6170], p. ej. +0x24530) y guarda el puntero en [0x100b4874].
- v0.1.2 busca VR_GetGenericInterface: 1) openvr_api.dll cargado, 2) tabla de importación diferida de cualquier módulo cargado (solo si ya apunta afuera del módulo), 3) vrclient.dll → VRClientCoreFactory("IVRClientCore_003") → slot 3 GetGenericInterface. Siempre IVRSystem_017 (el mismo que usa MotherVR), slot 11 GetDeviceToAbsoluteTrackingPose.
- `src/diferidos.c` sin windows.h; `pruebas/prueba_diferidos.c` lo prueba con el dxgi.dll real (6/6 OK). Sin probar en el juego.

## v0.1.3 (26/10)
- Log de v0.1.2: de nuevo solo 3 líneas, ni siquiera "todavia no encuentro OpenVR" → paso() casi nunca corría. Sin mando de Xbox (código 1167), XInputGetState se llama poco: no sirve de reloj.
- Bucle principal: `0x7a8460` (máquina de estados de carga/juego; estado 5 = vtable+0x14 = cuadro del juego) llama en cada vuelta a `0xa12fb0` (bomba de mensajes: PeekMessageA vía `mov ebx,[0x119f388]` en 0xa13070, `call ebx` en 0xa13094, vuelta 0xa13096). Mismo hilo que el cuadro del juego.
- v0.1.3 reemplaza la entrada de PeekMessageA en la IAT de AI.exe (buscada con `importacion_rva`, probada con AI.exe) y corre paso() solo cuando la dirección de vuelta es 0xa13096+delta. Firma sin relocaciones verificada en 0xa13082 (39 bytes). Se instala en DllMain y se reintenta en iniciar() si la IAT todavía no estaba resuelta (se valida que apunte a otra DLL cargada). Si no se puede, vuelve a usar XInputGetState.
- Logs nuevos: "enganchado al bucle del juego", "primer paso desde ... (hilo N)". Sin probar en el juego.

## v0.1.4 (26/10)
- Log de v0.1.3: enganche al bucle OK (hilo 28628), OpenVR OK (por openvr_api.dll), cabeza 1.62 m parado (altura de ojos; Franco mide 1.70), detecta "abajo" a ~1.0 m. Pero "pendiente descartado (escondido)": el chequeo `+0x31c→+0x48` estaba mal entendido.
- `0x7cae20` Crouch / `0x7cae90` Uncrouch: no hacen nada si `[pj+0x31c]+0x48` != 0, que en juego normal está prendido. Son para scripts/NPC.
- El jugador se agacha por el script CATHODE al recibir el evento del botón: `0x5a8790` (vtable `0x12591f8`, thiscall, `ret 8`) compara el bit 1 de `[0x1759b44]+0x2c4` (acción agacharse, acciones cada 0x40) con su copia en `this+0xc` bit 1 y manda `0x8cff50` (apretado) / `0x8cff80` (soltado). `0x7d2ad0` copia el estado a `pj+0x2af`.
- v0.1.4 cambia esa entrada de la vtable: mientras `g_boton`, prende el bit 1 justo antes de llamar al original y lo restaura después. Modo alternar (toques de 0,15 s) por defecto; si un toque no deja `+0x2af` como se pide, pasa a mantener; y al revés. Hasta 3 intentos por cambio de postura, con 0,5 s entre intentos. Firmas verificadas en 0x5a8790, +0x13, +0x1ce. Sin probar en el juego.

## v0.1.5 (26/10)
- Log v0.1.4: el enganche al botón funciona (un toque para pararse la paró en 0,18 s). Para agacharse, +0x2af no cambió dentro de 0,5 s y el mod pasó a "mantener", generando más apretadas que cancelaban el agachado (Franco: "la cámara baja un poquito pero sigue parada"). Soltar el botón no la paró y un toque sí → el juego (con MotherVR) usa alternar.
- v0.1.5: solo toques (0,15 s), espera 1,5 s después de cada toque antes de decidir nada, máximo 2 toques por cambio. ModoBoton=1 en el ini fuerza mantener. Log "(el juego la puso X, N s despues del ultimo toque)" para medir cuánto tarda.

## v0.1.6 (26/10)
- Log v0.1.5: los toques no cambian +0x2af nunca. `0x5a8790` es la entidad CATHODE **MonitorActionMap** (eventos on_pressed_crouch / on_released_crouch, strings en 0x13dc0b8), no el control del jugador: el toque solo dispara algo de script (Franco: "mira un poco para abajo"). Los "agachada → parada en 12 ms" a ~18 s y ~48 s de arrancar son el arreglo automático de cámara de GRAND (0x10030702/0x10030826: llama Uncrouch/Crouch y escribe +0x2af).
- Estado de agachado del personaje: struct en pj+0x3e2c. `Crouch` pone pedido `+0x3e3a` = (`+0x3e2e` ? 1 : 0), con `+0x3e34` como forzado; `0x7d2ad0` (llamada desde 0x7d3db0 para cada personaje) copia el pedido a `+0x2af` cuando cambia respecto de `+0x3e46` y manda CROUCHED a la animación. El chequeo `+0x31c→+0x48` en Crouch solo saltea escribir +0x2af directo.
- Escriben +0x3e3a solo Crouch, Uncrouch y 0x7cb370. Llaman Crouch: 0x687b60 (acción tipo 7: 1 = parar si !+0x2b4, 2 = agachar) y 0x7cfd10.
- v0.1.6, Metodo=0 (por defecto): llama Crouch/Uncrouch; si después de Crouch el pedido quedó en 0, lo pone en 1. Anota en el log cada cambio de agachado/pedido/permite/fuerza/+0x2b0/+0x2b4/31c.48 (hasta 400 líneas). Metodo=1/2 deja el botón (MonitorActionMap) para pruebas. Sin probar en el juego.

## Resultado v0.1.6 (26/10)
- **Funciona**: Franco confirma que Ripley se agacha y se para con el cuerpo. Log: "llamo Crouch" → en 11 ms `agachado 1, pedido 1`; Uncrouch igual. `permite` (+0x3e2e) = 1 y `31c.48` = 1 siempre en juego normal; no hizo falta forzar el pedido.
- El botón real hace exactamente lo mismo (pedido 0→1, agachado 1 en 12 ms).
- Quejas: demora (detección: 0,35 m + 150 ms) y "mirar un poco abajo" mientras baja y estando agachado, que con el botón no pasa. La inclinación empieza antes de que el mod actúe → sospecha: MotherVR/GRAND con la cabeza bajando. Pedido a Franco: probar con Activado=0, y bajar a BajadaAgachar=0.25, BajadaParar=0.12, DemoraMs=50.

## Prueba Activado=0 (v0.1.6)
- Franco: con Activado=0 igual mira para abajo al agacharse con el cuerpo. Log: solo carga, engancha PeekMessageA y anota "mando 0: no conectado". No lee el visor ni llama Crouch.
- Con Activado=0 el DLL solo reenvia las 7 funciones de XInput que GRAND pide por nombre (todas exportadas) y pasa PeekMessageA sin tocar nada.
- GRAND tiene su propio grand.ini (CameraOffset, HideBody, HeightOffset, OGMotherVRMode...) y su log ai-grand.log. No escribe el ini.
- Siguiente: volver atras del todo (XINPUT1_3.dll original, sin xinput_ag.dll) y ver si sigue. Pedidos grand.ini y ai-grand.log.
- Franco: con el XINPUT1_3.dll original (sin mi DLL) NO mira para abajo. Con mi DLL y Activado=0 si.
- Revisado: nadie mas (MotherVR, GRAND) toca PeekMessageA; el parche de GRAND solo cambia el nombre del DLL que carga; GRAND no se chequea a si mismo (CRC solo de AI.exe, dxgi.dll, MOTHER.PAK).
- Ojo: v0.1.1 a v0.1.3 ya cargaban igual y no reporto mirar abajo; aparecio en la v0.1.4 (la que tocaba el boton).
- GRAND solo escribe ai-grand.log si el juego arranca con "-log" (opciones de lanzamiento de Steam). Pedido A/B con -log: original vs mio con Activado=0.
- 18:09 Franco: con el XINPUT1_3.dll que mando (md5 9d9baa61 = GRAND v0.6.3 original SIN parchear, no carga xinput_ag.dll) TAMBIEN mira abajo. ai-grand.log: CameraOffset=0.1, HideBody=0, HeightOffset=0, RecenterOnFirstLevelLoad=1, OGMotherVRMode=0.
- Conclusion: no es mi DLL. Viene de GRAND/MotherVR/juego. Proxima prueba: [Debug] OGMotherVRMode=1 en grand.ini (y/o HideBody=1) para ver si es GRAND.
- 18:20 Franco: con OGMotherVRMode=1 (GRAND casi apagado, entro con teclado) SIGUE mirando abajo, proporcional a cuanto baja. => viene de MotherVR (o del juego con MotherVR), no de GRAND ni de mi DLL.
- MotherVR (dxgi.dll) analizado en Ghidra (/tmp/claude-0/gmvr, decompilado entero en /tmp/claude-0/mvr/all.c). Tabla de direcciones de AI.exe en 0x10066bc8 (127 entradas, offsets desde 0x400000). Vtable del backend SteamVR 0x1005dda4: +0x14 pose de cabeza (FUN_100070e0), +0x18 pose de "cuerpo" solo yaw + posicion (FUN_10007220). Interfaz principal vtable 0x1005a5c0 (+0x2c cabeza, +0x54 cuerpo). Todavia no encontre donde se convierte la altura en giro.
- Ojo: en OGMotherVRMode=1 GRAND igual inyecta ("OGMotherVRMode: Injection finished"), asi que esa prueba no descarta GRAND del todo. Prueba limpia: sin XINPUT1_3.dll.
- 18:28 Franco: sin XINPUT1_3.dll (MotherVR solo) TAMBIEN mira abajo => es MotherVR.

## v0.1.7: altura fija para MotherVR
- MotherVR::OpenVR vtable (RVA 0x5ddac): +0x14 pose cabeza (0x100070e0), +0x18 pose cuerpo (0x10007220). Ambas: GetLastPoses -> this+0x10, salida 3x4 = pose * diag(-1,-1,1,-escala). Emulado con unicorn (pruebas/emular_pose_mothervr.py): la altura solo aparece en out[7] = -escala*y.
- El DLL envuelve esas dos entradas de vtable (no la funcion, que GRAND detourea por dentro) y hace out[7] -= escala*bajada, con bajada = referencia parado (logica propia en el espacio de MotherVR) - y. GRAND sigue viendo la altura real en this+0x10.
- Firma: bytes 8d73106a006a006a10568bc8ff520c en RVA 0x7149 + vtable apuntando a 0x70e0/0x7220. Corre aunque Activado=0. Ini: FijarAltura (default 1).
- Defaults nuevos: 0.25 / 0.12 / 50 ms. Sin probar en el juego.
- 18:41 Franco: v0.1.7 FUNCIONA (ya no mira abajo). Queda ~0.5 s de demora al agacharse (su ini: 0.25 / 0.12 / DemoraMs 5).

## v0.1.8: agachar por velocidad
- logica: velocidad vertical suavizada (~30 ms). Si baja a mas de Velocidad (0.6 m/s) y ya bajo BajadaRapida (0.12 m), AGACHAR sin demora. Si esta abajo y sube a mas de 0.6 m/s con bajada < BajadaAgachar, PARAR.
- Pruebas: 1 m/s agacha a 0.11 s; cabeceo de 10 cm a 0.8 m/s y bajada lenta de 22 cm no agachan. Sin probar en el juego.
- 18:48 Franco (v0.1.8): bajada "perfecta"; subida tosca. Log: "arriba" recien a 1.37-1.40 m (bajada < BajadaAgachar). Una vez Uncrouch (15:46:25) no cambio nada (el juego lo deshizo en el mismo cuadro) y otra el juego la volvio a agachar solo (15:46:39, pedido 1 sin llamada nuestra) 1.7 s despues de un Uncrouch.

## v0.1.9
- Subida rapida: cuenta desde el minimo alcanzado agachado (SubidaRapida 0.15). Bloqueo de cambio por umbral si te moves en sentido contrario (>0.1 m/s), para no re-agachar mientras subis.
- directo_paso vigila 2.5 s tras cada cambio y reintenta (0.2 s entre intentos, max 5) si el juego deshace. Log "reintento".
- Pruebas: desde 0.95 m a 1 m/s para a 0.14 s; acomodarse 10 cm no para. Sin probar en el juego.
- 18:53 Franco: v0.1.9 "funciona absolutamente perfecto". Version estable de agacharse.
- Pendiente para despues (Franco): microfono/alien. Al retomarlo, partir de estas fuentes (v0.1.9) y compilar con el microfono; el zip microfono_v0.2.1 NO tiene estos arreglos.

## v0.1.10 (idea A15 de ideas_gameplay/DETALLE.md)
- Recarga de agachar.ini en vivo: cada 1 s mira la fecha del archivo (GetFileAttributesExA) en el hilo del juego; relee todo y lo anota. Universo distinto => logica_reset. Metodo 1/2 engancha el boton si hace falta.
- Tecla de panico [General] TeclaPanico (default ctrl+f12; parser en tecla.c, probado en Linux). GetAsyncKeyState solo con la ventana del juego al frente. Alterna g_panico: no corre paso() y la altura fija pasa derecho. MessageBeep al cambiar.
- Cambio: iniciar() ahora hace los chequeos de version aunque Activado=0 (para poder prenderlo en vivo). Activado=0 o panico => en_pausa(); al volver, logica_reset.
- "F7" en el pedido de Franco es el codigo de la idea original, no la tecla (DETALLE dice Ctrl+F12). Se puede poner TeclaPanico=f7.
- Sin probar en el juego.

## v0.1.11: cambiar la logica sin salir del juego
- Dos DLL: xinput_ag.dll = cargador (cargador.c: XInput, PeekMessageA, vtable de MotherVR, boton, log, recarga) y agachar_logica.dll (logica_juego.c, exporta ag_iniciar/ag_tick/ag_compensar/ag_boton/ag_terminar). Interfaz en anfitrion.h (ANFITRION_VERSION 1).
- El cargador carga una COPIA en %TEMP% (agachar_logica_<pid>_<n>.dll) para que el original se pueda pisar. Los enganches quedan siempre; llaman a la logica con un SRWLOCK compartido; la recarga cambia los punteros con el exclusivo, llama ag_terminar y FreeLibrary.
- Recarga: [General] TeclaRecargar (ctrl+f11) o RecargarSolo=1 (fecha+tamano de agachar_logica.dll, quieto 1 s).
- Probado en Wine (wine 9.0 + xvfb, WINEDLLOVERRIDES=dxgi=n) con el dxgi.dll real de MotherVR mapeado sin ejecutar y la pose falsa: 20 recargas con ~293M poses pedidas desde otro hilo, sin cuelgues; DLL roto -> falla y sigue. Sin probar en el juego.
- El microfono NO esta en esta estructura todavia.
- 28/10 23:17 Franco (v0.1.11 en el juego): Windows lo dejo pisar agachar_logica.dll con el juego abierto (DLL de prueba "PRUEBA2" en ../recarga/prueba_v0.1.11/), y Ctrl+F12 apaga/prende bien. Falta ver en agachar.log "recarga lista" + "PRUEBA2". Diseño general de recarga: ../recarga/EVALUACION.md.
- 28/10 23:18 log de Franco: PRUEBA2 cargado YA AL ARRANCAR (20:14:31), sin ninguna linea "recargando": la recarga en vivo todavia NO se ejercito en el juego. Agacharse anda bien con PRUEBA2 (Crouch/Uncrouch ok, altura fija ok), panico ok. Nuevo: "mando 0: conectado (codigo 0)" (antes 1167). Pedido: con el juego abierto, Ctrl+F11 y despues pisar con el agachar_logica.dll original del zip -> esperar "logica de agacharse v0.1.11 (recarga N)".
- 28/10 23:23 CONFIRMADO EN EL JUEGO: recarga en vivo de v0.1.11. Ctrl+F11 (20:23:00, "PRUEBA2 (recarga 1)") y pisar el archivo (20:23:14, "PRUEBA3 (recarga 2)"), cada una en ~25 ms, sin cuelgues; agacharse y la altura fija siguen andando despues. La referencia de altura se recalibra en cada recarga (de ahi el "baul" de ../recarga/EVALUACION.md). v0.1.11 pasa a ser la base para v0.2.0.
- 28/10 23:25 Franco: en el menu de pausa no puede "aceptar" (se mueve por las opciones, atras con R3 y salir con el boton de pausa si). Pista: desde el 28/10 los logs dicen "mando 0: conectado (codigo 0)"; todos los del 26/10 decian "no conectado (1167)". El cargador solo reenvia XInputGetState (no lo toca). Hipotesis: un mando de Xbox virtual nuevo (Steam Input u otro programa) pone el juego en modo mando. Pedido: joy.cpl y desactivar Steam Input del juego.
- 28/10 23:30 RESUELTO: el menu de pausa que no aceptaba era Steam Input (mando virtual de Xbox). Franco lo desactivo en Propiedades > Controlador del juego y se arreglo. Si vuelve a aparecer 'mando 0: conectado', es eso.

## v0.2.0: cargador general + carpeta mods_vr (diseño en ../recarga/EVALUACION.md)
- xinput_ag.dll (cargador.c) + mods_vr\<feature>.dll + mods_vr\<feature>.ini. cargador.ini (TeclaPanico ctrl+f12, TeclaRecargar ctrl+f11, RecargarSolo, Log). Log unico: ai_mods.log, lineas de feature con "[nombre]".
- Interfaz anfitrion.h (ANFITRION_VERSION 2): la feature exporta solo mod_iniciar(const Anfitrion*, Feature*). Feature: tam, prioridad, tick(dt), pose(cabeza,self,out,escala) en cadena por prioridad, config (cambio su .ini), pausa (panico), vibracion_juego (XInputSetState), terminar. Anfitrion: tam, log(yo,...), openvr() (lo busca el cargador), estado_mvr, baul_leer/baul_guardar, enganchar_puntero, enganchar_codigo (puesto fijo jmp [destino] + trampolin; al descargar destino vuelve a lo original; dos features no pueden lo mismo).
- Cargador: mira mods_vr cada 1 s (nueva -> carga, cambiada y quieta 1 s -> recarga solo esa, borrada -> descarga), copias en %TEMP%\ai_mods_<pid>\, FreeLibrary de la copia vieja 2 s despues. Panico y teclas en el cargador.
- agachar.dll: igual que v0.1.11 (metodo directo + altura fija), sin metodos 1/2 (boton), guarda Estado (calibracion) en el baul -> no recalibra al recargar. Prioridad de pose 10.
- Wine (dxgi.dll real, feature de prueba pruebas/feature_prueba.c): 32/32 ok: 40 recargas con hilo de pose y hilo llamando a lo enganchado (~3 mil millones de llamadas, 0 resultados raros), baul, DLL roto, borrar, dos features mismo enganche, .ini, Ctrl+F12, Ctrl+F11. Sin probar en el juego.
- 28/10 23:53 Franco, v0.2.0 EN EL JUEGO: carga bien (cargador, mods_vr\agachar.dll, PeekMessageA, pose de MotherVR, OpenVR), agacharse anda igual que antes, Ctrl+F12 ok. Falta: pisar agachar.dll jugando (no hay 'recargada' en ese log). Sigue 'mando 0: conectado'.
- 28/10 23:56 CONFIRMADO EN EL JUEGO v0.2.0: pisar mods_vr\agachar.dll jugando -> 'agachar recargada (archivo nuevo)' en ~22 ms, 'recupero la calibracion' (baul ok), agacharse sigue andando. Ctrl+F11 no se probo en esta version (si en v0.1.11). v0.2.0 = base estable para las features nuevas.

## manos v0.1 (diagnostico), 29/10 (hilo "Evitar que las manos atraviesen paredes")
- mods_vr\manos.dll, solo log. Fuente en manos_v0.1_diagnostico.zip (fuentes/manos.c; compila igual que agachar.c, ANFITRION_VERSION 2, sin cambiar el cargador).
- enganchar_puntero sobre MotherVR+0x676fc (puntero de la funcion de pose estilo ovr_GetTrackingState, cdecl (salida, sesion, double, int), 0x138 bytes; verificado con "a1 <slot>" en +0x24336). Guarda cabeza/manos (posicion en +0x10 de cada pose; manos en 0x60/0xb8).
- tick: cada Intervalo anota hilo de pose vs juego, cabeza, manos, pj+0x70, cada 5 s pj+0x40..0x9c (buscar el giro), y rayos 0x7ebab0 (desde/hasta/normal alineados a 16) desde pj+0x70+(0,AlturaPecho,0) a +-x/+-z, filtros 23 y 10.
- Sin probar en el juego ni en Wine. Detalle del analisis: ideas_gameplay/ANALISIS_MANOS.md.
- 29/10 00:31 log de Franco v0.1: carga ok, rayos 0x7ebab0 ANDAN sin colgar (filtro 23 da paredes a ~0.45 m; filtro 10 choca con mas cosas). Y es arriba (pj+0x70 y=7.614 fijo). Pero [MotherVR+0x676fc] es NULL con SteamVR (solo se llena con LibOVR de Oculus): esa pose no sirve.
- GRAND con SteamVR saca las manos de la lista de poses de MotherVR (self+0x10, TrackedDevicePose_t de 0x50, indice por IVRSystem slot 18 GetTrackedDeviceIndexForControllerRole 1/2) dentro de su gancho a la pose de cabeza 0x100070e0 (GRAND 0x10035040). Esa lista la llena IVRCompositor::GetLastPoses (slot 3) con el compositor guardado en [MotherVR+0x6af50].
## manos v0.2 (diagnostico), 29/10
- enganchar_puntero sobre la tabla del IVRCompositor (slot 3 GetLastPoses, thiscall ret 0x10); verifica "a1 <base+0x6af50>" en MotherVR+0x7123. Anota visor/manos/giro del visor; PruebaSubirDerecha (m) sube render[der].m[1][3] para ver si GRAND la sigue; volcado pj+0..0x3fc cada 3 s para buscar el giro de Ripley. Sin probar.
- 29/10 00:38 Franco v0.2: CONFIRMADO EN EL JUEGO: PruebaSubirDerecha=0.2 sube la mano derecha de Ripley (GRAND la sigue). El enganche a GetLastPoses sirve para frenar las manos. Espacio del visor: y arriba, visor ~1.25 m (sentado/crudo). El volcado pj+0..0x3fc no mostro el giro de Ripley.
## manos v0.3 (diagnostico), 29/10
- Camara: manager [[0x16f0c88]+0x58]; activa [cm+0x1d8] (0x432300 la usa si +0x14d y no +0x14f; +0x1f0 la pisa si cm+0x221), jugador [cm+0x1dc]. Camara: cuaternion x,y,z,w en +0x2c, posicion +0x3c, fov +0x48. v0.3 los anota con dos giros candidatos. Sin probar.
- 29/10 00:47 log v0.3: la camara activa [cm+0x1d8] es la de los ojos: pos y = pies + ~1.95 parado / ~1.2 agachado (no sigue la posicion del visor en el cuarto), y su cuaternion es casi solo giro en y y SIGUE AL STICK Y A LA CABEZA. Con mis formulas: giroZ_cam = -giro_visor + C (C cambia con el stick). Falta saber que eje de la camara es adelante (+Z o -Z) y cual derecha.
## manos v0.4 (diagnostico), 29/10 (DLL suelto en agachar/manos_v0.4/)
- Rayos desde la posicion de la camara a lo largo de sus ejes +-X y +-Z (horizontales). Prueba: pared adelante y pared a la derecha. RayosMundo=0 apaga los de Ripley.
- 29/10 00:51 log v0.4: pared de frente -> rayo camara +Z 0.57; pared a la derecha (giro a la izquierda: visor -111 -> -15, camara 138 -> 42) -> +X 0.51. => camara: +Z adelante, +X derecha, +Y arriba. Visor OpenVR: -Z adelante. Mapa = ejes_camara * diag(1,1,-1) * visor^T (tiene espejo; coincide con giro_cam = -giro_visor + C).
## manos v0.5 (la de verdad), 29/10 (agachar/manos_v0.5/: DLL, ini y fuentes)
- Hilo del juego: v = mano - visor (crudo), a ejes del visor, z invertida, a ejes de la camara; rayo filtro 23 desde camara pos hasta camara + v*(d+Margen)/d; permitido = (fr*(d+Margen)-Margen)/d. Solo con camara activa == camara del jugador.
- Hilo que dibuja (GetLastPoses): mano = visor + permitido*(mano - visor), solo posicion (velocidades intactas). Un cuadro de atraso.
- Log: "frenada: izq N de M cuadros (hasta X% del brazo)". ini: Activado, Margen 0.06, Escala 1.0, Log, Intervalo, Filtro, PruebaSubirDerecha. Sin probar.
- 29/10 01:00 Franco v0.5 EN EL JUEGO: "funciona muy bien". Detalle: en el marco de una puerta, con la derecha (arma) a veces atraviesa en ciertos angulos; con la izquierda no.
## manos v0.6, 29/10 (agachar/manos_v0.6/)
- Ademas del rayo al centro: 4 rayos a puntos a Radio (0.05) alrededor de la mano, perpendiculares al camino ojos-mano, y 1 a Adelante (0.12) hacia donde apunta el mando (-z del mando). Se toma el minimo. 6 rayos por mano por cuadro. Sin probar.
- 29/10 01:05 Franco v0.6 EN EL JUEGO: funciona (tambien el marco de la puerta). Handoff actualizado.

## tacto_calibrar v0.1 (29/10, hilo "Elegir features probables al inicio"): manos v0.7 + agachar v0.2.1
- En agachar/tacto_calibrar_v0.1/ (mods_vr\ con DLL e .ini, fuentes\) y tacto_calibrar_v0.1.zip. No cambia el cargador (ANFITRION_VERSION 2).
- vibrar.c/h (compartido, cada feature lo compila adentro): IVRSystem_017 slot 33 GetControllerState (VRControllerState_t pack 4, 60 bytes; gatillo = bit 33 o eje 1 x), slot 35 TriggerHapticPulse (max 3999 us por llamada, se repite cada cuadro para toques largos). Slots verificados contra openvr.h v1.0.10. Prueba con IVRSystem falso (fuentes/prueba_vibrar.c, gcc -m32): ok.
- manos v0.7 "pared al tacto": cuando permitido de una mano baja de 0.97 (suelta arriba de 0.995), un toque de VibrarMs (40) a VibrarFuerza (0.8), maximo uno cada VibrarCada (0.5 s). Log: "vibro (pared al tacto)" las 3 primeras veces y "pared al tacto: N toques" cada Intervalo. Es tambien la prueba de vibracion de la T0.
- agachar v0.2.1 "calibrar con gesto" ([Calibrar]): los dos gatillos (Boton=1: agarres) Segundos (2) -> la altura actual (y la de MotherVR) pasa a ser la de parada fija (manda sobre AlturaParado y sobre la automatica; baul v2, lee el v1). Sentado si cabeza < LimiteSentado (1.45; Franco: ojos sentado ~1.30 m). Confirma: 1 toque parado / 2 sentado + MessageBeep. Log "calibrado con el gesto: SENTADO/PARADO".
- Sin probar en el juego ni en Wine.
- 29/10 01:36 Franco: PARED AL TACTO CONFIRMADA EN EL JUEGO ("funciona perfecto"): TriggerHapticPulse (slot 35) llega por Steam Link al Quest 3. Calibrar con gesto no hacia nada.
- Causa: VRControllerState_t en Windows mide 64 (el pack 4 de openvr.h es solo Linux/Mac); le pasabamos 60 y GetControllerState devolvia false. agachar v0.2.2 (agachar/tacto_calibrar_v0.2/, solo agachar.dll) lo arregla y anota en el log los cambios de los botones (20 primeras) y si no puede leerlos. Sin probar en el juego.
- 29/10 01:43 log de Franco v0.2.2: CALIBRAR CON GESTO ANDA en el juego (lee los gatillos; SENTADO 1.21-1.27 m, PARADO 1.61-1.64 m; despues de calibrar parado agacha al bajar a 1.38). Franco dijo que de sentado a parado "no cambia la altura": es lo esperado (la vista es siempre la de Ripley parada; solo cambia desde donde cuenta agacharse). Se lo explique; esperando si queria otra cosa.
- 29/10 01:46 Franco aclaro: la VISTA si sigue la altura real que ve MotherVR. Calibrado sentado se ve bien; parado y recalibrado queda mas alto (MotherVR ve 1.63). Quiere que calibrar deje la vista igual en las dos posturas.
- agachar v0.2.3 (agachar/tacto_calibrar_v0.3/): con gesto hecho y [Calibrar] AlturaVista>0 (def 1.30), MotherVR ve AlturaVista + max(0, cabeza - referencia) (abajo de la referencia queda fija como antes). AlturaVista=0 = comportamiento v0.2.2. Sin probar en el juego.

## pausa v0.1 (29/10, mismo hilo): mods_vr\pausa.dll + pausa.ini (agachar/pausa_v0.1/)
- A16. Sacado = sensor (IVRSystem_017 slot 16 GetTrackedDeviceActivityLevel(0) no es 1/2) o visor quieto Quieto s (posicion < 1 mm y matriz < 0.002, o pose invalida), sostenido Segundos. Aprieta Esc una vez con SendInput (scancode DIK 0x01, 150 ms) solo si la ventana del juego tiene el foco, hay Ripley, control libre (ctl+0x1f) y camara activa == jugador. No despausa al volver.
- Diagnostico: anota el nivel del sensor cuando cambia, y "control libre / camara del jugador / control+1c..23" al sacar, cada 3 s en pausa y al poner: para encontrar la bandera de pausa (si en pausa el control libre sigue en 1, un segundo Esc con el menu abierto lo cerraria).
- Sin probar en el juego.
- 29/10 01:57 Franco: PAUSA CONFIRMADA EN EL JUEGO (visor en la mesa -> pausa "despues de un tiempo", probablemente por Quieto 4 s + Segundos 2, no por el sensor). Sin log todavia: falta saber si el sensor llega por Steam Link y la bandera de pausa.

## tecla_prueba v0.1 (29/10, mismo hilo): prueba de la tecla virtual (agachar/tecla_prueba_v0.1/)
- Desensamblado de 0x986d80 (capstone): ECX = mapa ([0x1759b44]+0x48, confirmado en 0x990a05). Accion i en mapa+4+i*0x40: +0x30 combinaciones, +0x34 cantidad, +0x38 estado (solo se consulta si bit 0x10). Combinacion: +4 codigo (word, 0xffff = ninguno), +6 tipo, +7 banderas (0x40 -> va por [mapa+0x1034], no por el dispositivo; 0x80 -> marca distinta en esp+0x12/0x13). Dispositivo = [mapa+0x103c+tipo*4], tiene que tener [disp+0x10] != 0; abajo = vtable[1](codigo), thiscall, 1 arg, devuelve al. Cantidad de acciones en mapa+0x1054.
- La feature engancha vtable[1] del dispositivo de la primera tecla valida de la accion (enganchar_puntero) y contesta 1 para (dispositivo, codigo) mientras dura. Ctrl+F9 = accion 8 (correr) 3 s, Ctrl+F10 = accion 9 (agacharse) 0.2 s. Log: todas las acciones con sus teclas y los dispositivos (una vez), y cambios del estado de las dos acciones. Sin probar en el juego.
- 29/10 02:07 Franco: TECLA VIRTUAL CONFIRMADA EN EL JUEGO con agacharse (Ctrl+F10, accion 9). Correr (Ctrl+F9, accion 8) no anduvo; esperando el log.
- 29/10 02:07 log tecla_prueba: 41 acciones. Dispositivos: tipo 0 (codigos 0x01xx, el mando/pad virtual de GRAND?) vtable 0x1ad7670 (rebasado), tipo 1 teclado DIK vtable 0x1ad7628, tipo 2 mouse vtable 0x1ad75e0, tipo 5 raro. Correr (8) = [0108 tipo 0 fl 80] [002a LShift tipo 1]; agacharse 9 = [0109 t0] [002e C t1]; asomarse 18 = [0112 t0] [001d LCtrl t1]; linterna 29 = [011d t0] [0021 F t1]; tracker 28 = [011c t0] [0039 Space t1]; recargar 16 = [0110 t0] [0013 R t1]; usar 17 = [0111 t0] [0013 R? t1]; mapa/menu 38 = [0126 t0] [000f Tab t1]. Contestando 'abajo' al tipo 0 el estado de 8 paso a 0xda (mantenido) y el de 9 a 0xd2 -> AGACHARSE ANDA; correr no corrio.
- Lector de correr 0x679d60: corre si 0x677c00 (bl), pj+0x3e32 == 0, pj+0x3e34 == 0, 0x678000(this), [mapa+0x23c] & 2 (accion 8 mantenida) y 0xea70f0(pj+0x3e18); entonces pj+0x2b8 = 1. Sospecha: en la Torrens (inicio) el script no deja correr. Pregunte a Franco si corre con el boton real.
- 29/10 02:10 Franco: en la Torrens tampoco corre con el boton real -> TECLA VIRTUAL CONFIRMADA (correr bloqueado por el nivel, no por el mod).

## mapa v0.1 (29/10, mismo hilo): mapa en la muneca (agachar/mapa_v0.1/)
- tecla_virtual.c/h: modulo reusable (engancha vtable[1] de cada clase de dispositivo de la accion, hasta 3 clases y 4 teclas; contesta "abajo" mientras tv_apretar(1)). Ojo: dos features no pueden enganchar la misma vtable (borrar tecla_prueba.dll).
- Gesto: mando izquierdo (Mano) a < Distancia 0.45 m del visor y su Eje (-x por defecto, sin confirmar) a < Angulo 35 grados de la direccion a los ojos, Demora 0.25 s; suelta con +10 cm / +15 grados por 0.2 s. Pose: IVRSystem slot 11 universo 2 (16 dispositivos), mando con resultado 200 y valida (idea 30).
- Accion 38 (Tab) con un toque de 0.12 s (Modo 0) o mantenida (Modo 1). Abre solo si control libre (ctl+0x1f). Cierra solo si no esta en control libre (CerrarSoloSiMenu), salvo que 0.6 s despues de abrir siga libre: entonces cierra siempre (la bandera no sirve). LogEjes: grados de los 6 ejes cada 0.5 s con el mando cerca. Sin probar en el juego.
- 29/10 02:21 Franco: MAPA EN LA MUNECA CONFIRMADO EN EL JUEGO ("increible, lo amo"; Eje=-x anduvo). Pero al cerrar es tosco: "se traba medio segundo".
- mapa v0.2 (agachar/mapa_v0.2/): cierra a los DemoraCerrar 0.03 s (antes 0.2) con margen MargenAngulo 8 grados y 5 cm (antes 15 y 10), y al instante si la muneca gira para afuera a > 120 grados/s. Si la trabada sigue, puede ser la animacion de cierre del propio juego (comparar con Tab a mano).
- mapa v0.3 (agachar/mapa_v0.3/): con el mapa abierto, el stick de la otra mano (vibrar_stick: GetControllerState eje 0) aprieta teclas sueltas del teclado (tecla_virtual: tv_tecla_preparar(tipo 1) + tv_tecla(DIK)). TeclasMover def "11 1e 1f 20" (WASD); alternativa flechas "c8 cb d0 cd". ZonaMuerta 0.45. Sin probar en el juego.
- 29/10 02:27 Franco: v0.3 (WASD) mueve el mapa pero "re contra tosco" (digital).
- Desensamblado: analogicos en 0x986d80 segunda parte: lista en mapa+0xa74 (combinaciones), +0xa78 cantidad, +0xa80 valor, +0xa84 banderas (0x20 activa; tipo&0xf == 2 -> vtable[2](cod), si no vtable[3](cod, invertir = bit 5 de banderas de la combinacion)); paso 0x44, cantidad en mapa+0x1058; se queda con el de mayor |valor|. Mando tipo 0 (vtable 0x13e7670 sin rebase): [1] 0x976730 EstaAbajo (solo cod < 0x10 bits en +0x1c; 0x10/0x11 gatillos > umbral), [2] 0x976780 (0x10 +0x278, 0x11 +0x27c, 0x16 +0x280, 0x17 +0x284), [3] 0x976820 -> 0x976630 sobre this+0x18: 0x10 +0x260, 0x11 +0x264, 0x12 +0x250, 0x13 +0x254, 0x14 +0x258, 0x15 +0x25c, 0x16 +0x268, 0x17 +0x26c (x -1 si invertir).
- mapa v0.4 (agachar/mapa_v0.4/): StickModo=1 engancha vtable[3] del mando tipo 0 y, con el mapa abierto, para los codigos del stick izquierdo (StickEjes, supuesto 0x12/0x13) devuelve el del derecho (0x14/0x15) si es mayor en modulo. Log "sticks:" cada 1 s con el mapa abierto (0x10..0x17) para confirmar los codigos. Sin probar en el juego.
- 29/10 02:31 log v0.4: el enganche a vtable[3] del mando tipo 0 anda, pero 0x10..0x17 dan 0.00 siempre con el mapa abierto: el stick izquierdo NO llega por ese mando. Con el mapa abierto Ripley sigue en control libre (ctl+0x1f = 1).
- mapa v0.5 (diagnostico, agachar/mapa_v0.5/, solo DLL): al abrir el mapa anota las listas de ejes analogicos de los 3 mapas (+0x48, +0x2998, +0x3a0c) y cada 0.5 s los ejes != 0 junto con los sticks VR por OpenVR.
- 29/10 02:33 log v0.5: el mapa usa el mapa de MENUS (+0x2998): ejes 2/3 = stick izquierdo (cod 0x215/0x216, tipo 0, vtable[3]), ejes 4/5 = derecho (0x217/0x218). Y arriba positivo en los dos. Mapa de juego (+0x48): ejes 0129..012e; mapa +0x3a0c: 0420..0427 (con teclado D/A y W/S).
- mapa v0.6 (agachar/mapa_v0.6/): StickEjes=215 216 217 218, engancha vtable[3] del tipo 0 del mapa de menus, sin chequear el dispositivo. LogEjes=0 por defecto (el gesto ya anda). Hay que usar el .ini nuevo (el de v0.4 tenia 12 13 14 15). Sin probar.
- 29/10 02:36 Franco: MAPA v0.6 CONFIRMADO ("es perfecto"): gesto de reloj + stick derecho analogico en el mapa. Versiones vigentes de este hilo: manos v0.7 (pared al tacto), agachar v0.2.3 (calibrar con gesto, AlturaVista), pausa v0.1 (Quieto=2, Segundos=1), mapa v0.6. tecla_prueba ya no se usa.

## usar v0.1 (29/10, hilo "Elegir más features para el inicio"): usar con la palma (agachar/usar_v0.1/)
- mods_vr\usar.dll + usar.ini. Gesto (usar_gesto.c, prueba_gesto.c ok con gcc): mano (Mano=1 derecha) hacia el frente del visor (sin altura) a > Velocidad 1.0 m/s, > Alcance 0.30 m delante de los ojos, desvio < Angulo 40, 2 cuadros seguidos; Espera 0.6 s y la mano tiene que volver (delante < 0.6*Alcance o yendo atras) para el siguiente. Velocidad de OpenVR (PoseVR.vel, universo 2).
- Aprieta la accion 17 (usar) con SendInput scancode (0.12 s), NO con tecla virtual: mapa.dll ya tiene enganchadas las vtables de los dispositivos tipo 0 y 1 y el cargador no deja que dos features enganchen lo mismo (puesto_tomar: "ya lo usa"). Tecla = la de tipo 1 de la accion en el mapa (Tecla= en el .ini la pisa; ojo, el log de tecla_prueba decia 0x13 R para usar, puede ser 0x12 E). Solo con foco del juego y control libre.
- Buscador (Buscar=1): cada 0.1 s compara punteros de pj+0..0x4000 y [pj+0x64]+0..0x400; 5 s de calentamiento + mas de 4 cambios en 20 s = ruido (se ignora). Anota "buscador: pj+X a -> b" con "pos +k a N m" si el objeto (o uno de sus punteros) tiene una posicion a < 5 m de Ripley (pj+0x70). Al apretar usar (boton real: estado accion 17 bit 1, o el gesto) anota los que cambiaron en los ultimos 3 s. Objetivo: encontrar el objeto de interaccion para pedir que la mano lo toque.
- LogGesto=1: un renglon por movimiento rapido (vel max, delante, grados, empujon o no). Sin probar en el juego.
- 29/10 02:47 Franco: usar v0.1 "no hace nada". El log de tecla_prueba dice accion 17 = [0111 t0][0013 R t1] (igual que recargar 16): la 17 NO es usar. E (0x12) esta en las acciones 12 y 15. usar v0.1.1 (agachar/usar_v0.1.1/): Accion=12 por defecto (aprieta la E). Esperando log.
- 29/10 02:47 log v0.1: el GESTO ANDA (empujones a 2-4 m/s, 0.3-0.5 m delante; varios fallaron por 45-54 grados) pero apretaba la R (accion 17). Buscador inutil: los floats 0x3f.../0xbf... pasaban por punteros (el proceso tiene memoria alta).
- usar v0.1.2 (agachar/usar_v0.1.2/, reemplaza la v0.1.1 que no se entrego): Accion=12 (E), Angulo=50, y el buscador solo cuenta punteros a objetos cuya vtable cae dentro de AI.exe. Sin probar.
- 29/10 02:51 Franco: usar v0.1.2 FUNCIONA EN EL JUEGO (puertas), pero no en las computadoras (falta log). Y empujar con la mano izquierda a veces abre el mapa (el dorso queda mirando a la cara).
- mapa v0.7 (agachar/mapa_v0.7/): la mano tiene que ir a menos de VelocidadMax 0.6 m/s (OpenVR vel) o se reinicia la Demora, y despues de un movimiento rapido no abre por Calma 0.3 s. Sin probar.
- 29/10 02:52 log v0.1.2: E por SendInput anda. Buscador: pj+3570 pasa a objeto (vtable de AI.exe, con [+24] posiciones a 0.1-0.6 m de Ripley) justo DESPUES de usar y vuelve a 0 al terminar (3 a 9 s): parece la interaccion en curso, no el objeto marcado antes de usar. pj+35c8/35cc acompanian. El objeto marcado sigue sin aparecer en pj ni en ctl.
- usar v0.1.3 (agachar/usar_v0.1.3/): Mano=2 (cualquiera) por defecto, Mantener (s con la tecla abajo, def 0.12) para probar si las computadoras piden mantener. Sin probar.
- 29/10 02:56 Franco + log v0.1.3/mapa v0.7: la computadora abrio 1-2 veces de muchas; los empujones frente a ella fallaban por delante 0.25-0.31 (< Alcance 0.30; manos no deja pasar la pared) y angulo 40-53 (empujar hacia abajo). Vibraba aunque fuera al aire. El mapa se siguio abriendo a veces entre empujones con la izquierda.
- usar v0.1.4 (agachar/usar_v0.1.4/): Alcance 0.22; Angulo mide solo el desvio a los costados (abajo-adelante vale; se rechaza si |vy| > 2*adelante). Vibrar=1 vibra solo si en 0.8 s aparece un objeto nuevo en pj+0x3560..0x358c (la interaccion en curso, pj+3570/3574); Vibrar=2 siempre. Log "el empujon uso algo" / "no uso nada".
- mapa v0.7.1 (agachar/mapa_v0.7.1/): Calma 1.0 s solo despues de un empujon (velocidad hacia el frente del visor > 1 m/s), asi subir la muneca rapido no demora el mapa. VelocidadMax 0.6 sigue reiniciando la Demora. Sin probar.
- 29/10 03:00 Franco: vibracion OK. Computadora: de lejos anda, pegado "medio medio". Log v0.1.4: pegado, el gesto se detecta y aprieta E pero "no uso nada" (~10 seguidos). Franco: pegado la indicacion SIGUE visible y el boton real ANDA. Hipotesis: al empujar se mueve la cabeza y se pierde la indicacion justo al apretar.
- usar v0.1.5 (agachar/usar_v0.1.5/): Alcance 0.12 (aprieta antes) y log "la cabeza se movio X cm y giro Y grados desde que empezo el empujon" (desde que la mano supera 0.4 m/s hacia adelante). Sin probar.
- 29/10 03:05 Franco: USAR v0.1.5 CONFIRMADO EN EL JUEGO, tambien pegado a la computadora. Versiones vigentes: manos v0.7, agachar v0.2.3, pausa v0.1, mapa v0.7.1, usar v0.1.5. Pendiente (no pedido): tocar el objeto de verdad (el objeto marcado antes de usar sigue sin encontrarse; pj+3570/3574 = interaccion en curso, despues de usar).

## manos v0.8 (29/10, hilo "Usar objetos con la palma"): usar al tocar (agachar/manos_v0.8/)
- Idea de Franco: si hay indicacion de usar y tocas el objeto, que se use. Como no sabemos leer la indicacion, cada vez que una mano empieza a chocar (mismo evento que la pared al tacto: permitido < 0.97) aprieta la E con SendInput (Tecla=12, 0.12 s), una vez cada UsarCada 1.0 s, solo con foco del juego (y el choque solo se calcula con la camara del jugador). Sin indicacion la E no hace nada. UsarAlTocar=1 por defecto. Log "la mano X toco algo: aprieto usar" (20 primeras). Base: manos v0.7 de tacto_calibrar_v0.1. Sin probar.
- 29/10 03:11 Franco probo usar al tocar solo: "mejor no, hay muchas cosas que directamente no puedo tocar" (no hay colision donde esta el objeto). Recomendado: usar.dll (empujon) Activado=1 y manos UsarAlTocar=0 (o ambos si quiere).

## agachar v0.2.4 (29/10, hilo "Elegir features para el resto del juego"): calibrar solo con los brazos adelante (agachar/calibrar_brazos_v0.2.4/)
- Pedido de Franco: que cerrar las manos (futura feature de dedos con gatillo/agarre) no recalibre. [Calibrar] BrazosAdelante=1 (def): ademas de los botones, cada mano tiene que estar a >= Alcance (0.40 m) delante de los ojos (direccion horizontal a la que mira el visor, -z de su matriz), < 0.45 m al costado y < 0.60 m por debajo. Si no, el tiempo vuelve a 0 y anota "botones apretados pero los brazos no estan adelante (...)" cada 5 s. Poses: GetDeviceToAbsoluteTrackingPose x16 en g_universo, mandos por vibrar_indice. Solo agachar.dll + agachar.ini. Sin probar en el juego.

## dedos v0.1 (29/10, mismo hilo): cerrar las manos con gatillo/agarre (agachar/dedos_v0.1/)
- Ghidra: LEFT_FINGERS/RIGHT_FINGERS (0xa4b280) son solo nombres de una mascara de partes del cuerpo para animaciones. GRAND engancha 0x6a5040 (paso de IK por personaje; ECX = pj+0x3f0) con 0x1002f2e0: pose = [pj+0x3f0+0x24c] = [pj+0x63c] (hkaPose: +0 esqueleto, +4 local, +0x10 modelo, +0x1c banderas; esqueleto +0xc padres, +0x18 huesos (8 bytes: nombre, bool), +0x1c cantidad). Accesos: 0xbb9e80 modelo (leer), 0xbbab40 modelo (escribir), 0xbba6b0 local (escribir, thiscall hueso), 0xbba090 sincronizar. GRAND escribe rotaciones de 7, 8, 0x1e, 0x1f; manos 0x20 y 0x30; guarda/restaura 0x21..0x2f (15 huesos = dedos de una mano).
- 0x6a5040 lo llama solo el trabajo 0x6a55e0 (cdecl, arg personaje; mov ecx,[esp+4]; add ecx,0x3f0; jmp 0x6a5040), encolado en 0x6a5655 para cada personaje con +0xf4c (sistema de trabajos: otros hilos).
- dedos.dll engancha 0x6a55e0 (firma 8B4C240481C1F0030000, 10 bytes) y, si es el jugador, antes de seguir gira en local los huesos de dedo (hijos a <= 4 niveles de 0x20/0x30; si hay nombres con finger/thumb/index..., solo esos) cierre*Angulo en EjeDedos (pulgar: AnguloPulgar, EjePulgar). Guarda base/escrita por hueso para no acumular si la animacion no los pisa. Cierre: Boton 0 gatillo (eje 1), 1 agarre (eje 2 o bit 2), 2 el mayor; suavizado Velocidad; solo con control libre; SoloManosVacias=1 usa [[pj+0x31c]+0x3c]+0x3bc8 (log "objeto en la mano"). Log: esqueleto completo (indice, nombre, padre) y dedos elegidos por mano. Sin probar en el juego.
