#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "tecla.h"

#define VK_F1_   0x70
#define VK_PAUSA 0x13

int tecla_leer(const char *texto, int *mods)
{
    char buf[64], *t;
    int vk = 0, m = 0, i;
    for (i = 0; texto[i] && i < (int)sizeof buf - 1; i++)
        buf[i] = (char)tolower((unsigned char)texto[i]);
    buf[i] = 0;
    for (t = strtok(buf, "+ \t"); t; t = strtok(NULL, "+ \t")) {
        if (!strcmp(t, "ctrl") || !strcmp(t, "control"))
            m |= TECLA_CTRL;
        else if (!strcmp(t, "alt"))
            m |= TECLA_ALT;
        else if (!strcmp(t, "shift"))
            m |= TECLA_SHIFT;
        else if (t[0] == 'f' && t[1] >= '1' && t[1] <= '9' && strspn(t + 1, "0123456789") == strlen(t + 1) &&
                 atoi(t + 1) <= 24)
            vk = VK_F1_ + atoi(t + 1) - 1;
        else if (!t[1] && ((t[0] >= 'a' && t[0] <= 'z') || (t[0] >= '0' && t[0] <= '9')))
            vk = toupper((unsigned char)t[0]);
        else if (!strcmp(t, "pausa") || !strcmp(t, "pause"))
            vk = VK_PAUSA;
        else
            return *mods = 0, 0;    /* "no", "ninguna" o algo que no entiendo */
    }
    *mods = vk ? m : 0;
    return vk;
}
