/*
 * Medicion de los dos packs 2S por el ADC, cada uno con su divisor 100k / 33k
 * (esquematico Rev B):
 *    CH1 = pack 1, motores      (BAT_MOT_ADC)
 *    CH2 = pack 2, electronica  (BAT_LOG_ADC)
 *   V_pin = V_bat * 33 / 133      (8,4 V -> 2,08 V en el pin)
 * ADC de 10 bits con referencia de 3,3 V.
 *
 * Una 18650 no tiene que bajar de ~3,0 V: con 2 en serie, ~6,0 V por pack
 * (BAT_MINIMO_MV). Por debajo de eso hay que parar y cargar.
 *
 * Para calibrar: medir V_bat con el tester, comparar con lo que da
 * bateriaMilivoltsCanal() y ajustar BAT_R_ARRIBA / BAT_R_ABAJO con los valores
 * medidos de las resistencias (o BAT_VREF_MV).
 */
#ifndef BATERIA_H
#define BATERIA_H

#include <stdint.h>
#include "sapi.h"

#define BAT_VREF_MV      3300u
#define BAT_R_ARRIBA     100000u
#define BAT_R_ABAJO      33000u
#define BAT_PROMEDIO     8          /* lecturas promediadas por medicion */
#define BAT_MINIMO_MV    6000u      /* 3,0 V por celda */

void     bateriaInit(void);
uint16_t bateriaCrudoCanal(adcMap_t canal);        /* 0..1023, promediado */
uint32_t bateriaPinMilivoltsCanal(adcMap_t canal); /* tension en el pin del ADC */
uint32_t bateriaMilivoltsCanal(adcMap_t canal);    /* tension del pack */

/* Atajos para cada pack */
uint32_t bateriaMotoresMv(void);       /* CH1 */
uint32_t bateriaElectronicaMv(void);   /* CH2 */

/* Compatibilidad con la version de un solo canal: leen CH1 (pack de motores) */
uint16_t bateriaCrudo(void);
uint32_t bateriaPinMilivolts(void);
uint32_t bateriaMilivolts(void);

#endif
