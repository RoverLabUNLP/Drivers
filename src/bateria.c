#include "bateria.h"
#include "pines.h"

void bateriaInit(void)
{
   adcConfig(ADC_ENABLE);
}

uint16_t bateriaCrudo(void)
{
   uint32_t suma = 0;
   for (int i = 0; i < BAT_PROMEDIO; i++) suma += adcRead(PIN_BATERIA);
   return (uint16_t)(suma / BAT_PROMEDIO);
}

uint32_t bateriaPinMilivolts(void)
{
   return ((uint32_t)bateriaCrudo() * BAT_VREF_MV) / 1023u;
}

uint32_t bateriaMilivolts(void)
{
   uint32_t mvPin = bateriaPinMilivolts();
   return (mvPin * (BAT_R_ARRIBA + BAT_R_ABAJO)) / BAT_R_ABAJO;
}
