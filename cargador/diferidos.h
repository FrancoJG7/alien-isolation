#ifndef DIFERIDOS_H
#define DIFERIDOS_H
#include <stdint.h>

/* Busca en la tabla de importación diferida de una imagen PE de 32 bits ya
 * cargada en memoria (base, tamaño) la función "funcion" de "dll". Devuelve
 * la dirección que tiene la tabla si ya se resolvió (apunta afuera de la
 * imagen), o 0 si no está o todavía no se cargó. */
uint32_t diferido_resuelto(const unsigned char *base, uint32_t tam,
                           const char *dll, const char *funcion);

#endif

#ifndef DIFERIDOS_H_IMPORT
#define DIFERIDOS_H_IMPORT
/* RVA de la entrada de la tabla de importación (IAT) de "funcion" de "dll"
 * en una imagen PE de 32 bits ya cargada, o 0 si no la importa por nombre. */
uint32_t importacion_rva(const unsigned char *base, uint32_t tam,
                         const char *dll, const char *funcion);
#endif
