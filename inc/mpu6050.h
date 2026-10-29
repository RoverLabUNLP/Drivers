/*
 * Driver de la IMU MPU6050 (modulo GY-521) por I2C.
 *
 * Acelerometro y giroscopio de 3 ejes + temperatura interna.
 * Comparte el bus I2C0 con los VL53L0X (direccion 0x68 con AD0 a GND).
 *
 * Configuracion que deja mpu6050Init:
 *  - reloj del PLL del giroscopio X (mas estable que el oscilador interno)
 *  - filtro pasabajos digital (DLPF) de ~44 Hz
 *  - una muestra nueva cada 10 ms (100 Hz)
 *  - aviso de "dato listo" en el registro INT_STATUS y en el pin INT
 *    (el pin es opcional: MPU_INT -> LCD_EN en el esquematico)
 *
 * Lectura no bloqueante: mpu6050Leer() devuelve 1 solo cuando hay una
 * muestra nueva. Resultados en enteros, sin float:
 *    aceleracion en mg (1000 mg = 1 g), giro en mdps (1000 mdps = 1 grado/s),
 *    temperatura en centesimas de grado C.
 *
 * Calibracion (los dos sensores traen errores fijos de fabrica):
 *  - giroscopio: unos grados/s aun quieto. mpu6050CalibrarGiro() lo mide con el
 *    sensor QUIETO (detecta si se movio) y lo resta en cada lectura. Conviene
 *    hacerla en cada arranque, porque cambia con la temperatura.
 *  - acelerometro: error de cero (hasta +-80 mg en Z segun datasheet) y de escala
 *    (+-3 %). Se mide una vez con 6 posiciones (prueba 7, tecla 'a') y los valores
 *    se cargan con mpu6050FijarCalAcc().
 */
#ifndef MPU6050_H
#define MPU6050_H

#include <stdint.h>
#include "sapi.h"

#define MPU6050_DIR_DEFAULT  0x68   /* AD0 a GND (0x69 con AD0 a 3,3 V) */

typedef enum { MPU6050_ACC_2G = 0, MPU6050_ACC_4G, MPU6050_ACC_8G, MPU6050_ACC_16G } mpu6050RangoAcc_t;
typedef enum { MPU6050_GIRO_250 = 0, MPU6050_GIRO_500, MPU6050_GIRO_1000, MPU6050_GIRO_2000 } mpu6050RangoGiro_t;

typedef struct {
   uint8_t  dir;            /* direccion I2C */
   uint8_t  ok;             /* 1 si el init termino bien */
   uint8_t  id;             /* valor leido de WHO_AM_I */
   uint16_t lsbPorG;        /* cuentas por g del acelerometro (nominal) */
   uint16_t lsbPorDpsX10;   /* cuentas por grado/s del giroscopio, x10 */
   int16_t  offGiro[3];     /* error del giroscopio en reposo (cuentas) */
   int16_t  offAcc[3];      /* error del acelerometro en 0 g (cuentas) */
   int16_t  cnt1gAcc[3];    /* cuentas reales que mide cada eje con 1 g */
   /* Modo por interrupcion (los completa mpu6050HabilitarIrq) */
   uint8_t  usaIrq;
   uint8_t  canalIrq;
   volatile uint8_t  listo;     /* la ISR avisa que hay muestra nueva */
   volatile uint32_t tAviso;    /* instante del aviso (ciclos, ver tiempo.h) */
   volatile uint32_t perdidas;  /* avisos que llegaron antes de leer el anterior */
} mpu6050_t;

typedef enum {
   MPU6050_CAL_OK = 0,
   MPU6050_CAL_MOVIDO,      /* el sensor se movio durante la medicion: no se guardo nada */
   MPU6050_CAL_SIN_DATOS    /* el sensor dejo de responder */
} mpu6050Cal_t;

typedef struct {            /* lo que entrega el sensor, sin convertir */
   int16_t acc[3];          /* X, Y, Z */
   int16_t giro[3];
   int16_t temp;
} mpu6050Crudo_t;

typedef struct {            /* ya convertido (y con el offset del giro restado) */
   int32_t acc_mg[3];       /* X, Y, Z */
   int32_t giro_mdps[3];
   int32_t temp_cC;         /* centesimas de grado C */
   uint32_t t_ciclos;       /* instante de la muestra (ciclos): con interrupcion es
                               el del aviso del sensor; con polling, el de la lectura.
                               dt entre dos muestras = tiempoCiclosAUs(t2 - t1) */
} mpu6050Datos_t;

/* Lee WHO_AM_I. Devuelve 1 si responde como un MPU6050 (0x68) o un clon conocido. */
uint8_t mpu6050Presente(uint8_t dir, uint8_t *id);

/* Despierta y configura el sensor. El bus I2C ya tiene que estar iniciado. */
uint8_t mpu6050Init(mpu6050_t *imu, uint8_t dir, mpu6050RangoAcc_t acc, mpu6050RangoGiro_t giro);

/* 1 si hay una muestra nueva (lee INT_STATUS) */
uint8_t mpu6050DatoListo(mpu6050_t *imu);

/* Lee una muestra cruda (14 bytes de una vez), haya o no dato nuevo */
void    mpu6050LeerCrudo(mpu6050_t *imu, mpu6050Crudo_t *c);

/* No bloqueante: 1 si habia muestra nueva y la deja convertida en *d.
 * Sin interrupcion pregunta por I2C (INT_STATUS); con interrupcion solo mira
 * el aviso de la ISR y va al I2C cuando hay algo para leer. */
uint8_t mpu6050Leer(mpu6050_t *imu, mpu6050Datos_t *d);

/* Modo por interrupcion: el pin INT del sensor ("dato listo", activo en alto,
 * pulso de 50 us cada 10 ms) entra por 'pin' (= GPIOpuerto[bit]) al canal de
 * interrupcion 'canal'. La ISR solo guarda el instante y levanta un aviso; la
 * lectura por I2C la hace mpu6050Leer() desde el programa (o desde una tarea de
 * FreeRTOS: ahi conviene que el aviso despierte a la tarea).
 * Llamar despues de mpu6050Init. Antes de que 'imu' deje de existir, llamar a
 * mpu6050DeshabilitarIrq: la ISR guarda un puntero a 'imu'. */
void    mpu6050HabilitarIrq(mpu6050_t *imu, gpioMap_t pin, uint8_t puerto, uint8_t bit, uint8_t canal);
void    mpu6050DeshabilitarIrq(mpu6050_t *imu);

/* Bloqueante (~0,2 s + muestras x 10 ms): promedia el giroscopio QUIETO y guarda
 * el offset. Si detecta movimiento devuelve MPU6050_CAL_MOVIDO y deja el anterior. */
mpu6050Cal_t mpu6050CalibrarGiro(mpu6050_t *imu, uint16_t muestras);

/* Bloqueante: promedio crudo del acelerometro con el sensor QUIETO (para la
 * calibracion de 6 posiciones). Devuelve MPU6050_CAL_MOVIDO si se movio. */
mpu6050Cal_t mpu6050PromedioAcc(mpu6050_t *imu, int32_t prom[3], uint16_t muestras);

/* Carga la calibracion del acelerometro (por ejemplo, valores medidos antes y
 * copiados en el codigo). Por eje: offset en cuentas y cuentas que mide con 1 g.
 * Con 6 posiciones: off = (arriba + abajo) / 2,  cnt1g = (arriba - abajo) / 2 */
void    mpu6050FijarCalAcc(mpu6050_t *imu, const int16_t off[3], const int16_t cnt1g[3]);

#endif
