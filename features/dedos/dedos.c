/* dedos.dll (en mods_vr): las manos de Ripley se cierran cuando apretás el
 * gatillo o el agarre del mando, y se abren al soltarlo.
 *
 * Cómo: las manos que se ven las pone GRAND moviendo el esqueleto de Ripley
 * (hkaPose de Havok en [pj+0x63c]). Los dedos son los huesos que cuelgan de
 * cada mano (GRAND usa 0x20 y 0x30 como manos y guarda 0x21..0x2f como los 15
 * huesos de los dedos de una). En cada cuadro, justo antes de que corra el
 * paso de IK del jugador (0x6a55e0, que salta a 0x6a5040, donde engancha
 * GRAND), giramos cada hueso de dedo en su espacio local un ángulo según
 * cuánto apretás. Usamos el mismo acceso que GRAND (0xbba6b0, local
 * writable), así Havok recalcula solo los huesos de abajo.
 *
 * El paso de IK lo corre el sistema de trabajos del juego (otros hilos):
 * el enganche solo copia lo que preparó tick (hilo del juego) y no anota en
 * el log. Los nombres de los huesos se leen en tick y van al log una vez,
 * para saber cuáles son los dedos y cuál es el pulgar.
 *
 * Nada de esto está probado en el juego: el eje en el que se dobla cada dedo
 * no se sabe todavía (EjeDedos / EjePulgar en el .ini, se cambian en vivo). */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "anfitrion.h"
#include "vibrar.h"

#ifndef VERSION
#define VERSION "dedos v0.1.1"
#endif

#define DIR_MANAGER_PJ      0x016f0c88u
#define DIR_PASO_IK         0x006a55e0u   /* trabajo por personaje: ecx = arg+0x3f0, jmp 0x6a5040 */
#define FIRMA_PASO_IK       "8B4C240481C1F0030000"
#define BYTES_PASO_IK       10
#define DIR_HUESO_LOCAL     0x00bba6b0u   /* hkaPose::accessBoneLocalSpace (thiscall, hueso) */

#define OFS_POSE            0x63c         /* pj + 0x3f0 + 0x24c, como GRAND */
#define OFS_CONTROL         0x64
#define OFS_ESTADO_31C      0x31c

#define SLOT_ESTADO_MANDO   33
#define MAX_HUESOS_DEDO     20            /* por mano */
#define MAX_HUESOS          512

typedef void *(__attribute__((thiscall)) *FnHuesoLocal)(void *pose, int hueso);
typedef void (__cdecl *FnPasoIK)(void *personaje);
typedef char (__attribute__((thiscall)) *FnEstadoMando)(void *sys, unsigned indice, void *estado, unsigned tam);

typedef struct {
    unsigned paquete;
    unsigned relleno;
    unsigned long long apretados;
    unsigned long long tocados;
    struct { float x, y; } eje[5];
} EstadoMando;                      /* VRControllerState_t, 64 bytes en Windows */

static const Anfitrion *g_host;
static unsigned g_delta;
static volatile int g_pausado;
static FnPasoIK g_paso_original;
static FnHuesoLocal g_hueso_local;

/* Configuración */
static int g_activado = 1;
static int g_boton = 2;             /* 0 gatillo, 1 agarre, 2 el más apretado de los dos */
static float g_angulo = 70.0f;      /* grados por articulación con la mano cerrada */
static float g_angulo_pulgar = 35.0f;
static int g_eje = 2, g_eje_pulgar = 0;   /* 0 x, 1 y, 2 z del hueso */
static float g_signo = 1.0f, g_signo_pulgar = 1.0f;
static float g_velocidad = 12.0f;   /* qué tan rápido sigue al botón (1/s) */
static int g_hueso_mano[2] = { 0x09, 0x20 };  /* izquierda, derecha (si los huesos tienen nombre, se decide por nombre) */
static int g_solo_vacias = 1;
static int g_log = 1;

/* Lo que tick le deja al enganche. Se cambia de una vez (g_listo = 0 mientras). */
typedef struct {
    int hueso;
    int pulgar;
} Dedo;
static struct {
    volatile LONG listo;
    void *pj;
    void *pose;
    int n[2];
    Dedo d[2][MAX_HUESOS_DEDO];
    float cierre[2];                /* 0 abierta, 1 cerrada */
    int eje, eje_pulgar;
    float ang, ang_pulgar;          /* radianes, con el signo */
} g_c;

/* Por hueso: la rotación que vino de la animación y la que escribimos, para
 * no acumular si un cuadro la animación no la pisa. */
static float g_base[2][MAX_HUESOS_DEDO][4];
static float g_escrita[2][MAX_HUESOS_DEDO][4];
static int g_hay_escrita[2][MAX_HUESOS_DEDO];

/* Estado de tick */
static float g_cierre[2];
static void *g_pose_vista;          /* pose de la que ya anotamos los huesos */
static float g_t_mandos = 10.0f, g_t_log;
static unsigned g_equipado_antes = 0xffffffffu;
static int g_libre;

#define logf_(...) do { if (g_log && g_host) g_host->log(g_host, __VA_ARGS__); } while (0)

static void *leer_ptr(const void *dir)
{
    if (!dir || IsBadReadPtr(dir, 4))
        return NULL;
    return *(void *const *)dir;
}

static unsigned char *jugador(void)
{
    unsigned char *m = leer_ptr((void *)(UINT_PTR)(DIR_MANAGER_PJ + g_delta)), *lista;
    if (!m)
        return NULL;
    m = leer_ptr(m + 8);
    if (!m || IsBadReadPtr(m, 0x88) || *(unsigned *)(m + 0x84) == 0)
        return NULL;
    lista = leer_ptr(m + 0x80);
    return lista ? leer_ptr(lista) : NULL;
}

/* --- Esqueleto --- */
typedef struct {
    int n;
    const short *padres;
    const unsigned char *huesos;    /* hkaBone: nombre (hkStringPtr) + bool, 8 bytes */
} Esqueleto;

static int esqueleto(void *pose, Esqueleto *e)
{
    unsigned char *s = leer_ptr(pose);
    memset(e, 0, sizeof *e);
    if (!s || IsBadReadPtr(s, 0x20))
        return 0;
    e->padres = leer_ptr(s + 0xc);
    e->huesos = leer_ptr(s + 0x18);
    e->n = *(int *)(s + 0x1c);
    if (e->n <= 0 || e->n > MAX_HUESOS || !e->padres || !e->huesos ||
        IsBadReadPtr(e->padres, e->n * 2) || IsBadReadPtr(e->huesos, e->n * 8))
        return 0;
    return 1;
}

static const char *nombre_hueso(const Esqueleto *e, int i)
{
    const char *p;
    if (i < 0 || i >= e->n)
        return "?";
    p = (const char *)((UINT_PTR)leer_ptr(e->huesos + i * 8) & ~(UINT_PTR)1);  /* hkStringPtr usa el bit 0 */
    if (!p || IsBadReadPtr(p, 1))
        return "?";
    return p;
}

static int contiene(const char *s, const char *sub)
{
    size_t n = strlen(sub);
    for (; *s; s++) {
        size_t i;
        for (i = 0; i < n && s[i] && tolower((unsigned char)s[i]) == sub[i]; i++)
            ;
        if (i == n)
            return 1;
    }
    return 0;
}

static int es_dedo(const char *nombre)
{
    static const char *pal[] = { "finger", "thumb", "index", "middle", "ring", "pinky", "little", "dedo" };
    size_t i;
    for (i = 0; i < sizeof pal / sizeof pal[0]; i++)
        if (contiene(nombre, pal[i]))
            return 1;
    return 0;
}

/* 1 si "hueso" cuelga de "mano" a 4 niveles o menos. */
static int cuelga_de(const Esqueleto *e, int hueso, int mano)
{
    int k, p = hueso;
    for (k = 0; k < 4; k++) {
        if (p < 0 || p >= e->n)
            return 0;
        p = e->padres[p];
        if (p == mano)
            return 1;
    }
    return 0;
}

/* 1 si el hueso es parte de una cadena de dedo: o su padre no es la mano
 * (segunda o tercera falange), o es hijo directo de la mano y tiene a su vez
 * un hijo (primera falange). Deja afuera huesos sueltos colgados de la mano,
 * como el punto donde se agarran los objetos (0x30 en Ripley). */
static int en_cadena(const Esqueleto *e, int hueso, int mano)
{
    int i;
    if (e->padres[hueso] != mano)
        return 1;
    for (i = 0; i < e->n; i++)
        if (e->padres[i] == hueso)
            return 1;
    return 0;
}

/* 0 izquierda, 1 derecha, -1 no se sabe por el nombre. */
static int lado(const char *nombre)
{
    char c0 = (char)tolower((unsigned char)nombre[0]);
    if (contiene(nombre, "left") || ((c0 == 'l') && (nombre[1] == '_' || nombre[1] == ' ')))
        return 0;
    if (contiene(nombre, "right") || ((c0 == 'r') && (nombre[1] == '_' || nombre[1] == ' ')))
        return 1;
    return -1;
}

/* Arma la lista de dedos de cada mano. Una vez por pose nueva, anota todo
 * el esqueleto en el log. */
static void preparar_dedos(void *pose, int nuevos[2], Dedo d[2][MAX_HUESOS_DEDO], int anotar)
{
    Esqueleto e;
    int m, i, mano_de[2], por_nombre;
    nuevos[0] = nuevos[1] = 0;
    if (!esqueleto(pose, &e))
        return;
    if (anotar) {
        logf_("esqueleto de Ripley: %d huesos", e.n);
        for (i = 0; i < e.n; i++)
            logf_("  hueso %3d (0x%02x) %-28s padre %d", i, i, nombre_hueso(&e, i), e.padres[i]);
    }
    /* Qué hueso es la mano izquierda (0) y cuál la derecha (1). */
    mano_de[0] = g_hueso_mano[0];
    mano_de[1] = g_hueso_mano[1];
    if (lado(nombre_hueso(&e, mano_de[0])) == 1 || lado(nombre_hueso(&e, mano_de[1])) == 0) {
        int t = mano_de[0];
        mano_de[0] = mano_de[1];
        mano_de[1] = t;
    }
    for (m = 0; m < 2; m++) {
        int h = mano_de[m];
        if (h <= 0 || h >= e.n)
            continue;
        /* Si algún hijo tiene nombre de dedo, solo esos; si no, todos los que cuelgan. */
        por_nombre = 0;
        for (i = 0; i < e.n; i++)
            if (cuelga_de(&e, i, h) && es_dedo(nombre_hueso(&e, i)))
                por_nombre = 1;
        for (i = 0; i < e.n && nuevos[m] < MAX_HUESOS_DEDO; i++) {
            const char *nb = nombre_hueso(&e, i);
            if (!cuelga_de(&e, i, h))
                continue;
            if (por_nombre ? !es_dedo(nb) : (!en_cadena(&e, i, h) || contiene(nb, "weapon") || contiene(nb, "prop") || contiene(nb, "attach")))
                continue;
            d[m][nuevos[m]].hueso = i;
            d[m][nuevos[m]].pulgar = contiene(nb, "thumb");
            nuevos[m]++;
        }
        if (anotar) {
            char buf[512];
            int k, o = 0;
            buf[0] = 0;
            for (k = 0; k < nuevos[m] && o < (int)sizeof buf - 48; k++)
                o += snprintf(buf + o, sizeof buf - o, " %s%s", nombre_hueso(&e, d[m][k].hueso), d[m][k].pulgar ? "(pulgar)" : "");
            logf_("mano %s = hueso 0x%02x %s, %d dedos%s:%s", m ? "derecha" : "izquierda", h, nombre_hueso(&e, h),
                  nuevos[m], por_nombre ? " (por nombre)" : " (todo lo que cuelga)", buf);
        }
    }
}

/* --- Cuaterniones (x, y, z, w) --- */
static void q_mul(const float a[4], const float b[4], float r[4])
{
    float x = a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1];
    float y = a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0];
    float z = a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3];
    float w = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];
    r[0] = x; r[1] = y; r[2] = z; r[3] = w;
}

/* --- Enganche (hilos de trabajo del juego) --- */
static void __cdecl paso_ik(void *personaje)
{
    if (g_c.listo && !g_pausado && g_hueso_local && personaje == g_c.pj && g_c.pose &&
        leer_ptr((unsigned char *)personaje + OFS_POSE) == g_c.pose) {
        int m, k;
        for (m = 0; m < 2; m++) {
            float c = g_c.cierre[m];
            for (k = 0; k < g_c.n[m]; k++) {
                const Dedo *d = &g_c.d[m][k];
                float *t = g_hueso_local(g_c.pose, d->hueso), *q, r[4] = { 0, 0, 0, 1 }, s;
                int eje = d->pulgar ? g_c.eje_pulgar : g_c.eje;
                float ang = (d->pulgar ? g_c.ang_pulgar : g_c.ang) * c;
                if (!t)
                    continue;
                q = t + 4;              /* hkQsTransform: traslación, rotación, escala */
                if (!g_hay_escrita[m][k] || memcmp(q, g_escrita[m][k], 16) != 0)
                    memcpy(g_base[m][k], q, 16);    /* la animación la puso de nuevo */
                if (c < 0.001f) {
                    memcpy(q, g_base[m][k], 16);
                    g_hay_escrita[m][k] = 0;
                    continue;
                }
                s = sinf(ang * 0.5f);
                r[eje] = s;
                r[3] = cosf(ang * 0.5f);
                q_mul(g_base[m][k], r, q);
                memcpy(g_escrita[m][k], q, 16);
                g_hay_escrita[m][k] = 1;
            }
        }
    }
    g_paso_original(personaje);
}

/* --- tick (hilo del juego) --- */
static float leer_cierre(void *sys, int mano)
{
    EstadoMando e;
    int k = vibrar_indice(mano);
    float gat, agarre;
    if (!sys || k <= 0)
        return 0.0f;
    memset(&e, 0, sizeof e);
    if (!((FnEstadoMando)(*(void ***)sys)[SLOT_ESTADO_MANDO])(sys, (unsigned)k, &e, sizeof e))
        return 0.0f;
    gat = e.eje[1].x;
    agarre = e.eje[2].x;            /* en los Touch el agarre es analógico (eje 2) */
    if (e.apretados & BOTON_AGARRE && agarre < 1.0f)
        agarre = 1.0f;
    if (!(gat >= 0.0f && gat <= 1.0f))
        gat = 0.0f;
    if (!(agarre >= 0.0f && agarre <= 1.0f))
        agarre = 0.0f;
    if (g_boton == 0)
        return gat;
    if (g_boton == 1)
        return agarre;
    return gat > agarre ? gat : agarre;
}

static unsigned equipado(unsigned char *pj)
{
    unsigned char *o = leer_ptr(pj + OFS_ESTADO_31C), *g;
    if (!o || IsBadReadPtr(o + 0x3c, 4))
        return 0xfffffffeu;
    g = leer_ptr(o + 0x3c);         /* gestor de equipo: [[pj+0x31c]+0x3c] */
    if (!g || IsBadReadPtr(g + 0x3bc8, 4))
        return 0xfffffffeu;
    return *(unsigned *)(g + 0x3bc8);
}

static void f_tick(float dt)
{
    void *sys = g_host->openvr();
    unsigned char *pj, *ctl;
    void *pose;
    int m, nuevos[2];
    static Dedo d[2][MAX_HUESOS_DEDO];
    unsigned eq;
    int con_arma;

    if (dt > 0.1f)
        dt = 0.1f;
    g_t_mandos += dt;
    if (g_t_mandos > 2.0f) {
        g_t_mandos = 0.0f;
        vibrar_buscar_mandos(sys);
    }
    pj = jugador();
    if (!g_activado || g_pausado || !pj || IsBadReadPtr(pj, 0x700)) {
        g_c.listo = 0;
        g_cierre[0] = g_cierre[1] = 0.0f;
        return;
    }
    pose = leer_ptr(pj + OFS_POSE);
    if (!pose) {
        g_c.listo = 0;
        return;
    }
    ctl = leer_ptr(pj + OFS_CONTROL);
    g_libre = ctl && !IsBadReadPtr(ctl + 0x1f, 1) && ctl[0x1f];

    eq = equipado(pj);
    if (eq != g_equipado_antes) {
        logf_("objeto en la mano: %08x%s", eq, eq == 0 ? " (nada)" : eq >= 0xfffffffeu ? " (no se pudo leer)" : "");
        g_equipado_antes = eq;
    }
    con_arma = g_solo_vacias && eq != 0 && eq < 0xfffffffeu;

    for (m = 0; m < 2; m++) {
        float objetivo = (g_libre && !con_arma) ? leer_cierre(sys, m) : 0.0f;
        float a = g_velocidad * dt;
        if (a > 1.0f)
            a = 1.0f;
        g_cierre[m] += (objetivo - g_cierre[m]) * a;
        if (g_cierre[m] < 0.0f)
            g_cierre[m] = 0.0f;
    }

    if (pose != g_pose_vista || g_c.pose != pose) {
        g_c.listo = 0;
        preparar_dedos(pose, nuevos, d, pose != g_pose_vista);
        g_pose_vista = pose;
        memcpy(g_c.d, d, sizeof d);
        g_c.n[0] = nuevos[0];
        g_c.n[1] = nuevos[1];
        g_c.pj = pj;
        g_c.pose = pose;
        memset(g_hay_escrita, 0, sizeof g_hay_escrita);
    }
    g_c.cierre[0] = g_cierre[0];
    g_c.cierre[1] = g_cierre[1];
    g_c.eje = g_eje;
    g_c.eje_pulgar = g_eje_pulgar;
    g_c.ang = g_signo * g_angulo * 3.14159265f / 180.0f;
    g_c.ang_pulgar = g_signo_pulgar * g_angulo_pulgar * 3.14159265f / 180.0f;
    g_c.listo = g_paso_original != NULL && (g_c.n[0] || g_c.n[1]);

    g_t_log += dt;
    if (g_t_log > 5.0f && (g_cierre[0] > 0.05f || g_cierre[1] > 0.05f)) {
        g_t_log = 0.0f;
        logf_("cierre izq %.2f der %.2f", g_cierre[0], g_cierre[1]);
    }
}

/* --- .ini --- */
static int leer_eje(const char *ruta, const char *clave, const char *def, float *signo)
{
    char buf[16], *p = buf;
    GetPrivateProfileStringA("Dedos", clave, def, buf, sizeof buf, ruta);
    *signo = 1.0f;
    if (*p == '-') {
        *signo = -1.0f;
        p++;
    } else if (*p == '+')
        p++;
    switch (tolower((unsigned char)*p)) {
    case 'x': return 0;
    case 'y': return 1;
    default: return 2;
    }
}

static float leer_float(const char *ruta, const char *clave, float def, float min, float max)
{
    char buf[32], d[32];
    float v;
    snprintf(d, sizeof d, "%g", def);
    GetPrivateProfileStringA("Dedos", clave, d, buf, sizeof buf, ruta);
    v = (float)atof(buf);
    return v >= min && v <= max ? v : def;
}

static void leer_ini(void)
{
    const char *ruta = g_host->ini;
    g_activado = GetPrivateProfileIntA("Dedos", "Activado", 1, ruta);
    g_boton = GetPrivateProfileIntA("Dedos", "Boton", 2, ruta);
    g_angulo = leer_float(ruta, "Angulo", 70.0f, 0.0f, 120.0f);
    g_angulo_pulgar = leer_float(ruta, "AnguloPulgar", 35.0f, 0.0f, 120.0f);
    g_eje = leer_eje(ruta, "EjeDedos", "-z", &g_signo);
    g_eje_pulgar = leer_eje(ruta, "EjePulgar", "x", &g_signo_pulgar);
    g_velocidad = leer_float(ruta, "Velocidad", 12.0f, 1.0f, 60.0f);
    g_hueso_mano[0] = GetPrivateProfileIntA("Dedos", "HuesoManoA", 0x09, ruta);
    g_hueso_mano[1] = GetPrivateProfileIntA("Dedos", "HuesoManoB", 0x20, ruta);
    g_solo_vacias = GetPrivateProfileIntA("Dedos", "SoloManosVacias", 1, ruta);
    g_log = GetPrivateProfileIntA("Dedos", "Log", 1, ruta);
}

static void f_config(void)
{
    static const char ejes[] = "xyz";
    leer_ini();
    g_pose_vista = NULL;            /* volver a armar (y anotar) los dedos */
    logf_("ini: Activado=%d Boton=%d Angulo=%.0f AnguloPulgar=%.0f EjeDedos=%s%c EjePulgar=%s%c Velocidad=%.0f manos 0x%02x/0x%02x SoloManosVacias=%d",
          g_activado, g_boton, g_angulo, g_angulo_pulgar, g_signo < 0 ? "-" : "", ejes[g_eje],
          g_signo_pulgar < 0 ? "-" : "", ejes[g_eje_pulgar], g_velocidad, g_hueso_mano[0], g_hueso_mano[1], g_solo_vacias);
}

static void f_pausa(int pausado)
{
    g_pausado = pausado;
}

static void f_terminar(void)
{
    g_c.listo = 0;
    Sleep(50);                      /* que termine un paso de IK que estuviera adentro */
    g_host = NULL;
}

__declspec(dllexport) int mod_iniciar(const Anfitrion *a, Feature *f)
{
    if (!a || a->version != ANFITRION_VERSION || a->tam < (int)sizeof(Anfitrion))
        return ANFITRION_VERSION;
    g_host = a;
    g_delta = a->delta;
    logf_(VERSION " (recarga %d)", a->recarga);
    f_config();
    g_hueso_local = (FnHuesoLocal)(UINT_PTR)(DIR_HUESO_LOCAL + g_delta);
    g_paso_original = (FnPasoIK)a->enganchar_codigo(a, (void *)(UINT_PTR)(DIR_PASO_IK + g_delta),
                                                    FIRMA_PASO_IK, BYTES_PASO_IK, (void *)paso_ik);
    if (!g_paso_original)
        logf_("no pude enganchar el paso de IK del jugador: los dedos no se van a mover");
    f->tam = sizeof *f;
    f->prioridad = 60;
    f->tick = f_tick;
    f->config = f_config;
    f->pausa = f_pausa;
    f->terminar = f_terminar;
    return ANFITRION_VERSION;
}
