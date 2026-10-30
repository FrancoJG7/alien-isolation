#include "logica.h"

void logica_config_defecto(Config *c)
{
    c->bajada_agachar = 0.35f;
    c->bajada_parar = 0.20f;
    c->demora = 0.15f;
    c->altura_fija = 0.0f;
    c->olvido = 0.005f;
    c->parar_solo = 1;
    c->velocidad = 0.6f;
    c->bajada_rapida = 0.12f;
    c->subida_rapida = 0.15f;
}

void logica_reset(Estado *e)
{
    e->calibrado = 0;
    e->altura_parado = 0.0f;
    e->abajo = 0;
    e->candidato = 0;
    e->tiempo_candidato = 0.0f;
    e->hay_previa = 0;
    e->altura_previa = 0.0f;
    e->velocidad = 0.0f;
    e->minimo = 0.0f;
}

Accion logica_paso(const Config *c, Estado *e, float altura, float dt)
{
    float bajada;
    int quiere;

    if (c->altura_fija > 0.0f) {
        e->altura_parado = c->altura_fija;
        e->calibrado = 1;
    } else if (!e->calibrado) {
        e->altura_parado = altura;
        e->calibrado = 1;
    } else if (altura > e->altura_parado) {
        /* La referencia sube enseguida (te paraste más derecho)... */
        e->altura_parado = altura;
    } else if (!e->abajo && e->altura_parado - altura < c->bajada_parar) {
        /* ...y baja muy despacio solo mientras estás parado, para corregir
         * deriva del tracking sin afectar los ratos agachado. */
        e->altura_parado -= c->olvido * dt;
        if (e->altura_parado < altura)
            e->altura_parado = altura;
    }

    /* Velocidad vertical suavizada (unos 30 ms) para no esperar a bajar
     * todo el umbral cuando el movimiento ya es claramente agacharse. */
    if (e->hay_previa && dt > 0.0f) {
        float v = (altura - e->altura_previa) / dt;
        float a = dt / (0.03f + dt);
        e->velocidad += a * (v - e->velocidad);
    }
    e->hay_previa = 1;
    e->altura_previa = altura;

    if (e->abajo && altura < e->minimo)
        e->minimo = altura;

    bajada = e->altura_parado - altura;
    if (c->velocidad > 0.0f && !e->abajo && e->velocidad < -c->velocidad && bajada > c->bajada_rapida) {
        e->abajo = 1;
        e->minimo = altura;
        e->candidato = 1;
        e->tiempo_candidato = 0.0f;
        return AGACHAR;
    }
    /* Subiendo rápido: cuenta desde lo más bajo que llegaste, no desde la
     * altura de parado (si no, recién se para cuando ya casi estás arriba). */
    if (c->velocidad > 0.0f && e->abajo && e->velocidad > c->velocidad &&
        altura - e->minimo > c->subida_rapida) {
        e->abajo = 0;
        e->candidato = 0;
        e->tiempo_candidato = 0.0f;
        return c->parar_solo ? PARAR : NADA;
    }
    if (e->abajo)
        quiere = bajada > c->bajada_parar;      /* sigue abajo hasta subir bastante */
    else
        quiere = bajada > c->bajada_agachar;    /* baja recién pasando el umbral */

    /* Si te estás moviendo en sentido contrario, no se cambia por umbral:
     * por ejemplo, recién parado por velocidad pero todavía abajo del umbral. */
    if (c->velocidad > 0.0f && quiere != e->abajo &&
        ((quiere && e->velocidad > 0.1f) || (!quiere && e->velocidad < -0.1f)))
        quiere = e->abajo;

    if (quiere == e->abajo) {
        e->candidato = e->abajo;
        e->tiempo_candidato = 0.0f;
        return NADA;
    }

    if (quiere != e->candidato) {
        e->candidato = quiere;
        e->tiempo_candidato = 0.0f;
    }
    e->tiempo_candidato += dt;
    if (e->tiempo_candidato < c->demora)
        return NADA;

    e->abajo = quiere;
    e->minimo = altura;
    e->tiempo_candidato = 0.0f;
    if (e->abajo)
        return AGACHAR;
    return c->parar_solo ? PARAR : NADA;
}
