/* Lógica pura de "agacharse en la vida real": decide cuándo agachar o
 * parar a Ripley a partir de la altura del visor. No toca el juego, así se
 * puede probar fuera de Windows. */
#ifndef LOGICA_H
#define LOGICA_H

typedef struct {
    float bajada_agachar;   /* metros bajo la altura parado para agacharse */
    float bajada_parar;     /* metros bajo la altura parado para volver a pararse */
    float demora;           /* segundos que hay que sostener la postura */
    float altura_fija;      /* 0 = calibración automática */
    float olvido;           /* m/s que baja la altura de referencia estando parado */
    int   parar_solo;       /* 1 = también para a Ripley al levantarte */
    float velocidad;        /* m/s: bajando (o subiendo) así de rápido no se espera la demora; 0 = no */
    float bajada_rapida;    /* metros que alcanzan para agachar si bajás rápido */
    float subida_rapida;    /* metros desde lo más bajo que alcanzan para parar si subís rápido */
} Config;

typedef struct {
    int   calibrado;
    float altura_parado;    /* referencia de altura parado */
    int   abajo;            /* postura física decidida (con histéresis) */
    int   candidato;        /* postura que se está esperando confirmar */
    float tiempo_candidato;
    int   hay_previa;
    float altura_previa;
    float velocidad;        /* m/s suavizada, negativa bajando */
    float minimo;           /* altura más baja desde que quedó abajo */
} Estado;

typedef enum { NADA = 0, AGACHAR = 1, PARAR = 2 } Accion;

void logica_config_defecto(Config *c);
void logica_reset(Estado *e);

/* altura: metros (eje Y del visor). dt: segundos desde la última llamada.
 * Devuelve AGACHAR o PARAR solo en el momento en que cambia la postura
 * física, así el botón de agacharse sigue funcionando normal el resto del
 * tiempo. Quien llama decide si el juego está en condiciones de hacerlo. */
Accion logica_paso(const Config *c, Estado *e, float altura, float dt);

#endif
