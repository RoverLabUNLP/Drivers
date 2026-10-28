/*
 * Auto remoto - Taller de Proyecto I (UNLP)
 * drivers_auto: drivers de bajo nivel de la EDU-CIAA + pruebas individuales
 *
 * Un solo programa con todos los drivers. Desde la consola (USB, 115200 8N1)
 * se elige que periferico probar. Conectar SOLO el modulo que se va a probar.
 *
 * Los drivers (motores, encoders, hcsr04, vl53l0x, bateria, gpio_irq, tiempo)
 * no dependen de los tests ni de FreeRTOS: se copian tal cual al firmware final.
 */
#include "sapi.h"
#include "consola.h"
#include "tiempo.h"
#include "tests.h"

static void menu(void)
{
   imprimir("\r\n=========== Auto remoto - drivers_auto ===========\r\n"
            "  1  Encoders LM393        (GPIO7 / GPIO8)\r\n"
            "  2  HC-SR04               (LCD1 trig / LCD2 echo)\r\n"
            "  3  VL53L0X               (I2C0 + XSHUT GPIO4/5/6)\r\n"
            "  4  Motores L298N         (T_FIL1 / T_FIL3 + GPIO0..3)\r\n"
            "  5  Bateria por ADC       (CH1)\r\n"
            "  6  UART con ESP32-CAM    (RS232_TXD / RS232_RXD)\r\n"
            "Elegir: ");
}

int main(void)
{
   boardConfig();
   consolaInit();
   tiempoInit();

   while (1) {
      menu();
      gpioWrite(LEDG, ON);                 /* verde = esperando en el menu */
      int c = teclaEsperar();
      gpioWrite(LEDG, OFF);
      imprimir("%c\r\n", c);

      gpioWrite(LED3, ON);                 /* LED3 = corriendo una prueba */
      switch (c) {
      case '1': testEncoders(); break;
      case '2': testHcsr04();   break;
      case '3': testTof();      break;
      case '4': testMotores();  break;
      case '5': testBateria();  break;
      case '6': testUartEsp();  break;
      default:  imprimir("Opcion invalida\r\n"); break;
      }
      gpioWrite(LED3, OFF);
   }
   return 0;
}
