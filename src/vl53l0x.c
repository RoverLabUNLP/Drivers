/*
 * VL53L0X - port a C de pololu/vl53l0x-arduino (licencia MIT-like, ver README de Pololu).
 * Las secuencias de registros "magicos" vienen de la API de ST (VL53L0X_api.c,
 * vl53l0x_tuning.h); no estan documentadas en el datasheet.
 */
#include "vl53l0x.h"
#include "pines.h"
#include "tiempo.h"
#include "gpio_irq.h"

/* ---------------- Registros ---------------- */
#define SYSRANGE_START                              0x00
#define SYSTEM_SEQUENCE_CONFIG                      0x01
#define SYSTEM_INTERMEASUREMENT_PERIOD              0x04
#define SYSTEM_INTERRUPT_CONFIG_GPIO                0x0A
#define SYSTEM_INTERRUPT_CLEAR                      0x0B
#define RESULT_INTERRUPT_STATUS                     0x13
#define RESULT_RANGE_STATUS                         0x14
#define FINAL_RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT 0x44
#define MSRC_CONFIG_TIMEOUT_MACROP                  0x46
#define PRE_RANGE_CONFIG_VCSEL_PERIOD               0x50
#define PRE_RANGE_CONFIG_TIMEOUT_MACROP_HI          0x51
#define MSRC_CONFIG_CONTROL                         0x60
#define FINAL_RANGE_CONFIG_VCSEL_PERIOD             0x70
#define FINAL_RANGE_CONFIG_TIMEOUT_MACROP_HI        0x71
#define GPIO_HV_MUX_ACTIVE_HIGH                     0x84
#define VHV_CONFIG_PAD_SCL_SDA__EXTSUP_HV           0x89
#define I2C_SLAVE_DEVICE_ADDRESS                    0x8A
#define GLOBAL_CONFIG_SPAD_ENABLES_REF_0            0xB0
#define GLOBAL_CONFIG_REF_EN_START_SELECT           0xB6
#define DYNAMIC_SPAD_NUM_REQUESTED_REF_SPAD         0x4E
#define DYNAMIC_SPAD_REF_EN_START_OFFSET            0x4F
#define IDENTIFICATION_MODEL_ID                     0xC0
#define OSC_CALIBRATE_VAL                           0xF8

#define TIMEOUT_US  500000u

/* ---------------- I2C de bajo nivel (sAPI) ---------------- */
static void wr8(vl53l0x_t *d, uint8_t reg, uint8_t v)
{
   uint8_t b[2] = { reg, v };
   i2cWrite(BUS_I2C, d->dir, b, 2, TRUE);
}

static void wr16(vl53l0x_t *d, uint8_t reg, uint16_t v)
{
   uint8_t b[3] = { reg, (uint8_t)(v >> 8), (uint8_t)v };
   i2cWrite(BUS_I2C, d->dir, b, 3, TRUE);
}

static void wr32(vl53l0x_t *d, uint8_t reg, uint32_t v)
{
   uint8_t b[5] = { reg, (uint8_t)(v >> 24), (uint8_t)(v >> 16),
                    (uint8_t)(v >> 8), (uint8_t)v };
   i2cWrite(BUS_I2C, d->dir, b, 5, TRUE);
}

static void wrMulti(vl53l0x_t *d, uint8_t reg, const uint8_t *src, uint8_t n)
{
   uint8_t b[8];
   b[0] = reg;
   for (uint8_t i = 0; i < n && i < 7; i++) b[i + 1] = src[i];
   i2cWrite(BUS_I2C, d->dir, b, n + 1, TRUE);
}

static void rdMultiDir(uint8_t dir, uint8_t reg, uint8_t *dst, uint8_t n)
{
   for (uint8_t i = 0; i < n; i++) dst[i] = 0;
   i2cRead(BUS_I2C, dir, &reg, 1, FALSE, dst, n, TRUE);
}

static uint8_t rd8(vl53l0x_t *d, uint8_t reg)
{
   uint8_t v;
   rdMultiDir(d->dir, reg, &v, 1);
   return v;
}

static uint16_t rd16(vl53l0x_t *d, uint8_t reg)
{
   uint8_t b[2];
   rdMultiDir(d->dir, reg, b, 2);
   return (uint16_t)((b[0] << 8) | b[1]);
}

/* ---------------- Auxiliares de timing (de la API de ST) ---------------- */
#define decodeVcselPeriod(r)   ((uint8_t)(((r) + 1) << 1))
#define calcMacroPeriod(vp)    ((((uint32_t)2304 * (vp) * 1655) + 500) / 1000)

typedef struct { uint8_t tcc, msrc, dss, preRange, finalRange; } pasos_t;
typedef struct {
   uint16_t preVcselPclks, finalVcselPclks;
   uint16_t msrcDssTccMclks, preRangeMclks, finalRangeMclks;
   uint32_t msrcDssTccUs, preRangeUs, finalRangeUs;
} timeouts_t;

static uint16_t decodeTimeout(uint16_t r)
{
   return (uint16_t)((r & 0x00FF) << ((r & 0xFF00) >> 8)) + 1;
}

static uint16_t encodeTimeout(uint32_t mclks)
{
   uint32_t ls = 0;
   uint16_t ms = 0;
   if (mclks == 0) return 0;
   ls = mclks - 1;
   while ((ls & 0xFFFFFF00) > 0) { ls >>= 1; ms++; }
   return (uint16_t)((ms << 8) | (ls & 0xFF));
}

static uint32_t mclksAUs(uint16_t mclks, uint8_t vcselPclks)
{
   uint32_t macroNs = calcMacroPeriod(vcselPclks);
   return ((mclks * macroNs) + 500) / 1000;
}

static uint32_t usAMclks(uint32_t us, uint8_t vcselPclks)
{
   uint32_t macroNs = calcMacroPeriod(vcselPclks);
   return ((us * 1000) + (macroNs / 2)) / macroNs;
}

static void leerPasos(vl53l0x_t *d, pasos_t *p)
{
   uint8_t s = rd8(d, SYSTEM_SEQUENCE_CONFIG);
   p->tcc        = (s >> 4) & 1;
   p->dss        = (s >> 3) & 1;
   p->msrc       = (s >> 2) & 1;
   p->preRange   = (s >> 6) & 1;
   p->finalRange = (s >> 7) & 1;
}

static void leerTimeouts(vl53l0x_t *d, const pasos_t *p, timeouts_t *t)
{
   t->preVcselPclks   = decodeVcselPeriod(rd8(d, PRE_RANGE_CONFIG_VCSEL_PERIOD));
   t->msrcDssTccMclks = rd8(d, MSRC_CONFIG_TIMEOUT_MACROP) + 1;
   t->msrcDssTccUs    = mclksAUs(t->msrcDssTccMclks, (uint8_t)t->preVcselPclks);
   t->preRangeMclks   = decodeTimeout(rd16(d, PRE_RANGE_CONFIG_TIMEOUT_MACROP_HI));
   t->preRangeUs      = mclksAUs(t->preRangeMclks, (uint8_t)t->preVcselPclks);

   t->finalVcselPclks = decodeVcselPeriod(rd8(d, FINAL_RANGE_CONFIG_VCSEL_PERIOD));
   t->finalRangeMclks = decodeTimeout(rd16(d, FINAL_RANGE_CONFIG_TIMEOUT_MACROP_HI));
   if (p->preRange) t->finalRangeMclks -= t->preRangeMclks;
   t->finalRangeUs    = mclksAUs(t->finalRangeMclks, (uint8_t)t->finalVcselPclks);
}

#define OV_START   1910
#define OV_END      960
#define OV_MSRC     660
#define OV_TCC      590
#define OV_DSS      690
#define OV_PRE      660
#define OV_FINAL    550

static uint32_t leerPresupuesto(vl53l0x_t *d)
{
   pasos_t p; timeouts_t t;
   uint32_t us = OV_START + OV_END;
   leerPasos(d, &p);
   leerTimeouts(d, &p, &t);
   if (p.tcc)       us += t.msrcDssTccUs + OV_TCC;
   if (p.dss)       us += 2 * (t.msrcDssTccUs + OV_DSS);
   else if (p.msrc) us += t.msrcDssTccUs + OV_MSRC;
   if (p.preRange)  us += t.preRangeUs + OV_PRE;
   if (p.finalRange) us += t.finalRangeUs + OV_FINAL;
   d->presupuestoUs = us;
   return us;
}

uint8_t vl53l0xPresupuesto(vl53l0x_t *d, uint32_t budgetUs)
{
   pasos_t p; timeouts_t t;
   uint32_t usado = OV_START + OV_END;

   if (budgetUs < 20000) return 0;
   leerPasos(d, &p);
   leerTimeouts(d, &p, &t);

   if (p.tcc)       usado += t.msrcDssTccUs + OV_TCC;
   if (p.dss)       usado += 2 * (t.msrcDssTccUs + OV_DSS);
   else if (p.msrc) usado += t.msrcDssTccUs + OV_MSRC;
   if (p.preRange)  usado += t.preRangeUs + OV_PRE;

   if (p.finalRange) {
      usado += OV_FINAL;
      if (usado > budgetUs) return 0;
      uint32_t finalUs    = budgetUs - usado;
      uint32_t finalMclks = usAMclks(finalUs, (uint8_t)t.finalVcselPclks);
      if (p.preRange) finalMclks += t.preRangeMclks;
      wr16(d, FINAL_RANGE_CONFIG_TIMEOUT_MACROP_HI, encodeTimeout(finalMclks));
      d->presupuestoUs = budgetUs;
   }
   return 1;
}

/* ---------------- Secuencias de init ---------------- */
static uint8_t leerSpadInfo(vl53l0x_t *d, uint8_t *cuenta, uint8_t *esApertura)
{
   wr8(d, 0x80, 0x01); wr8(d, 0xFF, 0x01); wr8(d, 0x00, 0x00);
   wr8(d, 0xFF, 0x06); wr8(d, 0x83, rd8(d, 0x83) | 0x04);
   wr8(d, 0xFF, 0x07); wr8(d, 0x81, 0x01);
   wr8(d, 0x80, 0x01);
   wr8(d, 0x94, 0x6B); wr8(d, 0x83, 0x00);

   uint32_t t0 = tiempoCiclos();
   while (rd8(d, 0x83) == 0x00) {
      if (tiempoUsDesde(t0) > TIMEOUT_US) { d->timeout = 1; return 0; }
   }
   wr8(d, 0x83, 0x01);
   uint8_t tmp = rd8(d, 0x92);
   *cuenta     = tmp & 0x7F;
   *esApertura = (tmp >> 7) & 0x01;

   wr8(d, 0x81, 0x00); wr8(d, 0xFF, 0x06);
   wr8(d, 0x83, rd8(d, 0x83) & ~0x04);
   wr8(d, 0xFF, 0x01); wr8(d, 0x00, 0x01);
   wr8(d, 0xFF, 0x00); wr8(d, 0x80, 0x00);
   return 1;
}

static uint8_t calibracionRef(vl53l0x_t *d, uint8_t vhvInit)
{
   wr8(d, SYSRANGE_START, 0x01 | vhvInit);
   uint32_t t0 = tiempoCiclos();
   while ((rd8(d, RESULT_INTERRUPT_STATUS) & 0x07) == 0) {
      if (tiempoUsDesde(t0) > TIMEOUT_US) { d->timeout = 1; return 0; }
   }
   wr8(d, SYSTEM_INTERRUPT_CLEAR, 0x01);
   wr8(d, SYSRANGE_START, 0x00);
   return 1;
}

/* Tabla DefaultTuningSettings de la API de ST: {registro, valor} */
static const uint8_t tuning[][2] = {
   {0xFF,0x01},{0x00,0x00},{0xFF,0x00},{0x09,0x00},{0x10,0x00},{0x11,0x00},
   {0x24,0x01},{0x25,0xFF},{0x75,0x00},{0xFF,0x01},{0x4E,0x2C},{0x48,0x00},
   {0x30,0x20},{0xFF,0x00},{0x30,0x09},{0x54,0x00},{0x31,0x04},{0x32,0x03},
   {0x40,0x83},{0x46,0x25},{0x60,0x00},{0x27,0x00},{0x50,0x06},{0x51,0x00},
   {0x52,0x96},{0x56,0x08},{0x57,0x30},{0x61,0x00},{0x62,0x00},{0x64,0x00},
   {0x65,0x00},{0x66,0xA0},{0xFF,0x01},{0x22,0x32},{0x47,0x14},{0x49,0xFF},
   {0x4A,0x00},{0xFF,0x00},{0x7A,0x0A},{0x7B,0x00},{0x78,0x21},{0xFF,0x01},
   {0x23,0x34},{0x42,0x00},{0x44,0xFF},{0x45,0x26},{0x46,0x05},{0x40,0x40},
   {0x0E,0x06},{0x20,0x1A},{0x43,0x40},{0xFF,0x00},{0x34,0x03},{0x35,0x44},
   {0xFF,0x01},{0x31,0x04},{0x4B,0x09},{0x4C,0x05},{0x4D,0x04},{0xFF,0x00},
   {0x44,0x00},{0x45,0x20},{0x47,0x08},{0x48,0x28},{0x67,0x00},{0x70,0x04},
   {0x71,0x01},{0x72,0xFE},{0x76,0x00},{0x77,0x00},{0xFF,0x01},{0x0D,0x01},
   {0xFF,0x00},{0x80,0x01},{0x01,0xF8},{0xFF,0x01},{0x8E,0x01},{0x00,0x01},
   {0xFF,0x00},{0x80,0x00},
};

/* ---------------- API ---------------- */
uint8_t vl53l0xPresente(uint8_t dir)
{
   uint8_t id;
   rdMultiDir(dir, IDENTIFICATION_MODEL_ID, &id, 1);
   return id == 0xEE;
}

uint8_t vl53l0xInit(vl53l0x_t *d)
{
   d->ok = 0;
   d->timeout = 0;
   d->usaIrq = 0;
   d->listo = 0;
   d->perdidas = 0;
   if (!vl53l0xPresente(d->dir)) return 0;

   /* I/O a 2,8 V (lo que usa el regulador del modulo) */
   wr8(d, VHV_CONFIG_PAD_SCL_SDA__EXTSUP_HV, rd8(d, VHV_CONFIG_PAD_SCL_SDA__EXTSUP_HV) | 0x01);

   /* modo I2C estandar + leer stop_variable */
   wr8(d, 0x88, 0x00);
   wr8(d, 0x80, 0x01); wr8(d, 0xFF, 0x01); wr8(d, 0x00, 0x00);
   d->stopVariable = rd8(d, 0x91);
   wr8(d, 0x00, 0x01); wr8(d, 0xFF, 0x00); wr8(d, 0x80, 0x00);

   /* desactiva los chequeos SIGNAL_RATE_MSRC y SIGNAL_RATE_PRE_RANGE */
   wr8(d, MSRC_CONFIG_CONTROL, rd8(d, MSRC_CONFIG_CONTROL) | 0x12);

   /* limite de tasa de senal: 0,25 MCPS en formato 9.7 -> 0,25 * 128 = 32 */
   wr16(d, FINAL_RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT, 32);

   wr8(d, SYSTEM_SEQUENCE_CONFIG, 0xFF);

   /* --- SPADs de referencia --- */
   uint8_t spadCuenta, spadApertura;
   if (!leerSpadInfo(d, &spadCuenta, &spadApertura)) return 0;

   uint8_t mapa[6];
   rdMultiDir(d->dir, GLOBAL_CONFIG_SPAD_ENABLES_REF_0, mapa, 6);

   wr8(d, 0xFF, 0x01);
   wr8(d, DYNAMIC_SPAD_REF_EN_START_OFFSET, 0x00);
   wr8(d, DYNAMIC_SPAD_NUM_REQUESTED_REF_SPAD, 0x2C);
   wr8(d, 0xFF, 0x00);
   wr8(d, GLOBAL_CONFIG_REF_EN_START_SELECT, 0xB4);

   uint8_t primero = spadApertura ? 12 : 0;
   uint8_t habilitados = 0;
   for (uint8_t i = 0; i < 48; i++) {
      if (i < primero || habilitados == spadCuenta)
         mapa[i / 8] &= (uint8_t)~(1 << (i % 8));
      else if ((mapa[i / 8] >> (i % 8)) & 0x1)
         habilitados++;
   }
   wrMulti(d, GLOBAL_CONFIG_SPAD_ENABLES_REF_0, mapa, 6);

   /* --- ajustes por defecto de ST --- */
   for (uint32_t i = 0; i < sizeof(tuning) / sizeof(tuning[0]); i++)
      wr8(d, tuning[i][0], tuning[i][1]);

   /* --- interrupcion: "nueva muestra lista", pin GPIO1 activo en bajo --- */
   wr8(d, SYSTEM_INTERRUPT_CONFIG_GPIO, 0x04);
   wr8(d, GPIO_HV_MUX_ACTIVE_HIGH, rd8(d, GPIO_HV_MUX_ACTIVE_HIGH) & ~0x10);
   wr8(d, SYSTEM_INTERRUPT_CLEAR, 0x01);

   /* --- timing budget --- */
   uint32_t budget = leerPresupuesto(d);
   wr8(d, SYSTEM_SEQUENCE_CONFIG, 0xE8);   /* sin MSRC ni TCC */
   vl53l0xPresupuesto(d, budget);          /* recalcula con la nueva secuencia */

   /* --- calibraciones de referencia (VHV y fase) --- */
   wr8(d, SYSTEM_SEQUENCE_CONFIG, 0x01);
   if (!calibracionRef(d, 0x40)) return 0;
   wr8(d, SYSTEM_SEQUENCE_CONFIG, 0x02);
   if (!calibracionRef(d, 0x00)) return 0;
   wr8(d, SYSTEM_SEQUENCE_CONFIG, 0xE8);

   d->ok = 1;
   return 1;
}

void vl53l0xCambiarDireccion(vl53l0x_t *d, uint8_t nueva)
{
   wr8(d, I2C_SLAVE_DEVICE_ADDRESS, nueva & 0x7F);
   d->dir = nueva & 0x7F;
}

void vl53l0xApagar(gpioMap_t xshut)
{
   if (xshut == VL53L0X_SIN_XSHUT) return;
   gpioInit(xshut, GPIO_OUTPUT);
   gpioWrite(xshut, OFF);
}

void vl53l0xPrender(gpioMap_t xshut)
{
   if (xshut == VL53L0X_SIN_XSHUT) return;
   gpioInit(xshut, GPIO_INPUT);     /* alta impedancia: el modulo lo sube a 2,8 V */
}

uint8_t vl53l0xIniciarVarios(vl53l0x_t *devs, const gpioMap_t *xshuts,
                             const uint8_t *dirs, uint8_t n)
{
   uint8_t buenos = 0;

   for (uint8_t i = 0; i < n; i++) vl53l0xApagar(xshuts[i]);
   delay(10);

   for (uint8_t i = 0; i < n; i++) {
      devs[i].xshut  = xshuts[i];
      devs[i].dir    = VL53L0X_DIR_DEFAULT;
      devs[i].ok     = 0;
      devs[i].usaIrq = 0;
      devs[i].listo  = 0;

      vl53l0xPrender(xshuts[i]);
      delay(10);                        /* arranque: 1,2 ms segun datasheet */

      if (!vl53l0xPresente(VL53L0X_DIR_DEFAULT)) continue;
      vl53l0xCambiarDireccion(&devs[i], dirs[i]);
      delay(2);
      if (vl53l0xInit(&devs[i])) buenos++;
   }
   return buenos;
}

void vl53l0xContinuo(vl53l0x_t *d, uint32_t periodoMs)
{
   wr8(d, 0x80, 0x01); wr8(d, 0xFF, 0x01); wr8(d, 0x00, 0x00);
   wr8(d, 0x91, d->stopVariable);
   wr8(d, 0x00, 0x01); wr8(d, 0xFF, 0x00); wr8(d, 0x80, 0x00);

   if (periodoMs != 0) {
      uint16_t osc = rd16(d, OSC_CALIBRATE_VAL);
      d->oscCal = osc;
      if (osc != 0) periodoMs *= osc;
      wr32(d, SYSTEM_INTERMEASUREMENT_PERIOD, periodoMs);
      wr8(d, SYSRANGE_START, 0x04);     /* modo continuo temporizado */
   } else {
      wr8(d, SYSRANGE_START, 0x02);     /* back-to-back */
   }
}

void vl53l0xDetener(vl53l0x_t *d)
{
   wr8(d, SYSRANGE_START, 0x01);
   wr8(d, 0xFF, 0x01); wr8(d, 0x00, 0x00);
   wr8(d, 0x91, 0x00);
   wr8(d, 0x00, 0x01); wr8(d, 0xFF, 0x00);
}

uint8_t vl53l0xLeer(vl53l0x_t *d, uint16_t *mm)
{
   if (d->usaIrq) {
      if (!d->listo) {
         /* Sin aviso de la ISR. El pin queda en bajo hasta que se limpia la
          * interrupcion del sensor, asi que un nivel bajo puede ser un flanco
          * perdido... o el pin que todavia no termino de subir despues de la
          * lectura anterior (algunos modulos lo suben despacio). Solo en ese
          * caso se confirma por I2C, para no leer dos veces la misma medicion. */
         if (gpioRead(d->pinInt)) return 0;
         if ((rd8(d, RESULT_INTERRUPT_STATUS) & 0x07) == 0) return 0;
      }
      d->listo = 0;
   } else {
      if ((rd8(d, RESULT_INTERRUPT_STATUS) & 0x07) == 0) return 0;  /* todavia no */
   }
   *mm = rd16(d, RESULT_RANGE_STATUS + 10);
   wr8(d, SYSTEM_INTERRUPT_CLEAR, 0x01);    /* el pin G vuelve a subir */
   return 1;
}

/* ---------------- Modo por interrupcion ---------------- */
static void cbIrq(void *ctx, uint8_t nivel)
{
   vl53l0x_t *d = (vl53l0x_t *)ctx;
   /* Un aviso real deja el pin en bajo hasta que se lee la medicion. Si ya
    * volvio a alto, fue un pico de ruido en el flanco: se ignora. */
   if (nivel) return;
   if (d->listo) d->perdidas++;
   d->tAviso = tiempoCiclos();
   d->listo = 1;
}

void vl53l0xHabilitarIrq(vl53l0x_t *d, gpioMap_t pin, uint8_t puerto, uint8_t bit, uint8_t canal)
{
   /* GPIO1 es de drenaje abierto: el pull-up interno asegura el nivel alto
    * aunque el modulo no traiga su propia resistencia */
   gpioInit(pin, GPIO_INPUT_PULLUP);
   d->pinInt = pin;
   d->canalIrq = canal;
   d->listo = 0;
   d->perdidas = 0;
   gpioIrqConfigCtx(canal, puerto, bit, GPIO_IRQ_BAJADA, cbIrq, d);
   d->usaIrq = 1;
}

void vl53l0xDeshabilitarIrq(vl53l0x_t *d)
{
   if (!d->usaIrq) return;
   gpioIrqDeshabilitar(d->canalIrq);
   d->usaIrq = 0;
   d->listo = 0;
}

uint16_t vl53l0xMedirUnaVez(vl53l0x_t *d)
{
   uint16_t mm = 0xFFFF;
   d->timeout = 0;

   wr8(d, 0x80, 0x01); wr8(d, 0xFF, 0x01); wr8(d, 0x00, 0x00);
   wr8(d, 0x91, d->stopVariable);
   wr8(d, 0x00, 0x01); wr8(d, 0xFF, 0x00); wr8(d, 0x80, 0x00);
   wr8(d, SYSRANGE_START, 0x01);

   uint32_t t0 = tiempoCiclos();
   while (rd8(d, SYSRANGE_START) & 0x01) {
      if (tiempoUsDesde(t0) > TIMEOUT_US) { d->timeout = 1; return 0xFFFF; }
   }
   t0 = tiempoCiclos();
   while (!vl53l0xLeer(d, &mm)) {
      if (tiempoUsDesde(t0) > TIMEOUT_US) { d->timeout = 1; return 0xFFFF; }
   }
   return mm;
}
