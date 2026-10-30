/* xinput_ag.dll: el cargador. GRAND (XINPUT1_3.dll en la carpeta del juego)
 * carga este DLL en lugar del XInput de Windows.
 *
 * Hace lo que tiene que quedar fijo mientras el juego está abierto:
 * - reenvía todo al XInput de Windows;
 * - se engancha al bucle del juego (PeekMessageA) y a MotherVR (pose del
 *   visor);
 * - carga cada feature de la carpeta mods_vr (un DLL por feature) y desde los
 *   enganches las llama;
 * - engancha a pedido otras funciones para las features (ver anfitrion.h);
 * - busca OpenVR, guarda el baúl de cada feature, tiene la tecla de pánico y
 *   escribe ai_mods.log.
 *
 * Para cambiar una feature sin salir del juego: copiar el DLL nuevo encima del
 * que está en mods_vr. A los 2 segundos el cargador lo toma solo (o con
 * Ctrl + F11, que recarga todas). Para apagarla, sacarla de la carpeta. El
 * cargador nunca usa esos archivos directamente: carga una copia en la
 * carpeta temporal de Windows, así quedan libres para pisarlos.
 * Mientras cambia una feature, los enganches siguen puestos y solo dejan de
 * llamarla (un cerrojo evita descargarla mientras otro hilo la está usando, y
 * la copia vieja se libera recién 2 segundos después).
 *
 * Direcciones para AI.exe de Steam (crc32 0x17c2184b), sin rebasar. */
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include "diferidos.h"
#include "tecla.h"
#include "anfitrion.h"

#define VERSION "cargador v0.2.0"
#define BASE_EXE            0x00400000u
#define CARPETA_MODS        "mods_vr"

typedef DWORD (WINAPI *FnGetState)(DWORD, void *);
typedef DWORD (WINAPI *FnSetState)(DWORD, void *);
typedef DWORD (WINAPI *FnGetCaps)(DWORD, DWORD, void *);
typedef void  (WINAPI *FnEnable)(BOOL);
typedef DWORD (WINAPI *FnGetGuids)(DWORD, GUID *, GUID *);
typedef DWORD (WINAPI *FnGetBattery)(DWORD, BYTE, void *);
typedef DWORD (WINAPI *FnGetKeystroke)(DWORD, DWORD, void *);

static HMODULE g_real;
static FnGetState g_get_state;
static FnSetState g_set_state;
static FnGetCaps g_get_caps;
static FnEnable g_enable;
static FnGetGuids g_get_guids;
static FnGetBattery g_get_battery;
static FnGetKeystroke g_get_keystroke;

static unsigned g_delta;        /* rebase de AI.exe */
static char g_carpeta[MAX_PATH];
static char g_carpeta_mods[MAX_PATH];
static FILE *g_flog;
static int g_log = 1;
static CRITICAL_SECTION g_cs_log;
static int g_enganche;          /* bucle: 0 sin intentar, 1 ok, <0 motivo de falla */
static DWORD g_ultimo_resultado[4] = {0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu};
static volatile LONG g_panico;  /* 1 = todo apagado con la tecla */

static void escribir_log(const char *prefijo, const char *fmt, va_list ap)
{
    SYSTEMTIME t;
    if (!g_log)
        return;
    EnterCriticalSection(&g_cs_log);
    if (!g_flog) {
        char ruta[MAX_PATH];
        snprintf(ruta, sizeof ruta, "%sai_mods.log", g_carpeta);
        g_flog = fopen(ruta, "w");
    }
    if (g_flog) {
        GetLocalTime(&t);
        fprintf(g_flog, "%02d:%02d:%02d.%03d ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
        if (prefijo)
            fprintf(g_flog, "[%s] ", prefijo);
        vfprintf(g_flog, fmt, ap);
        fputc('\n', g_flog);
        fflush(g_flog);
    }
    LeaveCriticalSection(&g_cs_log);
}

static void logf_(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    escribir_log(NULL, fmt, ap);
    va_end(ap);
}

static void log_feature(const Anfitrion *yo, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    escribir_log(yo ? yo->nombre : "?", fmt, ap);
    va_end(ap);
}

static void cargar_xinput_real(void)
{
    char ruta[MAX_PATH];
    UINT n;
    if (g_real)
        return;
    n = GetSystemDirectoryA(ruta, MAX_PATH - 20);
    lstrcpyA(ruta + n, "\\XINPUT1_3.dll");
    g_real = LoadLibraryA(ruta);
    if (!g_real)
        return;
    g_get_state = (FnGetState)GetProcAddress(g_real, "XInputGetState");
    g_set_state = (FnSetState)GetProcAddress(g_real, "XInputSetState");
    g_get_caps = (FnGetCaps)GetProcAddress(g_real, "XInputGetCapabilities");
    g_enable = (FnEnable)GetProcAddress(g_real, "XInputEnable");
    g_get_guids = (FnGetGuids)GetProcAddress(g_real, "XInputGetDSoundAudioDeviceGuids");
    g_get_battery = (FnGetBattery)GetProcAddress(g_real, "XInputGetBatteryInformation");
    g_get_keystroke = (FnGetKeystroke)GetProcAddress(g_real, "XInputGetKeystroke");
}

/* Compara los bytes en p con "hex". */
static int bytes_iguales(const unsigned char *p, const char *hex)
{
    size_t i, n = strlen(hex) / 2;
    if (IsBadReadPtr(p, n))
        return 0;
    for (i = 0; i < n; i++) {
        unsigned v;
        if (sscanf(hex + 2 * i, "%2x", &v) != 1 || p[i] != v)
            return 0;
    }
    return 1;
}

static int coincide(unsigned dir, const char *hex)
{
    return bytes_iguales((const unsigned char *)(UINT_PTR)(dir + g_delta), hex);
}

static int cambiar_entrada(void **slot, void *nuevo, void **viejo)
{
    DWORD prot;
    if (!VirtualProtect(slot, 4, PAGE_READWRITE, &prot))
        return 0;
    if (viejo)
        *viejo = *slot;
    InterlockedExchange((LONG *)slot, (LONG)(UINT_PTR)nuevo);
    VirtualProtect(slot, 4, prot, &prot);
    return 1;
}

/* --- Las features ---
 * Una ranura por DLL de mods_vr. Las listas que recorren los enganches
 * (g_orden) se cambian con el cerrojo exclusivo; los enganches las leen con
 * el compartido, así nunca se descarga una feature mientras otro hilo (el que
 * dibuja, por MotherVR) está adentro. */
#define MAX_FEATURES 32

typedef struct {
    int usada;
    char nombre[64];
    char origen[MAX_PATH];
    char ini[MAX_PATH];
    char copia[MAX_PATH];
    WIN32_FILE_ATTRIBUTE_DATA datos;    /* del DLL que se cargó (o que falló) */
    WIN32_FILE_ATTRIBUTE_DATA visto;    /* para esperar que deje de cambiar */
    int hay_visto;
    int presente;
    FILETIME fecha_ini;
    HMODULE mod;                        /* NULL = sin cargar (falló o recién vista) */
    int cargas;
    Anfitrion host;
    union {
        Feature f;
        char relleno[512];              /* por si la feature trae una Feature más nueva */
    } u;
} Ranura;

static SRWLOCK g_cerrojo = SRWLOCK_INIT;
static Ranura g_r[MAX_FEATURES];
static int g_orden[MAX_FEATURES];       /* ranuras cargadas, por prioridad */
static int g_n_orden;

/* 1 si la feature (compilada con otra versión de anfitrion.h) trae ese campo. */
#define TIENE(r, campo) ((r)->u.f.tam >= (int)(offsetof(Feature, campo) + sizeof((r)->u.f.campo)) && (r)->u.f.campo)

static void rehacer_orden(void)
{
    int i, j, n = 0;
    for (i = 0; i < MAX_FEATURES; i++)
        if (g_r[i].usada && g_r[i].mod)
            g_orden[n++] = i;
    for (i = 1; i < n; i++)             /* pocas: inserción */
        for (j = i; j > 0 && g_r[g_orden[j]].u.f.prioridad < g_r[g_orden[j - 1]].u.f.prioridad; j--) {
            int t = g_orden[j];
            g_orden[j] = g_orden[j - 1];
            g_orden[j - 1] = t;
        }
    g_n_orden = n;
}

/* --- Baúl: un bloque por feature que sobrevive a las recargas --- */
typedef struct {
    char nombre[64];
    int version;
    int tam;
    void *datos;
} Baul;

static Baul g_baul[MAX_FEATURES * 2];
static CRITICAL_SECTION g_cs_baul;

static int host_baul_leer(const Anfitrion *yo, int version, void *dst, int tam)
{
    int i, ok = 0;
    if (!yo || !dst || tam <= 0)
        return 0;
    EnterCriticalSection(&g_cs_baul);
    for (i = 0; i < (int)(sizeof g_baul / sizeof g_baul[0]); i++)
        if (g_baul[i].datos && lstrcmpiA(g_baul[i].nombre, yo->nombre) == 0) {
            if (g_baul[i].version == version && g_baul[i].tam == tam) {
                memcpy(dst, g_baul[i].datos, tam);
                ok = 1;
            }
            break;
        }
    LeaveCriticalSection(&g_cs_baul);
    return ok;
}

static void host_baul_guardar(const Anfitrion *yo, int version, const void *src, int tam)
{
    int i, libre = -1;
    void *copia;
    if (!yo || !src || tam <= 0 || tam > 1 << 20)
        return;
    copia = malloc(tam);
    if (!copia)
        return;
    memcpy(copia, src, tam);
    EnterCriticalSection(&g_cs_baul);
    for (i = 0; i < (int)(sizeof g_baul / sizeof g_baul[0]); i++) {
        if (g_baul[i].datos && lstrcmpiA(g_baul[i].nombre, yo->nombre) == 0)
            break;
        if (!g_baul[i].datos && libre < 0)
            libre = i;
    }
    if (i == (int)(sizeof g_baul / sizeof g_baul[0]))
        i = libre;
    if (i >= 0) {
        free(g_baul[i].datos);
        lstrcpynA(g_baul[i].nombre, yo->nombre, sizeof g_baul[i].nombre);
        g_baul[i].version = version;
        g_baul[i].tam = tam;
        g_baul[i].datos = copia;
    } else {
        free(copia);
    }
    LeaveCriticalSection(&g_cs_baul);
}

/* --- Enganches a pedido ---
 * Cada enganche es un "puesto" del cargador, en una página propia:
 *   [+0]  destino: a dónde se salta ahora (la feature, o lo original)
 *   [+8]  jmp dword ptr [destino]   (6 bytes; acá apunta el juego)
 *   [+16] trampolín: los bytes originales movidos + jmp de vuelta
 * El parche al juego (la entrada de la tabla, o un jmp al comienzo de la
 * función) se pone una sola vez y no se saca nunca: para apagar la feature se
 * cambia destino a lo original. Si la misma feature lo vuelve a pedir después
 * de recargarse, reusa el puesto. */
#define MAX_PUESTOS     64
#define TAM_PUESTO      64
#define MAX_MOVER       32

typedef struct {
    int tipo;                   /* 0 libre, 1 puntero, 2 código */
    void *dir;                  /* la entrada de la tabla, o el comienzo de la función */
    void *original;             /* el puntero viejo, o el trampolín */
    unsigned char *base;        /* el puesto en g_pagina */
    char dueno[64];             /* "" = nadie: pasa derecho */
    void *funcion;
    int bytes;                  /* código: cuántos se movieron */
} Puesto;

static Puesto g_puestos[MAX_PUESTOS];
static unsigned char *g_pagina;
static CRITICAL_SECTION g_cs_puestos;

static void poner_destino(Puesto *p, void *d)
{
    InterlockedExchange((LONG *)p->base, (LONG)(UINT_PTR)d);
}

static Puesto *puesto_nuevo(int tipo, void *dir)
{
    int i;
    if (!g_pagina) {
        g_pagina = VirtualAlloc(NULL, MAX_PUESTOS * TAM_PUESTO, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (!g_pagina)
            return NULL;
    }
    for (i = 0; i < MAX_PUESTOS; i++)
        if (!g_puestos[i].tipo) {
            Puesto *p = &g_puestos[i];
            memset(p, 0, sizeof *p);
            p->tipo = tipo;
            p->dir = dir;
            p->base = g_pagina + i * TAM_PUESTO;
            p->base[8] = 0xff;          /* jmp dword ptr [base] */
            p->base[9] = 0x25;
            *(DWORD *)(p->base + 10) = (DWORD)(UINT_PTR)p->base;
            return p;
        }
    return NULL;
}

static Puesto *puesto_buscar(int tipo, void *dir)
{
    int i;
    for (i = 0; i < MAX_PUESTOS; i++)
        if (g_puestos[i].tipo == tipo && g_puestos[i].dir == dir)
            return &g_puestos[i];
    return NULL;
}

/* Le da el puesto a la feature. 0 si lo tiene otra. */
static int puesto_tomar(const Anfitrion *yo, Puesto *p, void *funcion)
{
    if (p->dueno[0] && lstrcmpiA(p->dueno, yo->nombre) != 0) {
        log_feature(yo, "no puedo enganchar %p: ya lo usa %s", p->dir, p->dueno);
        return 0;
    }
    lstrcpynA(p->dueno, yo->nombre, sizeof p->dueno);
    p->funcion = funcion;
    poner_destino(p, g_panico ? p->original : funcion);
    return 1;
}

/* 1 si "yo" es el Anfitrion de una ranura del cargador. */
static int es_nuestro(const Anfitrion *yo)
{
    return yo && (const char *)yo >= (const char *)g_r && (const char *)yo < (const char *)(g_r + MAX_FEATURES);
}

static void *host_enganchar_puntero(const Anfitrion *yo, void **slot, void *funcion)
{
    Puesto *p;
    void *r = NULL;
    if (!es_nuestro(yo))
        return NULL;
    if (!slot || !funcion) {
        log_feature(yo, "no engancho: falta la entrada o la funcion");
        return NULL;
    }
    EnterCriticalSection(&g_cs_puestos);
    p = puesto_buscar(1, slot);
    if (p) {
        if (puesto_tomar(yo, p, funcion))
            r = p->original;
    } else if (IsBadReadPtr(slot, 4) || !*slot) {
        log_feature(yo, "no puedo enganchar la entrada %p: no se puede leer", slot);
    } else if (!(p = puesto_nuevo(1, slot))) {
        log_feature(yo, "no puedo enganchar la entrada %p: no hay mas lugar", slot);
    } else {
        p->original = *slot;
        poner_destino(p, p->original);
        if (!cambiar_entrada(slot, p->base + 8, NULL)) {
            log_feature(yo, "no puedo escribir la entrada %p", slot);
            p->tipo = 0;
        } else if (puesto_tomar(yo, p, funcion)) {
            log_feature(yo, "enganchada la entrada %p", slot);
            r = p->original;
        }
    }
    LeaveCriticalSection(&g_cs_puestos);
    return r;
}

static void *host_enganchar_codigo(const Anfitrion *yo, void *dir, const char *firma, int bytes, void *funcion)
{
    Puesto *p;
    void *r = NULL;
    unsigned char *d = dir, *t;
    DWORD prot;
    int i;
    if (!es_nuestro(yo))
        return NULL;
    if (!dir || !firma || !funcion) {
        log_feature(yo, "no engancho: falta la direccion, la firma o la funcion");
        return NULL;
    }
    EnterCriticalSection(&g_cs_puestos);
    p = puesto_buscar(2, dir);
    if (p) {
        if (puesto_tomar(yo, p, funcion))
            r = p->original;
        goto fin;
    }
    if (bytes < 5 || bytes > MAX_MOVER || (int)strlen(firma) < 2 * bytes) {
        log_feature(yo, "no engancho %p: hay que mover entre 5 y %d bytes y la firma tiene que cubrirlos", dir, MAX_MOVER);
        goto fin;
    }
    if (!bytes_iguales(d, firma)) {
        if (!IsBadReadPtr(d, 1) && (d[0] == 0xe9 || d[0] == 0xe8 || d[0] == 0xeb || d[0] == 0xcc))
            log_feature(yo, "no engancho %p: ya la cambio otro (empieza con %02x)", dir, d[0]);
        else
            log_feature(yo, "no engancho %p: los bytes no son los esperados (otra version?)", dir);
        goto fin;
    }
    if (!(p = puesto_nuevo(2, dir))) {
        log_feature(yo, "no engancho %p: no hay mas lugar", dir);
        goto fin;
    }
    t = p->base + 16;
    memcpy(t, d, bytes);
    t[bytes] = 0xe9;
    *(DWORD *)(t + bytes + 1) = (DWORD)((d + bytes) - (t + bytes + 5));
    p->original = t;
    p->bytes = bytes;
    poner_destino(p, t);
    if (!VirtualProtect(d, bytes, PAGE_EXECUTE_READWRITE, &prot)) {
        log_feature(yo, "no puedo escribir en %p", dir);
        p->tipo = 0;
        goto fin;
    }
    for (i = 5; i < bytes; i++)
        d[i] = 0x90;
    *(DWORD *)(d + 1) = (DWORD)((p->base + 8) - (d + 5));
    d[0] = 0xe9;
    VirtualProtect(d, bytes, prot, &prot);
    FlushInstructionCache(GetCurrentProcess(), d, bytes);
    if (puesto_tomar(yo, p, funcion)) {
        log_feature(yo, "enganchada la funcion en %p (%d bytes)", dir, bytes);
        r = p->original;
    }
fin:
    LeaveCriticalSection(&g_cs_puestos);
    return r;
}

/* Todo lo de esa feature vuelve a lo original. */
static void soltar_puestos(const char *nombre)
{
    int i;
    EnterCriticalSection(&g_cs_puestos);
    for (i = 0; i < MAX_PUESTOS; i++) {
        Puesto *p = &g_puestos[i];
        if (p->tipo && lstrcmpiA(p->dueno, nombre) == 0) {
            poner_destino(p, p->original);
            p->dueno[0] = 0;
            p->funcion = NULL;
        }
    }
    LeaveCriticalSection(&g_cs_puestos);
}

static void puestos_panico(int apagar)
{
    int i;
    EnterCriticalSection(&g_cs_puestos);
    for (i = 0; i < MAX_PUESTOS; i++) {
        Puesto *p = &g_puestos[i];
        if (p->tipo && p->funcion)
            poner_destino(p, apagar ? p->original : p->funcion);
    }
    LeaveCriticalSection(&g_cs_puestos);
}

/* Solo si se descarga el cargador (no pasa con GRAND): deshacer los parches. */
static void deshacer_puestos(void)
{
    int i;
    DWORD prot;
    for (i = 0; i < MAX_PUESTOS; i++) {
        Puesto *p = &g_puestos[i];
        if (p->tipo == 1)
            cambiar_entrada(p->dir, p->original, NULL);
        else if (p->tipo == 2 && VirtualProtect(p->dir, p->bytes, PAGE_EXECUTE_READWRITE, &prot)) {
            memcpy(p->dir, p->original, p->bytes);
            VirtualProtect(p->dir, p->bytes, prot, &prot);
        }
    }
}

/* --- OpenVR ---
 * VR_GetGenericInterface que un módulo ya cargado (MotherVR) importa en
 * diferido desde openvr_api.dll. MotherVR no deja openvr_api.dll a la vista
 * de GetModuleHandle, pero su tabla de importación diferida tiene la
 * dirección real una vez que inicializó OpenVR. */
typedef void *(__cdecl *FnGetInterface)(const char *version, int *error);
typedef void *(__cdecl *FnFabrica)(const char *version, int *error);
typedef void *(__attribute__((thiscall)) *FnCoreInterface)(void *core, const char *version, int *error);

static void *volatile g_vrsys;
static DWORD g_ultimo_intento_vr;

static FnGetInterface buscar_en_diferidos(char *quien, int n)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
    MODULEENTRY32 me;
    uint32_t v = 0;
    if (snap == INVALID_HANDLE_VALUE)
        return NULL;
    me.dwSize = sizeof me;
    if (Module32First(snap, &me)) {
        do {
            if (IsBadReadPtr(me.modBaseAddr, me.modBaseSize))
                continue;
            v = diferido_resuelto(me.modBaseAddr, me.modBaseSize, "openvr_api.dll", "VR_GetGenericInterface");
            if (v)
                lstrcpynA(quien, me.szModule, n);
        } while (!v && Module32Next(snap, &me));
    }
    CloseHandle(snap);
    return (FnGetInterface)(UINT_PTR)v;
}

static void *conseguir_vrsys(void)
{
    static int avisado;
    char quien[MAX_PATH] = "";
    FnGetInterface get = NULL;
    HMODULE m;
    void *sys = NULL;
    int err = 0;
    const char *via = "";

    /* 1) openvr_api.dll cargado normalmente. */
    m = GetModuleHandleA("openvr_api.dll");
    if (m && (get = (FnGetInterface)GetProcAddress(m, "VR_GetGenericInterface")))
        via = "openvr_api.dll";
    /* 2) La que usa MotherVR. */
    if (!get && (get = buscar_en_diferidos(quien, sizeof quien)))
        via = quien;
    if (get) {
        err = 0;
        sys = get("IVRSystem_017", &err);
        if (sys) {
            logf_("OpenVR listo (por %s)", via);
            return sys;
        }
    }
    /* 3) El cliente de SteamVR directo (es uno solo por proceso). */
    m = GetModuleHandleA("vrclient.dll");
    if (m) {
        FnFabrica fab = (FnFabrica)GetProcAddress(m, "VRClientCoreFactory");
        int e2 = 0;
        void *core = fab ? fab("IVRClientCore_003", &e2) : NULL;
        if (core && !IsBadReadPtr(*(void **)core, 4 * 4)) {
            e2 = 0;
            sys = ((FnCoreInterface)(*(void ***)core)[3])(core, "IVRSystem_017", &e2);
            if (sys) {
                logf_("OpenVR listo (por vrclient.dll)");
                return sys;
            }
        }
        if (!avisado)
            logf_("vrclient.dll esta pero no da IVRSystem_017 (error %d)", e2);
    }
    if (!avisado) {
        avisado = 1;
        logf_("todavia no encuentro OpenVR (openvr_api.dll %s, MotherVR %s, vrclient.dll %s); sigo buscando",
              GetModuleHandleA("openvr_api.dll") ? "cargado" : "no",
              get ? "si" : "no", GetModuleHandleA("vrclient.dll") ? "cargado" : "no");
        if (get)
            logf_("  VR_GetGenericInterface respondio error %d", err);
    }
    return NULL;
}

/* Se busca desde el hilo del juego (en cada tick, cada 2 s hasta encontrarlo);
 * las features solo leen el resultado. */
static void buscar_openvr(void)
{
    DWORD ahora;
    if (g_vrsys)
        return;
    ahora = GetTickCount();
    if (g_ultimo_intento_vr && ahora - g_ultimo_intento_vr < 2000)
        return;
    g_ultimo_intento_vr = ahora;
    g_vrsys = conseguir_vrsys();
}

static void *host_openvr(void)
{
    return g_vrsys;
}

/* --- Pose de MotherVR ---
 * La altura del visor le llega al juego por dos funciones de MotherVR::OpenVR:
 * pose de la cabeza (vtable +0x14, 0x100070e0) y pose "de cuerpo" (+0x18,
 * 0x10007220). Se envuelven esas dos entradas de la vtable y se le pasa la
 * matriz que devuelven a cada feature que tenga "pose", en orden de
 * prioridad. GRAND engancha 0x100070e0 por dentro, así que sigue viendo la
 * pose real. Direcciones de MotherVR 0.8.1. */
#define MVR_RVA_VT_CABEZA   0x5ddc0
#define MVR_RVA_VT_CUERPO   0x5ddc4
#define MVR_RVA_CABEZA      0x70e0
#define MVR_RVA_CUERPO      0x7220
#define MVR_RVA_FIRMA       0x7149      /* lea esi,[ebx+10h]; push 0; push 0; push 10h; push esi; mov ecx,eax; call [edx+0Ch] */
#define MVR_FIRMA           "8d73106a006a006a10568bc8ff520c"

typedef void (__attribute__((thiscall)) *FnPoseMvr)(void *self, float *out, float escala);

static int g_mvr_estado;            /* 0 sin intentar, 1 ok, <0 motivo */
static DWORD g_mvr_intento;
static FnPoseMvr g_cabeza_real, g_cuerpo_real;
static void **g_slot_cabeza, **g_slot_cuerpo;

static void cadena_pose(int cabeza, void *self, float *out, float escala)
{
    int i;
    if (g_panico)
        return;
    AcquireSRWLockShared(&g_cerrojo);
    for (i = 0; i < g_n_orden; i++) {
        Ranura *r = &g_r[g_orden[i]];
        if (TIENE(r, pose))
            r->u.f.pose(cabeza, self, out, escala);
    }
    ReleaseSRWLockShared(&g_cerrojo);
}

static void __attribute__((thiscall)) cabeza_mvr(void *self, float *out, float escala)
{
    g_cabeza_real(self, out, escala);
    cadena_pose(1, self, out, escala);
}

static void __attribute__((thiscall)) cuerpo_mvr(void *self, float *out, float escala)
{
    g_cuerpo_real(self, out, escala);
    cadena_pose(0, self, out, escala);
}

/* Busca el dxgi.dll de MotherVR entre los módulos cargados (el de Windows
 * también se llama dxgi.dll) y engancha las dos entradas. */
static void enganchar_mvr(void)
{
    HANDLE snap;
    MODULEENTRY32 me;
    unsigned char *base = NULL;
    DWORD ahora = GetTickCount();

    if (g_mvr_estado == 1 || g_mvr_estado == -2)
        return;
    if (g_mvr_intento && ahora - g_mvr_intento < 2000)
        return;
    g_mvr_intento = ahora;
    snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
    if (snap == INVALID_HANDLE_VALUE)
        return;
    me.dwSize = sizeof me;
    if (Module32First(snap, &me)) {
        do {
            const unsigned char *b = me.modBaseAddr;
            if (lstrcmpiA(me.szModule, "dxgi.dll") != 0 || me.modBaseSize < 0x60000 ||
                IsBadReadPtr(b, 0x60000))
                continue;
            if (bytes_iguales(b + MVR_RVA_FIRMA, MVR_FIRMA) &&
                *(unsigned *)(b + MVR_RVA_VT_CABEZA) == (unsigned)(UINT_PTR)(b + MVR_RVA_CABEZA) &&
                *(unsigned *)(b + MVR_RVA_VT_CUERPO) == (unsigned)(UINT_PTR)(b + MVR_RVA_CUERPO)) {
                base = me.modBaseAddr;
                break;
            }
        } while (Module32Next(snap, &me));
    }
    CloseHandle(snap);
    if (!base) {
        if (g_mvr_estado == 0)
            logf_("pose de MotherVR: todavia no encuentro MotherVR 0.8.1; sigo buscando");
        g_mvr_estado = -1;
        return;
    }
    if (!cambiar_entrada((void **)(base + MVR_RVA_VT_CABEZA), (void *)cabeza_mvr, (void **)&g_cabeza_real) ||
        !cambiar_entrada((void **)(base + MVR_RVA_VT_CUERPO), (void *)cuerpo_mvr, (void **)&g_cuerpo_real)) {
        logf_("pose de MotherVR: no pude escribir en MotherVR");
        g_mvr_estado = -2;
        return;
    }
    g_slot_cabeza = (void **)(base + MVR_RVA_VT_CABEZA);
    g_slot_cuerpo = (void **)(base + MVR_RVA_VT_CUERPO);
    g_mvr_estado = 1;
    logf_("pose de MotherVR: enganchado (dxgi.dll en %p)", base);
}

static int host_estado_mvr(void)
{
    return g_mvr_estado == 1;
}

/* --- Cargar, recargar y descargar features --- */

/* Copias que se descargaron: se liberan 2 s después, por si algún hilo
 * todavía estaba adentro de un enganche a pedido. */
typedef struct {
    HMODULE mod;
    DWORD desde;
    char copia[MAX_PATH];
} Pendiente;

static Pendiente g_pendientes[32];

static void liberar_despues(HMODULE m, const char *copia)
{
    int i;
    for (i = 0; i < 32; i++)
        if (!g_pendientes[i].mod) {
            g_pendientes[i].mod = m;
            g_pendientes[i].desde = GetTickCount();
            lstrcpynA(g_pendientes[i].copia, copia, MAX_PATH);
            return;
        }
    FreeLibrary(m);                 /* no debería pasar: 32 descargas en 2 s */
    DeleteFileA(copia);
}

static void liberar_pendientes(int todas)
{
    int i;
    DWORD ahora = GetTickCount();
    for (i = 0; i < 32; i++)
        if (g_pendientes[i].mod && (todas || ahora - g_pendientes[i].desde > 2000)) {
            FreeLibrary(g_pendientes[i].mod);
            DeleteFileA(g_pendientes[i].copia);
            g_pendientes[i].mod = NULL;
        }
}

static void carpeta_temporal(char *dst, int n)
{
    char tmp[MAX_PATH];
    if (!GetTempPathA(sizeof tmp, tmp))
        lstrcpynA(tmp, g_carpeta, sizeof tmp);
    snprintf(dst, n, "%sai_mods_%lu\\", tmp, GetCurrentProcessId());
}

/* Copias de juegos anteriores (y las de la v0.1.11). Las que están en uso
 * (otro juego abierto) no se borran. */
static void borrar_copias_viejas(void)
{
    char tmp[MAX_PATH], patron[MAX_PATH], ruta[MAX_PATH], dir[MAX_PATH];
    WIN32_FIND_DATAA d, d2;
    HANDLE h, h2;
    if (!GetTempPathA(sizeof tmp, tmp))
        return;
    snprintf(patron, sizeof patron, "%sagachar_logica_*.dll", tmp);
    h = FindFirstFileA(patron, &d);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            snprintf(ruta, sizeof ruta, "%s%s", tmp, d.cFileName);
            DeleteFileA(ruta);
        } while (FindNextFileA(h, &d));
        FindClose(h);
    }
    snprintf(patron, sizeof patron, "%sai_mods_*", tmp);
    h = FindFirstFileA(patron, &d);
    if (h == INVALID_HANDLE_VALUE)
        return;
    do {
        if (!(d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            continue;
        snprintf(dir, sizeof dir, "%s%s\\", tmp, d.cFileName);
        snprintf(patron, sizeof patron, "%s*.dll", dir);
        h2 = FindFirstFileA(patron, &d2);
        if (h2 != INVALID_HANDLE_VALUE) {
            do {
                snprintf(ruta, sizeof ruta, "%s%s", dir, d2.cFileName);
                DeleteFileA(ruta);
            } while (FindNextFileA(h2, &d2));
            FindClose(h2);
        }
        RemoveDirectoryA(dir);
    } while (FindNextFileA(h, &d));
    FindClose(h);
}

static int mismos_datos(const WIN32_FILE_ATTRIBUTE_DATA *a, const WIN32_FILE_ATTRIBUTE_DATA *b)
{
    return CompareFileTime(&a->ftLastWriteTime, &b->ftLastWriteTime) == 0 &&
           a->nFileSizeLow == b->nFileSizeLow && a->nFileSizeHigh == b->nFileSizeHigh;
}

static int fecha_archivo(const char *ruta, FILETIME *f)
{
    WIN32_FILE_ATTRIBUTE_DATA d;
    if (!GetFileAttributesExA(ruta, GetFileExInfoStandard, &d)) {
        memset(f, 0, sizeof *f);
        return 0;
    }
    *f = d.ftLastWriteTime;
    return 1;
}

static void descargar(Ranura *r, const char *por)
{
    HMODULE m = r->mod;
    if (!m)
        return;
    /* Primero nadie más entra; recién ahí se la apaga. */
    AcquireSRWLockExclusive(&g_cerrojo);
    r->mod = NULL;
    rehacer_orden();
    ReleaseSRWLockExclusive(&g_cerrojo);
    soltar_puestos(r->nombre);
    if (TIENE(r, terminar))
        r->u.f.terminar();
    logf_("%s descargada (%s)", r->nombre, por);
    liberar_despues(m, r->copia);
}

/* Devuelve 1 si quedó cargada. */
static int cargar(Ranura *r)
{
    char dir[MAX_PATH];
    HMODULE m;
    FnModIniciar iniciar;
    int v;

    if (!GetFileAttributesExA(r->origen, GetFileExInfoStandard, &r->datos))
        memset(&r->datos, 0, sizeof r->datos);
    carpeta_temporal(dir, sizeof dir);
    CreateDirectoryA(dir, NULL);
    snprintf(r->copia, sizeof r->copia, "%s%s_%d.dll", dir, r->nombre, r->cargas);
    if (!CopyFileA(r->origen, r->copia, FALSE)) {
        logf_("%s: no pude copiarla a %s (error %lu)", r->nombre, r->copia, GetLastError());
        return 0;
    }
    m = LoadLibraryA(r->copia);
    if (!m) {
        logf_("%s: no pude cargarla (error %lu)", r->nombre, GetLastError());
        DeleteFileA(r->copia);
        return 0;
    }
    iniciar = (FnModIniciar)GetProcAddress(m, "mod_iniciar");
    if (!iniciar) {
        logf_("%s: no es una feature (le falta mod_iniciar)", r->nombre);
        FreeLibrary(m);
        DeleteFileA(r->copia);
        return 0;
    }
    /* Sin borrarla antes: si algún hilo quedó adentro de la versión anterior
     * (enganches a pedido), sigue viendo los mismos datos. */
    r->host.tam = sizeof r->host;
    r->host.version = ANFITRION_VERSION;
    r->host.nombre = r->nombre;
    r->host.delta = g_delta;
    r->host.carpeta = g_carpeta;
    r->host.ini = r->ini;
    r->host.recarga = r->cargas;
    r->host.enganche_bucle = g_enganche == 1;
    r->host.log = log_feature;
    r->host.openvr = host_openvr;
    r->host.estado_mvr = host_estado_mvr;
    r->host.baul_leer = host_baul_leer;
    r->host.baul_guardar = host_baul_guardar;
    r->host.enganchar_puntero = host_enganchar_puntero;
    r->host.enganchar_codigo = host_enganchar_codigo;
    memset(&r->u, 0, sizeof r->u);
    fecha_archivo(r->ini, &r->fecha_ini);   /* antes de que la lea: si cambia mientras, se avisa */
    v = iniciar(&r->host, &r->u.f);
    if (v != ANFITRION_VERSION || r->u.f.tam < (int)sizeof(int)) {
        if (v != ANFITRION_VERSION)
            logf_("%s: necesita otro xinput_ag.dll (version %d, este es %d): hay que cerrar el juego para cambiarlo",
                  r->nombre, v, ANFITRION_VERSION);
        else
            logf_("%s: no completo su Feature", r->nombre);
        soltar_puestos(r->nombre);
        FreeLibrary(m);
        DeleteFileA(r->copia);
        return 0;
    }
    r->cargas++;
    if (g_panico && TIENE(r, pausa))
        r->u.f.pausa(1);
    AcquireSRWLockExclusive(&g_cerrojo);
    r->mod = m;
    rehacer_orden();
    ReleaseSRWLockExclusive(&g_cerrojo);
    return 1;
}

static void recargar(Ranura *r, const char *por, int sonar)
{
    int ok, antes = r->mod != NULL;
    if (antes)
        descargar(r, por);
    ok = cargar(r);
    if (ok)
        logf_("%s %s (%s)", r->nombre, antes ? "recargada" : "cargada", por);
    else
        logf_("%s: fallo al cargar; queda apagada hasta que copies una que ande", r->nombre);
    if (sonar || !ok)
        MessageBeep(ok ? MB_ICONASTERISK : MB_ICONHAND);
}

static Ranura *ranura_de(const char *nombre)
{
    int i, libre = -1;
    for (i = 0; i < MAX_FEATURES; i++) {
        if (g_r[i].usada && lstrcmpiA(g_r[i].nombre, nombre) == 0)
            return &g_r[i];
        if (!g_r[i].usada && !g_r[i].mod && libre < 0)
            libre = i;
    }
    if (libre < 0)
        return NULL;
    memset(&g_r[libre], 0, sizeof g_r[libre]);
    g_r[libre].usada = 1;
    lstrcpynA(g_r[libre].nombre, nombre, sizeof g_r[libre].nombre);
    snprintf(g_r[libre].origen, sizeof g_r[libre].origen, "%s%s.dll", g_carpeta_mods, nombre);
    snprintf(g_r[libre].ini, sizeof g_r[libre].ini, "%s%s.ini", g_carpeta_mods, nombre);
    return &g_r[libre];
}

/* Mira mods_vr: carga las nuevas, recarga las que cambiaron (cuando el
 * archivo quedó igual en dos miradas seguidas, por si todavía se está
 * copiando) y descarga las que ya no están.
 * inicial = 1: al arrancar, todo sin esperar. todas = 1: Ctrl+F11. */
static void revisar_carpeta(int inicial, int todas)
{
    static int avisado;
    char patron[MAX_PATH], nombre[64];
    WIN32_FIND_DATAA d;
    HANDLE h;
    int i;

    for (i = 0; i < MAX_FEATURES; i++)
        g_r[i].presente = 0;
    snprintf(patron, sizeof patron, "%s*.dll", g_carpeta_mods);
    h = FindFirstFileA(patron, &d);
    if (h == INVALID_HANDLE_VALUE) {
        if (!avisado) {
            avisado = 1;
            logf_("no hay ninguna feature en %s (la carpeta %s, al lado de AI.exe)", g_carpeta_mods, CARPETA_MODS);
        }
    } else {
        avisado = 0;
        do {
            Ranura *r;
            WIN32_FILE_ATTRIBUTE_DATA a;
            size_t n = strlen(d.cFileName);
            if ((d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || n < 5 || n - 4 >= sizeof nombre ||
                lstrcmpiA(d.cFileName + n - 4, ".dll") != 0)
                continue;
            memcpy(nombre, d.cFileName, n - 4);
            nombre[n - 4] = 0;
            r = ranura_de(nombre);
            if (!r) {
                logf_("demasiadas features: %s queda afuera", nombre);
                continue;
            }
            r->presente = 1;
            if (!GetFileAttributesExA(r->origen, GetFileExInfoStandard, &a))
                continue;
            if (todas || inicial) {
                r->hay_visto = 0;
                recargar(r, todas ? "tecla" : "al arrancar", todas);
                continue;
            }
            if (mismos_datos(&a, &r->datos)) {   /* la que está cargada, o la que ya falló */
                r->hay_visto = 0;
                continue;
            }
            if (r->hay_visto && mismos_datos(&a, &r->visto)) {
                r->hay_visto = 0;
                recargar(r, r->mod ? "archivo nuevo" : "aparecio en la carpeta", 1);
                continue;
            }
            r->visto = a;
            r->hay_visto = 1;
        } while (FindNextFileA(h, &d));
        FindClose(h);
    }
    for (i = 0; i < MAX_FEATURES; i++)
        if (g_r[i].usada && !g_r[i].presente) {
            if (g_r[i].mod) {
                descargar(&g_r[i], "la sacaste de la carpeta");
                MessageBeep(MB_ICONASTERISK);
            }
            g_r[i].usada = 0;
        }
}

/* El .ini de cada feature: si cambió, se le avisa. */
static void revisar_inis(void)
{
    int i;
    for (i = 0; i < MAX_FEATURES; i++) {
        Ranura *r = &g_r[i];
        FILETIME f;
        if (!r->mod)
            continue;
        fecha_archivo(r->ini, &f);
        if (CompareFileTime(&f, &r->fecha_ini) == 0)
            continue;
        r->fecha_ini = f;
        logf_("%s.ini cambio", r->nombre);
        if (TIENE(r, config))
            r->u.f.config();
    }
}

/* --- cargador.ini: teclas y log --- */
static int g_tecla_recargar, g_mods_recargar;
static int g_tecla_panico, g_mods_panico;
static int g_recargar_solo = 1;

static void leer_config(void)
{
    char ruta[MAX_PATH], recargar[64], panico[64];
    snprintf(ruta, sizeof ruta, "%scargador.ini", g_carpeta);
    g_log = GetPrivateProfileIntA("General", "Log", 1, ruta);
    GetPrivateProfileStringA("General", "TeclaRecargar", "ctrl+f11", recargar, sizeof recargar, ruta);
    g_tecla_recargar = tecla_leer(recargar, &g_mods_recargar);
    GetPrivateProfileStringA("General", "TeclaPanico", "ctrl+f12", panico, sizeof panico, ruta);
    g_tecla_panico = tecla_leer(panico, &g_mods_panico);
    g_recargar_solo = GetPrivateProfileIntA("General", "RecargarSolo", 1, ruta);
    logf_("recargar sola una feature al copiar una nueva: %s", g_recargar_solo ? "si" : "no");
    logf_("tecla para recargar todas: %s", g_tecla_recargar ? recargar : "ninguna");
    logf_("tecla de panico: %s", g_tecla_panico ? panico : "ninguna");
}

static int tecla_abajo(int vk)
{
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

/* Solo con el juego al frente (que no salte mientras escribís en otro lado). */
static int combinacion_abajo(int vk, int mods)
{
    HWND v;
    DWORD pid = 0;
    if (!vk)
        return 0;
    v = GetForegroundWindow();
    if (v)
        GetWindowThreadProcessId(v, &pid);
    return pid == GetCurrentProcessId() && tecla_abajo(vk) &&
           (!(mods & TECLA_CTRL) || tecla_abajo(VK_CONTROL)) &&
           (!(mods & TECLA_ALT) || tecla_abajo(VK_MENU)) &&
           (!(mods & TECLA_SHIFT) || tecla_abajo(VK_SHIFT));
}

static void cambiar_panico(void)
{
    int i, apagar = !g_panico;
    InterlockedExchange(&g_panico, apagar);
    puestos_panico(apagar);
    logf_("tecla de panico: todo %s", apagar ? "APAGADO" : "prendido de nuevo");
    AcquireSRWLockShared(&g_cerrojo);
    for (i = 0; i < g_n_orden; i++) {
        Ranura *r = &g_r[g_orden[i]];
        if (TIENE(r, pausa))
            r->u.f.pausa(apagar);
    }
    ReleaseSRWLockShared(&g_cerrojo);
    MessageBeep(apagar ? MB_ICONHAND : MB_ICONASTERISK);
}

static void revisar_teclas_y_carpeta(void)
{
    static int antes_recargar, antes_panico;
    static DWORD ultima;
    int recargar_ahora = combinacion_abajo(g_tecla_recargar, g_mods_recargar);
    int panico_ahora = combinacion_abajo(g_tecla_panico, g_mods_panico);
    DWORD ahora = GetTickCount();
    if (panico_ahora && !antes_panico)
        cambiar_panico();
    if (recargar_ahora && !antes_recargar) {
        logf_("recargando todas (tecla) ...");
        revisar_carpeta(0, 1);
        ultima = ahora;
    } else if (ahora - ultima >= 1000) {
        ultima = ahora;
        if (g_recargar_solo)
            revisar_carpeta(0, 0);
        revisar_inis();
    }
    antes_recargar = recargar_ahora;
    antes_panico = panico_ahora;
}

/* --- Enganche al bucle principal del juego ---
 * Sin un mando de Xbox conectado, el juego casi no lee XInput, así que no
 * sirve para mirar el visor en cada cuadro. El bucle principal (0x7a8460)
 * llama en cada vuelta a la función de mensajes de Windows (0xa12fb0), que
 * llama a PeekMessageA. Se reemplaza la entrada de PeekMessageA en la tabla
 * de importación de AI.exe y, cuando la llamada viene de ese lugar, se hace
 * el paso: es el hilo principal, entre un cuadro y el siguiente. */
#define DIR_BOMBA_PEEK      0x00a13082u   /* mov esi,100 ... call ebx (PeekMessageA) */
#define DIR_VUELTA_PEEK     0x00a13096u   /* justo después del call ebx */

typedef BOOL (WINAPI *FnPeek)(LPMSG, HWND, UINT, UINT, UINT);
static FnPeek g_peek_real;
static DWORD *g_peek_slot;
static volatile LONG g_estado_inicio;   /* 0 nada, 1 iniciando, 2 listo */
static int g_primer_paso;
static LARGE_INTEGER g_freq, g_ultimo_tick;
static void enganchar_bucle(void);

static void iniciar(void)
{
    char *barra;
    GetModuleFileNameA(NULL, g_carpeta, MAX_PATH);
    barra = strrchr(g_carpeta, '\\');
    if (barra)
        barra[1] = 0;
    snprintf(g_carpeta_mods, sizeof g_carpeta_mods, "%s" CARPETA_MODS "\\", g_carpeta);
    logf_(VERSION ", AI.exe rebase %+d", (int)g_delta);
    leer_config();
    if (g_enganche != 1)
        enganchar_bucle();      /* segundo intento, ya con el juego andando */
    if (g_enganche == 1)
        logf_("enganchado al bucle del juego (PeekMessageA)");
    else
        logf_("no pude enganchar el bucle del juego (motivo %d): uso la lectura del mando", g_enganche);
    QueryPerformanceFrequency(&g_freq);
    QueryPerformanceCounter(&g_ultimo_tick);
    borrar_copias_viejas();
    revisar_carpeta(1, 0);
}

static void tick(const char *desde)
{
    LARGE_INTEGER ahora;
    float dt;
    int i;
    if (g_estado_inicio != 2) {
        if (InterlockedCompareExchange(&g_estado_inicio, 1, 0) == 0) {
            iniciar();
            InterlockedExchange(&g_estado_inicio, 2);
        }
        return;
    }
    if (!g_primer_paso) {
        g_primer_paso = 1;
        logf_("primer paso desde %s (hilo %lu)", desde, GetCurrentThreadId());
    }
    revisar_teclas_y_carpeta();
    liberar_pendientes(0);
    enganchar_mvr();
    buscar_openvr();
    QueryPerformanceCounter(&ahora);
    dt = (float)(ahora.QuadPart - g_ultimo_tick.QuadPart) / (float)g_freq.QuadPart;
    g_ultimo_tick = ahora;
    if (dt < 0.0f || dt > 0.1f)
        dt = 0.1f;
    if (g_panico)
        return;
    AcquireSRWLockShared(&g_cerrojo);
    for (i = 0; i < g_n_orden; i++) {
        Ranura *r = &g_r[g_orden[i]];
        if (TIENE(r, tick))
            r->u.f.tick(dt);
    }
    ReleaseSRWLockShared(&g_cerrojo);
}

static BOOL WINAPI peek_enganchado(LPMSG msg, HWND ventana, UINT min, UINT max, UINT quitar)
{
    BOOL r = g_peek_real(msg, ventana, min, max, quitar);
    if ((UINT_PTR)__builtin_return_address(0) == DIR_VUELTA_PEEK + g_delta)
        tick("el bucle del juego");
    return r;
}

static void enganchar_bucle(void)
{
    unsigned char *exe = (unsigned char *)GetModuleHandleA(NULL);
    IMAGE_NT_HEADERS32 *nt = (IMAGE_NT_HEADERS32 *)(exe + ((IMAGE_DOS_HEADER *)exe)->e_lfanew);
    uint32_t rva;
    DWORD viejo, actual;
    if (!coincide(DIR_BOMBA_PEEK, "be640000006a016a006a006a008d44242050ffd385c074138d4c241051ffd78d54241052ffd54e")) {
        g_enganche = -1;
        return;
    }
    rva = importacion_rva(exe, nt->OptionalHeader.SizeOfImage, "USER32.dll", "PeekMessageA");
    if (!rva) {
        g_enganche = -2;
        return;
    }
    g_peek_slot = (DWORD *)(exe + rva);
    actual = *g_peek_slot;
    if (actual == (DWORD)(UINT_PTR)peek_enganchado) {
        g_enganche = 1;
        return;
    }
    {
        /* Tiene que apuntar a otra DLL ya cargada (si Windows todavía no
         * completó la tabla, ahí hay un número que no es una dirección). */
        HMODULE dueno = NULL;
        if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                (LPCSTR)(UINT_PTR)actual, &dueno) ||
            (unsigned char *)dueno == exe) {
            g_enganche = -3;
            return;
        }
    }
    if (!VirtualProtect(g_peek_slot, 4, PAGE_READWRITE, &viejo)) {
        g_enganche = -4;
        return;
    }
    g_peek_real = (FnPeek)(UINT_PTR)actual;
    InterlockedExchange((LONG *)g_peek_slot, (LONG)(UINT_PTR)peek_enganchado);
    VirtualProtect(g_peek_slot, 4, viejo, &viejo);
    g_enganche = 1;
}

static void desenganchar_bucle(void)
{
    DWORD viejo;
    if (g_enganche != 1 || !g_peek_slot || !g_peek_real)
        return;
    if (VirtualProtect(g_peek_slot, 4, PAGE_READWRITE, &viejo)) {
        InterlockedExchange((LONG *)g_peek_slot, (LONG)(UINT_PTR)g_peek_real);
        VirtualProtect(g_peek_slot, 4, viejo, &viejo);
    }
}

DWORD WINAPI XInputGetState(DWORD indice, void *estado)
{
    DWORD r;
    cargar_xinput_real();
    r = g_get_state ? g_get_state(indice, estado) : ERROR_DEVICE_NOT_CONNECTED;
    if (g_estado_inicio != 2)
        tick("el mando");
    /* Para diagnosticar el mando: se anota cuando un mando aparece o se va.
     * Si aparece "conectado" sin mando de Xbox, es Steam Input. */
    if (indice < 4 && r != g_ultimo_resultado[indice]) {
        if (indice == 0 || r == ERROR_SUCCESS || g_ultimo_resultado[indice] == ERROR_SUCCESS)
            logf_("mando %lu: %s (codigo %lu)%s", indice,
                  r == ERROR_SUCCESS ? "conectado" : "no conectado", r,
                  g_get_state ? "" : " - no se pudo cargar el XInput de Windows");
        g_ultimo_resultado[indice] = r;
    }
    /* Si no se pudo enganchar el bucle, se usa la lectura del mando como antes. */
    if (g_enganche != 1)
        tick("el mando");
    return r;
}

DWORD WINAPI XInputSetState(DWORD indice, void *vibracion)
{
    int i;
    cargar_xinput_real();
    if (g_estado_inicio == 2 && !g_panico) {
        AcquireSRWLockShared(&g_cerrojo);
        for (i = 0; i < g_n_orden; i++) {
            Ranura *r = &g_r[g_orden[i]];
            if (TIENE(r, vibracion_juego))
                r->u.f.vibracion_juego(indice, vibracion);
        }
        ReleaseSRWLockShared(&g_cerrojo);
    }
    return g_set_state ? g_set_state(indice, vibracion) : ERROR_DEVICE_NOT_CONNECTED;
}

DWORD WINAPI XInputGetCapabilities(DWORD indice, DWORD flags, void *caps)
{
    cargar_xinput_real();
    return g_get_caps ? g_get_caps(indice, flags, caps) : ERROR_DEVICE_NOT_CONNECTED;
}

void WINAPI XInputEnable(BOOL activar)
{
    cargar_xinput_real();
    if (g_enable)
        g_enable(activar);
}

DWORD WINAPI XInputGetDSoundAudioDeviceGuids(DWORD indice, GUID *render, GUID *captura)
{
    cargar_xinput_real();
    return g_get_guids ? g_get_guids(indice, render, captura) : ERROR_DEVICE_NOT_CONNECTED;
}

DWORD WINAPI XInputGetBatteryInformation(DWORD indice, BYTE tipo, void *info)
{
    cargar_xinput_real();
    return g_get_battery ? g_get_battery(indice, tipo, info) : ERROR_DEVICE_NOT_CONNECTED;
}

DWORD WINAPI XInputGetKeystroke(DWORD indice, DWORD reservado, void *tecla)
{
    cargar_xinput_real();
    return g_get_keystroke ? g_get_keystroke(indice, reservado, tecla) : ERROR_DEVICE_NOT_CONNECTED;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD motivo, LPVOID reservado)
{
    int i;
    if (motivo == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(inst);
        InitializeCriticalSection(&g_cs_log);
        InitializeCriticalSection(&g_cs_baul);
        InitializeCriticalSection(&g_cs_puestos);
        /* El XInput de Windows se carga ya, en el mismo momento en que lo
         * cargaba GRAND original (Steam engancha el mando ahí). */
        cargar_xinput_real();
        g_delta = (unsigned)(UINT_PTR)GetModuleHandleA(NULL) - BASE_EXE;
        enganchar_bucle();
    }
    else if (motivo == DLL_PROCESS_DETACH) {
        if (!reservado) {
            /* Descarga en caliente (no pasa con GRAND): devolver todo. */
            desenganchar_bucle();
            if (g_mvr_estado == 1) {
                cambiar_entrada(g_slot_cabeza, (void *)g_cabeza_real, NULL);
                cambiar_entrada(g_slot_cuerpo, (void *)g_cuerpo_real, NULL);
            }
            for (i = 0; i < MAX_FEATURES; i++)
                descargar(&g_r[i], "se cierra el cargador");
            deshacer_puestos();
            liberar_pendientes(1);
        }
        if (g_flog)
            fclose(g_flog);
    }
    return TRUE;
}
