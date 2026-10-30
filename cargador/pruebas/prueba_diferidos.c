/* Prueba de diferido_resuelto con el dxgi.dll de MotherVR 0.8.1: se arma la
 * imagen en memoria como la cargaría Windows (secciones en su RVA, debajo de
 * 4 GB) y se mira qué devuelve antes y después de "resolver" OpenVR.
 * Uso: prueba_diferidos dxgi.dll */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>
#include "../src/diferidos.h"

static uint32_t l32(const unsigned char *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static int fallas;
static void ver(const char *n, int ok) { printf("%s %s\n", ok ? "OK   " : "FALLA", n); fallas += !ok; }

int main(int argc, char **argv)
{
    FILE *f = fopen(argc > 1 ? argv[1] : "dxgi.dll", "rb");
    unsigned char *arch, *img;
    long largo;
    uint32_t pe, tam, nsec, opt, i, slot = 0x6769c, interno, externo = 0x7a000000u;
    if (!f) { perror("dxgi.dll"); return 2; }
    fseek(f, 0, SEEK_END); largo = ftell(f); rewind(f);
    arch = malloc(largo);
    if (fread(arch, 1, largo, f) != (size_t)largo) return 2;
    pe = l32(arch + 0x3c);
    tam = l32(arch + pe + 0x50);
    nsec = arch[pe + 6] | arch[pe + 7] << 8;
    opt = arch[pe + 20] | arch[pe + 21] << 8;
    img = mmap(NULL, tam, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
    if (img == MAP_FAILED) { perror("mmap"); return 2; }
    memcpy(img, arch, l32(arch + pe + 0x54));
    for (i = 0; i < nsec; i++) {
        unsigned char *s = arch + pe + 24 + opt + 40 * i;
        uint32_t va = l32(s + 12), raw = l32(s + 20), off = l32(s + 16);
        if (off) memcpy(img + va, arch + raw, off < l32(s + 8) || !l32(s + 8) ? off : l32(s + 8));
    }
    /* Sin rebase: el slot tiene la dirección del cargador diferido (adentro). */
    interno = l32(img + slot);
    printf("      slot VR_GetGenericInterface = %08x (imagen en %08x, %u bytes)\n",
           interno, (uint32_t)(uintptr_t)img, tam);
    /* Como Windows la cargaría en img, se ajustan las 6 direcciones internas
     * de la tabla (reubicación). */
    for (i = 0; i < 6; i++) {
        uint32_t v = l32(img + slot + 4 * i) - 0x10000000u + (uint32_t)(uintptr_t)img;
        memcpy(img + slot + 4 * i, &v, 4);
    }
    ver("sin cargar OpenVR no devuelve nada",
        diferido_resuelto(img, tam, "openvr_api.dll", "VR_GetGenericInterface") == 0);
    memcpy(img + slot, &externo, 4);
    ver("con OpenVR cargado devuelve la direccion real",
        diferido_resuelto(img, tam, "openvr_api.dll", "VR_GetGenericInterface") == externo);
    ver("mayusculas en el nombre de la dll no importan",
        diferido_resuelto(img, tam, "OpenVR_API.DLL", "VR_GetGenericInterface") == externo);
    ver("otra funcion no se confunde",
        diferido_resuelto(img, tam, "openvr_api.dll", "VR_InitInternal2") == 0);
    ver("otra dll no se confunde",
        diferido_resuelto(img, tam, "vrclient.dll", "VR_GetGenericInterface") == 0);
    ver("una imagen cortada no se lee de mas",
        diferido_resuelto(img, 0x1000, "openvr_api.dll", "VR_GetGenericInterface") == 0);
    printf(fallas ? "\n%d pruebas fallaron\n" : "\nTodo OK\n", fallas);
    return fallas != 0;
}
