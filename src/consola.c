#include "consola.h"
#include "sapi.h"
#include "chip.h"
#include <stdarg.h>
#include <stdio.h>

#define CONSOLA_UART   LPC_USART2    /* la que usa la sAPI para UART_USB */
#define COLA_TAM       2048
#define FIFO_TX        16

static char     cola[COLA_TAM];
static uint32_t colaIn, colaOut;     /* colaIn == colaOut -> vacia */
static uint32_t descartados;

void consolaInit(void)
{
   uartConfig(UART_USB, 115200);
}

void imprimir(const char *fmt, ...)
{
   static char buf[512];   /* los textos de ayuda de los tests superan 160 caracteres */
   va_list ap;

   /* Si hay algo encolado, se manda antes, para no mezclar el orden */
   while (!consolaColaVacia()) consolaServicio();

   va_start(ap, fmt);
   vsnprintf(buf, sizeof(buf), fmt, ap);
   va_end(ap);
   uartWriteString(UART_USB, buf);
}

void imprimirCola(const char *fmt, ...)
{
   static char buf[256];
   va_list ap;
   va_start(ap, fmt);
   int n = vsnprintf(buf, sizeof(buf), fmt, ap);
   va_end(ap);
   if (n < 0) return;
   if (n >= (int)sizeof(buf)) n = sizeof(buf) - 1;

   for (int i = 0; i < n; i++) {
      uint32_t sig = (colaIn + 1) % COLA_TAM;
      if (sig == colaOut) { descartados += (uint32_t)(n - i); return; }   /* llena */
      cola[colaIn] = buf[i];
      colaIn = sig;
   }
}

void consolaServicio(void)
{
   /* THRE = la FIFO de transmision esta vacia: entran hasta 16 bytes */
   if (!(Chip_UART_ReadLineStatus(CONSOLA_UART) & UART_LSR_THRE)) return;
   for (int i = 0; i < FIFO_TX && colaOut != colaIn; i++) {
      Chip_UART_SendByte(CONSOLA_UART, (uint8_t)cola[colaOut]);
      colaOut = (colaOut + 1) % COLA_TAM;
   }
}

uint8_t  consolaColaVacia(void)   { return colaIn == colaOut; }
uint32_t consolaDescartados(void) { return descartados; }

int teclaLeer(void)
{
   uint8_t c;
   if (uartReadByte(UART_USB, &c)) return c;
   return -1;
}

int teclaEsperar(void)
{
   int c;
   while ((c = teclaLeer()) < 0) { consolaServicio(); }
   return c;
}

int pedidoSalir(int tecla) { return tecla == 'q' || tecla == 'Q'; }
