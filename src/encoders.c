#include "encoders.h"
#include "pines.h"
#include "gpio_irq.h"
#include "tiempo.h"
#include "chip.h"

typedef struct {
   volatile uint32_t pulsos;
   volatile uint32_t rebotes;
   volatile uint32_t tUltimo;     /* ciclos del ultimo pulso valido */
   volatile uint32_t periodo;     /* ciclos entre los dos ultimos pulsos */
   volatile uint8_t  valido;      /* hay al menos 2 pulsos -> periodo valido */
   volatile uint32_t tBajada;     /* ciclos de la ultima bajada */
   volatile uint8_t  bajo;        /* la senal esta (o estuvo) en bajo */
} encEstado_t;

static encEstado_t enc[2];

/* Se interrumpe en los dos flancos. Un pulso cuenta en la SUBIDA, pero solo
 * si la senal estuvo en bajo al menos ENC_MIN_BAJO_US: asi se descartan los
 * rebotes del comparador cuando la ranura se tapa o destapa despacio. */
static void flanco(encoder_t e, uint8_t nivel)
{
   uint32_t ahora = tiempoCiclos();
   encEstado_t *s = &enc[e];

   if (!nivel) {                       /* bajada: arranca a contar el tiempo en bajo */
      s->tBajada = ahora;
      s->bajo = 1;
      return;
   }

   /* subida */
   if (!s->bajo || tiempoCiclosAUs(ahora - s->tBajada) < ENC_MIN_BAJO_US) {
      s->rebotes++;
      return;
   }
   s->bajo = 0;

   if (s->pulsos > 0) {
      uint32_t dt = ahora - s->tUltimo;
      if (tiempoCiclosAUs(dt) < ENC_ANTIRREBOTE_US) {
         s->rebotes++;
         return;
      }
      s->periodo = dt;
      s->valido  = 1;
   }
   s->tUltimo = ahora;
   s->pulsos++;
}

static void cbIzq(uint8_t nivel) { flanco(ENC_IZQ, nivel); }
static void cbDer(uint8_t nivel) { flanco(ENC_DER, nivel); }

void encodersInit(void)
{
   gpioInit(PIN_ENC_IZQ, GPIO_INPUT);
   gpioInit(PIN_ENC_DER, GPIO_INPUT);
   encoderReset(ENC_IZQ);
   encoderReset(ENC_DER);
   /* Si arranca en bajo, la primera subida cuenta */
   enc[ENC_IZQ].tBajada = enc[ENC_DER].tBajada = tiempoCiclos();
   enc[ENC_IZQ].bajo = !gpioRead(PIN_ENC_IZQ);
   enc[ENC_DER].bajo = !gpioRead(PIN_ENC_DER);
   gpioIrqConfig(IRQ_CANAL_ENC_IZQ, ENC_IZQ_PUERTO, ENC_IZQ_BIT, GPIO_IRQ_AMBOS, cbIzq);
   gpioIrqConfig(IRQ_CANAL_ENC_DER, ENC_DER_PUERTO, ENC_DER_BIT, GPIO_IRQ_AMBOS, cbDer);
}

void encoderReset(encoder_t e)
{
   __disable_irq();
   enc[e].pulsos = 0;
   enc[e].rebotes = 0;
   enc[e].periodo = 0;
   enc[e].valido = 0;
   __enable_irq();
}

uint32_t encoderPulsos(encoder_t e)  { return enc[e].pulsos; }
uint32_t encoderRebotes(encoder_t e) { return enc[e].rebotes; }

uint32_t encoderPeriodoUs(encoder_t e)
{
   __disable_irq();
   uint32_t valido  = enc[e].valido;
   uint32_t periodo = enc[e].periodo;
   uint32_t tUlt    = enc[e].tUltimo;
   __enable_irq();

   if (!valido) return 0;
   /* Si hace mucho que no llega un pulso, la rueda esta parada.
    * (Hay que llamar a esta funcion al menos cada ~20 s para que la
    *  vuelta del contador de ciclos no engane; en la practica se llama
    *  cada pocos ms.) */
   if (tiempoUsDesde(tUlt) > ENC_TIMEOUT_US) return 0;
   return tiempoCiclosAUs(periodo);
}

uint32_t encoderRpmX10(encoder_t e)
{
   uint32_t p = encoderPeriodoUs(e);
   if (p == 0) return 0;
   /* rpm = 60e6 / (periodo_us * ranuras)  ->  x10 = 600e6 / (...) */
   return 600000000u / (p * ENC_RANURAS);
}
