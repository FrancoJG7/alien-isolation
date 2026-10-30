# Análisis Ghidra: manos que no atraviesen paredes (29/10/2026)

AI.exe Steam (base 0x400000), MotherVR = `uploads/hearth/96964a49…` (dxgi.dll, base 0x10000000), GRAND v0.6.3 original. Solo estático: **nada probado en el juego**.

> **Corrección 29/10 (probado en el juego):** la sección 1 vale solo para Oculus. Con SteamVR el puntero [0x100676fc] es NULL; GRAND saca las manos de la lista de poses de MotherVR (self+0x10), que llena IVRCompositor::GetLastPoses. La feature que anda (manos v0.6) engancha ahí. La sección 3 se resolvió con la cámara del jugador. Ver HANDOFF.md sección 4 y `../agachar/NOTAS.md`.
Herramientas: `ghidra_scripts/run.sh` (AI.exe), `runm.sh` (MotherVR, proyecto pmvr), `rung.sh` (GRAND, proyecto pgr).

## 1. De dónde salen las manos
- El juego tiene el código viejo de Oculus DK2 (ovrHmd_BeginFrame, ovrHmd_StartSensor… en 0x9b8df0..0x9b9200). MotherVR le hace de LibOVR.
- La pose de cabeza **y de las dos manos** sale de una función estilo `ovr_GetTrackingState` de MotherVR: **0x10024320** (RVA 0x24320). Copia 0x138 bytes desde el puntero **[0x100676fc]** (RVA 0x676fc), que es el backend real.
- Layout (LibOVR 1.x ovrTrackingState, 0x138): HeadPose @0x00, HandPoses[0] @0x60, HandPoses[1] @0xb8 (cada uno: quat 16 bytes + **posición en +0x10**, velocidades después), HandStatusFlags @0x110.
- **GRAND la intercepta**: detour en MotherVR+0x24320 (`FUN_10024cc0(base+0x90c8*4, 0x100346c0, 6)`, ojo que GRAND multiplica por 4 porque usa HMODULE*). Su gancho 0x100346c0 guarda cabeza (0x100b47b8..), mano 1 (0x100b47c8..) y mano 2 (0x100b47f8..), y la velocidad de la mano para tirar la IED.
- MotherVR solo no tiene manos (los mandos son botones: "Aim R Grip", "Motion Tracker L Grip"). Las manos son de GRAND.
- GRAND pone las manos con IK sobre el esqueleto de Ripley: usa 0xbbab40 (hkaPose: transformación del hueso en espacio del modelo) con los huesos 0x30 y 0x20, en su función 0x1002f2e0.

**Conclusión:** si envolvemos el puntero [0x100676fc] (llamar al original y después correr la posición de HandPoses[0/1]), GRAND ve la mano ya frenada, y con ella el arma. No hace falta tocar GRAND.

## 2. Rayos del juego
- `hkpWorld::castRay` = **0xb41f30** (thiscall, string "TtworldCastRay"). Con colector: 0xb41e80.
- Envoltorio del juego: **0x7ebab0**, cdecl:
  `bool Rayo(const float desde[4], const float hasta[4], float* fraccion, float normal[4], uint32 filtro)`
  Usa el mundo **[0x170791c]** (si es 0 devuelve false), rechaza NaN.
- Filtros que usa el juego: **0x17** (0x695660, dos rayos) y **10** (0x7744a0, cerca del muestreo de luz 0x7755c0). Falta ver cuál ignora a Ripley y a la propia arma: probarlo con el log.
- También existe la entidad de script `Raycast@CATHODE` (vtable 0x13ab04c), no hizo falta.

## 3. Del espacio del visor al mundo
- Posición de Ripley: `pj+0x70..0x78` (GetPlayer 0x78baf0).
- GRAND trabaja en espacio del modelo de Ripley (huesos), así que mundo = transformación de Ripley × espacio del modelo. Falta: dónde guarda Ripley su giro (probablemente un cuaternión cerca de +0x60/+0x80). Sale del log.
- La cámara del juego: CAMERA_UPDATE 0x7b4090; MotherVR redirige la llamada de 0x7b4100 (a 0x9be2a0, objeto [0x1766a2c]) a su 0x1001f8a0, que es su actualización por cuadro (giro por pasos, recentrar), no la matriz. Sistema de vistas: [0x1766a00] (0x9b45a0). No hizo falta seguir.

## 4. Plan
1. **Cargador (ANFITRION_VERSION 3):** nuevo enganche que envuelve [MotherVR+0x676fc] y pasa la estructura de 0x138 bytes a las features en cadena (`manos(estado)`), igual que la pose de cabeza. Firma a verificar al cargar: el código de 0x10024320 (si GRAND ya lo desvió, igual se puede cambiar el puntero porque GRAND llama al original que lo lee).
2. **Feature `manos.dll` de diagnóstico** (no cambia nada): cada cuadro anota en el log, cada 0,5 s: hilo que llama, pj+0x70, cabeza y manos en espacio del visor, y el resultado de un rayo desde el pecho de Ripley hacia adelante 1 m con filtro 0x17 y con 10. Franco se para frente a una pared, después lejos, y pasa el log.
3. **Feature `manos.dll` real:** rayo desde la cabeza hasta cada mano; si choca, mover la mano (en espacio del visor) hasta el choque menos `Margen`. Suavizar la salida para que no tiemble.

## 5. Riesgos
- Hilo: si quien pide la pose no es el hilo del juego, hacer los rayos en el tick del juego y aplicar el resultado al cuadro siguiente.
- El rayo puede chocar con el arma o el cuerpo de Ripley: elegir el filtro con el log.
- GRAND usa la misma mano para gestos y la velocidad para tirar la IED: solo tocar la posición, no las velocidades.
