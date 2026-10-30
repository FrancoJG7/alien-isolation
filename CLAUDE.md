# Reglas de este repo (mods VR de Alien: Isolation)

## Números de versión
- Cada feature (y el cargador) tiene su **propio número**, en `#define VERSION "<nombre> vX.Y.Z"` de su .c y en la tabla de `VERSIONES.md`.
- **Nunca se vuelve a empezar de cero** ni se reutiliza un número. Siempre se parte del último de `VERSIONES.md`.
- Arreglo o ajuste chico: sube el último número (v0.2.4 -> v0.2.5). Comportamiento nuevo: sube el del medio (v0.2.5 -> v0.3.0).
- Una feature nueva arranca en v0.1.0. Si una feature se renombra o se junta con otra, sigue con el número más alto que tenía.
- Cada cambio de versión: actualizar el `#define VERSION`, la fila de `VERSIONES.md` (con fecha y una línea de qué cambió) y hacer un commit que diga la versión.

## Compilar e instalar
`./compilar.sh <feature>` y `./instalar.sh` (ver README.md). Los .ini de Franco no se pisan.
