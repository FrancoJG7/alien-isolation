/* Lectura de la importación diferida (ImgDelayDescr) sin windows.h, para
 * poder probarla fuera de Windows con el dxgi.dll de MotherVR. */
#include <string.h>
#include <ctype.h>
#include "diferidos.h"

static uint32_t leer32(const unsigned char *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static int iguales_sin_mayus(const char *a, const char *b)
{
    for (; *a && *b; a++, b++)
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return 0;
    return *a == *b;
}

/* Cadena terminada en 0 completamente dentro de la imagen. */
static const char *cadena(const unsigned char *base, uint32_t tam, uint32_t rva)
{
    uint32_t i;
    if (rva >= tam)
        return NULL;
    for (i = rva; i < tam; i++)
        if (!base[i])
            return (const char *)base + rva;
    return NULL;
}

uint32_t diferido_resuelto(const unsigned char *base, uint32_t tam,
                           const char *dll, const char *funcion)
{
    uint32_t pe, nrva, dir, d, inicio = (uint32_t)(uintptr_t)base;
    if (tam < 0x40 || base[0] != 'M' || base[1] != 'Z')
        return 0;
    pe = leer32(base + 0x3c);
    if (pe > tam - 0xf8 || memcmp(base + pe, "PE\0\0", 4) != 0 ||
        base[pe + 0x18] != 0x0b || base[pe + 0x19] != 0x01)    /* PE32 */
        return 0;
    nrva = leer32(base + pe + 0x74);
    if (nrva <= 13)
        return 0;
    dir = leer32(base + pe + 0x78 + 13 * 8);                /* DELAY_IMPORT */
    if (!dir || dir >= tam)
        return 0;
    for (d = dir; d + 32 <= tam && leer32(base + d + 4); d += 32) {
        uint32_t attr = leer32(base + d), iat = leer32(base + d + 12), nom = leer32(base + d + 16), i;
        const char *n = cadena(base, tam, leer32(base + d + 4));
        if (!(attr & 1) || !n || !iguales_sin_mayus(n, dll) || iat >= tam || nom >= tam)
            continue;
        for (i = 0; i < 256 && nom + 4 * i + 4 <= tam && iat + 4 * i + 4 <= tam; i++) {
            uint32_t t = leer32(base + nom + 4 * i), v;
            const char *f;
            if (!t)
                break;
            if (t & 0x80000000u)
                continue;           /* por ordinal */
            f = cadena(base, tam, t + 2);
            if (!f || strcmp(f, funcion) != 0)
                continue;
            v = leer32(base + iat + 4 * i);
            if (v && (v < inicio || v >= inicio + tam))
                return v;
            return 0;               /* todavía apunta al cargador diferido */
        }
    }
    return 0;
}

uint32_t importacion_rva(const unsigned char *base, uint32_t tam,
                         const char *dll, const char *funcion)
{
    uint32_t pe, dir, d;
    if (tam < 0x40 || base[0] != 'M' || base[1] != 'Z')
        return 0;
    pe = leer32(base + 0x3c);
    if (pe > tam - 0xf8 || memcmp(base + pe, "PE\0\0", 4) != 0 ||
        base[pe + 0x18] != 0x0b || base[pe + 0x19] != 0x01)
        return 0;
    if (leer32(base + pe + 0x74) <= 1)
        return 0;
    dir = leer32(base + pe + 0x78 + 1 * 8);                 /* IMPORT */
    if (!dir || dir >= tam)
        return 0;
    /* IMAGE_IMPORT_DESCRIPTOR: nombres originales, fecha, cadena, nombre, IAT */
    for (d = dir; d + 20 <= tam && leer32(base + d + 12); d += 20) {
        uint32_t nombres = leer32(base + d), iat = leer32(base + d + 16), i;
        const char *n = cadena(base, tam, leer32(base + d + 12));
        if (!n || !iguales_sin_mayus(n, dll) || !nombres || nombres >= tam || iat >= tam)
            continue;
        for (i = 0; i < 4096 && nombres + 4 * i + 4 <= tam && iat + 4 * i + 4 <= tam; i++) {
            uint32_t t = leer32(base + nombres + 4 * i);
            const char *f;
            if (!t)
                break;
            if (t & 0x80000000u)
                continue;
            f = cadena(base, tam, t + 2);
            if (f && strcmp(f, funcion) == 0)
                return iat + 4 * i;
        }
    }
    return 0;
}
