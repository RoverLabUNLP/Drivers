/*
 * Pruebas de sensores: encoders, HC-SR04, VL53L0X y bateria.
 * Cada una corre hasta que se aprieta 'q' en la consola.
 */
#include "tests.h"
#include "consola.h"
#include "pines.h"
#include "encoders.h"
#include "hcsr04.h"
#include "vl53l0x.h"
#include "bateria.h"

/* ------------------------------------------------------------------ */
void testEncoders(void)
{
   imprimir("\r\n--- ENCODERS LM393 ---\r\n"
            "Izq: DO -> GPIO7   Der: DO -> GPIO8   VCC 3,3 V\r\n"
            "Pasar algo por la ranura o girar el disco a mano.\r\n"
            "'r' = poner cuentas a cero, 'q' = volver\r\n\r\n");
   encodersInit();

   delay_t t;
   delayConfig(&t, 200);
   while (1) {
      int c = teclaLeer();
      if (pedidoSalir(c)) break;
      if (c == 'r') { encoderReset(ENC_IZQ); encoderReset(ENC_DER); }

      if (delayRead(&t)) {
         uint32_t rpmI = encoderRpmX10(ENC_IZQ), rpmD = encoderRpmX10(ENC_DER);
         imprimir("IZQ: %5lu pulsos %4lu.%lu rpm (reb %lu) | DER: %5lu pulsos %4lu.%lu rpm (reb %lu)  [nivel %d/%d]\r\n",
                  (unsigned long)encoderPulsos(ENC_IZQ), (unsigned long)(rpmI / 10), (unsigned long)(rpmI % 10),
                  (unsigned long)encoderRebotes(ENC_IZQ),
                  (unsigned long)encoderPulsos(ENC_DER), (unsigned long)(rpmD / 10), (unsigned long)(rpmD % 10),
                  (unsigned long)encoderRebotes(ENC_DER),
                  gpioRead(PIN_ENC_IZQ), gpioRead(PIN_ENC_DER));
      }
   }
}

/* ------------------------------------------------------------------ */
void testHcsr04(void)
{
   imprimir("\r\n--- HC-SR04 ---\r\n"
            "VCC 5 V   TRIG -> LCD1   ECHO -> divisor 1k/2k -> LCD2\r\n"
            "Mide cada 100 ms. 'q' = volver\r\n\r\n");
   hcsr04Init();

   delay_t t;
   delayConfig(&t, 100);
   while (1) {
      if (pedidoSalir(teclaLeer())) break;

      hcsr04Estado_t e = hcsr04Estado();
      if (e == HCSR04_LISTO) {
         uint32_t mm = hcsr04LeerMm();
         imprimir("%4lu.%lu cm   (eco %lu us)\r\n",
                  (unsigned long)(mm / 10), (unsigned long)(mm % 10),
                  (unsigned long)hcsr04AnchoUs());
      } else if (e == HCSR04_SIN_ECO) {
         imprimir("sin eco (fuera de rango, o revisar 5 V / divisor de ECHO)\r\n");
         hcsr04LeerMm();   /* consume el aviso; no cambia la ultima medicion */
      }

      if (delayRead(&t)) hcsr04Disparar();
   }
}

/* ------------------------------------------------------------------ */
static const gpioMap_t xshuts[3] = { PIN_XSHUT1, PIN_XSHUT2, PIN_XSHUT3 };
static const uint8_t   dirsTof[3] = { 0x30, 0x31, 0x32 };

void testTof(void)
{
   static const gpioMap_t pinesInt[3]  = { PIN_TOF1_INT, PIN_TOF2_INT, PIN_TOF3_INT };
   static const uint8_t   puertosInt[3] = { TOF1_INT_PUERTO, TOF2_INT_PUERTO, TOF3_INT_PUERTO };
   static const uint8_t   bitsInt[3]    = { TOF1_INT_BIT, TOF2_INT_BIT, TOF3_INT_BIT };
   static const uint8_t   canalesInt[3] = { IRQ_CANAL_TOF1, IRQ_CANAL_TOF2, IRQ_CANAL_TOF3 };

   vl53l0x_t tof[3];
   uint16_t  ultimo[3] = { 0, 0, 0 };
   uint32_t  cuenta[3] = { 0, 0, 0 };
   uint8_t   n;

   imprimir("\r\n--- VL53L0X ---\r\n"
            "VIN 3,3 V   SDA/SCL -> I2C_SDA/I2C_SCL   X -> GPIO4/5/6\r\n"
            "  1 = UN sensor (X puede quedar sin conectar)\r\n"
            "  3 = TRES sensores con XSHUT (direcciones 0x30/0x31/0x32)\r\n> ");
   int c;
   do { c = teclaEsperar(); } while (c != '1' && c != '3' && !pedidoSalir(c));
   if (pedidoSalir(c)) return;
   imprimir("%c\r\n", c);

   i2cInit(BUS_I2C, I2C_VELOCIDAD);

   if (c == '1') {
      n = 1;
      /* Puede haber quedado con otra direccion de una prueba anterior */
      static const uint8_t candidatas[] = { 0x29, 0x30, 0x31, 0x32 };
      tof[0].dir = 0;
      tof[0].xshut = VL53L0X_SIN_XSHUT;
      tof[0].ok = 0;
      tof[0].usaIrq = 0;
      for (uint32_t i = 0; i < sizeof(candidatas); i++) {
         if (vl53l0xPresente(candidatas[i])) { tof[0].dir = candidatas[i]; break; }
      }
      if (tof[0].dir == 0) {
         imprimir("No responde ningun VL53L0X (0x29..0x32). Revisar VIN, GND, SDA, SCL.\r\n");
         return;
      }
      imprimir("Encontrado en 0x%02X, inicializando... ", tof[0].dir);
      vl53l0xInit(&tof[0]);
      if (tof[0].ok) imprimir("OK\r\n");
      else           imprimir("FALLO%s\r\n", tof[0].timeout ? " (timeout)" : "");
   } else {
      n = 3;
      uint8_t ok = vl53l0xIniciarVarios(tof, xshuts, dirsTof, 3);
      for (int i = 0; i < 3; i++)
         imprimir("ToF%d (XSHUT GPIO%d) -> 0x%02X: %s\r\n", i + 1, 4 + i, dirsTof[i],
                  tof[i].ok ? "OK" : "no responde / fallo el init");
      imprimir("%d de 3 listos\r\n", ok);
   }

   imprimir("\r\nModo de lectura:\r\n"
            "  p = polling (pregunta por I2C)\r\n"
            "  i = interrupcion (pin G de ToF1/2/3 -> LCD3 / LCD4 / LCDRS)\r\n> ");
   do { c = teclaEsperar(); } while (c != 'p' && c != 'i' && !pedidoSalir(c));
   if (pedidoSalir(c)) {
      for (uint8_t i = 0; i < n; i++) if (tof[i].ok) vl53l0xDetener(&tof[i]);
      return;
   }
   imprimir("%c\r\n", c);
   uint8_t conIrq = (c == 'i');

   for (uint8_t i = 0; i < n; i++) {
      if (!tof[i].ok) continue;
      if (conIrq) vl53l0xHabilitarIrq(&tof[i], pinesInt[i], puertosInt[i], bitsInt[i], canalesInt[i]);
      vl53l0xContinuo(&tof[i], 50);   /* una medicion cada 50 ms = 20 Hz */
   }

   for (uint8_t i = 0; i < n; i++)
      if (tof[i].ok)
         imprimir("ToF%d: presupuesto %lu us, OSC_CALIBRATE_VAL %u\r\n", i + 1,
                  (unsigned long)tof[i].presupuestoUs, tof[i].oscCal);
   imprimir("Cada segundo: mediciones leidas por sensor (tiene que dar ~20).\r\n'q' = volver\r\n\r\n");
   delay_t t, tSeg;
   delayConfig(&t, 100);
   delayConfig(&tSeg, 1000);
   while (1) {
      if (pedidoSalir(teclaLeer())) break;

      for (uint8_t i = 0; i < n; i++) {
         uint16_t mm;
         if (tof[i].ok && vl53l0xLeer(&tof[i], &mm)) { ultimo[i] = mm; cuenta[i]++; }
      }
      if (delayRead(&t)) {
         for (uint8_t i = 0; i < n; i++) {
            if (!tof[i].ok)                         imprimir("ToF%d:   --     ", i + 1);
            else if (ultimo[i] >= VL53L0X_FUERA_RANGO) imprimir("ToF%d: fuera    ", i + 1);
            else                                    imprimir("ToF%d: %4u mm  ", i + 1, ultimo[i]);
         }
         imprimir("\r\n");
      }
      if (delayRead(&tSeg)) {
         imprimir("   [%s]", conIrq ? "interrupcion" : "polling");
         for (uint8_t i = 0; i < n; i++) {
            if (!tof[i].ok) continue;
            imprimir("  ToF%d %2lu/s", i + 1, (unsigned long)cuenta[i]);
            if (conIrq) imprimir(" (perdidas %lu)", (unsigned long)tof[i].perdidas);
            cuenta[i] = 0;
         }
         imprimir("\r\n");
      }
   }

   for (uint8_t i = 0; i < n; i++) {
      if (!tof[i].ok) continue;
      vl53l0xDeshabilitarIrq(&tof[i]);   /* la ISR apunta a 'tof', que deja de existir */
      vl53l0xDetener(&tof[i]);
   }
}

/* ------------------------------------------------------------------ */
void testBateria(void)
{
   imprimir("\r\n--- BATERIA (ADC CH1) ---\r\n"
            "Divisor 100k/33k a CH1. Sin bateria se puede probar con un\r\n"
            "potenciometro entre 3,3 V y GND (cursor a CH1). NUNCA mas de 3,3 V en CH1.\r\n"
            "'q' = volver\r\n\r\n");
   bateriaInit();

   delay_t t;
   delayConfig(&t, 500);
   while (1) {
      if (pedidoSalir(teclaLeer())) break;
      if (delayRead(&t)) {
         uint32_t pin = bateriaPinMilivolts(), bat = bateriaMilivolts();
         imprimir("ADC %4u   pin %lu.%03lu V   bateria %lu.%02lu V\r\n",
                  bateriaCrudo(),
                  (unsigned long)(pin / 1000), (unsigned long)(pin % 1000),
                  (unsigned long)(bat / 1000), (unsigned long)((bat % 1000) / 10));
      }
   }
}
