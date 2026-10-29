#include "mpu6050.h"
#include "pines.h"
#include "tiempo.h"
#include "gpio_irq.h"
#include "chip.h"

/* ---------------- Registros (mapa de registros del MPU-6000/6050, rev 4.2) ---------------- */
#define REG_SMPLRT_DIV    0x19
#define REG_CONFIG        0x1A
#define REG_GYRO_CONFIG   0x1B
#define REG_ACCEL_CONFIG  0x1C
#define REG_INT_PIN_CFG   0x37
#define REG_INT_ENABLE    0x38
#define REG_INT_STATUS    0x3A
#define REG_ACCEL_XOUT_H  0x3B   /* de aca salen 14 bytes: acc XYZ, temp, giro XYZ */
#define REG_PWR_MGMT_1    0x6B
#define REG_WHO_AM_I      0x75

#define TIMEOUT_US        50000u

static const uint16_t lsbAcc[4]       = { 16384, 8192, 4096, 2048 };
static const uint16_t lsbGiroX10[4]   = { 1310, 655, 328, 164 };

/* ---------------- I2C (sAPI) ---------------- */
static void wr8(uint8_t dir, uint8_t reg, uint8_t v)
{
   uint8_t b[2] = { reg, v };
   i2cWrite(BUS_I2C, dir, b, 2, TRUE);
}

static void rd(uint8_t dir, uint8_t reg, uint8_t *dst, uint8_t n)
{
   for (uint8_t i = 0; i < n; i++) dst[i] = 0;
   i2cRead(BUS_I2C, dir, &reg, 1, FALSE, dst, n, TRUE);
}

static uint8_t rd8(uint8_t dir, uint8_t reg)
{
   uint8_t v;
   rd(dir, reg, &v, 1);
   return v;
}

/* ---------------- API ---------------- */
uint8_t mpu6050Presente(uint8_t dir, uint8_t *id)
{
   uint8_t v = rd8(dir, REG_WHO_AM_I);
   if (id) *id = v;
   /* 0x68 = MPU6050 original. Muchos GY-521 traen clones que responden distinto:
    * 0x70 (MPU6500), 0x72, 0x98. Funcionan igual para acc/giro. */
   return v == 0x68 || v == 0x70 || v == 0x72 || v == 0x98;
}

uint8_t mpu6050Init(mpu6050_t *imu, uint8_t dir, mpu6050RangoAcc_t acc, mpu6050RangoGiro_t giro)
{
   imu->dir = dir;
   imu->ok = 0;
   imu->usaIrq = 0;
   imu->listo = 0;
   imu->perdidas = 0;
   imu->lsbPorG      = lsbAcc[acc & 3];
   imu->lsbPorDpsX10 = lsbGiroX10[giro & 3];
   for (int i = 0; i < 3; i++) {
      imu->offGiro[i]  = 0;
      imu->offAcc[i]   = 0;
      imu->cnt1gAcc[i] = (int16_t)imu->lsbPorG;   /* sin calibrar: el valor nominal */
   }

   if (!mpu6050Presente(dir, &imu->id)) return 0;

   wr8(dir, REG_PWR_MGMT_1, 0x80);      /* reset de todos los registros */
   delay(100);
   wr8(dir, REG_PWR_MGMT_1, 0x01);      /* despierta (sale de sleep) con el PLL del giro X */
   delay(10);

   wr8(dir, REG_CONFIG,       0x03);    /* DLPF ~44 Hz -> el muestreo interno pasa a 1 kHz */
   wr8(dir, REG_SMPLRT_DIV,   9);       /* 1 kHz / (1 + 9) = 100 Hz */
   wr8(dir, REG_GYRO_CONFIG,  (uint8_t)((giro & 3) << 3));
   wr8(dir, REG_ACCEL_CONFIG, (uint8_t)((acc  & 3) << 3));
   wr8(dir, REG_INT_PIN_CFG,  0x00);    /* pin INT activo en alto, push-pull; el aviso se limpia al leer INT_STATUS */
   wr8(dir, REG_INT_ENABLE,   0x01);    /* aviso de dato listo */

   /* Verificacion: lo escrito tiene que leerse igual */
   if (rd8(dir, REG_PWR_MGMT_1) != 0x01) return 0;
   if (rd8(dir, REG_SMPLRT_DIV) != 9)    return 0;

   imu->ok = 1;
   return 1;
}

uint8_t mpu6050DatoListo(mpu6050_t *imu)
{
   return rd8(imu->dir, REG_INT_STATUS) & 0x01;
}

/* Arma la muestra cruda a partir de los 14 bytes que arrancan en ACCEL_XOUT_H */
static void desarmar(const uint8_t *b, mpu6050Crudo_t *c)
{
   for (int i = 0; i < 3; i++) {
      c->acc[i]  = (int16_t)((b[2 * i] << 8) | b[2 * i + 1]);
      c->giro[i] = (int16_t)((b[8 + 2 * i] << 8) | b[8 + 2 * i + 1]);
   }
   c->temp = (int16_t)((b[6] << 8) | b[7]);
}

void mpu6050LeerCrudo(mpu6050_t *imu, mpu6050Crudo_t *c)
{
   uint8_t b[14];
   rd(imu->dir, REG_ACCEL_XOUT_H, b, 14);
   desarmar(b, c);
}

static void convertir(mpu6050_t *imu, const mpu6050Crudo_t *c, mpu6050Datos_t *d)
{
   for (int i = 0; i < 3; i++) {
      d->acc_mg[i]    = ((int32_t)(c->acc[i] - imu->offAcc[i]) * 1000) / imu->cnt1gAcc[i];
      d->giro_mdps[i] = ((int32_t)(c->giro[i] - imu->offGiro[i]) * 10000) / imu->lsbPorDpsX10;
   }
   /* Temp [C] = cuentas / 340 + 36,53  (datasheet) */
   d->temp_cC = ((int32_t)c->temp * 100) / 340 + 3653;
}

uint8_t mpu6050Leer(mpu6050_t *imu, mpu6050Datos_t *d)
{
   mpu6050Crudo_t c;

   if (imu->usaIrq) {
      if (!imu->listo) return 0;              /* no hay aviso: ni se toca el I2C */
      __disable_irq();
      uint32_t t = imu->tAviso;
      imu->listo = 0;
      __enable_irq();

      /* INT_STATUS (0x3A) y los 14 bytes de datos (0x3B..0x48) son contiguos:
       * una sola lectura de 15 bytes limpia el aviso y trae la muestra */
      uint8_t b[15];
      rd(imu->dir, REG_INT_STATUS, b, 15);
      desarmar(&b[1], &c);
      convertir(imu, &c, d);
      d->t_ciclos = t;
      return 1;
   }

   if (!mpu6050DatoListo(imu)) return 0;
   uint32_t t = tiempoCiclos();
   mpu6050LeerCrudo(imu, &c);
   convertir(imu, &c, d);
   d->t_ciclos = t;
   return 1;
}

/* ---------------- Modo por interrupcion ---------------- */
static void cbIrq(void *ctx, uint8_t nivel)
{
   mpu6050_t *imu = (mpu6050_t *)ctx;
   (void)nivel;
   if (imu->listo) imu->perdidas++;   /* el programa no llego a leer la anterior */
   imu->tAviso = tiempoCiclos();
   imu->listo = 1;
}

void mpu6050HabilitarIrq(mpu6050_t *imu, gpioMap_t pin, uint8_t puerto, uint8_t bit, uint8_t canal)
{
   gpioInit(pin, GPIO_INPUT);             /* INT es push-pull: no hace falta pull-up */
   imu->listo = 0;
   imu->perdidas = 0;
   imu->canalIrq = canal;
   rd8(imu->dir, REG_INT_STATUS);         /* descarta un aviso viejo */
   gpioIrqConfigCtx(canal, puerto, bit, GPIO_IRQ_SUBIDA, cbIrq, imu);
   imu->usaIrq = 1;
}

void mpu6050DeshabilitarIrq(mpu6050_t *imu)
{
   if (!imu->usaIrq) return;
   gpioIrqDeshabilitar(imu->canalIrq);
   imu->usaIrq = 0;
   imu->listo = 0;
}

/* Espera la proxima muestra y la lee. 0 si el sensor no responde. */
static uint8_t siguienteMuestra(mpu6050_t *imu, mpu6050Crudo_t *c)
{
   uint32_t t0 = tiempoCiclos();
   while (!mpu6050DatoListo(imu)) {
      if (tiempoUsDesde(t0) > TIMEOUT_US) return 0;
   }
   mpu6050LeerCrudo(imu, c);
   return 1;
}

/* Promedia 'muestras' de acc (usarAcc = 1) o de giro (0) y verifica que el
 * sensor haya estado quieto: la diferencia entre el maximo y el minimo de cada
 * eje no puede pasar 'umbral' cuentas. */
static mpu6050Cal_t promediar(mpu6050_t *imu, uint8_t usarAcc, uint16_t muestras,
                              int32_t umbral, int32_t prom[3])
{
   int32_t suma[3] = { 0, 0, 0 };
   int16_t min[3] = { 32767, 32767, 32767 }, max[3] = { -32768, -32768, -32768 };
   mpu6050Crudo_t c;

   if (muestras == 0) muestras = 1;
   for (uint16_t n = 0; n < muestras; n++) {
      if (!siguienteMuestra(imu, &c)) return MPU6050_CAL_SIN_DATOS;
      for (int i = 0; i < 3; i++) {
         int16_t v = usarAcc ? c.acc[i] : c.giro[i];
         suma[i] += v;
         if (v < min[i]) min[i] = v;
         if (v > max[i]) max[i] = v;
      }
   }
   for (int i = 0; i < 3; i++) {
      if ((int32_t)max[i] - min[i] > umbral) return MPU6050_CAL_MOVIDO;
      prom[i] = suma[i] / muestras;
   }
   return MPU6050_CAL_OK;
}

/* La calibracion lee por polling: si la interrupcion esta habilitada, la ISR
 * marcaria esas muestras como perdidas. Se guarda el contador y se restaura. */
static mpu6050Cal_t finCal(mpu6050_t *imu, uint32_t perdidasAntes, mpu6050Cal_t r)
{
   __disable_irq();
   imu->listo = 0;
   imu->perdidas = perdidasAntes;
   __enable_irq();
   return r;
}

static mpu6050Cal_t calibrarGiroInterno(mpu6050_t *imu, uint16_t muestras)
{
   mpu6050Crudo_t c;
   int32_t prom[3];

   /* Descarta ~200 ms: justo despues del init (o de apoyarlo) el giro todavia se asienta */
   for (int n = 0; n < 20; n++)
      if (!siguienteMuestra(imu, &c)) return MPU6050_CAL_SIN_DATOS;

   /* Quieto, el ruido del giro es de algunas decimas de grado/s: si un eje
    * varia mas de 3 grados/s entre su minimo y su maximo, hubo movimiento */
   int32_t umbral = (int32_t)imu->lsbPorDpsX10 * 3 / 10;
   mpu6050Cal_t r = promediar(imu, 0, muestras, umbral, prom);
   if (r != MPU6050_CAL_OK) return r;

   for (int i = 0; i < 3; i++) imu->offGiro[i] = (int16_t)prom[i];
   return MPU6050_CAL_OK;
}

mpu6050Cal_t mpu6050CalibrarGiro(mpu6050_t *imu, uint16_t muestras)
{
   uint32_t p = imu->perdidas;
   return finCal(imu, p, calibrarGiroInterno(imu, muestras));
}

mpu6050Cal_t mpu6050PromedioAcc(mpu6050_t *imu, int32_t prom[3], uint16_t muestras)
{
   uint32_t p = imu->perdidas;
   /* Quieto, el ruido del acelerometro es de pocos mg: mas de 50 mg = se movio */
   return finCal(imu, p, promediar(imu, 1, muestras, imu->lsbPorG / 20, prom));
}

void mpu6050FijarCalAcc(mpu6050_t *imu, const int16_t off[3], const int16_t cnt1g[3])
{
   for (int i = 0; i < 3; i++) {
      imu->offAcc[i]   = off[i];
      imu->cnt1gAcc[i] = (cnt1g[i] > 0) ? cnt1g[i] : (int16_t)imu->lsbPorG;
   }
}
