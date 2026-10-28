#include "consola.h"
#include "sapi.h"
#include <stdarg.h>
#include <stdio.h>

void consolaInit(void)
{
   uartConfig(UART_USB, 115200);
}

void imprimir(const char *fmt, ...)
{
   static char buf[512];   /* los textos de ayuda de los tests superan 160 caracteres */
   va_list ap;
   va_start(ap, fmt);
   vsnprintf(buf, sizeof(buf), fmt, ap);
   va_end(ap);
   uartWriteString(UART_USB, buf);
}

int teclaLeer(void)
{
   uint8_t c;
   if (uartReadByte(UART_USB, &c)) return c;
   return -1;
}

int teclaEsperar(void)
{
   int c;
   while ((c = teclaLeer()) < 0) { }
   return c;
}

int pedidoSalir(int tecla) { return tecla == 'q' || tecla == 'Q'; }
