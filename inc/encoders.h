/*
 * Encoders opticos de horquilla LM393 (salida DO digital) por interrupcion.
 *
 * Cada flanco de subida = una ranura del disco. Se guarda:
 *  - la cuenta total de pulsos (odometria)
 *  - el periodo entre los dos ultimos pulsos (velocidad, sirve bien a baja velocidad)
 *
 * Antirrebote por software, en dos partes:
 *  - una subida cuenta solo si la senal estuvo en bajo al menos ENC_MIN_BAJO_US
 *    (el comparador rebota cuando la ranura se tapa/destapa despacio, y eso
 *     metia un pulso extra en cada cambio: 2 pulsos por tapada)
 *  - ademas se descartan pulsos a menos de ENC_ANTIRREBOTE_US del anterior.
 * A 200 rpm con 20 ranuras hay un pulso cada 15 ms (~7 ms en bajo), asi que
 * estos valores no pierden pulsos reales.
 *
 * El encoder NO sabe el sentido de giro: la cuenta siempre sube.
 * El sentido lo pone quien lo usa, con el signo del PWM que mando.
 */
#ifndef ENCODERS_H
#define ENCODERS_H

#include <stdint.h>

#define ENC_RANURAS          20        /* ranuras del disco = pulsos por vuelta */
#define ENC_ANTIRREBOTE_US   1000
#define ENC_MIN_BAJO_US      2000      /* tiempo minimo en bajo antes de una subida valida */
#define ENC_TIMEOUT_US       500000    /* sin pulsos en 0,5 s -> velocidad 0 */

typedef enum { ENC_IZQ = 0, ENC_DER = 1 } encoder_t;

void     encodersInit(void);
uint32_t encoderPulsos(encoder_t e);         /* cuenta total desde el init/reset */
void     encoderReset(encoder_t e);
uint32_t encoderPeriodoUs(encoder_t e);      /* 0 si esta parado */
uint32_t encoderRpmX10(encoder_t e);         /* rpm * 10 (entero, sin float) */
uint32_t encoderRebotes(encoder_t e);        /* flancos descartados (diagnostico) */

#endif
