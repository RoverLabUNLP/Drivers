/*
 * Prueba 9: enlace UART con la ESP32-CAM (driver enlace_esp).
 *
 *  - Muestra los $TWIST que llegan desde la web (v, w), hace cuanto llego el
 *    ultimo y los contadores de errores.
 *  - Cada 100 ms manda telemetria de prueba: $SENS (distancia que sube y baja
 *    entre 20 y 100 cm + la bateria de electronica real), $ODOM (pulsos reales
 *    de los encoders, si estan conectados) y $IMU (1.0, 2.0, 3.0 fijos). En la web
 *    se tienen que ver esos valores en vez de los simulados por la ESP32.
 *  - 'm' = manejar los motores desde la web: izq = v - w, der = v + w, suponiendo
 *    v y w entre -1 y 1 (UNIDADES A CONFIRMAR). Si pasan 500 ms sin $TWIST, para.
 *  - 'q' = volver
 */
#include "tests.h"
#include "consola.h"
#include "pines.h"
#include "enlace_esp.h"
#include "motores.h"
#include "encoders.h"
#include "bateria.h"

#define WATCHDOG_MS   500

static int16_t aPorcentaje(int32_t milesimas)
{
   int32_t p = milesimas / 10;          /* 1.000 -> 100 % */
   if (p > 100)  p = 100;
   if (p < -100) p = -100;
   return (int16_t)p;
}

static void imprimirMilesimas(int32_t v)
{
   const char *s = "";
   if (v < 0) { s = "-"; v = -v; }
   imprimirCola("%s%ld.%03ld", s, (long)(v / 1000), (long)(v % 1000));
}

void testEnlace(void)
{
   uint8_t  motoresWeb = 0;
   int32_t  dist = 200;    /* mm, para el $SENS de prueba */
   int32_t  paso = 20;
   enlaceTwist_t tw = { 0, 0, 0 };

   imprimir("\r\n--- ENLACE CON LA ESP32-CAM (driver enlace_esp) ---\r\n"
            "RS232_TXD -> IO13 de la ESP32, RS232_RXD <- IO14, GND comun.\r\n"
            "Abrir la pagina de la camara y mover con W/A/S/D.\r\n"
            "  m = manejar los motores desde la web (v y w supuestos entre -1 y 1)\r\n"
            "  q = volver\r\n\r\n");

   enlaceInit();
   encodersInit();
   bateriaInit();
   motoresInit();

   delay_t tTele, tImp;
   delayConfig(&tTele, 100);
   delayConfig(&tImp, 250);

   while (1) {
      consolaServicio();
      enlaceProcesar();

      int c = teclaLeer();
      if (pedidoSalir(c)) break;
      if (c == 'm') {
         motoresWeb = !motoresWeb;
         motoresParar();
         imprimirCola("\r\n*** Motores desde la web: %s ***\r\n\r\n", motoresWeb ? "SI" : "NO");
      }

      /* Comando nuevo desde la web */
      if (enlaceTwistNuevo(&tw) && motoresWeb) {
         motorVelocidad(MOTOR_IZQ, aPorcentaje(tw.v - tw.w));
         motorVelocidad(MOTOR_DER, aPorcentaje(tw.v + tw.w));
      }
      /* Seguridad: sin comandos -> parar */
      if (motoresWeb && enlaceTwistVencido(WATCHDOG_MS) &&
          (motorLeerVelocidad(MOTOR_IZQ) || motorLeerVelocidad(MOTOR_DER))) {
         motoresParar();
         imprimirCola("(sin $TWIST por %d ms: motores parados)\r\n", WATCHDOG_MS);
      }

      /* Telemetria de prueba hacia la ESP32 */
      if (delayRead(&tTele)) {
         dist += paso;
         if (dist >= 1000) { dist = 1000; paso = -20; }
         if (dist <= 200)  { dist = 200;  paso = 20; }
         enlaceEnviarSens((uint32_t)dist, bateriaElectronicaMv());
         enlaceEnviarOdom((int32_t)encoderPulsos(ENC_IZQ), (int32_t)encoderPulsos(ENC_DER));
         enlaceEnviarImu(1000, 2000, 3000);
      }

      /* Estado en la consola */
      if (delayRead(&tImp)) {
         enlaceStats_t s;
         enlaceLeerStats(&s);
         uint32_t ms = enlaceMsDesdeTwist();
         imprimirCola("TWIST v ");
         imprimirMilesimas(tw.v);
         imprimirCola(" w ");
         imprimirMilesimas(tw.w);
         if (ms == 0xFFFFFFFFu) imprimirCola(" (todavia no llego ninguno)");
         else                   imprimirCola(" (hace %lu ms)", (unsigned long)ms);
         imprimirCola(" | motores %s izq %4d%% der %4d%% | tramas %lu (twist %lu, otras %lu) err cs %lu fmt %lu desb %lu\r\n",
                      motoresWeb ? "WEB" : "off",
                      motorLeerVelocidad(MOTOR_IZQ), motorLeerVelocidad(MOTOR_DER),
                      (unsigned long)s.tramasOk, (unsigned long)s.twist, (unsigned long)s.desconocidas,
                      (unsigned long)s.errChecksum, (unsigned long)s.errFormato,
                      (unsigned long)(s.rxDesborde + s.txDescartados));
      }
   }

   motoresParar();
   enlaceDetener();
   while (!consolaColaVacia()) consolaServicio();
}
