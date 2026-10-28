/*
 * HC-SR04 sin espera activa (req. 4.1).
 *
 *  1. hcsr04Disparar(): pulso de 10 us en TRIG (lo unico "bloqueante": 10 us)
 *  2. La interrupcion de ECHO (ambos flancos) mide el ancho del pulso
 *  3. hcsr04Estado() dice si ya hay medicion, si sigue esperando o si no volvio eco
 *  4. hcsr04LeerMm() devuelve la ultima distancia valida
 *
 * distancia [mm] = ancho [us] * 343 m/s / 2  ~= ancho / 5.83
 * Entre disparos dejar >= 60 ms (lo pide el datasheet para que se apaguen los ecos).
 *
 * ECHO sale a 5 V: SIEMPRE por el divisor 1k / 2k antes de LCD2.
 */
#ifndef HCSR04_H
#define HCSR04_H

#include <stdint.h>

#define HCSR04_TIMEOUT_US   38000   /* el modulo baja ECHO a los ~38 ms si no hay eco */
#define HCSR04_PERIODO_MIN_MS 60

typedef enum {
   HCSR04_INACTIVO,     /* nunca se disparo */
   HCSR04_MIDIENDO,     /* esperando el eco */
   HCSR04_LISTO,        /* hay una medicion nueva */
   HCSR04_SIN_ECO       /* timeout: nada en rango o sensor desconectado */
} hcsr04Estado_t;

void           hcsr04Init(void);
uint8_t        hcsr04Disparar(void);   /* 0 si todavia estaba midiendo */
hcsr04Estado_t hcsr04Estado(void);
uint32_t       hcsr04LeerMm(void);     /* ultima medicion valida; consume LISTO o SIN_ECO */
uint32_t       hcsr04AnchoUs(void);    /* ancho crudo del pulso, para depurar */

#endif
