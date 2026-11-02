/*
 * Prueba 8: todo junto, con el auto armado y las alimentaciones reales.
 *
 * Los motores se manejan con el teclado (como en la prueba 4) y mientras tanto
 * se leen TODOS los sensores a la vez, con interrupciones, como en el firmware
 * final. Una vez por segundo se muestra un resumen.
 *
 * Que buscar (lo que las pruebas individuales no muestran):
 *  - ruido de los motores: pulsos o "reb" en los encoders con las ruedas quietas,
 *    VL53L0X o IMU que dejan de responder (I2C colgado) al arrancar los motores
 *  - caidas de tension: la bateria de electronica no tendria que moverse cuando
 *    arrancan los motores; si la ESP32 o la EDU-CIAA se reinician, falta corriente
 *  - el lazo principal: "lazo max" es la vuelta mas lenta del ultimo segundo;
 *    si pasa de ~10 ms, algo esta bloqueando y se pierden muestras de la IMU
 *
 * La consola usa la cola sin espera (imprimirCola) para no frenar los sensores.
 *
 * Telemetria real hacia la ESP32 cada 100 ms (driver enlace_esp), para verla en la web:
 *   $SENS,<distancia mas cercana entre los 3 VL53L0X y el HC-SR04 [cm]>,<pack mas bajo [V]>
 *   $ODOM,<pulsos izq>,<pulsos der>
 *   $IMU,<giro Z [grados/s]>,0,<temperatura [C]>     (significado A CONFIRMAR con la web)
 * Los $TWIST que llegan de la web se muestran pero NO mueven los motores (eso es la prueba 9).
 */
#include "tests.h"
#include "consola.h"
#include "pines.h"
#include "tiempo.h"
#include "motores.h"
#include "encoders.h"
#include "hcsr04.h"
#include "vl53l0x.h"
#include "mpu6050.h"
#include "bateria.h"
#include "enlace_esp.h"
#include <string.h>

#define AUTOPARADA_MS   5000
#define PERIODO_HC_MS   100

static const gpioMap_t xshutsI[3]   = { PIN_XSHUT1, PIN_XSHUT2, PIN_XSHUT3 };
static const uint8_t   dirsI[3]     = { 0x30, 0x31, 0x32 };
static const gpioMap_t intsI[3]     = { PIN_TOF1_INT, PIN_TOF2_INT, PIN_TOF3_INT };
static const uint8_t   puertosI[3]  = { TOF1_INT_PUERTO, TOF2_INT_PUERTO, TOF3_INT_PUERTO };
static const uint8_t   bitsI[3]     = { TOF1_INT_BIT, TOF2_INT_BIT, TOF3_INT_BIT };
static const uint8_t   canalesI[3]  = { IRQ_CANAL_TOF1, IRQ_CANAL_TOF2, IRQ_CANAL_TOF3 };

/* Valor con signo en milesimas -> "-12.3" (1 decimal) */
static void fmtX1000(char *b, int32_t v)
{
   char s = ' ';
   if (v < 0) { s = '-'; v = -v; }
   b[0] = s;
   int n = 1;
   int32_t ent = v / 1000, dec = (v % 1000) / 100;
   char tmp[12]; int k = 0;
   do { tmp[k++] = (char)('0' + ent % 10); ent /= 10; } while (ent && k < 10);
   while (k) b[n++] = tmp[--k];
   b[n++] = '.'; b[n++] = (char)('0' + dec); b[n] = 0;
}

void testIntegral(void)
{
   vl53l0x_t tof[3];
   mpu6050_t imu;
   mpu6050Datos_t d = { { 0, 0, 0 }, { 0, 0, 0 }, 0, 0 };
   uint8_t imuOk;
   int16_t vel = 50;
   const char *estado = "libre";

   imprimir("\r\n--- 8 · TODO JUNTO (auto armado, alimentaciones reales) ---\r\n"
            "Teclas de motores: w s a d | 1 2 un lado | + - velocidad | espacio freno | x libre\r\n"
            "Autoparada a los 5 s sin teclas. 'q' = volver.\r\n\r\n"
            "Iniciando (dejar el auto QUIETO para calibrar el giroscopio)...\r\n");

   /* ---- init de todo ---- */
   motoresInit();
   encodersInit();
   hcsr04Init();
   bateriaInit();
   enlaceInit();
   i2cInit(BUS_I2C, I2C_VELOCIDAD);

   uint8_t nTof = vl53l0xIniciarVarios(tof, xshutsI, dirsI, 3);
   for (int i = 0; i < 3; i++) {
      imprimir("  VL53L0X %d (0x%02X): %s\r\n", i + 1, dirsI[i], tof[i].ok ? "OK" : "NO RESPONDE");
      if (!tof[i].ok) continue;
      vl53l0xHabilitarIrq(&tof[i], intsI[i], puertosI[i], bitsI[i], canalesI[i]);
      vl53l0xContinuo(&tof[i], 50);
   }

   imuOk = mpu6050Init(&imu, MPU6050_DIR_DEFAULT, MPU6050_ACC_2G, MPU6050_GIRO_500);
   if (imuOk) {
      mpu6050Cal_t r = MPU6050_CAL_MOVIDO;
      for (int k = 0; k < 3 && r == MPU6050_CAL_MOVIDO; k++) r = mpu6050CalibrarGiro(&imu, 200);
      imprimir("  IMU: OK, giroscopio %s\r\n",
               r == MPU6050_CAL_OK ? "calibrado" : "SIN calibrar (se movio)");
      mpu6050HabilitarIrq(&imu, PIN_MPU_INT, MPU_INT_PUERTO, MPU_INT_BIT, IRQ_CANAL_MPU);
   } else {
      imprimir("  IMU: NO RESPONDE\r\n");
   }
   imprimir("  %d de 3 VL53L0X. Cada segundo: resumen de 3 lineas.\r\n\r\n", nTof);

   /* ---- estado de la prueba ---- */
   uint16_t tofMm[3] = { 0, 0, 0 };
   uint32_t tofCuenta[3] = { 0, 0, 0 };
   uint32_t imuCuenta = 0, usSinEco = 0, lazoMaxUs = 0;
   uint32_t usMm = 0;
   uint8_t  usValido = 0;
   int32_t  gzMin = 0, gzMax = 0;
   enlaceTwist_t tw = { 0, 0, 0 };

   delay_t tSeg, tHc, tAuto, tTele;
   delayConfig(&tTele, 100);
   delayConfig(&tSeg, 1000);
   delayConfig(&tHc, PERIODO_HC_MS);
   delayConfig(&tAuto, AUTOPARADA_MS);

   while (1) {
      uint32_t t0 = tiempoCiclos();
      consolaServicio();

      /* ---- teclado / motores ---- */
      int c = teclaLeer();
      if (pedidoSalir(c)) break;
      if (c >= 0) delayConfig(&tAuto, AUTOPARADA_MS);
      switch (c) {
      case 'w': motorVelocidad(MOTOR_IZQ,  vel); motorVelocidad(MOTOR_DER,  vel); estado = "adelante"; break;
      case 's': motorVelocidad(MOTOR_IZQ, -vel); motorVelocidad(MOTOR_DER, -vel); estado = "atras";    break;
      case 'a': motorVelocidad(MOTOR_IZQ, -vel); motorVelocidad(MOTOR_DER,  vel); estado = "gira izq"; break;
      case 'd': motorVelocidad(MOTOR_IZQ,  vel); motorVelocidad(MOTOR_DER, -vel); estado = "gira der"; break;
      case '1': motorVelocidad(MOTOR_IZQ,  vel); motorLibre(MOTOR_DER);           estado = "solo izq"; break;
      case '2': motorLibre(MOTOR_IZQ);           motorVelocidad(MOTOR_DER,  vel); estado = "solo der"; break;
      case ' ': motorFrenar(MOTOR_IZQ); motorFrenar(MOTOR_DER);                  estado = "freno";    break;
      case 'x': motoresParar();                                                   estado = "libre";    break;
      case '+': if (vel < 100) vel += 10; break;
      case '-': if (vel > 0)   vel -= 10; break;
      default: break;
      }
      if (c == '+' || c == '-') {
         int16_t vi = motorLeerVelocidad(MOTOR_IZQ), vd = motorLeerVelocidad(MOTOR_DER);
         if (vi) motorVelocidad(MOTOR_IZQ, vi > 0 ? vel : -vel);
         if (vd) motorVelocidad(MOTOR_DER, vd > 0 ? vel : -vel);
      }
      if (delayRead(&tAuto)) { motoresParar(); estado = "libre"; }

      /* ---- sensores (no bloqueante) ---- */
      for (int i = 0; i < 3; i++) {
         uint16_t mm;
         if (tof[i].ok && vl53l0xLeer(&tof[i], &mm)) { tofMm[i] = mm; tofCuenta[i]++; }
      }
      if (imuOk && mpu6050Leer(&imu, &d)) {
         imuCuenta++;
         if (d.giro_mdps[2] < gzMin) gzMin = d.giro_mdps[2];
         if (d.giro_mdps[2] > gzMax) gzMax = d.giro_mdps[2];
      }
      hcsr04Estado_t e = hcsr04Estado();
      if (e == HCSR04_LISTO)        { usMm = hcsr04LeerMm(); usValido = 1; }
      else if (e == HCSR04_SIN_ECO) { hcsr04LeerMm(); usSinEco++; usValido = 0; }
      if (delayRead(&tHc)) hcsr04Disparar();

      /* ---- enlace con la ESP32 ---- */
      enlaceProcesar();
      enlaceTwistNuevo(&tw);
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

      /* ---- resumen cada 1 s ---- */
      if (delayRead(&tSeg)) {
         uint32_t ri = encoderRpmX10(ENC_IZQ), rd = encoderRpmX10(ENC_DER);
         imprimirCola("MOT %-8s %3d%% | izq %4d%% der %4d%% | ENC izq %3lu.%lu rpm %5lu p (reb %lu)  der %3lu.%lu rpm %5lu p (reb %lu)\r\n",
                      estado, vel, motorLeerVelocidad(MOTOR_IZQ), motorLeerVelocidad(MOTOR_DER),
                      (unsigned long)(ri / 10), (unsigned long)(ri % 10), (unsigned long)encoderPulsos(ENC_IZQ),
                      (unsigned long)encoderRebotes(ENC_IZQ),
                      (unsigned long)(rd / 10), (unsigned long)(rd % 10), (unsigned long)encoderPulsos(ENC_DER),
                      (unsigned long)encoderRebotes(ENC_DER));

         imprimirCola("TOF");
         for (int i = 0; i < 3; i++) {
            if (!tof[i].ok)                          imprimirCola("  %d: --          ", i + 1);
            else if (tofMm[i] >= VL53L0X_FUERA_RANGO) imprimirCola("  %d: fuera %2lu/s ", i + 1, (unsigned long)tofCuenta[i]);
            else imprimirCola("  %d: %4u mm %2lu/s", i + 1, tofMm[i], (unsigned long)tofCuenta[i]);
            tofCuenta[i] = 0;
         }
         if (usValido) imprimirCola(" | US %3lu.%lu cm", (unsigned long)(usMm / 10), (unsigned long)(usMm % 10));
         else          imprimirCola(" | US sin eco  ");
         if (imuOk) {
            char a[16], z[16];
            fmtX1000(a, gzMin); fmtX1000(z, gzMax);
            imprimirCola(" | IMU %3lu/s perd %lu, giro Z %s..%s grados/s\r\n",
                         (unsigned long)imuCuenta, (unsigned long)imu.perdidas, a, z);
         } else {
            imprimirCola(" | IMU --\r\n");
         }

         uint32_t bm = bateriaMotoresMv(), be = bateriaElectronicaMv();
         enlaceStats_t es;
         enlaceLeerStats(&es);
         uint32_t msTw = enlaceMsDesdeTwist();
         imprimirCola("BAT motores %lu.%02lu V%s electronica %lu.%02lu V%s | ESP %lu tramas (err %lu), TWIST %ld,%ld %s | lazo max %lu us%s\r\n\r\n",
                      (unsigned long)(bm / 1000), (unsigned long)((bm % 1000) / 10),
                      (bm > 1000 && bm < BAT_MINIMO_MV) ? " BAJA!" : "",
                      (unsigned long)(be / 1000), (unsigned long)((be % 1000) / 10),
                      (be > 1000 && be < BAT_MINIMO_MV) ? " BAJA!" : "",
                      (unsigned long)es.tramasOk, (unsigned long)(es.errChecksum + es.errFormato),
                      (long)tw.v, (long)tw.w, msTw == 0xFFFFFFFFu ? "(ninguno)" : (msTw > 1000 ? "(viejo)" : ""),
                      (unsigned long)lazoMaxUs,
                      consolaDescartados() ? " (consola saturada)" : "");
         imuCuenta = 0; gzMin = 0; gzMax = 0; lazoMaxUs = 0;
      }

      uint32_t dt = tiempoUsDesde(t0);
      if (dt > lazoMaxUs) lazoMaxUs = dt;
   }

   /* ---- apagar todo: la ISR guarda punteros a variables de esta funcion ---- */
   motoresParar();
   for (int i = 0; i < 3; i++) {
      if (!tof[i].ok) continue;
      vl53l0xDeshabilitarIrq(&tof[i]);
      vl53l0xDetener(&tof[i]);
   }
   if (imuOk) mpu6050DeshabilitarIrq(&imu);
   enlaceDetener();
   while (!consolaColaVacia()) consolaServicio();
   (void)usSinEco;
}
