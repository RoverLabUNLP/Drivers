#include "gpio_irq.h"
#include "chip.h"

#define N_CANALES 8

static gpioIrqCallback_t callbacks[N_CANALES];
static uint8_t puertos[N_CANALES];
static uint8_t bits[N_CANALES];

void gpioIrqConfig(uint8_t canal, uint8_t puerto, uint8_t bit,
                   gpioIrqFlanco_t flanco, gpioIrqCallback_t cb)
{
   if (canal >= N_CANALES) return;

   callbacks[canal] = cb;
   puertos[canal]   = puerto;
   bits[canal]      = bit;

   /* Conecta GPIOpuerto[bit] al canal de interrupcion */
   Chip_SCU_GPIOIntPinSel(canal, puerto, bit);

   Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH(canal));
   Chip_PININT_SetPinModeEdge(LPC_GPIO_PIN_INT, PININTCH(canal));

   if (flanco == GPIO_IRQ_SUBIDA || flanco == GPIO_IRQ_AMBOS)
      Chip_PININT_EnableIntHigh(LPC_GPIO_PIN_INT, PININTCH(canal));
   else
      Chip_PININT_DisableIntHigh(LPC_GPIO_PIN_INT, PININTCH(canal));

   if (flanco == GPIO_IRQ_BAJADA || flanco == GPIO_IRQ_AMBOS)
      Chip_PININT_EnableIntLow(LPC_GPIO_PIN_INT, PININTCH(canal));
   else
      Chip_PININT_DisableIntLow(LPC_GPIO_PIN_INT, PININTCH(canal));

   NVIC_ClearPendingIRQ((IRQn_Type)(PIN_INT0_IRQn + canal));
   NVIC_EnableIRQ((IRQn_Type)(PIN_INT0_IRQn + canal));
}

void gpioIrqDeshabilitar(uint8_t canal)
{
   if (canal >= N_CANALES) return;
   NVIC_DisableIRQ((IRQn_Type)(PIN_INT0_IRQn + canal));
   Chip_PININT_DisableIntHigh(LPC_GPIO_PIN_INT, PININTCH(canal));
   Chip_PININT_DisableIntLow(LPC_GPIO_PIN_INT, PININTCH(canal));
   callbacks[canal] = 0;
}

static void atender(uint8_t canal)
{
   Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH(canal));
   if (callbacks[canal]) {
      uint8_t nivel = Chip_GPIO_GetPinState(LPC_GPIO_PORT, puertos[canal], bits[canal]);
      callbacks[canal](nivel);
   }
}

/* Nombres de los handlers en el vector de LPCOpen / firmware_v3 */
void GPIO0_IRQHandler(void) { atender(0); }
void GPIO1_IRQHandler(void) { atender(1); }
void GPIO2_IRQHandler(void) { atender(2); }
void GPIO3_IRQHandler(void) { atender(3); }
void GPIO4_IRQHandler(void) { atender(4); }
void GPIO5_IRQHandler(void) { atender(5); }
void GPIO6_IRQHandler(void) { atender(6); }
void GPIO7_IRQHandler(void) { atender(7); }
