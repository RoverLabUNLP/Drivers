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
            "  2  HC-SR04               (GPIO3 trig / GPIO5 echo)\r\n"
            "  3  VL53L0X               (I2C0 + XSHUT LCD3/LCD2/LCD1)\r\n"
            "  4  Motores L298N         (T_FIL1 / T_COL1 + T_COL0, T_FIL2, T_FIL3, T_FIL0)\r\n"
            "  5  Baterias por ADC      (CH2 motores, CH1 electronica)\r\n"
            "  6  UART con ESP32-CAM    (RS232_TXD / RS232_RXD)\r\n"
            "  7  IMU MPU6050           (I2C0, direccion 0x68)\r\n"
            "  8  TODO JUNTO            (motores + todos los sensores, auto armado)\r\n"
            "  9  Enlace ESP32-CAM      (driver enlace_esp: $TWIST y telemetria)\r\n"
            "Elegir: ");
}

/* Espera una tecla hasta 'ms' milisegundos. -1 si no llego ninguna. */
static int teclaEsperarMs(uint32_t ms)
{
   delay_t t;
   int c;
   delayConfig(&t, ms);
   while ((c = teclaLeer()) < 0) {
      consolaServicio();
      if (delayRead(&t)) return -1;
   }
   return c;
}

int main(void)
{
   boardConfig();
   consolaInit();
   tiempoInit();

   /* Arranque sin compu: si en 3 s no llega ninguna tecla por la consola USB,
    * entra directo a la prueba 8 (todos los sensores + motores desde la web).
    * Con la compu conectada, apretar cualquier tecla para ir al menu. */
   imprimir("\r\nAuto remoto: tecla en 3 s = menu. Si no, MODO AUTO (prueba 8 + web).\r\n");
   gpioWrite(LEDB, ON);
   int t = teclaEsperarMs(3000);
   gpioWrite(LEDB, OFF);
   if (t < 0) {
      gpioWrite(LED3, ON);
      testIntegral();
      gpioWrite(LED3, OFF);
   }

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
      case '7': testImu();      break;
      case '8': testIntegral(); break;
      case '9': testEnlace();   break;
      default:  imprimir("Opcion invalida\r\n"); break;
      }
      gpioWrite(LED3, OFF);
   }
   return 0;
}
