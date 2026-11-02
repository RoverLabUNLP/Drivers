#include "bateria.h"
#include "pines.h"

void bateriaInit(void)
{
   adcConfig(ADC_ENABLE);
}

uint16_t bateriaCrudoCanal(adcMap_t canal)
{
   uint32_t suma = 0;
   for (int i = 0; i < BAT_PROMEDIO; i++) suma += adcRead(canal);
   return (uint16_t)(suma / BAT_PROMEDIO);
}

uint32_t bateriaPinMilivoltsCanal(adcMap_t canal)
{
   return ((uint32_t)bateriaCrudoCanal(canal) * BAT_VREF_MV) / 1023u;
}

uint32_t bateriaMilivoltsCanal(adcMap_t canal)
{
   uint32_t mvPin = bateriaPinMilivoltsCanal(canal);
   return (mvPin * (BAT_R_ARRIBA + BAT_R_ABAJO)) / BAT_R_ABAJO;
}

uint32_t bateriaMotoresMv(void)     { return bateriaMilivoltsCanal(PIN_BAT_MOTORES); }
uint32_t bateriaElectronicaMv(void) { return bateriaMilivoltsCanal(PIN_BAT_ELECTRONICA); }

uint16_t bateriaCrudo(void)        { return bateriaCrudoCanal(PIN_BAT_MOTORES); }
uint32_t bateriaPinMilivolts(void) { return bateriaPinMilivoltsCanal(PIN_BAT_MOTORES); }
uint32_t bateriaMilivolts(void)    { return bateriaMilivoltsCanal(PIN_BAT_MOTORES); }
