/*
 * Auto remoto - Taller de Proyecto I (UNLP)
 * Programa del auto (sin el menu de pruebas): arranca solo al darle energia.
 *
 *  - Motores: los maneja la web con $TWIST,<v>,<w> (v y w entre -1 y 1).
 *      izq = v - w, der = v + w (1.00 = 100 % de PWM). Si pasan 500 ms sin
 *      $TWIST, para los motores.
 *  - Telemetria a la ESP32 cada 100 ms:
 *      $SENS,<distancia mas cercana: 3 VL53L0X y HC-SR04 [cm]>,<pack mas bajo [V]>
 *      $ODOM,<pulsos izq>,<pulsos der>
 *      $IMU,<giro Z [grados/s]>,0,<temperatura [C]>
 *  - Consola USB (115200, opcional): estado al arrancar y una linea por segundo.
 *
 * Al arrancar calibra el giroscopio: dejar el auto quieto unos 2 s.
 *
 * Lectura: VL53L0X por polling (en la PCB sus pines G no estan conectados);
 * IMU por interrupcion de "dato listo" (INT -> GPIO1).
 */
#include "sapi.h"
#include "pines.h"
#include "tiempo.h"
#include "consola.h"
#include "motores.h"
#include "encoders.h"
#include "hcsr04.h"
#include "vl53l0x.h"
#include "mpu6050.h"
#include "bateria.h"
#include "enlace_esp.h"

#define WATCHDOG_MS     500     /* sin $TWIST por mas de esto -> motores parados */
#define PERIODO_TELE_MS 100
#define PERIODO_HC_MS   100

static const gpioMap_t xshuts[3]  = { PIN_XSHUT1, PIN_XSHUT2, PIN_XSHUT3 };
static const uint8_t   dirs[3]    = { 0x30, 0x31, 0x32 };
#if TOF_INT_EN_PCB
static const gpioMap_t ints[3]    = { PIN_TOF1_INT, PIN_TOF2_INT, PIN_TOF3_INT };
static const uint8_t   puertos[3] = { TOF1_INT_PUERTO, TOF2_INT_PUERTO, TOF3_INT_PUERTO };
static const uint8_t   bits[3]    = { TOF1_INT_BIT, TOF2_INT_BIT, TOF3_INT_BIT };
static const uint8_t   canales[3] = { IRQ_CANAL_TOF1, IRQ_CANAL_TOF2, IRQ_CANAL_TOF3 };
#endif

static vl53l0x_t tof[3];
static mpu6050_t imu;
static uint8_t   imuOk;

/* milesimas (1.000 = 100 %) -> porcentaje -100..100 */
static int16_t aPorcentaje(int32_t milesimas)
{
   int32_t p = milesimas / 10;
   if (p > 100)  p = 100;
   if (p < -100) p = -100;
   return (int16_t)p;
}

static void iniciarTodo(void)
{
   motoresInit();
   encodersInit();
   hcsr04Init();
   bateriaInit();
   enlaceInit();
   i2cInit(BUS_I2C, I2C_VELOCIDAD);

   imprimir("\r\n=== Auto remoto ===\r\n");

   vl53l0xIniciarVarios(tof, xshuts, dirs, 3);
   for (int i = 0; i < 3; i++) {
      imprimir("VL53L0X %d: %s\r\n", i + 1, tof[i].ok ? "OK" : "NO RESPONDE");
      if (!tof[i].ok) continue;
#if TOF_INT_EN_PCB
      vl53l0xHabilitarIrq(&tof[i], ints[i], puertos[i], bits[i], canales[i]);
#endif
      vl53l0xContinuo(&tof[i], 50);
   }

   imuOk = mpu6050Init(&imu, MPU6050_DIR_DEFAULT, MPU6050_ACC_2G, MPU6050_GIRO_500);
   if (imuOk) {
      mpu6050Cal_t r = MPU6050_CAL_MOVIDO;
      for (int k = 0; k < 3 && r == MPU6050_CAL_MOVIDO; k++) r = mpu6050CalibrarGiro(&imu, 200);
      imprimir("IMU: OK, giroscopio %s\r\n", r == MPU6050_CAL_OK ? "calibrado" : "SIN calibrar (se movio)");
      mpu6050HabilitarIrq(&imu, PIN_MPU_INT, MPU_INT_PUERTO, MPU_INT_BIT, IRQ_CANAL_MPU);
   } else {
      imprimir("IMU: NO RESPONDE\r\n");
   }
   imprimir("Listo: esperando comandos de la web.\r\n\r\n");
}

int main(void)
{
   boardConfig();
   consolaInit();
   tiempoInit();
   iniciarTodo();

   uint16_t tofMm[3] = { 0, 0, 0 };
   uint32_t usMm = 0;
   uint8_t  usValido = 0;
   mpu6050Datos_t d = { { 0, 0, 0 }, { 0, 0, 0 }, 0, 0 };
   enlaceTwist_t tw = { 0, 0, 0 };

   delay_t tTele, tHc, tSeg;
   delayConfig(&tTele, PERIODO_TELE_MS);
   delayConfig(&tHc, PERIODO_HC_MS);
   delayConfig(&tSeg, 1000);

   while (1) {
      consolaServicio();

      /* ---- comandos de la web ---- */
      enlaceProcesar();
      if (enlaceTwistNuevo(&tw)) {
         motorVelocidad(MOTOR_IZQ, aPorcentaje(tw.v - tw.w));
         motorVelocidad(MOTOR_DER, aPorcentaje(tw.v + tw.w));
      }
      if (enlaceTwistVencido(WATCHDOG_MS) &&
          (motorLeerVelocidad(MOTOR_IZQ) || motorLeerVelocidad(MOTOR_DER))) {
         motoresParar();
      }

      /* ---- sensores (no bloqueante) ---- */
      for (int i = 0; i < 3; i++) {
         uint16_t mm;
         if (tof[i].ok && vl53l0xLeer(&tof[i], &mm)) tofMm[i] = mm;
      }
      if (imuOk) mpu6050Leer(&imu, &d);
      hcsr04Estado_t e = hcsr04Estado();
      if (e == HCSR04_LISTO)        { usMm = hcsr04LeerMm(); usValido = 1; }
      else if (e == HCSR04_SIN_ECO) { hcsr04LeerMm(); usValido = 0; }
      if (delayRead(&tHc)) hcsr04Disparar();

      /* ---- telemetria a la web ---- */
      if (delayRead(&tTele)) {
         uint32_t cerca = 0xFFFFFFFFu;
         for (int i = 0; i < 3; i++)
            if (tof[i].ok && tofMm[i] && tofMm[i] < VL53L0X_FUERA_RANGO && tofMm[i] < cerca) cerca = tofMm[i];
         if (usValido && usMm < cerca) cerca = usMm;
         if (cerca == 0xFFFFFFFFu) cerca = 0;
         uint32_t bm = bateriaMotoresMv(), be = bateriaElectronicaMv();
         enlaceEnviarSens(cerca, bm < be ? bm : be);
         enlaceEnviarOdom((int32_t)encoderPulsos(ENC_IZQ), (int32_t)encoderPulsos(ENC_DER));
         enlaceEnviarImu(imuOk ? d.giro_mdps[2] : 0, 0, imuOk ? d.temp_cC * 10 : 0);
      }

      /* ---- estado por la consola (si hay USB) ---- */
      if (delayRead(&tSeg)) {
         uint32_t ms = enlaceMsDesdeTwist();
         imprimirCola("motores izq %4d%% der %4d%% | TOF %u %u %u mm | US %lu mm | enc %lu %lu | web %s\r\n",
                      motorLeerVelocidad(MOTOR_IZQ), motorLeerVelocidad(MOTOR_DER),
                      tofMm[0], tofMm[1], tofMm[2], usValido ? (unsigned long)usMm : 0ul,
                      (unsigned long)encoderPulsos(ENC_IZQ), (unsigned long)encoderPulsos(ENC_DER),
                      ms == 0xFFFFFFFFu ? "sin comandos" : (ms > WATCHDOG_MS ? "quieta" : "manejando"));
      }
   }
   return 0;
}
