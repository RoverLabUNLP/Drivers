/*
 * Prueba de la IMU MPU6050 (GY-521).
 *
 *  'c' = recalibrar el giroscopio (sensor quieto)
 *  'a' = calibrar el acelerometro en 6 posiciones (guiado)
 *  'q' = volver al menu
 *
 * La calibracion del acelerometro se hace UNA vez por sensor: al terminar,
 * la prueba imprime los valores para copiarlos en CAL_ACC_OFF / CAL_ACC_1G
 * (abajo) y que se carguen solos en cada arranque.
 */
#include "tests.h"
#include "consola.h"
#include "pines.h"
#include "mpu6050.h"
#include "tiempo.h"

/* Calibracion del acelerometro de NUESTRO GY-521 (6 posiciones, 29/10/2026).
 * Error encontrado: X +55 mg, Y +10 mg, Z -96 mg; escala 99,8 / 99,9 / 101,1 %.
 * Todo en 0 = sin calibrar. Si se cambia el sensor, repetir con la tecla 'a'. */
static const int16_t CAL_ACC_OFF[3] = { 913, 164, -1580 };
static const int16_t CAL_ACC_1G[3]  = { 16360, 16383, 16568 };

/* Imprime un valor en milesimas con 1 decimal y signo: -1234 -> " -1.2" */
static void imprimirX1000(int32_t v)
{
   char s = ' ';
   if (v < 0) { s = '-'; v = -v; }
   imprimir("%c%3ld.%ld", s, (long)(v / 1000), (long)((v % 1000) / 100));
}

static uint32_t raizEntera(uint32_t x)
{
   uint32_t r = 0, b = 1u << 30;
   while (b > x) b >>= 2;
   while (b) {
      if (x >= r + b) { x -= r + b; r = (r >> 1) + b; }
      else            { r >>= 1; }
      b >>= 2;
   }
   return r;
}

/* Calibra el giro reintentando si se movio. 1 si quedo bien. */
static uint8_t calibrarGiro(mpu6050_t *imu)
{
   for (int intento = 1; intento <= 5; intento++) {
      imprimir("No muevas el sensor: calibrando el giroscopio (2 s)... ");
      mpu6050Cal_t r = mpu6050CalibrarGiro(imu, 200);
      if (r == MPU6050_CAL_OK) {
         imprimir("listo. Offset X %d  Y %d  Z %d cuentas\r\n",
                  imu->offGiro[0], imu->offGiro[1], imu->offGiro[2]);
         return 1;
      }
      if (r == MPU6050_CAL_SIN_DATOS) {
         imprimir("el sensor no responde.\r\n");
         return 0;
      }
      imprimir("se movio, reintento (%d/5)\r\n", intento);
      delay(1000);
   }
   imprimir("No se pudo: apoyalo en la mesa y no lo toques. Se queda la calibracion anterior.\r\n");
   return 0;
}

/* ---------------- Calibracion del acelerometro en 6 posiciones ---------------- */
typedef struct { uint8_t eje; int8_t signo; const char *texto; } posicion_t;

static const posicion_t posiciones[6] = {
   { 2, +1, "PLANO sobre la mesa, con el chip hacia ARRIBA" },
   { 2, -1, "DADO VUELTA, con el chip hacia ABAJO" },
   { 0, +1, "PARADO sobre un borde, con la flecha X de la placa hacia ARRIBA" },
   { 0, -1, "PARADO sobre el borde opuesto, con la flecha X hacia ABAJO" },
   { 1, +1, "PARADO sobre un borde, con la flecha Y de la placa hacia ARRIBA" },
   { 1, -1, "PARADO sobre el borde opuesto, con la flecha Y hacia ABAJO" },
};

/* Devuelve 1 si termino; 0 si se cancelo con 'q' */
static uint8_t calibrarAcc(mpu6050_t *imu)
{
   static const char nombre[3] = { 'X', 'Y', 'Z' };
   int32_t arriba[3] = { 0, 0, 0 }, abajo[3] = { 0, 0, 0 };
   int32_t prom[3];
   int16_t off[3], cnt1g[3];

   imprimir("\r\n=== Calibracion del acelerometro (6 posiciones) ===\r\n"
            "En cada paso: acomodalo, soltalo para que quede QUIETO y apreta espacio.\r\n"
            "Conviene apoyarlo contra algo recto (una caja) para que quede derecho.\r\n"
            "'q' = cancelar\r\n");

   for (int p = 0; p < 6; p++) {
      const posicion_t *pos = &posiciones[p];
      for (;;) {
         imprimir("\r\n[%d/6] %s  -> espacio\r\n", p + 1, pos->texto);
         int c;
         do { c = teclaEsperar(); } while (c != ' ' && !pedidoSalir(c));
         if (pedidoSalir(c)) { imprimir("Cancelada, no se cambio nada.\r\n"); return 0; }

         delay(300);   /* que se apague la vibracion de apretar la tecla */
         mpu6050Cal_t r = mpu6050PromedioAcc(imu, prom, 100);
         if (r == MPU6050_CAL_SIN_DATOS) { imprimir("El sensor no responde.\r\n"); return 0; }
         if (r == MPU6050_CAL_MOVIDO)    { imprimir("Se movio, repetimos esta posicion.\r\n"); continue; }

         /* El eje que tiene que apuntar a la gravedad tiene que medir al menos 0,7 g */
         int32_t v = prom[pos->eje] * pos->signo;
         if (v < (int32_t)imu->lsbPorG * 7 / 10) {
            imprimir("El eje %c no esta %s (mide %ld cuentas). Acomodalo y repetimos.\r\n",
                     nombre[pos->eje], pos->signo > 0 ? "hacia arriba" : "hacia abajo",
                     (long)prom[pos->eje]);
            continue;
         }
         if (pos->signo > 0) arriba[pos->eje] = prom[pos->eje];
         else                abajo[pos->eje]  = prom[pos->eje];
         imprimir("OK: eje %c = %ld cuentas\r\n", nombre[pos->eje], (long)prom[pos->eje]);
         break;
      }
   }

   for (int i = 0; i < 3; i++) {
      off[i]   = (int16_t)((arriba[i] + abajo[i]) / 2);
      cnt1g[i] = (int16_t)((arriba[i] - abajo[i]) / 2);
   }
   mpu6050FijarCalAcc(imu, off, cnt1g);

   imprimir("\r\nListo. Error encontrado por eje:\r\n");
   for (int i = 0; i < 3; i++) {
      int32_t offMg   = ((int32_t)off[i] * 1000) / imu->lsbPorG;
      int32_t escalaX = ((int32_t)cnt1g[i] * 1000) / imu->lsbPorG;   /* 1000 = escala perfecta */
      imprimir("  %c: cero %ld mg, escala %ld.%ld %%\r\n", nombre[i], (long)offMg,
               (long)(escalaX / 10), (long)(escalaX % 10));
   }
   imprimir("\r\nPara que quede guardada, copia esto en test_imu.c (o en el firmware final):\r\n"
            "static const int16_t CAL_ACC_OFF[3] = { %d, %d, %d };\r\n"
            "static const int16_t CAL_ACC_1G[3]  = { %d, %d, %d };\r\n"
            "Ahora volve a apoyarlo plano: tiene que dar Z ~ +1000 mg y |a| ~ 1000 mg.\r\n\r\n",
            off[0], off[1], off[2], cnt1g[0], cnt1g[1], cnt1g[2]);
   delay(1500);
   return 1;
}

/* ------------------------------------------------------------------ */
void testImu(void)
{
   mpu6050_t imu;
   mpu6050Datos_t d = { { 0, 0, 0 }, { 0, 0, 0 }, 0, 0 };
   uint8_t id;

   imprimir("\r\n--- IMU MPU6050 (GY-521) ---\r\n"
            "VCC -> +5 V (o +3,3 V)   GND   SDA -> I2C_SDA   SCL -> I2C_SCL\r\n"
            "AD0 -> GND (direccion 0x68). INT -> LCDEN (solo modo interrupcion). XDA, XCL sin conectar.\r\n"
            "'c' = recalibrar giroscopio, 'a' = calibrar acelerometro, 'q' = volver\r\n"
            "'v' = mostrar/ocultar los datos (para medir los tiempos sin la demora de la consola)\r\n\r\n");

   i2cInit(BUS_I2C, I2C_VELOCIDAD);

   if (!mpu6050Presente(MPU6050_DIR_DEFAULT, &id)) {
      imprimir("No responde en 0x68 (WHO_AM_I = 0x%02X). Revisar VCC, GND, SDA, SCL y AD0 a GND.\r\n", id);
      return;
   }
   imprimir("WHO_AM_I = 0x%02X %s\r\n", id, id == 0x68 ? "(MPU6050)" : "(clon compatible)");

   if (!mpu6050Init(&imu, MPU6050_DIR_DEFAULT, MPU6050_ACC_2G, MPU6050_GIRO_500)) {
      imprimir("FALLO el init: el sensor no acepto la configuracion.\r\n");
      return;
   }
   imprimir("Init OK: +-2 g, +-500 grados/s, 100 Hz\r\n");

   if (CAL_ACC_1G[0] && CAL_ACC_1G[1] && CAL_ACC_1G[2]) {
      mpu6050FijarCalAcc(&imu, CAL_ACC_OFF, CAL_ACC_1G);
      imprimir("Acelerometro: calibracion cargada de CAL_ACC_OFF / CAL_ACC_1G\r\n");
   } else {
      imprimir("Acelerometro: SIN calibrar (apreta 'a' para calibrarlo)\r\n");
   }

   calibrarGiro(&imu);

   imprimir("\r\nModo de lectura:\r\n"
            "  p = polling (pregunta por I2C)\r\n"
            "  i = interrupcion (pin INT -> LCDEN)\r\n> ");
   int c;
   do { c = teclaEsperar(); } while (c != 'p' && c != 'i' && !pedidoSalir(c));
   if (pedidoSalir(c)) return;
   imprimir("%c\r\n", c);
   uint8_t conIrq = (c == 'i');
   if (conIrq) mpu6050HabilitarIrq(&imu, PIN_MPU_INT, MPU_INT_PUERTO, MPU_INT_BIT, IRQ_CANAL_MPU);

   imprimir("Cada segundo: muestras leidas (tiene que dar 100) y tiempo entre muestras (10 ms).\r\n"
            "Cada linea de datos tarda ~10 ms en salir por la consola y puede pisar una muestra:\r\n"
            "para medir los tiempos, ocultalas con 'v'.\r\n\r\n");
   uint8_t verDatos = 1;

   uint32_t cuenta = 0, tPrev = 0, dtMin = 0xFFFFFFFF, dtMax = 0;
   uint8_t  hayPrev = 0;
   delay_t t, tSeg;
   delayConfig(&t, 200);
   delayConfig(&tSeg, 1000);
   while (1) {
      c = teclaLeer();
      if (pedidoSalir(c)) break;
      if (c == 'v') { verDatos = !verDatos; cuenta = 0; hayPrev = 0; dtMin = 0xFFFFFFFF; dtMax = 0; imu.perdidas = 0; }
      if (c == 'c' || c == 'a') {
         /* La calibracion espera teclas y lee por polling: se pausa la interrupcion */
         if (conIrq) mpu6050DeshabilitarIrq(&imu);
         imprimir("\r\n");
         if (c == 'c') calibrarGiro(&imu); else calibrarAcc(&imu);
         imprimir("\r\n");
         if (conIrq) mpu6050HabilitarIrq(&imu, PIN_MPU_INT, MPU_INT_PUERTO, MPU_INT_BIT, IRQ_CANAL_MPU);
         hayPrev = 0; cuenta = 0; dtMin = 0xFFFFFFFF; dtMax = 0;
         delayConfig(&t, 200);
         delayConfig(&tSeg, 1000);
      }

      if (mpu6050Leer(&imu, &d)) {       /* una muestra nueva cada 10 ms */
         cuenta++;
         if (hayPrev) {
            uint32_t dt = tiempoCiclosAUs(d.t_ciclos - tPrev);
            if (dt < dtMin) dtMin = dt;
            if (dt > dtMax) dtMax = dt;
         }
         tPrev = d.t_ciclos;
         hayPrev = 1;
      }

      if (delayRead(&t) && verDatos) {
         int32_t x = d.acc_mg[0], y = d.acc_mg[1], z = d.acc_mg[2];
         uint32_t modulo = raizEntera((uint32_t)(x * x + y * y + z * z));
         imprimir("acc [mg] X %6ld Y %6ld Z %6ld |a| %5lu | giro [grados/s] X",
                  (long)x, (long)y, (long)z, (unsigned long)modulo);
         imprimirX1000(d.giro_mdps[0]); imprimir(" Y");
         imprimirX1000(d.giro_mdps[1]); imprimir(" Z");
         imprimirX1000(d.giro_mdps[2]);
         imprimir(" | %ld.%ld C\r\n", (long)(d.temp_cC / 100), (long)((d.temp_cC % 100 + 100) % 100 / 10));
      }

      if (delayRead(&tSeg)) {
         imprimir("   [%s] %lu muestras/s, dt min %lu.%lu ms, max %lu.%lu ms",
                  conIrq ? "interrupcion" : "polling", (unsigned long)cuenta,
                  (unsigned long)(dtMin == 0xFFFFFFFF ? 0 : dtMin / 1000),
                  (unsigned long)(dtMin == 0xFFFFFFFF ? 0 : (dtMin % 1000) / 100),
                  (unsigned long)(dtMax / 1000), (unsigned long)((dtMax % 1000) / 100));
         if (conIrq) imprimir(", perdidas %lu", (unsigned long)imu.perdidas);
         imprimir("\r\n");
         cuenta = 0; dtMin = 0xFFFFFFFF; dtMax = 0;
      }
   }

   mpu6050DeshabilitarIrq(&imu);   /* la ISR apunta a 'imu', que deja de existir */
}
