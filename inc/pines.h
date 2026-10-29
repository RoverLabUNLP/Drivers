/*
 * Auto remoto - Taller de Proyecto I (UNLP)
 * Mapa de pines de la EDU-CIAA (esquematico Rev A, hoja 2, 19/10/2026)
 *
 * Es el UNICO lugar donde se nombran pines. Si cambia el esquematico,
 * se cambia aca y nada mas.
 *
 * Los puertos GPIO (GPIOx[y]) se verificaron con el datasheet del LPC4337
 * y se usan para las interrupciones por pin (PININT), que la sAPI no tiene.
 */
#ifndef PINES_H
#define PINES_H

#include "sapi.h"

/* ---------------- L298N ---------------- */
#define PIN_PWM_IZQ      PWM0     /* T_FIL1 = P4_1 = CTOUT_1 -> ENA */
#define PIN_PWM_DER      PWM4     /* T_FIL3 = P4_3 = CTOUT_3 -> ENB */
#define PIN_IN1          GPIO0    /* P6_1 */
#define PIN_IN2          GPIO1    /* P6_4 */
#define PIN_IN3          GPIO2    /* P6_5 */
#define PIN_IN4          GPIO3    /* P6_7 */

/* ---------------- Encoders LM393 (salida DO) ---------------- */
#define PIN_ENC_IZQ      GPIO7    /* P6_11 = GPIO3[7] */
#define ENC_IZQ_PUERTO   3
#define ENC_IZQ_BIT      7
#define PIN_ENC_DER      GPIO8    /* P6_12 = GPIO2[8] */
#define ENC_DER_PUERTO   2
#define ENC_DER_BIT      8

/* ---------------- HC-SR04 ---------------- */
#define PIN_TRIG         LCD1     /* P4_4 = GPIO2[4] */
#define PIN_ECHO         LCD2     /* P4_5 = GPIO2[5]  (por divisor 1k/2k!) */
#define ECHO_PUERTO      2
#define ECHO_BIT         5

/* ---------------- Bateria ---------------- */
#define PIN_BATERIA      CH1      /* ADC0_1, divisor 100k / 33k */

/* ---------------- I2C: VL53L0X x3 (+ GY-521) ---------------- */
#define BUS_I2C          I2C0     /* pines I2C_SDA / I2C_SCL */
#define PIN_XSHUT1       GPIO4    /* P6_8  */
#define PIN_XSHUT2       GPIO5    /* P6_9  */
#define PIN_XSHUT3       GPIO6    /* P6_10 */
/* 400 kHz: lo soportan los VL53L0X y el MPU6050. Si con cables largos de
 * protoboard aparecen errores, bajar a 100000. */
#define I2C_VELOCIDAD    400000

/* Avisos de "medicion lista" (pin G = GPIO1 de cada VL53L0X, activo en bajo) */
#define PIN_TOF1_INT     LCD3     /* P4_6  = GPIO2[6]  (CON2_26) */
#define TOF1_INT_PUERTO  2
#define TOF1_INT_BIT     6
#define PIN_TOF2_INT     LCD4     /* P4_10 = GPIO5[14] (CON2_22) */
#define TOF2_INT_PUERTO  5
#define TOF2_INT_BIT     14
#define PIN_TOF3_INT     LCDRS    /* P4_8  = GPIO5[12] (CON2_24) */
#define TOF3_INT_PUERTO  5
#define TOF3_INT_BIT     12

/* GY-521 (MPU6050) en el mismo bus, direccion 0x68 (AD0 a GND).
 * Su pin INT ("dato listo", activo en alto) va a LCD_EN. */
#define PIN_MPU_INT      LCDEN    /* P4_9  = GPIO5[13] (CON2_23) */
#define MPU_INT_PUERTO   5
#define MPU_INT_BIT      13

/* ---------------- UART con la ESP32-CAM ---------------- */
#define UART_ESP         UART_232 /* RS232_TXD = P2_3, RS232_RXD = P2_4 (USART3) */
#define UART_ESP_BAUD    115200

/* ---------------- Canales de interrupcion por pin (0..7) ----------------
 * El LPC4337 tiene 8. Cada canal tiene su handler GPIOn_IRQHandler.       */
#define IRQ_CANAL_ENC_IZQ  0
#define IRQ_CANAL_ENC_DER  1
#define IRQ_CANAL_ECHO     2
#define IRQ_CANAL_TOF1     3
#define IRQ_CANAL_TOF2     4
#define IRQ_CANAL_TOF3     5
#define IRQ_CANAL_MPU      6
/* 7 queda libre */

#endif
