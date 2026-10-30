/* Pruebas de la lógica de postura (se compilan en Linux, sin el juego). */
#include <stdio.h>
#include "../src/logica.h"

static int fallas;
#define CHEQ(c, msg) do { if (!(c)) { printf("FALLA: %s\n", msg); fallas++; } else printf("ok: %s\n", msg); } while (0)

/* Sostiene una altura durante 'seg' segundos a 90 Hz y devuelve la última acción != NADA. */
static Accion sostener(Config *c, Estado *e, float y, float seg, int *cuantas)
{
    Accion ult = NADA; int n = 0;
    for (float t = 0; t < seg; t += 1.0f / 90) {
        Accion a = logica_paso(c, e, y, 1.0f / 90);
        if (a != NADA) { ult = a; n++; }
    }
    if (cuantas) *cuantas = n;
    return ult;
}

/* Va de la altura actual a 'hasta' a 'vel' m/s (a 90 Hz) y sigue ahí 'luego' segundos.
 * Devuelve la primera acción y en 'cuando' los segundos desde que empezó a moverse. */
static Accion moverse(Config *c, Estado *e, float desde, float hasta, float vel, float luego, float *cuando, int *cuantas)
{
    Accion pri = NADA; int n = 0; float t = 0, y = desde, paso = vel / 90;
    float total = (hasta > desde ? hasta - desde : desde - hasta) / vel + luego;
    for (; t < total; t += 1.0f / 90) {
        if (hasta < desde) { y -= paso; if (y < hasta) y = hasta; }
        else { y += paso; if (y > hasta) y = hasta; }
        Accion a = logica_paso(c, e, y, 1.0f / 90);
        if (a != NADA) { if (pri == NADA && cuando) *cuando = t; if (pri == NADA) pri = a; n++; }
    }
    if (cuantas) *cuantas = n;
    return pri;
}

int main(void)
{
    Config c; Estado e; int n;
    float t;
    logica_config_defecto(&c);

    /* Agacharse rápido: no esperar a bajar todo el umbral. */
    logica_reset(&e);
    sostener(&c, &e, 1.62f, 1, NULL);
    CHEQ(moverse(&c, &e, 1.62f, 1.15f, 1.0f, 1, &t, &n) == AGACHAR && n == 1 && t < 0.2f,
         "agacharse a 1 m/s agacha en menos de 0.2 s");
    printf("    (agacho a los %.2f s)\n", t);
    CHEQ(moverse(&c, &e, 1.15f, 1.62f, 1.0f, 1, &t, &n) == PARAR && n == 1 && t < 0.35f,
         "pararse a 1 m/s para en menos de 0.35 s");
    printf("    (paro a los %.2f s)\n", t);
    /* Agachado bien abajo y pararse rápido: no esperar a llegar cerca de parado. */
    logica_reset(&e);
    sostener(&c, &e, 1.62f, 1, NULL);
    moverse(&c, &e, 1.62f, 0.95f, 1.0f, 1, NULL, NULL);
    CHEQ(moverse(&c, &e, 0.95f, 1.62f, 1.0f, 1, &t, &n) == PARAR && n == 1 && t < 0.25f,
         "desde 0.95 m, pararse a 1 m/s para en menos de 0.25 s");
    printf("    (paro a los %.2f s, cabeza a %.2f m)\n", t, 0.95f + t);
    logica_reset(&e);
    sostener(&c, &e, 1.62f, 1, NULL);
    moverse(&c, &e, 1.62f, 0.95f, 1.0f, 1, NULL, NULL);
    CHEQ(moverse(&c, &e, 0.95f, 1.05f, 1.0f, 1, &t, &n) == NADA, "agachado, acomodarse 10 cm rapido no la para");
    CHEQ(moverse(&c, &e, 1.05f, 0.95f, 1.0f, 1, &t, &n) == NADA, "y volver a bajar tampoco hace nada");

    logica_reset(&e);
    sostener(&c, &e, 1.62f, 1, NULL);
    CHEQ(moverse(&c, &e, 1.62f, 1.52f, 0.8f, 1, &t, &n) == NADA, "cabeceo rapido de 10 cm (mirar abajo) no agacha");
    CHEQ(moverse(&c, &e, 1.52f, 1.62f, 0.8f, 1, &t, &n) == NADA, "y volver no hace nada");
    CHEQ(moverse(&c, &e, 1.62f, 1.40f, 0.3f, 1, &t, &n) == NADA, "bajar despacio 22 cm no agacha");
    CHEQ(moverse(&c, &e, 1.40f, 1.62f, 0.3f, 1, &t, &n) == NADA, "y volver despacio tampoco");
    logica_reset(&e);
    sostener(&c, &e, 1.62f, 1, NULL);
    CHEQ(moverse(&c, &e, 1.62f, 1.20f, 0.3f, 1, &t, &n) == AGACHAR && n == 1, "agacharse despacio agacha igual (por el umbral)");

    /* El resto, como antes: con saltos de altura (sin velocidad). */
    c.velocidad = 0;

    logica_reset(&e);
    CHEQ(sostener(&c, &e, 1.70f, 2, &n) == NADA, "parado quieto no hace nada");
    CHEQ(sostener(&c, &e, 1.50f, 2, &n) == NADA, "bajar 20 cm (mirar algo bajo) no agacha");
    CHEQ(sostener(&c, &e, 1.20f, 0.1f, &n) == NADA, "agacharse menos que la demora no agacha");
    CHEQ(sostener(&c, &e, 1.20f, 0.5f, &n) == AGACHAR && n == 1, "agacharse 50 cm agacha una sola vez");
    CHEQ(sostener(&c, &e, 1.20f, 60, &n) == NADA, "quedarse agachado un minuto no hace nada");
    CHEQ(e.altura_parado > 1.69f, "la referencia no se olvida estando agachado");
    CHEQ(sostener(&c, &e, 1.45f, 2, &n) == NADA, "subir un poco (histéresis) sigue abajo");
    CHEQ(sostener(&c, &e, 1.65f, 0.5f, &n) == PARAR && n == 1, "pararse para a Ripley una vez");

    /* Temblor en el umbral: no debe alternar. */
    logica_reset(&e);
    sostener(&c, &e, 1.70f, 1, NULL);
    n = 0;
    for (int i = 0; i < 900; i++) {
        float y = 1.35f + ((i / 3) % 2 ? 0.02f : -0.02f);
        if (logica_paso(&c, &e, y, 1.0f / 90) != NADA) n++;
    }
    CHEQ(n <= 1, "temblor justo en el umbral no hace ir y venir");

    /* Arrancar sentado o agachado y después pararse. */
    logica_reset(&e);
    CHEQ(sostener(&c, &e, 1.10f, 1, &n) == NADA, "arrancar abajo no hace nada");
    CHEQ(sostener(&c, &e, 1.70f, 1, &n) == NADA, "pararse después solo sube la referencia");
    CHEQ(sostener(&c, &e, 1.25f, 1, &n) == AGACHAR, "y desde ahí agacharse funciona");

    /* Altura fija. */
    c.altura_fija = 1.60f;
    logica_reset(&e);
    CHEQ(sostener(&c, &e, 1.20f, 1, &n) == AGACHAR, "con altura fija agacha por debajo de 1.25");
    c.altura_fija = 0;

    /* Sin pararse solo. */
    c.parar_solo = 0;
    logica_reset(&e);
    sostener(&c, &e, 1.70f, 1, NULL);
    sostener(&c, &e, 1.20f, 1, NULL);
    CHEQ(sostener(&c, &e, 1.70f, 1, &n) == NADA, "PararSolo=0 no para a Ripley");

    printf(fallas ? "\n%d fallas\n" : "\ntodo ok\n", fallas);
    return fallas != 0;
}
