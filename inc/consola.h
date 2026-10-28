/* Consola por el USB de la EDU-CIAA (UART_USB, 115200 8N1) */
#ifndef CONSOLA_H
#define CONSOLA_H

void consolaInit(void);
void imprimir(const char *fmt, ...);
int  teclaLeer(void);          /* -1 si no hay tecla, si no el caracter */
int  teclaEsperar(void);       /* bloquea hasta que llegue una tecla */
int  pedidoSalir(int tecla);   /* 1 si la tecla es 'q' o 'Q' */

#endif
