/* Pruebas de la tecla de pánico (se compilan en Linux). */
#include <stdio.h>
#include "../src/tecla.h"

static int fallas;
static void cheq(const char *txt, int vk, int mods)
{
    int m = -1, v = tecla_leer(txt, &m);
    int ok = v == vk && m == mods;
    printf("%s: \"%s\" -> tecla 0x%02x, mods %d\n", ok ? "ok" : "FALLA", txt, v, m);
    fallas += !ok;
}

int main(void)
{
    cheq("ctrl+f12", 0x7b, TECLA_CTRL);
    cheq("Ctrl + F12", 0x7b, TECLA_CTRL);
    cheq("f7", 0x76, 0);
    cheq("F1", 0x70, 0);
    cheq("f24", 0x87, 0);
    cheq("f25", 0, 0);
    cheq("f1x", 0, 0);
    cheq("alt+shift+p", 'P', TECLA_ALT | TECLA_SHIFT);
    cheq("ctrl+9", '9', TECLA_CTRL);
    cheq("pausa", 0x13, 0);
    cheq("no", 0, 0);
    cheq("", 0, 0);
    cheq("ctrl", 0, 0);
    cheq("ctrl+inicio", 0, 0);
    printf(fallas ? "\n%d fallas\n" : "\ntodo ok\n", fallas);
    return fallas != 0;
}
