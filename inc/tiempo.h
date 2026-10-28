/*
 * Base de tiempo en microsegundos con el contador de ciclos del Cortex-M4 (DWT).
 *
 * El contador da la vuelta cada ~21 s (2^32 / 204 MHz). Por eso los
 * instantes se guardan en CICLOS y solo se convierte a us la DIFERENCIA:
 *    uint32_t t0 = tiempoCiclos();  ...  uint32_t us = tiempoUsDesde(t0);
 * La resta en uint32_t funciona aunque el contador haya dado la vuelta,
 * siempre que el intervalo medido sea menor a ~21 s.
 */
#ifndef TIEMPO_H
#define TIEMPO_H

#include <stdint.h>

void     tiempoInit(void);
uint32_t tiempoCiclos(void);
uint32_t tiempoCiclosAUs(uint32_t ciclos);
uint32_t tiempoUsDesde(uint32_t t0Ciclos);
void     tiempoEsperarUs(uint32_t us);   /* espera activa, solo para pulsos cortos */

#endif
