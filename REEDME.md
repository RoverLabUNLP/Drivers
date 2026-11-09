# Drivers de la EDU-CIAA y sus pruebas — Auto remoto

Drivers de bajo nivel de la EDU-CIAA (C, sAPI + LPCOpen, sin FreeRTOS) y un programa
de pruebas con menú para probar cada periférico por separado. Probado en la placa el 26 y el 29/10/2026.

| Periférico | Pines EDU-CIAA (conector) | Estado |
|---|---|---|
| Encoder LM393 izq / der | GPIO7 (CON2_38) / GPIO8 (CON2_40) | OK, 20 pulsos por vuelta |
| ADC (baterías) | CH2 pack motores (CON1_11), CH1 pack electrónica (CON1_13) | OK con 0 V y 3,3 V; falta probar con el divisor |
| UART con la ESP32-CAM | RS232_TXD → IO13, RS232_RXD ← IO14 | OK con la ESP32 real: driver `enlace_esp` probado (prueba 9, 02/11/2026), motores desde la web y telemetría |
| HC-SR04 | TRIG GPIO3 (CON2_34), ECHO GPIO5 (CON2_36) por divisor 1k/2k | OK (con los pines viejos) |
| 3 × VL53L0X | I2C_SDA / I2C_SCL (CON1_19 / 21), XSHUT LCD3 / LCD2 / LCD1 (CON2_26/28/30), G sin conectar | OK, máx. 1300 mm, ~20 mediciones/s; en la PCB solo por polling |
| IMU MPU6050 (GY-521) | I2C_SDA / I2C_SCL (0x68), INT → GPIO1 (CON2_32) | OK, 100 muestras/s por polling o por interrupción; giro y acelerómetro calibrados |
| Motores L298N | ENA T_FIL1 (CON1_36), ENB T_COL1 (CON1_31), IN1 T_COL0 (CON1_39), IN2 T_FIL2 (CON1_37), IN3 T_FIL3 (CON1_35), IN4 T_FIL0 (CON1_33) | OK, 1 motor por lado (con los pines viejos) |

Todos los pines están en `inc/pines.h` (PCB de 90 mm v2, 09/11/2026: pines reasignados para que la placa casi no tenga puentes; la versión anterior era la del esquemático Rev A). Los ToF y el HC-SR04, motores, baterías e IMU cambiaron de pin: repetir las pruebas 2, 3, 4, 5 y 7 con la placa. El I2C corre a 400 kHz (`I2C_VELOCIDAD`); si con cables largos aparecen errores, bajarlo a 100000.

## Contenido

```
config.mk                 configuración del proyecto (va en app/)
inc/                      headers de los drivers y de las pruebas
src/                      drivers + main.c (menú) + test_sensores.c + test_actuadores.c + test_imu.c
                          + test_integral.c + test_enlace.c
test_uart_esp32cam/       sketch de eco para la ESP32-CAM (prueba 6)
auto_remoto/main.c        programa del auto (sin menú): manejo desde la web + telemetría
```

Drivers (se copian tal cual al firmware final, no dependen de los tests ni de FreeRTOS):
`motores`, `encoders`, `hcsr04`, `vl53l0x`, `mpu6050`, `bateria`, `enlace_esp`, `gpio_irq`, `tiempo`.
Pruebas: `main.c`, `consola`, `test_sensores.c`, `test_actuadores.c`, `test_imu.c`, `test_integral.c`, `test_enlace.c`.

## Programa del auto (`auto_remoto/`)

`auto_remoto/main.c` es el programa del auto, sin el menú de pruebas: arranca solo al darle energía, los
motores los maneja la web por `$TWIST` (si pasan 500 ms sin comandos, para) y cada 100 ms manda `$SENS`,
`$ODOM` e `$IMU` a la ESP32-CAM. Para compilarlo: proyecto nuevo desde el template, copiar `config.mk`, `inc/`
y los drivers de `src/` (todo menos `main.c`, `test_*.c` y `tests.h`) y este `main.c`. Al arrancar calibra el
giroscopio: dejar el auto quieto unos 2 s.

## Cómo compilar y cargar (Embedded IDE, CIAA Software 1.1)

1. Crear un proyecto nuevo desde el template **EDU-CIAA-NXP lib-sAPI**.
2. Borrar `app/src/app.c` y `app/inc/app.h` del template.
3. Copiar `config.mk`, `inc/` y `src/` de esta carpeta adentro de `app/` (reemplazar el `config.mk`).
4. **Build**. Tiene que terminar mostrando el tamaño (~30 KB de `text`).
5. Cable al micro-USB **DEBUG** y ejecutar el target **`download`** (no `debug`: no graba y deja el USB tomado).
6. Primera vez en cada PC, si OpenOCD da `LIBUSB_ERROR_NOT_FOUND`: Zadig → *Options → List All Devices* →
   **Interface 0** del FTDI → **WinUSB**. No tocar la Interface 1 (es el puerto COM de la consola).

`config.mk` tiene `DEFINES+=OVERRIDE_SAPI_HCSR04_GPIO_IRQ`: sin eso, el driver de HC-SR04 de la sAPI
define `GPIO0/1/2_IRQHandler` y el enlazado falla con "multiple definition".

## Cómo usar el menú

Abrir el COM de la placa a **115200 8N1** (PuTTY recomendado: manda cada tecla al tocarla).
LED verde = menú, LED3 = prueba corriendo, `q` = volver al menú.
Al arrancar espera 3 s una tecla (LED azul): si no llega ninguna, entra directo a la prueba 8 con los motores
manejados desde la web (sirve para usarlo con las pilas, sin la compu).

| Tecla | Prueba |
|---|---|
| 1 | Encoders LM393 (`r` pone las cuentas a cero) |
| 2 | HC-SR04 |
| 3 | VL53L0X (después `1` = un sensor, `3` = tres con XSHUT; y solo polling: en la PCB los pines G no están conectados) |
| 4 | Motores L298N |
| 5 | Batería por ADC |
| 6 | UART con la ESP32-CAM |
| 7 | IMU MPU6050 (`p` / `i`; `c` recalibra el giro, `a` calibra el acelerómetro, `v` oculta los datos) |
| 8 | Todo junto: motores por teclado + todos los sensores por interrupción + telemetría real a la ESP32 |
| 9 | Enlace con la ESP32-CAM: `$TWIST` desde la web y telemetría de prueba; `m` = motores desde la web |

Reglas: desenchufar el USB antes de cablear y conectar **solo** el módulo que se prueba.
Los sensores se alimentan de +3,3 V y +5 V de la placa (+5 V da ~4,5 V por el diodo del USB).
Nada que no sea el divisor puede meter más de 3,3 V en un pin.

## Conexión de cada prueba

**1 · Encoders:** VCC +3,3 V, GND, DO → GPIO7 (izq) / GPIO8 (der). Tapar la ranura: 1 pulso por tapada.

**2 · HC-SR04:** VCC +5 V, GND, Trig → GPIO3. Echo → 1 kΩ → punto medio → GPIO5, y 2 kΩ del punto medio a GND.
Antes de conectar GPIO5, medir el punto medio con 5 V en la entrada del divisor: tiene que dar ≤ 3,3 V.

**3 · VL53L0X:** VIN +3,3 V (nunca 5 V: los pull-up del I2C del módulo van a VIN), GND, SDA → I2C_SDA, SCL → I2C_SCL,
X → LCD3 / LCD2 / LCD1 (con uno solo, X sin conectar). G sin conectar (se leen por polling; `TOF_INT_EN_PCB` en `pines.h`).
Cada segundo muestra cuántas mediciones leyó por sensor: tiene que dar ~20.

**4 · Motores:** en el L298N, jumpers de ENA/ENB **sacados**, jumper de 5 V puesto, bornera +5V sin conectar.
ENA/ENB van al pin **de afuera** de cada par (el de adentro es 5 V). IN1 → T_COL0, IN2 → T_FIL2, IN3 → T_FIL3, IN4 → T_FIL0; ENA → T_FIL1, ENB → T_COL1.
Batería + → +12V, batería − → GND, y **GND del L298N a GND de la EDU-CIAA**.
Motor izq en OUT1/OUT2, der en OUT3/OUT4 (con 4 motores, 2 en paralelo por lado).
Orden: USB → terminal → `4` → recién ahí la batería. Para apagar, al revés.
Teclas: `w` `s` `a` `d`, `1`/`2` un solo lado, `+`/`-` velocidad, espacio freno, `x` libre. Se paran solos a los 5 s.

**5 · ADC:** cable en CH2 (motores) o CH1 (electrónica), tocar GND (da 0) y +3,3 V (da 1023). En el auto: divisor 100k/33k desde la batería.

**6 · UART:** puente de RS232_TXD a RS232_RXD → vuelven los `PING n`. Con la ESP32-CAM: cargarle
`test_uart_esp32cam.ino` (RX GPIO13, TX GPIO14) → vuelven como `ESP: PING n`.

**7 · IMU:** VCC +3,3 V (o +5 V), GND, SDA → I2C_SDA, SCL → I2C_SCL, AD0 a GND o suelto (0x68), INT → GPIO1
(modo interrupción). XDA y XCL sin conectar. Al entrar calibra el giroscopio: no moverla esos 2 s (si se mueve,
avisa y reintenta). Quieta y plana: Z ≈ +1000 mg, |a| ≈ 1000 mg, giro ≈ 0 °/s. En modo `i`, con `v` (datos ocultos):
100 muestras/s, dt ≈ 9,9 ms, perdidas 0. La calibración del acelerómetro de nuestro GY-521 está cargada en
`CAL_ACC_OFF` / `CAL_ACC_1G` (`test_imu.c`); si se cambia el sensor, repetirla con `a` (6 posiciones).

**9 · Enlace ESP32-CAM:** mismo cableado que la 6 (RS232_TXD → IO13, RS232_RXD ← IO14, GND común), con el firmware
del repo esp32-cam. Tramas de texto `$CUERPO*HH\r\n` (checksum XOR opcional). Se descarta todo hasta un `$` (la ESP32
mete basura en IO14 al arrancar). En la web tienen que verse la distancia de prueba subiendo y bajando, la batería real
y la IMU en 1.0 / 2.0 / 3.0; al tocar W/A/S/D cambian v y w en la consola. `m` maneja los motores (izq = v − w,
der = v + w; si pasan 500 ms sin `$TWIST`, para). Probar con las ruedas en el aire.

## APIs para el firmware de alto nivel

| Módulo | Funciones |
|---|---|
| `motores` | `motoresInit()`, `motorVelocidad(MOTOR_IZQ, -100..100)`, `motorFrenar()`, `motorLibre()`, `motoresParar()` |
| `encoders` | `encodersInit()`, `encoderPulsos()`, `encoderPeriodoUs()`, `encoderRpmX10()`, `encoderReset()` |
| `hcsr04` | `hcsr04Init()`, `hcsr04Disparar()` → `hcsr04Estado()` → `hcsr04LeerMm()` |
| `vl53l0x` | `vl53l0xIniciarVarios()`, `vl53l0xContinuo()`, `vl53l0xLeer()` (no bloqueante), `vl53l0xHabilitarIrq()` |
| `mpu6050` | `mpu6050Init()`, `mpu6050CalibrarGiro()`, `mpu6050FijarCalAcc()`, `mpu6050Leer()` (no bloqueante, con `t_ciclos` por muestra), `mpu6050HabilitarIrq()` |
| `bateria` | `bateriaInit()`, `bateriaMilivolts()` |
| `enlace_esp` | `enlaceInit()`, `enlaceProcesar()` (llamar seguido), `enlaceTwistNuevo()`, `enlaceTwistVencido(ms)`, `enlaceEnviarSens()`, `enlaceEnviarOdom()`, `enlaceEnviarImu()`, `enlaceLeerStats()` |
| `gpio_irq` | `gpioIrqConfig()` / `gpioIrqConfigCtx()`: interrupción por flanco en cualquier GPIO |
| `tiempo` | `tiempoCiclos()`, `tiempoUsDesde()`: tiempo en µs con el contador de ciclos |

Para FreeRTOS:
- Los callbacks de `gpio_irq` corren dentro de la interrupción: solo funciones `...FromISR`.
- Modo interrupción de la IMU y los VL53L0X: la ISR (`cbIrq` de cada driver) solo marca el instante y levanta un
  aviso; la lectura por I2C la hace `...Leer()`. En FreeRTOS, agregar en `cbIrq` un `xTaskNotifyFromISR` para
  despertar a la tarea que lee.
- Para integrar el giro, usar el dt medido entre muestras (`tiempoCiclosAUs(t2 - t1)` con `t_ciclos`): el reloj
  interno del MPU6050 da ~9,9 ms, no 10.
- `vl53l0xInit` tarda (calibraciones): antes del scheduler o en una tarea de inicio.
- El I2C no es reentrante: los VL53L0X (y el MPU6050) desde una sola tarea o con un mutex.
- Sentido de giro: `MOTOR_INVERTIR_IZQ/DER` en `motores.h`, sin cruzar cables.

## Pendientes

- [ ] Sentido de giro de cada lado con las ruedas montadas
- [ ] PWM mínimo con el que arranca el motor (para el PID)
- [ ] Motores + encoders juntos: rpm por lado
- [ ] Corriente del motor libre y bloqueado (fusible)
- [ ] Prueba con los 4 motores (2 en paralelo por canal)
- [ ] Calibrar el offset de cada VL53L0X en su lugar definitivo
- [x] Probar `enlace_esp` con la ESP32-CAM (prueba 9)
- [ ] Arranque automático sin USB (con solo las pilas se queda en el menú)
- [ ] Acordar unidades de `$TWIST` (supuesto −1..1), campos de `$IMU` y checksum
- [ ] Batería real por ADC con el divisor 100k/33k (uno por pack: CH2 motores, CH1 electrónica)
- [ ] Fusión para el rumbo: Kalman 1D o filtro complementario con el giro Z + los encoders
