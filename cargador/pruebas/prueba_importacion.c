/* Prueba de importacion_rva con el AI.exe de Steam: tiene que encontrar la
 * entrada de PeekMessageA de USER32 (la que usa el bucle principal).
 * Uso: prueba_importacion AI.exe */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../src/diferidos.h"

static uint32_t l32(const unsigned char *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static int fallas;
static void ver(const char *n, int ok) { printf("%s %s\n", ok ? "OK   " : "FALLA", n); fallas += !ok; }

int main(int argc, char **argv)
{
    FILE *f = fopen(argc > 1 ? argv[1] : "AI.exe", "rb");
    unsigned char *arch, *img;
    long largo;
    uint32_t pe, tam, nsec, opt, i, r;
    if (!f) { perror("AI.exe"); return 2; }
    fseek(f, 0, SEEK_END); largo = ftell(f); rewind(f);
    arch = malloc(largo);
    if (fread(arch, 1, largo, f) != (size_t)largo) return 2;
    pe = l32(arch + 0x3c);
    tam = l32(arch + pe + 0x50);
    nsec = arch[pe + 6] | arch[pe + 7] << 8;
    opt = arch[pe + 20] | arch[pe + 21] << 8;
    img = calloc(1, tam);
    memcpy(img, arch, l32(arch + pe + 0x54));
    for (i = 0; i < nsec; i++) {
        unsigned char *s = arch + pe + 24 + opt + 40 * i;
        uint32_t vs = l32(s + 8), va = l32(s + 12), rs = l32(s + 16), raw = l32(s + 20);
        if (rs) memcpy(img + va, arch + raw, rs < vs || !vs ? rs : vs);
    }
    r = importacion_rva(img, tam, "USER32.dll", "PeekMessageA");
    printf("      PeekMessageA en RVA %08x (direccion sin rebase %08x)\n", r, r + 0x400000);
    ver("PeekMessageA de USER32 en 0x119f388", r + 0x400000 == 0x119f388);
    ver("PeekMessageW de USER32 en 0x119f2fc",
        importacion_rva(img, tam, "user32.dll", "PeekMessageW") + 0x400000 == 0x119f2fc);
    ver("una funcion que no importa da 0", importacion_rva(img, tam, "USER32.dll", "NoExiste") == 0);
    ver("de otra dll da 0", importacion_rva(img, tam, "KERNEL32.dll", "PeekMessageA") == 0);
    ver("imagen cortada da 0", importacion_rva(img, 0x1000, "USER32.dll", "PeekMessageA") == 0);
    printf(fallas ? "\n%d pruebas fallaron\n" : "\nTodo OK\n", fallas);
    return fallas != 0;
}
