/*
 * Pruebas de motores (L298N) y de la UART con la ESP32-CAM.
 */
#include "tests.h"
#include "consola.h"
#include "pines.h"
#include "motores.h"
#include "encoders.h"
#include <stdio.h>

#define MOTOR_AUTOPARADA_MS  5000   /* sin teclas por 5 s -> se paran solos */

/* ------------------------------------------------------------------ */
void testMotores(void)
{
   int16_t vel = 50;
   const char *estado = "libre";

   imprimir("\r\n--- MOTORES (L298N) ---\r\n"
            "ENA -> T_FIL1  ENB -> T_FIL3  IN1..IN4 -> GPIO0..GPIO3  (sacar jumpers ENA/ENB)\r\n"
            "Motores con la BATERIA en VS del L298N, GND comun con la EDU-CIAA.\r\n"
            "  w adelante   s atras   a gira izq   d gira der\r\n"
            "  1 solo izq   2 solo der   + / - velocidad (de a 10%%)\r\n"
            "  espacio = freno   x = rueda libre   q = volver\r\n"
            "Si no se toca nada por 5 s, se paran solos.\r\n\r\n");
   motoresInit();
   encodersInit();     /* si estan conectados, muestra las rpm */

   delay_t tImp, tSeg;
   delayConfig(&tImp, 300);
   delayConfig(&tSeg, MOTOR_AUTOPARADA_MS);

   while (1) {
      int c = teclaLeer();
      if (pedidoSalir(c)) break;
      if (c >= 0) delayConfig(&tSeg, MOTOR_AUTOPARADA_MS);   /* reinicia la autoparada */

      switch (c) {
      case 'w': motorVelocidad(MOTOR_IZQ,  vel); motorVelocidad(MOTOR_DER,  vel); estado = "adelante"; break;
      case 's': motorVelocidad(MOTOR_IZQ, -vel); motorVelocidad(MOTOR_DER, -vel); estado = "atras";    break;
      case 'a': motorVelocidad(MOTOR_IZQ, -vel); motorVelocidad(MOTOR_DER,  vel); estado = "gira izq"; break;
      case 'd': motorVelocidad(MOTOR_IZQ,  vel); motorVelocidad(MOTOR_DER, -vel); estado = "gira der"; break;
      case '1': motorVelocidad(MOTOR_IZQ,  vel); motorLibre(MOTOR_DER);           estado = "solo izq"; break;
      case '2': motorLibre(MOTOR_IZQ);           motorVelocidad(MOTOR_DER,  vel); estado = "solo der"; break;
      case ' ': motorFrenar(MOTOR_IZQ); motorFrenar(MOTOR_DER);                  estado = "freno";    break;
      case 'x': motoresParar();                                                   estado = "libre";    break;
      case '+': if (vel < 100) vel += 10; break;
      case '-': if (vel > 0)   vel -= 10; break;
      default: break;
      }

      /* Si cambio la velocidad mientras andaba, aplicarla */
      if (c == '+' || c == '-') {
         int16_t vi = motorLeerVelocidad(MOTOR_IZQ), vd = motorLeerVelocidad(MOTOR_DER);
         if (vi) motorVelocidad(MOTOR_IZQ, vi > 0 ? vel : -vel);
         if (vd) motorVelocidad(MOTOR_DER, vd > 0 ? vel : -vel);
      }

      if (delayRead(&tSeg)) {
         if (motorLeerVelocidad(MOTOR_IZQ) || motorLeerVelocidad(MOTOR_DER))
            imprimir("(autoparada)\r\n");
         motoresParar();
         estado = "libre";
      }

      if (delayRead(&tImp)) {
         uint32_t ri = encoderRpmX10(ENC_IZQ), rd = encoderRpmX10(ENC_DER);
         imprimir("%-9s vel %3d%%  | izq %4d%%  der %4d%% | enc izq %3lu.%lu rpm  der %3lu.%lu rpm\r\n",
                  estado, vel, motorLeerVelocidad(MOTOR_IZQ), motorLeerVelocidad(MOTOR_DER),
                  (unsigned long)(ri / 10), (unsigned long)(ri % 10),
                  (unsigned long)(rd / 10), (unsigned long)(rd % 10));
      }
   }
   motoresParar();
}

/* ------------------------------------------------------------------ */
void testUartEsp(void)
{
   uint32_t n = 0;
   uint8_t  b;
   char     linea[96];
   uint32_t largo = 0;

   imprimir("\r\n--- UART con la ESP32-CAM (UART_232, %d 8N1) ---\r\n"
            "RS232_TXD -> RX de la ESP32 (GPIO13)   RS232_RXD <- TX de la ESP32 (GPIO14)\r\n"
            "Sin ESP32: puentear RS232_TXD con RS232_RXD (loopback) y tienen que volver los PING.\r\n"
            "Cada 1 s se manda 'PING n'. Lo que se escribe aca tambien se manda.\r\n"
            "'q' = volver\r\n\r\n", UART_ESP_BAUD);
   uartConfig(UART_ESP, UART_ESP_BAUD);

   delay_t t;
   delayConfig(&t, 1000);
   while (1) {
      int c = teclaLeer();
      if (pedidoSalir(c)) break;
      if (c >= 0) uartWriteByte(UART_ESP, (uint8_t)c);

      if (delayRead(&t)) {
         char ping[24];
         snprintf(ping, sizeof(ping), "PING %lu\r\n", (unsigned long)n++);
         uartWriteString(UART_ESP, ping);
         imprimir("-> %s", ping);
      }

      /* Junta lo recibido por lineas para que se lea mejor */
      while (uartReadByte(UART_ESP, &b)) {
         if (b == '\n' || largo >= sizeof(linea) - 1) {
            linea[largo] = 0;
            imprimir("<- %s\r\n", linea);
            largo = 0;
         } else if (b != '\r') {
            linea[largo++] = (char)b;
         }
      }
   }
}
