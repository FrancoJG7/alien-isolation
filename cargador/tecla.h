/* Interpreta la tecla de pánico del .ini ("ctrl+f12", "alt+shift+p", "f7",
 * "no"...). Sin windows.h, así se prueba en Linux. */
#ifndef TECLA_H
#define TECLA_H

#define TECLA_CTRL  1
#define TECLA_ALT   2
#define TECLA_SHIFT 4

/* Devuelve la tecla virtual de Windows (0 = ninguna o no entendida) y en
 * *mods las teclas que hay que sostener. */
int tecla_leer(const char *texto, int *mods);

#endif
