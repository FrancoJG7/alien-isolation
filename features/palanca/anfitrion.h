/* Lo que se pasan el cargador (xinput_ag.dll, queda fijo mientras el juego
 * está abierto) y cada feature (un DLL en la carpeta mods_vr, se puede
 * cambiar en vivo).
 *
 * El cargador tiene los enganches al juego y a MotherVR y, desde ellos, llama
 * a las features. Una feature nunca escribe memoria del juego para
 * engancharse: se lo pide al cargador (enganchar_puntero, enganchar_codigo),
 * que deja el parche puesto para siempre y solo cambia a dónde salta. Así, al
 * recargar o borrar una feature nunca queda el juego saltando a código que ya
 * no existe.
 *
 * Las dos estructuras empiezan con su tamaño: el cargador y las features solo
 * agregan campos al final, así una feature vieja anda con un cargador nuevo y
 * una nueva ve qué le falta a un cargador viejo. Si cambia ANFITRION_VERSION
 * (algo incompatible) hay que cambiar también xinput_ag.dll, y eso sí pide
 * cerrar el juego. */
#ifndef ANFITRION_H
#define ANFITRION_H

#define ANFITRION_VERSION 2

typedef struct Anfitrion Anfitrion;

/* Cada feature recibe su propia copia. Vale mientras la feature esté cargada. */
struct Anfitrion {
    int tam;                        /* sizeof(Anfitrion) del cargador */
    int version;                    /* ANFITRION_VERSION del cargador */
    const char *nombre;             /* nombre de la feature (el del DLL, sin .dll) */
    unsigned delta;                 /* rebase de AI.exe */
    const char *carpeta;            /* carpeta del juego, con barra al final */
    const char *ini;                /* ruta de mods_vr\<nombre>.ini */
    int recarga;                    /* 0 la primera vez, N en la recarga N */
    int enganche_bucle;             /* 1 = tick corre en cada cuadro */

    /* Escribe en ai_mods.log con el nombre de la feature adelante. */
    void (*log)(const Anfitrion *yo, const char *fmt, ...);
    /* IVRSystem_017 de OpenVR (lo busca el cargador), o NULL si todavía no está. */
    void *(*openvr)(void);
    /* 1 = las poses de MotherVR están enganchadas (Feature.pose se llama). */
    int (*estado_mvr)(void);

    /* Baúl: un bloque de memoria por feature que sobrevive a las recargas.
     * La feature elige el número de versión; si cambia la estructura que
     * guarda, sube el número y el bloque viejo se ignora.
     * baul_leer devuelve 1 si había un bloque con esa versión y ese tamaño. */
    int (*baul_leer)(const Anfitrion *yo, int version, void *dst, int tam);
    void (*baul_guardar)(const Anfitrion *yo, int version, const void *src, int tam);

    /* Enganches a pedido. Devuelven a dónde llamar para seguir con lo original
     * (NULL si no se pudo; el motivo va al log). Se piden en mod_iniciar o en
     * tick (el hilo del juego). La función tiene que tener la misma convención
     * de llamada que la original. Al descargar la feature, el cargador vuelve
     * todo a lo original solo.
     * - enganchar_puntero: una entrada de vtable o de la tabla de importación.
     * - enganchar_codigo: el comienzo de una función, en la dirección ya
     *   rebasada. "firma" son los bytes esperados ahí, en hexa; "bytes" cuántos
     *   se mueven al trampolín (5 o más, instrucciones enteras y sin saltos
     *   relativos: sacarlo de Ghidra). */
    void *(*enganchar_puntero)(const Anfitrion *yo, void **slot, void *funcion);
    void *(*enganchar_codigo)(const Anfitrion *yo, void *dir, const char *firma, int bytes, void *funcion);
};

/* La completa la feature en mod_iniciar. Lo que no usa queda en NULL. */
typedef struct {
    int tam;                        /* sizeof(Feature) de la feature */
    int prioridad;                  /* menor = primero (tick y pose en cadena) */
    /* Hilo del juego, una vez por cuadro. dt en segundos. */
    void (*tick)(float dt);
    /* Pose de MotherVR (cabeza = 1, cuerpo = 0), desde el hilo que dibuja.
     * out es la matriz 3x4 que ya corrigieron las features de menor prioridad. */
    void (*pose)(int cabeza, void *self, float *out, float escala);
    /* Cambió mods_vr\<nombre>.ini (el cargador lo mira cada segundo). */
    void (*config)(void);
    /* Tecla de pánico: 1 = todo apagado, 0 = de nuevo prendido. */
    void (*pausa)(int pausado);
    /* El juego mandó vibración (XInputSetState). */
    void (*vibracion_juego)(unsigned mando, const void *vibracion);
    /* Antes de descargarla. Acá se guarda el baúl y se cierran hilos propios. */
    void (*terminar)(void);
} Feature;

/* Única exportación obligatoria de cada feature (cdecl).
 * Devuelve ANFITRION_VERSION; si no coincide, el cargador no la usa. */
typedef int (*FnModIniciar)(const Anfitrion *a, Feature *f);

#endif
