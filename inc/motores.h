/*
 * Driver de motores con el L298N.
 * Canal A (ENA, IN1, IN2) = lado izquierdo, canal B (ENB, IN3, IN4) = lado derecho.
 *
 * PWM con la sAPI: frecuencia fija de 1 kHz, resolucion 0..255.
 *
 *   IN_a IN_b  EN   resultado
 *    1    0   PWM   adelante
 *    0    1   PWM   atras
 *    =    =    1    freno activo
 *    x    x    0    rueda libre
 *
 * Si un lado gira al reves de lo esperado, cambiar MOTOR_INVERTIR_IZQ/DER
 * en vez de cruzar cables.
 */
#ifndef MOTORES_H
#define MOTORES_H

#include <stdint.h>

#define MOTOR_INVERTIR_IZQ  0
#define MOTOR_INVERTIR_DER  0

typedef enum { MOTOR_IZQ = 0, MOTOR_DER = 1 } motor_t;

void   motoresInit(void);                       /* deja los dos en rueda libre */
void   motorVelocidad(motor_t m, int16_t pct);  /* -100..100 (signo = sentido) */
void   motorFrenar(motor_t m);                  /* freno activo */
void   motorLibre(motor_t m);                   /* rueda libre */
void   motoresParar(void);                      /* los dos en rueda libre */
int16_t motorLeerVelocidad(motor_t m);          /* ultimo valor pedido */

#endif
