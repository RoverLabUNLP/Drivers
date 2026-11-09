/*
 * Auto remoto - Taller de Proyecto I (UNLP)
 * Mapa de pines de la EDU-CIAA (PCB de 90 mm, ruteo con 5 puentes, 09/11/2026)
 *
 * Es el UNICO lugar donde se nombran pines. Si cambia el esquematico,
 * se cambia aca y nada mas.
 *
 * Los pines se eligieron para que cada senal salga de la tira del lado de su
 * conector y en el mismo orden (asi la placa casi no necesita puentes).
 * Puertos GPIOx[y] verificados con sapi_gpio.c (firmware_v3) para PININT.
 */
#ifndef PINES_H
#define PINES_H

#include "sapi.h"

/* ---------------- L298N (todo en P1 = CON1, columna interna) ---------------- */
#define PIN_PWM_IZQ      PWM0     /* T_FIL1 = P4_1 = CTOUT_1  (CON1_36) -> ENA */
#define PIN_PWM_DER      PWM5     /* T_COL1 = P7_4 = CTOUT_13 (CON1_31) -> ENB */
#define PIN_IN1          T_COL0   /* P1_5  (CON1_39) */
#define PIN_IN2          T_FIL2   /* P4_2  (CON1_37) */
#define PIN_IN3          T_FIL3   /* P4_3  (CON1_35) */
#define PIN_IN4          T_FIL0   /* P4_0  (CON1_33) */

/* ---------------- Encoders LM393 (salida DO) - sin cambios ---------------- */
#define PIN_ENC_IZQ      GPIO7    /* P6_11 = GPIO3[7] (CON2_38) */
#define ENC_IZQ_PUERTO   3
#define ENC_IZQ_BIT      7
#define PIN_ENC_DER      GPIO8    /* P6_12 = GPIO2[8] (CON2_40) */
#define ENC_DER_PUERTO   2
#define ENC_DER_BIT      8

/* ---------------- HC-SR04 ---------------- */
#define PIN_TRIG         GPIO3    /* P6_7  = GPIO5[15] (CON2_34) */
#define PIN_ECHO         GPIO5    /* P6_9  = GPIO3[5]  (CON2_36)  (por divisor 1k/2k!) */
#define ECHO_PUERTO      3
#define ECHO_BIT         5

/* ---------------- Baterias (un divisor 100k / 33k por pack) ----------------
 * En el esquematico: CH2 (CON1_11) = BAT_MOT_ADC, CH1 (CON1_13) = BAT_LOG_ADC.
 * (La version anterior de este archivo los tenia al reves.)                  */
#define PIN_BAT_MOTORES     CH2   /* ADC0_2 = P1 pin 11 (BAT_MOT_ADC) */
#define PIN_BAT_ELECTRONICA CH1   /* ADC0_1 = P1 pin 13 (BAT_LOG_ADC) */
#define PIN_BATERIA         PIN_BAT_MOTORES   /* compatibilidad */

/* ---------------- I2C: VL53L0X x3 (+ GY-521) ---------------- */
#define BUS_I2C          I2C0     /* pines I2C_SDA / I2C_SCL (CON1_19 / CON1_21) */
#define PIN_XSHUT1       LCD3     /* P4_6  (CON2_26) -> J6 */
#define PIN_XSHUT2       LCD2     /* P4_5  (CON2_28) -> J7 */
#define PIN_XSHUT3       LCD1     /* P4_4  (CON2_30) -> J8 */
/* 400 kHz: lo soportan los VL53L0X y el MPU6050. Si con cables largos de
 * protoboard aparecen errores, bajar a 100000. */
#define I2C_VELOCIDAD    400000

/* Avisos de "medicion lista" de los VL53L0X: EN LA PCB NO ESTAN CONECTADOS.
 * Los ToF se leen por consulta (polling). Las definiciones quedan apuntando a
 * pines libres solo para que compilen los tests viejos que las nombran;
 * no usar el modo con interrupcion de los ToF con esta placa.               */
#define TOF_INT_EN_PCB   0
#define PIN_TOF1_INT     LCD4     /* libre (CON2_22) = GPIO5[14] */
#define TOF1_INT_PUERTO  5
#define TOF1_INT_BIT     14
#define PIN_TOF2_INT     LCDRS    /* libre (CON2_24) = GPIO5[12] */
#define TOF2_INT_PUERTO  5
#define TOF2_INT_BIT     12
#define PIN_TOF3_INT     LCDEN    /* libre (CON2_23) = GPIO5[13] */
#define TOF3_INT_PUERTO  5
#define TOF3_INT_BIT     13

/* GY-521 (MPU6050) en el mismo bus, direccion 0x68 (AD0 queda sin conectar:
 * el modulo trae pull-down). Su pin INT ("dato listo", activo en alto): */
#define PIN_MPU_INT      GPIO1    /* P6_4  = GPIO3[3] (CON2_32) */
#define MPU_INT_PUERTO   3
#define MPU_INT_BIT      3

/* ---------------- UART con la ESP32-CAM - sin cambios ---------------- */
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

/* Prioridad de las interrupciones de los drivers (GPIO y UART). Con FreeRTOS,
 * una ISR que llama a funciones ...FromISR tiene que tener prioridad numerica
 * >= configMAX_SYSCALL_INTERRUPT_PRIORITY (5 en la configuracion de la CIAA). */
#define IRQ_PRIORIDAD_DRIVERS  5

#endif
