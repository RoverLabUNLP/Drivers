/*
 * Enlace UART con la ESP32-CAM (UART_232 = USART3, RS232_TXD / RS232_RXD).
 *
 * Tramas de texto, una por linea:
 *        $CUERPO*HH\r\n
 *   - empiezan con '$' y terminan con '\n' ('\r' se ignora)
 *   - "*HH" es opcional: checksum XOR (en hexa) de los caracteres entre '$' y '*',
 *     como en NMEA. Si viene, se verifica; si no coincide, la trama se descarta.
 *   - todo lo que llegue antes de un '$' se descarta: asi se ignora la basura que
 *     manda la ESP32 por GPIO14 cuando arranca.
 *
 * Tramas que usa hoy el sistema (firmware de la ESP32 y la web):
 *   ESP32 -> EDU-CIAA   $TWIST,<v>,<w>          comando de movimiento
 *                       ($TWIST,0.00,0.00 = parar: lo manda la ESP32 al arrancar,
 *                        si se cierra la web o si pasan 500 ms sin comandos)
 *   EDU-CIAA -> ESP32   $SENS,<distancia_cm>,<bateria_V>
 *                       $ODOM,<izq>,<der>
 *                       $IMU,<v1>,<v2>,<v3>
 *
 * PENDIENTE de acordar con Valentino / Sebastian: unidades de v y w (-1..1 o
 * m/s y rad/s), que significa cada valor de $IMU, y si el checksum es obligatorio.
 *
 * Los numeros se manejan en punto fijo (milesimas): "0.50" -> 500, "-1.2" -> -1200.
 *
 * Uso (sin bloquear nunca):
 *    enlaceInit();
 *    en el lazo o en una tarea:
 *       enlaceProcesar();                       // arma las tramas recibidas
 *       if (enlaceTwistNuevo(&tw)) { ... }       // llego un comando
 *       if (enlaceTwistVencido(500)) parar();   // seguridad: se corto el enlace
 *       enlaceEnviarSens(mm, mV);               // telemetria (va a una cola)
 *
 * Con FreeRTOS: llamar a enlaceProcesar() desde UNA sola tarea. Los enlaceEnviar*()
 * se pueden llamar desde cualquier tarea, pero no desde una interrupcion.
 */
#ifndef ENLACE_ESP_H
#define ENLACE_ESP_H

#include <stdint.h>

#define ENLACE_CHECKSUM_TX          0    /* 1 = agregar "*HH" a lo que se manda */
#define ENLACE_CHECKSUM_OBLIGATORIO 0    /* 1 = descartar tramas recibidas sin "*HH" */
#define ENLACE_MAX_TRAMA            64   /* caracteres entre '$' y fin de linea */

typedef struct {
   int32_t  v;          /* velocidad lineal, en milesimas de la unidad que mande la web */
   int32_t  w;          /* velocidad angular, idem */
   uint32_t tCiclos;    /* cuando llego (ciclos, ver tiempo.h) */
} enlaceTwist_t;

typedef struct {
   uint32_t tramasOk;        /* tramas validas recibidas (de cualquier tipo) */
   uint32_t twist;           /* de esas, cuantas $TWIST */
   uint32_t desconocidas;    /* validas pero de un tipo que no se conoce */
   uint32_t errChecksum;     /* checksum que no coincide */
   uint32_t errFormato;      /* demasiado largas, caracteres invalidos, campos mal */
   uint32_t rxDesborde;      /* bytes perdidos porque el buffer de recepcion se lleno */
   uint32_t txDescartados;   /* bytes que no entraron en la cola de transmision */
} enlaceStats_t;

void     enlaceInit(void);
void     enlaceDetener(void);   /* apaga la interrupcion (la UART vuelve a quedar para polling) */
void     enlaceProcesar(void);

/* 1 si llego un $TWIST desde la ultima llamada (y lo deja en *t) */
uint8_t  enlaceTwistNuevo(enlaceTwist_t *t);
/* Ultimo $TWIST recibido (aunque ya se haya leido). 0 si todavia no llego ninguno */
uint8_t  enlaceUltimoTwist(enlaceTwist_t *t);
/* ms desde el ultimo $TWIST; 0xFFFFFFFF si nunca llego */
uint32_t enlaceMsDesdeTwist(void);
/* 1 si paso mas de timeoutMs sin $TWIST (o nunca llego): hay que parar los motores */
uint8_t  enlaceTwistVencido(uint32_t timeoutMs);

/* Ultima trama valida de tipo desconocido (texto sin '$' ni checksum) */
const char *enlaceUltimaDesconocida(void);

/* Telemetria hacia la ESP32 (no bloquea: va a una cola que vacia la interrupcion) */
void     enlaceEnviarSens(uint32_t distanciaMm, uint32_t bateriaMv);
void     enlaceEnviarOdom(int32_t izq, int32_t der);
void     enlaceEnviarImu(int32_t v1Milesimas, int32_t v2Milesimas, int32_t v3Milesimas);
/* Trama cualquiera: 'cuerpo' sin '$' ni fin de linea, p. ej. "PING,3" */
void     enlaceEnviarTrama(const char *cuerpo);

void     enlaceLeerStats(enlaceStats_t *s);

#endif
