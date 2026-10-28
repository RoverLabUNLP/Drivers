/*
 * Interrupciones por flanco en un pin GPIO (PININT del LPC4337).
 * La sAPI no las trae, asi que se hace con LPCOpen.
 *
 * Uso:
 *    gpioInit(PIN_ENC_IZQ, GPIO_INPUT);                 // configura el pin con sAPI
 *    gpioIrqConfig(0, 3, 7, GPIO_IRQ_SUBIDA, miCallback); // canal 0 <- GPIO3[7]
 *
 * El callback corre DENTRO de la interrupcion: tiene que ser corto
 * (tomar el tiempo, contar, guardar). Recibe el nivel del pin despues
 * del flanco (1 = subio, 0 = bajo).
 */
#ifndef GPIO_IRQ_H
#define GPIO_IRQ_H

#include <stdint.h>

typedef enum {
   GPIO_IRQ_SUBIDA,
   GPIO_IRQ_BAJADA,
   GPIO_IRQ_AMBOS
} gpioIrqFlanco_t;

typedef void (*gpioIrqCallback_t)(uint8_t nivel);

void gpioIrqConfig(uint8_t canal, uint8_t puerto, uint8_t bit,
                   gpioIrqFlanco_t flanco, gpioIrqCallback_t cb);
void gpioIrqDeshabilitar(uint8_t canal);

#endif
