# Handoff: arreglar cosas de MotherVR (Alien: Isolation en VR)

> **Para la próxima IA:** este archivo tiene que alcanzar solo, sin el historial del chat. Resume una conversación del 25/10/2026 en el proyecto "Doorkickers", donde Franco preguntó si podía aplicar a Alien: Isolation lo que veníamos haciendo con Door Kickers 1. Todavía **no se analizó ningún binario**: todo lo que sigue es evaluación y plan.

---

## 1. Objetivo de Franco

- Quiere jugar Alien: Isolation completo en VR con el mod **MotherVR** y "sentirse realmente en la nave".
- MotherVR tiene detalles que molestan y el desarrollador tarda en arreglarlos. Franco no quiere mandarle cada cosa chica al dev.
- Plan acordado: **decompilar el `dxgi.dll` de MotherVR (y lo necesario del exe del juego) y parchearlo para uso personal**, en su compu. **No lo va a distribuir.**
- No busca reconstruir el código fuente completo del juego. Busca arreglos puntuales.

## 2. Bugs que reportó (los primeros candidatos)

1. **Muerte por la ventilación del techo:** cuando el alien te mata desde un ducto, la cámara no queda alineada con la del personaje. No quedás debajo de la ventilación y no ves bien al alien matarte.
   - *Hipótesis (sin verificar):* la animación de muerte coloca al personaje exacto debajo del ducto, pero MotherVR le suma el desplazamiento real del visor (lo que Franco se movió en su cuarto respecto del centro calibrado). Arreglo probable: ignorar la posición del visor (o recentrarla) mientras está activa una cámara de animación o cinemática, y dejar solo la rotación, o ni eso.
2. **Agacharse bajo una mesa:** a veces parece que no funciona y el alien te ve aunque estés bajo la mesa, como si estuvieras parado.
   - *Hipótesis (sin verificar):* la detección del alien usa el estado del personaje del juego (parado o agachado según el botón) y la posición del personaje, no la cabeza real. Si Franco se agacha físicamente sin apretar el botón, el juego lo tiene parado. Y si aprieta el botón pero asoma la cabeza, ve por fuera de la mesa aunque el juego lo tenga escondido. Arreglos posibles: pasar a agachado automáticamente cuando la altura real de la cabeza baja de un umbral, o limitar cuánto puede alejarse la cámara del personaje.
3. Franco dice que hay "muchas cosas puntuales" más. Pedirle la lista cuando haga falta.

Mientras tanto se le recomendó recalibrar seguido (Ctrl + Alt en teclado, LB + RB en joystick de Xbox) y usar siempre el botón de agacharse.

## 3. Lo que se averiguó

### MotherVR
- Autor: **Nibre**. Repo `github.com/Nibre/MotherVR`: solo un README (4 commits, el último de 2018). **Sin código fuente ni licencia** → es de código cerrado.
- Se instala copiando un **`dxgi.dll`** en la carpeta del juego (proxy de DXGI que se carga con el juego y engancha el motor desde adentro). Se activa en Opciones → MotherVR dentro del juego.
- No se sabe todavía: qué SDK de VR usa (OpenVR/OpenXR/Oculus), si el DLL está empaquetado u ofuscado, si es de 32 o 64 bits (el juego es de 32, así que el DLL debería serlo).

### El juego (versión PC de Steam)
- Exe principal: `AI.exe`, **x86 de 32 bits**, compilado con Visual Studio 2010 SP1, flag `/LARGEADDRESSAWARE`. Igual que DK en lo básico.
- Middleware: Wwise 2013 (audio), Scaleform (UI), CRI (video). Lógica de niveles en el sistema de scripting **CATHODE** (archivos binarios, no XML).
- No se verificó si el exe de Steam tiene protección (SteamStub u otra). Mirarlo apenas se tenga el archivo.

### OpenIsolation (`github.com/OpenIsolation/OpenIsolation`)
- Intento de reconstruir el código fuente del juego. Un solo autor (Ryan J. Gray), último commit enero de 2025, 69 commits.
- 749 archivos pero solo 91 con contenido (unas 3.800 líneas). **No compila nada jugable ni engancha el exe.** No sirve como base de código.
- **Sí sirve como referencia:** la versión de iOS salió con todos los símbolos de debug, y sus notas (`Documentation/`) tienen la estructura de carpetas y los nombres reales de clases (cámara, IA, behavior trees, pathfinding, CATHODE). Útil para orientarse y nombrar funciones en Ghidra sobre el exe de PC.

### OpenCAGE (de Matt Filer, `github.com/MattFiler/OpenCAGE`)
- Kit de modding de Alien: Isolation: edita niveles, scripts CATHODE, parámetros de la IA, ítems, texturas, sin tocar el exe. Puede servir si algún arreglo se puede hacer con datos en vez de parches.

## 4. Plan de trabajo propuesto

1. **Pedirle a Franco** que suba (en .zip si la app no deja otra cosa):
   - El `dxgi.dll` de MotherVR que usa hoy (anotar versión y de dónde lo bajó).
   - `AI.exe` de la carpeta del juego en Steam.
2. **Diagnóstico de los binarios:** arquitectura, secciones, si están empaquetados, imports (qué SDK de VR), exports de proxy DXGI, protección del exe. Anotar MD5 de los originales.
3. **Encontrar los hooks de MotherVR en el juego:** qué direcciones de `AI.exe` parchea o engancha el DLL (buscar en el DLL constantes con direcciones del exe, patrones de bytes que busca, `VirtualProtect`, `WriteProcessMemory`, MinHook/Detours, etc.). Ahí se va a ver dónde aplica la pose del visor a la cámara.
4. **Atacar primero el bug de la cámara en las muertes** (apuesta: el más acotado). Encontrar cómo el juego indica que hay una cámara de animación activa y hacer que MotherVR deje de sumar la posición del visor en ese caso.
5. Después, agacharse bajo la mesa.

## 5. Cómo se trabajó en Door Kickers (reutilizar el flujo)

- **Ghidra headless propio** para decompilar (no pedirle a Franco que pegue decompilados): `analyzeHeadless <proj_dir> X -import <binario> -postScript Dec.java <direcciones...>`, con un script chico (DecompInterface + `decompileFunction`) que escribe el decompilado a un txt.
- **Parches en asm con nasm** (`bits 32`, binario plano) metidos en una sección nueva o en cuevas del binario. Cada hook: `E9 rel32` + NOPs, el código nuevo re-ejecuta las instrucciones pisadas y vuelve con `jmp`.
- **Un script de Python que parchea desde el original:** valida el MD5 del original, verifica los bytes originales de cada sitio antes de escribir y aborta si algo no coincide. Así, cuando salga una versión nueva de MotherVR, se sabe enseguida qué hooks dejaron de calzar. **Acá es todavía más importante**, porque cada actualización de MotherVR pisa los cambios.
- **Emuladores con unicorn en Python:** cargan el binario parcheado, arman objetos falsos en un heap y corren el código real entre dos direcciones, comparando con lo esperado. Sirven para la lógica (por ejemplo, "si la cámara de animación está activa, el desplazamiento del visor es cero"). **No prueban la experiencia en VR:** eso solo lo puede probar Franco con el visor.
- Cuidado con `.reloc` si el DLL tiene ASLR: revisar que ninguna relocación caiga sobre bytes cambiados (en DK se apagó ASLR en el exe; en un DLL no se puede apagar igual, el código nuevo tiene que ser relativo o usar relocaciones).
- Entregar siempre en un **.zip** con el binario parcheado, los fuentes, el script de parcheo y un `LEEME.txt` en español ("Qué trae / Instalar / Cómo se usa / Volver atrás / Sabido").

## 6. Cómo trabajar con Franco

- **Idioma:** español rioplatense, tono cercano y simple ("probá", "fijate", "copiá"). Frases completas, sin jerga de assembly. Respuestas cortas: va directo a probar.
- **Nivel:** principiante en ingeniería inversa, no sabe assembly. Usó Ghidra por primera vez con DK. Sigue bien instrucciones numeradas y cortas, de a un paso.
- **División del trabajo:** la IA hace toda la parte difícil (análisis, asm, scripts, verificación) y entrega un zip listo. Franco instala, prueba y reporta en pocas palabras, a veces con errores de tipeo. A veces se corrige en el mensaje siguiente: leer todo antes de actuar.
- **Bugs:** describe lo que ve. Si no se puede reproducir, hacerle 2 o 3 preguntas concretas que separen hipótesis. A veces manda capturas o videos.
- **Decisiones de diseño:** dárselas con 2 a 4 opciones y una recomendada. Cuando decide, no re-discutir.
- **Windows + Steam.** Decir siempre "abrí **PowerShell**". Tiene Python instalado.
- **Commits:** lleva un repositorio propio y pide título + descripción en español, asunto corto y cuerpo breve, terminando con "Sin probar en el juego." si corresponde.
- **No decir "probado"** de algo que solo se verificó en emulador.
- **Siempre construir sobre el último build** para no perder arreglos anteriores.
