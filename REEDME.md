# Drivers de la EDU-CIAA y sus pruebas — Auto remoto

Drivers de bajo nivel de la EDU-CIAA (C, sAPI + LPCOpen, sin FreeRTOS) y un programa
de pruebas con menú para probar cada periférico por separado. Probado en la placa el 26 y el 29/10/2026.

| Periférico | Pines EDU-CIAA (conector) | Estado |
|---|---|---|
| Encoder LM393 izq / der | GPIO7 (CON2_38) / GPIO8 (CON2_40) | OK, 20 pulsos por vuelta |
| ADC (batería) | CH1 | OK con 0 V y 3,3 V; falta probar con el divisor |
| UART con la ESP32-CAM | RS232_TXD / RS232_RXD | OK en loopback; falta la ESP32 real |
| HC-SR04 | TRIG LCD1 (CON2_30), ECHO LCD2 (CON2_28) por divisor 1k/2k | OK |
| 3 × VL53L0X | I2C_SDA / I2C_SCL, XSHUT GPIO4/5/6 (CON2_33/36/35), G → LCD3 / LCD4 / LCDRS (CON2_26/22/24) | OK, máx. 1300 mm, ~20 mediciones/s por polling o por interrupción |
| IMU MPU6050 (GY-521) | I2C_SDA / I2C_SCL (0x68), INT → LCDEN (CON2_23) | OK, 100 muestras/s por polling o por interrupción; giro y acelerómetro calibrados |
| Motores L298N | ENA T_FIL1 (CON1_36), ENB T_FIL3 (CON1_35), IN1–IN4 GPIO0–3 (CON2_29/32/31/34) | OK, 1 motor por lado |

Todos los pines están en `inc/pines.h` (esquemático Rev A, hoja 2). El I2C corre a 400 kHz (`I2C_VELOCIDAD`); si con cables largos aparecen errores, bajarlo a 100000.

## Contenido

```
config.mk                 configuración del proyecto (va en app/)
inc/                      headers de los drivers y de las pruebas
src/                      drivers + main.c (menú) + test_sensores.c + test_actuadores.c + test_imu.c
test_uart_esp32cam/       sketch de eco para la ESP32-CAM (prueba 6)
```

Drivers (se copian tal cual al firmware final, no dependen de los tests ni de FreeRTOS):
`motores`, `encoders`, `hcsr04`, `vl53l0x`, `mpu6050`, `bateria`, `gpio_irq`, `tiempo`.
Pruebas: `main.c`, `consola`, `test_sensores.c`, `test_actuadores.c`, `test_imu.c`.

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

| Tecla | Prueba |
|---|---|
| 1 | Encoders LM393 (`r` pone las cuentas a cero) |
| 2 | HC-SR04 |
| 3 | VL53L0X (después `1` = un sensor, `3` = tres con XSHUT; y `p` = polling, `i` = interrupción) |
| 4 | Motores L298N |
| 5 | Batería por ADC |
| 6 | UART con la ESP32-CAM |
| 7 | IMU MPU6050 (`p` / `i`; `c` recalibra el giro, `a` calibra el acelerómetro, `v` oculta los datos) |

Reglas: desenchufar el USB antes de cablear y conectar **solo** el módulo que se prueba.
Los sensores se alimentan de +3,3 V y +5 V de la placa (+5 V da ~4,5 V por el diodo del USB).
Nada que no sea el divisor puede meter más de 3,3 V en un pin.

## Conexión de cada prueba

**1 · Encoders:** VCC +3,3 V, GND, DO → GPIO7 (izq) / GPIO8 (der). Tapar la ranura: 1 pulso por tapada.

**2 · HC-SR04:** VCC +5 V, GND, Trig → LCD1. Echo → 1 kΩ → punto medio → LCD2, y 2 kΩ del punto medio a GND.
Antes de conectar LCD2, medir el punto medio con 5 V en la entrada del divisor: tiene que dar ≤ 3,3 V.

**3 · VL53L0X:** VIN +3,3 V (nunca 5 V: los pull-up del I2C del módulo van a VIN), GND, SDA → I2C_SDA, SCL → I2C_SCL,
X → GPIO4/5/6 (con uno solo, X sin conectar). Para el modo interrupción: G → LCD3 / LCD4 / LCDRS.
Cada segundo muestra cuántas mediciones leyó por sensor: tiene que dar ~20.

**4 · Motores:** en el L298N, jumpers de ENA/ENB **sacados**, jumper de 5 V puesto, bornera +5V sin conectar.
ENA/ENB van al pin **de afuera** de cada par (el de adentro es 5 V). IN1–IN4 → GPIO0–3.
Batería + → +12V, batería − → GND, y **GND del L298N a GND de la EDU-CIAA**.
Motor izq en OUT1/OUT2, der en OUT3/OUT4 (con 4 motores, 2 en paralelo por lado).
Orden: USB → terminal → `4` → recién ahí la batería. Para apagar, al revés.
Teclas: `w` `s` `a` `d`, `1`/`2` un solo lado, `+`/`-` velocidad, espacio freno, `x` libre. Se paran solos a los 5 s.

**5 · ADC:** cable en CH1, tocar GND (da 0) y +3,3 V (da 1023). En el auto: divisor 100k/33k desde la batería.

**6 · UART:** puente de RS232_TXD a RS232_RXD → vuelven los `PING n`. Con la ESP32-CAM: cargarle
`test_uart_esp32cam.ino` (RX GPIO13, TX GPIO14) → vuelven como `ESP: PING n`.

**7 · IMU:** VCC +3,3 V (o +5 V), GND, SDA → I2C_SDA, SCL → I2C_SCL, AD0 a GND o suelto (0x68), INT → LCDEN
(modo interrupción). XDA y XCL sin conectar. Al entrar calibra el giroscopio: no moverla esos 2 s (si se mueve,
avisa y reintenta). Quieta y plana: Z ≈ +1000 mg, |a| ≈ 1000 mg, giro ≈ 0 °/s. En modo `i`, con `v` (datos ocultos):
100 muestras/s, dt ≈ 9,9 ms, perdidas 0. La calibración del acelerómetro de nuestro GY-521 está cargada en
`CAL_ACC_OFF` / `CAL_ACC_1G` (`test_imu.c`); si se cambia el sensor, repetirla con `a` (6 posiciones).

## APIs para el firmware de alto nivel

| Módulo | Funciones |
|---|---|
| `motores` | `motoresInit()`, `motorVelocidad(MOTOR_IZQ, -100..100)`, `motorFrenar()`, `motorLibre()`, `motoresParar()` |
| `encoders` | `encodersInit()`, `encoderPulsos()`, `encoderPeriodoUs()`, `encoderRpmX10()`, `encoderReset()` |
| `hcsr04` | `hcsr04Init()`, `hcsr04Disparar()` → `hcsr04Estado()` → `hcsr04LeerMm()` |
| `vl53l0x` | `vl53l0xIniciarVarios()`, `vl53l0xContinuo()`, `vl53l0xLeer()` (no bloqueante), `vl53l0xHabilitarIrq()` |
| `mpu6050` | `mpu6050Init()`, `mpu6050CalibrarGiro()`, `mpu6050FijarCalAcc()`, `mpu6050Leer()` (no bloqueante, con `t_ciclos` por muestra), `mpu6050HabilitarIrq()` |
| `bateria` | `bateriaInit()`, `bateriaMilivolts()` |
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
- [ ] UART con la ESP32-CAM real
- [ ] Batería real por ADC con el divisor 100k/33k (uno por pack: CH1 motores, CH2 electrónica; sumar el canal al driver `bateria`)
- [ ] Fusión para el rumbo: Kalman 1D o filtro complementario con el giro Z + los encoders
