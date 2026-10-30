# Manos que no atraviesen paredes (evaluación, 28/10/2026)

Pedido de Franco: que las manos VR no atraviesen paredes. La cabeza ya no las atraviesa (el cuerpo del juego choca), los ductos no los probó.

## Veredicto
Factible, pero no trivial: necesita una tanda de Ghidra y una feature nueva en `mods_vr\` (nada en el cargador salvo, quizá, un enganche nuevo).

## Qué hay en el exe (strings de AI.exe, sin Ghidra todavía)
- La física es **Havok** (hkpWorld, hkpSimpleWorldRayCaster, hkCpuWorldRayCastCollector, LinearCast, ShapePhantom).
- El juego usa rayos propios: `line_of_sight_test`, `raycastInterface`, `raycastLayer`, `m_b_raycast_intersected`.
- MotherVR tiene la opción "ClipBlinding" (oscurecer si la cabeza se mete en algo): probablemente sea por eso que la cabeza "no pasa".

## Idea de implementación
1. Cada cuadro, en el hilo del juego (tarea por cuadro, ver 0x7d6370), tirar un rayo de Havok **desde la cabeza hasta cada mano** (la cabeza siempre está fuera de la pared).
2. Posición de la mano en el mundo sin entender a MotherVR por dentro: mano_mundo = cámara_mundo + R · (mano_tracking − cabeza_tracking), con R sacado de comparar el frente de la cámara del juego con el del visor.
3. Si el rayo choca antes de llegar a la mano, acercar la mano a la cabeza hasta el punto de choque menos ~5 cm.
4. Aplicarlo en la pose del mando antes de que la lean MotherVR y GRAND. Lugar más seguro: la salida de OpenVR (IVRCompositor GetLastPoses/WaitGetPoses), porque GRAND puede leer OpenVR por su cuenta ([0x100b4874]).

## Qué falta (tanda de Ghidra)
- La función de rayo del juego (envoltorio de hkpWorld::castRay o `line_of_sight_test`) y el puntero al hkpWorld; qué capa ignora al jugador.
- Posición y orientación de la cámara del juego en mundo (PLAYER_CAMERA_UPDATE / CAMERA_UPDATE 0x7b4090).
- Desde qué copia de la pose del mando dibujan MotherVR y GRAND las manos/arma.

## Riesgos
- Llamar a Havok desde el hilo equivocado puede colgar el juego: hacer el rayo en el hilo del juego y guardar el resultado.
- Si se corrige la pose en OpenVR, también se corren los gestos (agacharse no, pero linterna/dos manos sí, unos cm).
- El arma apuntará distinto pegada a la pared (como en la mayoría de juegos VR).

## Plan por pasos
1. Ghidra (1 tanda).
2. Feature de diagnóstico que solo anota en el log "distancia a la pared desde la cabeza hacia cada mano". Franco toca una pared y mira el log.
3. Recién ahí, la feature que frena las manos, con `[Manos] Activado=1, Margen=0.05`.

## Actualización 29/10
Análisis hecho: ver `ANALISIS_MANOS.md`. Las manos salen de la función de MotherVR 0x10024320 (puntero [0x100676fc]), que GRAND intercepta; el rayo del juego es 0x7ebab0 con el mundo en [0x170791c].
