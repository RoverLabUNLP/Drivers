/* Consola por el USB de la EDU-CIAA (UART_USB, 115200 8N1) */
#ifndef CONSOLA_H
#define CONSOLA_H

#include <stdint.h>

void consolaInit(void);
void imprimir(const char *fmt, ...);   /* bloquea hasta mandar todo (~87 us por caracter) */
int  teclaLeer(void);          /* -1 si no hay tecla, si no el caracter */
int  teclaEsperar(void);       /* bloquea hasta que llegue una tecla */
int  pedidoSalir(int tecla);   /* 1 si la tecla es 'q' o 'Q' */

/* Impresion sin esperar: el texto va a una cola y consolaServicio() lo manda de a
 * poco (hasta 16 bytes por llamada, lo que entra en la FIFO de la UART). Sirve
 * para mostrar datos sin frenar la lectura de los sensores. Si la cola se llena,
 * lo que no entra se descarta (consolaDescartados lo cuenta). */
void     imprimirCola(const char *fmt, ...);
void     consolaServicio(void);    /* llamar en cada vuelta del lazo principal */
uint8_t  consolaColaVacia(void);
uint32_t consolaDescartados(void);

#endif
