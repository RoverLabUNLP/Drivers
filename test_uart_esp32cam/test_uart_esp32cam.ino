/*
 * Auto remoto - Taller de Proyecto I (UNLP)
 * Eco por UART para probar el enlace EDU-CIAA <-> ESP32-CAM (AI-Thinker)
 *
 * Conexion (esquematico hoja 2):
 *   ESP32 GPIO13 (RX) <- RS232_TXD de la EDU-CIAA
 *   ESP32 GPIO14 (TX) -> RS232_RXD de la EDU-CIAA
 *   GND comun. ESP32-CAM alimentada a 5 V por su lado (no desde la EDU-CIAA).
 *
 * Todo lo que llega por GPIO13 se devuelve con el prefijo "ESP: " y
 * tambien se muestra en el monitor serie (USB-serie del programador, 115200).
 * En la EDU-CIAA, menu opcion 6: tienen que verse lineas "<- ESP: PING n".
 */

#define PIN_RX  13
#define PIN_TX  14

HardwareSerial ciaa(2);
String linea;

void setup() {
  Serial.begin(115200);
  ciaa.begin(115200, SERIAL_8N1, PIN_RX, PIN_TX);
  Serial.println("\n=== ESP32-CAM eco UART (RX 13 / TX 14) ===");
}

void loop() {
  while (ciaa.available()) {
    char c = ciaa.read();
    if (c == '\n') {
      linea.trim();
      Serial.println("recibido: " + linea);
      ciaa.print("ESP: " + linea + "\r\n");
      linea = "";
    } else if (linea.length() < 120) {
      linea += c;
    }
  }
}
