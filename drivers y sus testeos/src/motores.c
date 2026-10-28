#include "motores.h"
#include "pines.h"

typedef struct {
   pwmMap_t  pwm;
   gpioMap_t inA;
   gpioMap_t inB;
   bool_t    invertir;
   int16_t   pct;
} motorHw_t;

static motorHw_t motores[2] = {
   { PIN_PWM_IZQ, PIN_IN1, PIN_IN2, MOTOR_INVERTIR_IZQ, 0 },
   { PIN_PWM_DER, PIN_IN3, PIN_IN4, MOTOR_INVERTIR_DER, 0 },
};

static uint8_t pctA255(int16_t pct)
{
   if (pct < 0)   pct = -pct;
   if (pct > 100) pct = 100;
   return (uint8_t)((pct * 255) / 100);
}

void motoresInit(void)
{
   pwmConfig(0, PWM_ENABLE);                  /* arranca el SCT (1 kHz) */
   for (int i = 0; i < 2; i++) {
      gpioInit(motores[i].inA, GPIO_OUTPUT);
      gpioInit(motores[i].inB, GPIO_OUTPUT);
      pwmConfig(motores[i].pwm, PWM_ENABLE_OUTPUT);
      motorLibre((motor_t)i);
   }
}

void motorVelocidad(motor_t m, int16_t pct)
{
   motorHw_t *h = &motores[m];
   if (pct > 100)  pct = 100;
   if (pct < -100) pct = -100;
   h->pct = pct;

   bool_t adelante = (pct >= 0);
   if (h->invertir) adelante = !adelante;

   /* Primero bajar el PWM, despues cambiar el sentido: evita un pico
    * con el puente en un estado intermedio */
   pwmWrite(h->pwm, 0);
   gpioWrite(h->inA, adelante ? ON  : OFF);
   gpioWrite(h->inB, adelante ? OFF : ON);
   pwmWrite(h->pwm, pctA255(pct));
}

void motorFrenar(motor_t m)
{
   motorHw_t *h = &motores[m];
   h->pct = 0;
   gpioWrite(h->inA, OFF);
   gpioWrite(h->inB, OFF);
   pwmWrite(h->pwm, 255);       /* EN = 1 con IN iguales -> freno */
}

void motorLibre(motor_t m)
{
   motorHw_t *h = &motores[m];
   h->pct = 0;
   pwmWrite(h->pwm, 0);         /* EN = 0 -> rueda libre */
   gpioWrite(h->inA, OFF);
   gpioWrite(h->inB, OFF);
}

void motoresParar(void)
{
   motorLibre(MOTOR_IZQ);
   motorLibre(MOTOR_DER);
}

int16_t motorLeerVelocidad(motor_t m) { return motores[m].pct; }
