/* Prueba de la recarga en vivo de varias features (Windows o Wine).
 * Uso: prueba_recarga.exe <dxgi.dll de MotherVR 0.8.1>
 * En la carpeta: xinput_ag.dll, agachar_a.dll, agachar_b.dll, prueba_a.dll,
 * prueba_b.dll y mods_vr\agachar.ini.
 *
 * - Mapea el dxgi.dll de MotherVR sin ejecutarlo y le pone, en la función de
 *   la pose de la cabeza, un salto a una pose falsa (como hace GRAND).
 * - Carga xinput_ag.dll y llama XInputGetState como lo haría el juego.
 * - Un hilo "de dibujo" pide la pose sin parar y otro llama sin parar a las
 *   dos funciones que engancha la feature de prueba, mientras el principal
 *   pisa los DLL de mods_vr con otras versiones. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

typedef DWORD (WINAPI *FnGetState)(DWORD, void *);
typedef void (__attribute__((thiscall)) *FnPose)(void *self, float *out, float escala);
typedef int (__cdecl *FnObjetivo)(int);

/* push ebp; mov ebp,esp; mov eax,[ebp+8]; add eax,1; pop ebp; ret
 * (bytes fijos para poder engancharla moviendo 6). */
__asm__(".globl _objetivo\n_objetivo:\n"
        ".byte 0x55, 0x89, 0xe5, 0x8b, 0x45, 0x08, 0x83, 0xc0, 0x01, 0x5d, 0xc3\n"
        ".section .drectve\n.ascii \" -export:objetivo\"\n.text\n");
int __cdecl objetivo(int x);
static int __cdecl doble(int x) { return 2 * x; }
__declspec(dllexport) FnObjetivo tabla[1] = { doble };

static volatile float g_y = 1.60f;
static volatile LONG g_parar, g_llamadas, g_llamadas_obj, g_raras;
static unsigned char *g_mvr;
static unsigned char g_self[0x100];
static volatile float g_ultimo_out7, g_ultimo_out11;

static void __attribute__((thiscall)) pose_falsa(void *self, float *out, float escala)
{
    unsigned char *p = (unsigned char *)self + 0x10;
    int i;
    memset(p, 0, 0x50);
    *(float *)(p + 0x1c) = g_y;
    p[0x4c] = 1;                        /* pose válida */
    for (i = 0; i < 12; i++)
        out[i] = 0.0f;
    out[7] = -escala * g_y;
}

static DWORD WINAPI dibujar(void *x)
{
    FnPose *slot = (FnPose *)(g_mvr + 0x5ddc0);
    float out[12];
    (void)x;
    while (!g_parar) {
        (*slot)(g_self, out, 1.0f);
        g_ultimo_out7 = out[7];
        g_ultimo_out11 = out[11];
        InterlockedIncrement(&g_llamadas);
    }
    return 0;
}

/* Llama sin parar a lo enganchado: solo puede dar lo original o lo de la
 * feature, nunca otra cosa ni colgarse. */
static DWORD WINAPI martillar(void *x)
{
    (void)x;
    while (!g_parar) {
        int a = objetivo(1), b = tabla[0](3);
        if ((a != 2 && a != 102) || (b != 6 && b != 1006))
            InterlockedIncrement(&g_raras);
        InterlockedIncrement(&g_llamadas_obj);
    }
    return 0;
}

static int hay_en_log(const char *texto)
{
    char linea[512];
    int n = 0;
    FILE *f = fopen("ai_mods.log", "r");
    if (!f)
        return 0;
    while (fgets(linea, sizeof linea, f))
        if (strstr(linea, texto))
            n++;
    fclose(f);
    return n;
}

static int fallas;
#define CHEQ(c, msg) do { if (!(c)) { printf("FALLA: %s\n", msg); fallas++; } else printf("ok: %s\n", msg); } while (0)

static FnGetState g_get;

static void tics(int ms)
{
    char estado[16];
    MSG m;
    DWORD fin = GetTickCount() + ms;
    while (GetTickCount() < fin) {
        while (PeekMessageA(&m, NULL, 0, 0, PM_REMOVE))
            DispatchMessageA(&m);
        g_get(0, estado);
        Sleep(5);
    }
}

static void tocar(const char *ruta)       /* fecha nueva sin cambiar el contenido */
{
    HANDLE h = CreateFileA(ruta, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    FILETIME ft;
    SYSTEMTIME st;
    GetSystemTime(&st);
    SystemTimeToFileTime(&st, &ft);
    SetFileTime(h, NULL, NULL, &ft);
    CloseHandle(h);
}

static void copiar(const char *de, const char *a)
{
    CopyFileA(de, a, FALSE);
    tocar(a);
}

static void apretar(WORD vk1, WORD vk2)
{
    INPUT in[4];
    memset(in, 0, sizeof in);
    in[0].type = in[1].type = in[2].type = in[3].type = INPUT_KEYBOARD;
    in[0].ki.wVk = vk1;
    in[1].ki.wVk = vk2;
    in[2].ki.wVk = vk2;
    in[2].ki.dwFlags = KEYEVENTF_KEYUP;
    in[3].ki.wVk = vk1;
    in[3].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, in, sizeof in[0]);
    tics(200);
    SendInput(2, in + 2, sizeof in[0]);
    tics(200);
}

int main(int argc, char **argv)
{
    HMODULE xi;
    DWORD viejo;
    unsigned char *f;
    HANDLE h, h2;
    HWND ventana;
    int i, n;

    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc < 2) {
        printf("uso: prueba_recarga dxgi.dll\n");
        return 2;
    }
    DeleteFileA("ai_mods.log");
    DeleteFileA("mods_vr\\otra.dll");
    copiar("agachar_a.dll", "mods_vr\\agachar.dll");
    copiar("prueba_a.dll", "mods_vr\\prueba.dll");

    g_mvr = (unsigned char *)LoadLibraryExA(argv[1], NULL, DONT_RESOLVE_DLL_REFERENCES);
    CHEQ(g_mvr != NULL, "mapear el dxgi.dll de MotherVR");
    if (!g_mvr)
        return 1;
    f = g_mvr + 0x70e0;                 /* salto a la pose falsa, como el desvío de GRAND */
    VirtualProtect(f, 5, PAGE_EXECUTE_READWRITE, &viejo);
    f[0] = 0xe9;
    *(DWORD *)(f + 1) = (DWORD)((unsigned char *)pose_falsa - (f + 5));
    VirtualProtect(f, 5, viejo, &viejo);

    CHEQ(objetivo(1) == 2 && tabla[0](3) == 6, "sin el cargador, objetivo y tabla dan lo original");
    xi = LoadLibraryA("xinput_ag.dll");
    CHEQ(xi != NULL, "cargar xinput_ag.dll");
    g_get = (FnGetState)GetProcAddress(xi, "XInputGetState");
    tics(300);
    CHEQ(*(void **)(g_mvr + 0x5ddc0) != (void *)(g_mvr + 0x70e0), "el cargador engancho la vtable de MotherVR");
    CHEQ(hay_en_log("[agachar] agachar v0.2.0 A") == 1, "cargo agachar A");
    CHEQ(hay_en_log("[prueba] prueba A") == 1, "cargo prueba A");
    CHEQ(objetivo(1) == 102, "enganche de codigo a pedido anda");
    CHEQ(tabla[0](3) == 1006, "enganche de puntero a pedido anda");

    h = CreateThread(NULL, 0, dibujar, NULL, 0, NULL);
    h2 = CreateThread(NULL, 0, martillar, NULL, 0, NULL);
    tics(500);
    g_y = 1.10f;                        /* agacharse */
    tics(300);
    printf("    out[7] con la cabeza a 1.10: %.3f, out[11]: %.1f\n", g_ultimo_out7, g_ultimo_out11);
    CHEQ(g_ultimo_out7 < -1.55f && g_ultimo_out7 > -1.65f, "MotherVR ve la cabeza a la altura de parado");
    CHEQ(g_ultimo_out11 == 1.0f, "la pose pasa tambien por la feature de prueba");

    /* 20 vueltas: cada una pisa las dos features a la vez. */
    for (i = 0; i < 20; i++) {
        Sleep(20);
        copiar(i % 2 ? "agachar_a.dll" : "agachar_b.dll", "mods_vr\\agachar.dll");
        copiar(i % 2 ? "prueba_a.dll" : "prueba_b.dll", "mods_vr\\prueba.dll");
        tics(2300);                     /* el cargador espera que el archivo quede quieto */
    }
    n = hay_en_log("recargada (archivo nuevo)");
    printf("    recargas: %d, poses pedidas: %ld, llamadas a lo enganchado: %lu, raras: %ld\n",
           n, g_llamadas, (unsigned long)g_llamadas_obj, g_raras);
    CHEQ(n == 40, "40 recargas (20 de cada una) sin colgarse");
    CHEQ(hay_en_log("[agachar] agachar v0.2.0 B") == 10, "agachar B se cargo 10 veces");
    CHEQ(hay_en_log("[prueba] baul recuperado") == 20, "prueba recupero el baul en cada recarga");
    CHEQ(hay_en_log("[agachar] recupero la calibracion") == 20, "agachar recupero la calibracion en cada recarga");
    CHEQ(g_raras == 0, "lo enganchado nunca dio otra cosa mientras se recargaba");
    g_y = 1.60f;
    tics(300);
    g_y = 1.10f;
    tics(300);
    CHEQ(g_ultimo_out7 < -1.55f && g_ultimo_out7 > -1.65f, "despues de recargar sigue compensando");

    /* Otra feature que quiere lo mismo: no se lo dan. */
    copiar("prueba_a.dll", "mods_vr\\otra.dll");
    tics(2500);
    CHEQ(hay_en_log("[otra] no puedo enganchar") == 2, "dos features no pueden enganchar lo mismo");
    CHEQ(objetivo(1) == 102, "la primera sigue con su enganche");
    DeleteFileA("mods_vr\\otra.dll");
    tics(1500);

    /* Un archivo roto: falla, el resto sigue. */
    {
        FILE *roto = fopen("mods_vr\\prueba.dll", "wb");
        fputs("esto no es un dll", roto);
        fclose(roto);
    }
    tics(2500);
    CHEQ(hay_en_log("prueba: fallo al cargar") == 1, "un DLL roto falla sin romper nada");
    CHEQ(objetivo(1) == 2 && tabla[0](3) == 6, "sin la feature, lo enganchado vuelve a lo original");
    tics(100);
    CHEQ(g_ultimo_out11 == 0.0f, "sin la feature, la pose no pasa por ella");
    CHEQ(g_ultimo_out7 < -1.55f && g_ultimo_out7 > -1.65f, "agachar sigue andando");
    copiar("prueba_a.dll", "mods_vr\\prueba.dll");
    tics(2500);
    CHEQ(objetivo(1) == 102 && tabla[0](3) == 1006, "con una buena vuelve a andar (reusa los enganches)");

    /* Sacarla de la carpeta. */
    DeleteFileA("mods_vr\\prueba.dll");
    tics(1500);
    CHEQ(hay_en_log("prueba descargada (la sacaste de la carpeta)") == 1, "borrarla la descarga");
    CHEQ(objetivo(1) == 2 && tabla[0](3) == 6, "borrada, lo enganchado vuelve a lo original");

    /* Cambiar agachar.ini. */
    {
        FILE *ini = fopen("mods_vr\\agachar.ini", "a");
        fputs("; cambio\n", ini);
        fclose(ini);
    }
    tics(1500);
    CHEQ(hay_en_log("[agachar] agachar.ini recargado") == 1, "avisa cuando cambia agachar.ini");

    /* Teclas (necesitan una ventana al frente). */
    ventana = CreateWindowA("STATIC", "prueba", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 200, 100, NULL, NULL, NULL, NULL);
    SetForegroundWindow(ventana);
    tics(300);
    copiar("prueba_a.dll", "mods_vr\\prueba.dll");
    tics(2500);
    apretar(VK_CONTROL, VK_F12);
    g_y = 1.10f;
    tics(300);
    CHEQ(hay_en_log("tecla de panico: todo APAGADO") == 1, "Ctrl+F12 apaga todo");
    CHEQ(g_ultimo_out7 < -1.05f && g_ultimo_out7 > -1.15f && objetivo(1) == 2, "con panico no toca nada");
    apretar(VK_CONTROL, VK_F12);
    tics(300);
    CHEQ(g_ultimo_out7 < -1.55f && g_ultimo_out7 > -1.65f && objetivo(1) == 102, "al volver, todo anda de nuevo");
    n = hay_en_log("recargada (tecla)");
    apretar(VK_CONTROL, VK_F11);
    tics(300);
    CHEQ(hay_en_log("recargada (tecla)") == n + 2, "Ctrl+F11 recarga las dos");

    g_parar = 1;
    WaitForSingleObject(h, 2000);
    WaitForSingleObject(h2, 2000);
    printf(fallas ? "\n%d fallas\n" : "\ntodo ok\n", fallas);
    return fallas != 0;
}
