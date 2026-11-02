#include "enlace_esp.h"
#include "pines.h"
#include "tiempo.h"
#include "sapi.h"
#include "chip.h"
#include <stdio.h>
#include <string.h>

#ifdef SAPI_USE_INTERRUPTS
#error "enlace_esp define UART3_IRQHandler: con SAPI_USE_INTERRUPTS la sAPI tambien lo define"
#endif

#define UART_HW     LPC_USART3       /* UART_232 de la sAPI */
#define UART_IRQ    USART3_IRQn
#define RX_TAM      256
#define TX_TAM      512
#define FIFO_TX     16

/* ---------------- Buffers circulares (in == out -> vacio) ---------------- */
static volatile uint8_t  rxBuf[RX_TAM];
static volatile uint32_t rxIn, rxOut;
static volatile uint8_t  txBuf[TX_TAM];
static volatile uint32_t txIn, txOut;

static volatile uint32_t rxDesborde, txDescartados;

/* ---------------- Estado del armado de tramas (solo enlaceProcesar) ---------------- */
static char     trama[ENLACE_MAX_TRAMA + 1];
static uint32_t largo;
static uint8_t  enTrama;

static enlaceTwist_t twistUltimo;
static uint8_t       twistHay, twistNuevo;
static tick_t        twistTickMs;

static char          desconocida[ENLACE_MAX_TRAMA + 1] = "";
static enlaceStats_t st;

/* ================= Interrupcion de la UART ================= */
void UART3_IRQHandler(void)
{
   (void)Chip_UART_ReadIntIDReg(UART_HW);   /* reconoce la interrupcion */

   /* Recepcion: vaciar la FIFO al buffer */
   while (Chip_UART_ReadLineStatus(UART_HW) & UART_LSR_RDR) {
      uint8_t b = Chip_UART_ReadByte(UART_HW);
      uint32_t sig = (rxIn + 1) % RX_TAM;
      if (sig == rxOut) { rxDesborde++; continue; }
      rxBuf[rxIn] = b;
      rxIn = sig;
   }

   /* Transmision: llenar la FIFO (16 bytes) cuando esta vacia */
   if (Chip_UART_ReadLineStatus(UART_HW) & UART_LSR_THRE) {
      if (txOut == txIn) {
         Chip_UART_IntDisable(UART_HW, UART_IER_THREINT);   /* nada mas para mandar */
      } else {
         for (int i = 0; i < FIFO_TX && txOut != txIn; i++) {
            Chip_UART_SendByte(UART_HW, txBuf[txOut]);
            txOut = (txOut + 1) % TX_TAM;
         }
      }
   }
}

/* Encola bytes para transmitir y arranca la transmision si estaba parada */
static void txPoner(const char *s, uint32_t n)
{
   uint32_t primask = __get_PRIMASK();
   __disable_irq();                      /* varias tareas pueden mandar a la vez */

   for (uint32_t i = 0; i < n; i++) {
      uint32_t sig = (txIn + 1) % TX_TAM;
      if (sig == txOut) { txDescartados += n - i; break; }
      txBuf[txIn] = (uint8_t)s[i];
      txIn = sig;
   }
   /* Si la FIFO esta vacia, cargarla ya; la interrupcion sigue con el resto */
   if (Chip_UART_ReadLineStatus(UART_HW) & UART_LSR_THRE) {
      for (int i = 0; i < FIFO_TX && txOut != txIn; i++) {
         Chip_UART_SendByte(UART_HW, txBuf[txOut]);
         txOut = (txOut + 1) % TX_TAM;
      }
   }
   if (txOut != txIn) Chip_UART_IntEnable(UART_HW, UART_IER_THREINT);

   __set_PRIMASK(primask);
}

/* ================= Init ================= */
void enlaceInit(void)
{
   uartConfig(UART_ESP, UART_ESP_BAUD);      /* pines, baudios y FIFO con la sAPI */

   rxIn = rxOut = txIn = txOut = 0;
   enTrama = 0; largo = 0;
   twistHay = twistNuevo = 0;
   memset(&st, 0, sizeof(st));
   rxDesborde = txDescartados = 0;

   Chip_UART_IntEnable(UART_HW, UART_IER_RBRINT | UART_IER_RLSINT);
   NVIC_SetPriority(UART_IRQ, IRQ_PRIORIDAD_DRIVERS);
   NVIC_ClearPendingIRQ(UART_IRQ);
   NVIC_EnableIRQ(UART_IRQ);
}

void enlaceDetener(void)
{
   NVIC_DisableIRQ(UART_IRQ);
   Chip_UART_IntDisable(UART_HW, UART_IER_RBRINT | UART_IER_RLSINT | UART_IER_THREINT);
}

/* ================= Numeros en punto fijo ================= */
/* "-0.25" -> -250 (milesimas). Avanza *p. Devuelve 0 si no hay numero. */
static uint8_t leerMilesimas(const char **p, int32_t *out)
{
   const char *s = *p;
   int32_t signo = 1, ent = 0, dec = 0, escala = 100;
   uint8_t digitos = 0;

   if (*s == '-') { signo = -1; s++; }
   else if (*s == '+') { s++; }
   while (*s >= '0' && *s <= '9') {
      if (ent < 1000000) ent = ent * 10 + (*s - '0');
      s++; digitos++;
   }
   if (*s == '.') {
      s++;
      while (*s >= '0' && *s <= '9') {
         if (escala) { dec += (*s - '0') * escala; escala /= 10; }
         s++; digitos++;
      }
   }
   if (!digitos) return 0;
   *out = signo * (ent * 1000 + dec);
   *p = s;
   return 1;
}

static int hexa(char c)
{
   if (c >= '0' && c <= '9') return c - '0';
   if (c >= 'A' && c <= 'F') return c - 'A' + 10;
   if (c >= 'a' && c <= 'f') return c - 'a' + 10;
   return -1;
}

/* ================= Recepcion ================= */
static void procesarTrama(void)
{
   trama[largo] = 0;

   /* Checksum opcional: "...*HH" */
   char *ast = strchr(trama, '*');
   if (ast) {
      int h = hexa(ast[1]), l = hexa(ast[2]);
      if (h < 0 || l < 0 || ast[3] != 0) { st.errFormato++; return; }
      uint8_t x = 0;
      for (char *c = trama; c < ast; c++) x ^= (uint8_t)*c;
      if (x != (uint8_t)((h << 4) | l)) { st.errChecksum++; return; }
      *ast = 0;                                  /* queda solo el cuerpo */
   } else if (ENLACE_CHECKSUM_OBLIGATORIO) {
      st.errFormato++;
      return;
   }

   if (strncmp(trama, "TWIST,", 6) == 0) {
      const char *p = trama + 6;
      enlaceTwist_t t;
      if (!leerMilesimas(&p, &t.v) || *p != ',') { st.errFormato++; return; }
      p++;
      if (!leerMilesimas(&p, &t.w) || *p != 0)  { st.errFormato++; return; }
      t.tCiclos = tiempoCiclos();
      twistUltimo = t;
      twistHay = 1;
      twistNuevo = 1;
      twistTickMs = tickRead();
      st.twist++;
      st.tramasOk++;
      return;
   }

   strcpy(desconocida, trama);
   st.desconocidas++;
   st.tramasOk++;
}

void enlaceProcesar(void)
{
   while (rxOut != rxIn) {
      char b = (char)rxBuf[rxOut];
      rxOut = (rxOut + 1) % RX_TAM;

      if (b == '$') {                         /* empieza una trama (re-sincroniza) */
         if (enTrama && largo) st.errFormato++;   /* la anterior quedo cortada */
         enTrama = 1; largo = 0;
         continue;
      }
      if (!enTrama) continue;                 /* basura antes del '$': se descarta */
      if (b == '\r') continue;
      if (b == '\n') { procesarTrama(); enTrama = 0; continue; }
      if (b < 32 || b > 126 || largo >= ENLACE_MAX_TRAMA) {
         st.errFormato++; enTrama = 0;         /* caracter raro o demasiado larga */
         continue;
      }
      trama[largo++] = b;
   }
}

uint8_t enlaceTwistNuevo(enlaceTwist_t *t)
{
   if (!twistNuevo) return 0;
   twistNuevo = 0;
   *t = twistUltimo;
   return 1;
}

uint8_t enlaceUltimoTwist(enlaceTwist_t *t)
{
   if (!twistHay) return 0;
   *t = twistUltimo;
   return 1;
}

uint32_t enlaceMsDesdeTwist(void)
{
   if (!twistHay) return 0xFFFFFFFFu;
   return (uint32_t)(tickRead() - twistTickMs);
}

uint8_t enlaceTwistVencido(uint32_t timeoutMs)
{
   return enlaceMsDesdeTwist() > timeoutMs;
}

const char *enlaceUltimaDesconocida(void) { return desconocida; }

/* ================= Transmision ================= */
void enlaceEnviarTrama(const char *cuerpo)
{
   char b[ENLACE_MAX_TRAMA + 8];
   uint32_t n = 0;
   uint8_t x = 0;

   b[n++] = '$';
   for (const char *c = cuerpo; *c && n < ENLACE_MAX_TRAMA + 1; c++) { b[n++] = *c; x ^= (uint8_t)*c; }
#if ENLACE_CHECKSUM_TX
   static const char hx[] = "0123456789ABCDEF";
   b[n++] = '*'; b[n++] = hx[x >> 4]; b[n++] = hx[x & 0x0F];
#else
   (void)x;
#endif
   b[n++] = '\r'; b[n++] = '\n';
   txPoner(b, n);
}

/* milesimas -> texto con 1 decimal y signo: -1250 -> "-1.2" */
static void fmtDecimal(char *out, uint32_t tam, int32_t v)
{
   const char *signo = "";
   if (v < 0) { signo = "-"; v = -v; }
   snprintf(out, tam, "%s%ld.%ld", signo, (long)(v / 1000), (long)((v % 1000) / 100));
}

void enlaceEnviarSens(uint32_t distanciaMm, uint32_t bateriaMv)
{
   char c[40];
   snprintf(c, sizeof(c), "SENS,%lu,%lu.%lu", (unsigned long)(distanciaMm / 10),
            (unsigned long)(bateriaMv / 1000), (unsigned long)((bateriaMv % 1000) / 100));
   enlaceEnviarTrama(c);
}

void enlaceEnviarOdom(int32_t izq, int32_t der)
{
   char c[40];
   snprintf(c, sizeof(c), "ODOM,%ld,%ld", (long)izq, (long)der);
   enlaceEnviarTrama(c);
}

void enlaceEnviarImu(int32_t v1, int32_t v2, int32_t v3)
{
   char a[16], b[16], d[16], c[56];
   fmtDecimal(a, sizeof(a), v1);
   fmtDecimal(b, sizeof(b), v2);
   fmtDecimal(d, sizeof(d), v3);
   snprintf(c, sizeof(c), "IMU,%s,%s,%s", a, b, d);
   enlaceEnviarTrama(c);
}

void enlaceLeerStats(enlaceStats_t *s)
{
   *s = st;
   s->rxDesborde = rxDesborde;
   s->txDescartados = txDescartados;
}
