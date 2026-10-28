/*
 * Medicion de la bateria 2S por el ADC (CH1) con divisor 100k / 33k.
 *   V_pin = V_bat * 33 / 133      (8,4 V -> 2,08 V en el pin)
 * ADC de 10 bits con referencia de 3,3 V.
 *
 * Para calibrar: medir V_bat con el tester, comparar con lo que da
 * bateriaMilivolts() y ajustar BAT_R_ARRIBA / BAT_R_ABAJO con los valores
 * medidos de las resistencias (o BAT_VREF_MV).
 */
#ifndef BATERIA_H
#define BATERIA_H

#include <stdint.h>

#define BAT_VREF_MV      3300u
#define BAT_R_ARRIBA     100000u
#define BAT_R_ABAJO      33000u
#define BAT_PROMEDIO     8          /* lecturas promediadas por medicion */

void     bateriaInit(void);
uint16_t bateriaCrudo(void);        /* 0..1023, promediado */
uint32_t bateriaPinMilivolts(void); /* tension en el pin del ADC */
uint32_t bateriaMilivolts(void);    /* tension de la bateria */

#endif
