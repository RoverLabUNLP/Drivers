#include "hcsr04.h"
#include "pines.h"
#include "gpio_irq.h"
#include "tiempo.h"

static volatile hcsr04Estado_t estado = HCSR04_INACTIVO;
static volatile uint32_t tDisparo;
static volatile uint32_t tSubida;
static volatile uint8_t  subio;
static volatile uint32_t anchoUs;
static uint32_t ultimaMm;

static void cbEcho(uint8_t nivel)
{
   uint32_t ahora = tiempoCiclos();
   if (estado != HCSR04_MIDIENDO) return;

   if (nivel) {                 /* flanco de subida: empieza el eco */
      tSubida = ahora;
      subio = 1;
   } else if (subio) {          /* flanco de bajada: termina */
      anchoUs = tiempoCiclosAUs(ahora - tSubida);
      estado = (anchoUs >= HCSR04_TIMEOUT_US) ? HCSR04_SIN_ECO : HCSR04_LISTO;
   }
}

void hcsr04Init(void)
{
   gpioInit(PIN_TRIG, GPIO_OUTPUT);
   gpioWrite(PIN_TRIG, OFF);
   gpioInit(PIN_ECHO, GPIO_INPUT);
   estado = HCSR04_INACTIVO;
   gpioIrqConfig(IRQ_CANAL_ECHO, ECHO_PUERTO, ECHO_BIT, GPIO_IRQ_AMBOS, cbEcho);
}

uint8_t hcsr04Disparar(void)
{
   if (hcsr04Estado() == HCSR04_MIDIENDO) return 0;

   subio = 0;
   tDisparo = tiempoCiclos();
   estado = HCSR04_MIDIENDO;

   gpioWrite(PIN_TRIG, ON);
   tiempoEsperarUs(12);         /* el datasheet pide >= 10 us */
   gpioWrite(PIN_TRIG, OFF);
   return 1;
}

hcsr04Estado_t hcsr04Estado(void)
{
   /* Timeout por si ECHO nunca sube (sensor desconectado, sin 5 V, divisor mal) */
   if (estado == HCSR04_MIDIENDO &&
       tiempoUsDesde(tDisparo) > HCSR04_TIMEOUT_US + 5000) {
      estado = HCSR04_SIN_ECO;
   }
   return estado;
}

uint32_t hcsr04LeerMm(void)
{
   if (estado == HCSR04_LISTO) {
      ultimaMm = (anchoUs * 343u) / 2000u;   /* us * 0,343 mm/us / 2 */
      estado = HCSR04_INACTIVO;
   } else if (estado == HCSR04_SIN_ECO) {
      estado = HCSR04_INACTIVO;              /* se consume el aviso */
   }
   return ultimaMm;
}

uint32_t hcsr04AnchoUs(void) { return anchoUs; }
