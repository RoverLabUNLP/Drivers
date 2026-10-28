#include "tiempo.h"
#include "chip.h"

static uint32_t ciclosPorUs = 204;

void tiempoInit(void)
{
   SystemCoreClockUpdate();
   ciclosPorUs = SystemCoreClock / 1000000u;
   if (ciclosPorUs == 0) ciclosPorUs = 204;

   CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;  /* habilita el DWT */
   DWT->CTRL        |= DWT_CTRL_CYCCNTENA_Msk;      /* arranca el contador */
}

uint32_t tiempoCiclos(void)                 { return DWT->CYCCNT; }
uint32_t tiempoCiclosAUs(uint32_t ciclos)   { return ciclos / ciclosPorUs; }
uint32_t tiempoUsDesde(uint32_t t0Ciclos)   { return (DWT->CYCCNT - t0Ciclos) / ciclosPorUs; }

void tiempoEsperarUs(uint32_t us)
{
   uint32_t t0 = DWT->CYCCNT;
   uint32_t n  = us * ciclosPorUs;
   while ((DWT->CYCCNT - t0) < n) { }
}
