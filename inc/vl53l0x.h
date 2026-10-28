/*
 * Driver del sensor ToF VL53L0X por I2C (sin la API de ST).
 *
 * Es un port a C de la libreria de Pololu (github.com/pololu/vl53l0x-arduino),
 * que a su vez reproduce la secuencia de inicializacion de la API oficial de ST.
 *
 * Varios sensores en el mismo bus: todos arrancan en 0x29. Con XSHUT se
 * apagan todos y se prenden de a uno, cambiandole la direccion a cada uno
 * antes de prender el siguiente (vl53l0xIniciarVarios).
 * La direccion nueva se pierde al cortar la alimentacion o al bajar XSHUT.
 *
 * Lectura no bloqueante: modo continuo + vl53l0xLeer(), que devuelve 1
 * solo cuando hay una medicion nueva.
 *
 * XSHUT: salida en 0 = apagado; entrada sin pull-up = encendido (la placa
 * del modulo lo sube a 2,8 V). Nunca se le mete 3,3 V desde la EDU-CIAA.
 */
#ifndef VL53L0X_H
#define VL53L0X_H

#include <stdint.h>
#include "sapi.h"

#define VL53L0X_DIR_DEFAULT   0x29
#define VL53L0X_SIN_XSHUT     ((gpioMap_t)0xFF)
#define VL53L0X_FUERA_RANGO   8190    /* el sensor devuelve 8190/8191 sin objeto */

typedef struct {
   uint8_t   dir;            /* direccion I2C actual (7 bits) */
   gpioMap_t xshut;          /* pin de XSHUT o VL53L0X_SIN_XSHUT */
   uint8_t   stopVariable;   /* lo usa la secuencia interna de ST */
   uint32_t  presupuestoUs;  /* measurement timing budget */
   uint8_t   ok;             /* 1 si el init termino bien */
   uint8_t   timeout;        /* 1 si la ultima operacion dio timeout */
} vl53l0x_t;

/* Comprueba que en 'dir' responda un VL53L0X (registro 0xC0 = 0xEE) */
uint8_t  vl53l0xPresente(uint8_t dir);

/* Inicializa un sensor que ya esta prendido en dev->dir. Devuelve 1 si OK. */
uint8_t  vl53l0xInit(vl53l0x_t *dev);

/* Cambia la direccion I2C de un sensor ya prendido */
void     vl53l0xCambiarDireccion(vl53l0x_t *dev, uint8_t nueva);

/* Apaga todos con XSHUT, los prende de a uno, les asigna dirs[i] y los
 * inicializa. Devuelve cuantos quedaron OK. */
uint8_t  vl53l0xIniciarVarios(vl53l0x_t *devs, const gpioMap_t *xshuts,
                              const uint8_t *dirs, uint8_t n);

void     vl53l0xApagar(gpioMap_t xshut);
void     vl53l0xPrender(gpioMap_t xshut);

/* Tiempo de cada medicion: mas tiempo = mas precision. Minimo 20000 us.
 * Por defecto queda en ~33 ms (~30 Hz). */
uint8_t  vl53l0xPresupuesto(vl53l0x_t *dev, uint32_t us);

/* periodoMs = 0: mide lo mas rapido que puede (back-to-back) */
void     vl53l0xContinuo(vl53l0x_t *dev, uint32_t periodoMs);
void     vl53l0xDetener(vl53l0x_t *dev);

/* No bloqueante: 1 si habia medicion nueva (y la deja en *mm) */
uint8_t  vl53l0xLeer(vl53l0x_t *dev, uint16_t *mm);

/* Bloqueante (hasta ~presupuesto): una medicion suelta */
uint16_t vl53l0xMedirUnaVez(vl53l0x_t *dev);

#endif
