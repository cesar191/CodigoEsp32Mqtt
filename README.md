# ESP32 — Control y Monitoreo con MQTT

Firmware para **ESP32** que implementa adquisición de datos (temperatura con DS18B20 y corriente vía ADC) y control de actuadores (PWM, LEDs, ventiladores) a través del protocolo **MQTT**. El proyecto incluye variantes de un solo núcleo, doble núcleo (FreeRTOS), broker embebido y comunicación serial.

---

## Estructura del repositorio

```
CodigoEsp32Mqtt/
├── Broker/
│   ├── ESP32BrokerUnNucleo/        # ESP32 como broker MQTT (un solo núcleo)
│   └── ESP32BrokerDobleNucleo/     # ESP32 como broker MQTT (doble núcleo, FreeRTOS)
├── Client/
│   ├── Esp32ClienteTest/           # ESP32 como cliente MQTT (un solo núcleo)
│   └── Esp32ClientMQTTDobleNucleo/ # ESP32 como cliente MQTT (doble núcleo, FreeRTOS)
├── Serial/
│   ├── ArduinoSerialPlanta/        # Arduino UNO — planta física vía puerto serie
│   ├── SerilaComunicacionControl/  # ESP32 — control por serie (un núcleo)
│   └── SerialComunicacionControlDobleNucleo/ # ESP32 — control por serie (doble núcleo)
├── pruebaMQTT/                     # Prueba con PubSubClient + SD + WiFiMulti
└── libraries/                      # Librerías incluidas en el repositorio
```

---

## Mapa de pines (ESP32)

Todos los sketches del ESP32 usan el mismo mapa de pines:

| Función              | Pin GPIO |
|----------------------|----------|
| Sensor temp 1 (DS18B20) | 14    |
| Sensor temp 2 (DS18B20) | 27    |
| ADC corriente Q1     | 35       |
| ADC corriente Q2     | 34       |
| LED 1                | 26       |
| LED 2                | 25       |
| Ventilador 1 (PWM)   | 33       |
| Ventilador 2 (PWM)   | 32       |
| PWM transistor Q1    | 13       |
| PWM transistor Q2    | 12       |
| Pantalla OLED (I2C)  | SDA/SCL estándar (I2C, dirección `0x3C`) |

> [!NOTE]
> Los sketches del Arduino UNO (`ArduinoSerialPlanta`) usan pines analógicos A1–A4 para temperatura/corriente y pines digitales 5–11 para PWM/LEDs/ventiladores.

---

## Configuración inicial — archivo `data.h`

> [!IMPORTANT]
> El archivo `data.h` **no se sube al repositorio** (está en `.gitignore`). Debes crearlo manualmente en la carpeta de cada sketch antes de compilar.

Copia la siguiente plantilla y completa con tus datos:

```cpp
#ifndef DATA_H
#define DATA_H

// Credenciales Wi-Fi
constexpr char SSID[] = "TU_RED_WIFI";
constexpr char PASS[] = "TU_CONTRASEÑA";

// Broker MQTT
constexpr char BROKER_MQTT[]     = "192.168.X.X";  // IP de tu broker
constexpr uint16_t PUERTO_MQTT   = 1883;
constexpr char USER_MQTT[]       = "";              // Dejar vacío si no hay auth
constexpr char PASS_MQTT[]       = "";
constexpr char ESP_CLIENT_NAME[] = "esp32client";

// Tópicos de publicación (ESP32 → broker)
constexpr char TOPIC_PUB_TEMP1[] = "test/sensor/temperatura1";
constexpr char TOPIC_PUB_TEMP2[] = "test/sensor/temperatura2";
constexpr char TOPIC_PUB_IQ1[]   = "test/sensor/corrienteQ1";
constexpr char TOPIC_PUB_IQ2[]   = "test/sensor/corrienteQ2";
constexpr char TOPIC_PUB_TIME[]  = "test/sensor/tiempo";

// Tópicos de suscripción (broker → ESP32)
constexpr char TOPIC_SUB_PWM1[]  = "test/datos/pwm1";
constexpr char TOPIC_SUB_PWM2[]  = "test/datos/pwm2";
constexpr char TOPIC_SUB_LED1[]  = "test/datos/led1";
constexpr char TOPIC_SUB_LED2[]  = "test/datos/led2";
constexpr char TOPIC_SUB_VENT1[] = "test/datos/ventilador1";
constexpr char TOPIC_SUB_VENT2[] = "test/datos/ventilador2";

#endif
```

---

## Descripción de cada sketch

### `Broker/ESP32BrokerUnNucleo` — Broker en un solo núcleo

El ESP32 aloja su propio broker MQTT usando **PicoMQTT**. Todo corre en el `loop()` principal: lectura de sensores, pantalla OLED, publicación y atención de clientes.

- **Librería MQTT:** `PicoMQTT::Server`
- **Cuándo usarlo:** Pruebas rápidas o cuando solo hay un cliente conectado.

> [!WARNING]
> Al correr en un solo núcleo, el `loop()` hace todo: sensores, OLED y broker. Si los sensores DS18B20 tardan demasiado en convertir, el broker puede dejar de responder brevemente a los clientes.

---

### `Broker/ESP32BrokerDobleNucleo` — Broker en doble núcleo (FreeRTOS) ✅ Recomendado

Versión optimizada con tres tareas independientes usando `xTaskCreatePinnedToCore`:

| Tarea         | Núcleo | Prioridad | Función                                     |
|---------------|--------|-----------|---------------------------------------------|
| `BrokerTask`  | 0      | 2 (alta)  | Mantiene el broker y publica cada 1 segundo |
| `SensorTask`  | 1      | 2 (alta)  | Lee temperatura (cada 800 ms) y ADC (cada 50 ms) |
| `DisplayTask` | 1      | 1 (baja)  | Actualiza la pantalla OLED cada 300 ms      |

Los datos se comparten entre tareas a través de un `SemaphoreHandle_t` (mutex).

El broker también incluye **autenticación** de usuario y contraseña mediante una clase `SecureBroker` que hereda de `PicoMQTT::Server`.

> [!IMPORTANT]
> **No reinicies el ESP32 mientras la interfaz de control (HMI) esté conectada.** Al resetear, el broker desaparece de la red y los clientes reciben excepciones de desconexión abrupta hasta que el ESP32 vuelve a arrancar.

---

### `Client/Esp32ClienteTest` — Cliente MQTT en un solo núcleo

El ESP32 se conecta a un broker **externo** (ej. Mosquitto en PC o Raspberry Pi). Todo corre en `loop()`.

- **Librería MQTT:** `MQTT` (`MQTTClient`)
- **Cuándo usarlo:** Cuando el broker corre en otra máquina de la red.

---

### `Client/Esp32ClientMQTTDobleNucleo` — Cliente MQTT en doble núcleo (FreeRTOS) ✅ Recomendado

Versión optimizada del cliente con tres tareas:

| Tarea         | Núcleo | Prioridad | Función                                          |
|---------------|--------|-----------|--------------------------------------------------|
| `SensorTask`  | 1      | 2 (alta)  | Lee temperatura y corriente                      |
| `MQTTTask`    | 0      | 1         | Mantiene WiFi + MQTT, publica cada 500 ms        |
| `DisplayTask` | 1      | 1 (baja)  | Actualiza OLED cada 250 ms                       |

Incluye **reconexión automática** a WiFi y MQTT con reintentos y timeout, y muestra en la OLED si hay desconexión (`WiFi: Desconectado` o `MQTT: ---`).

---

### `Serial/ArduinoSerialPlanta` — Arduino UNO como planta física

Sketch para un **Arduino UNO** (no ESP32). Simula la planta física: lee sensores LM35 (temperatura analógica) y envía telemetría por puerto serie.

**Protocolo de telemetría (Arduino → PC):**
```
#Temp1<valor>\n
#Temp2<valor>\n
#Corri1<valor>\n
#Corri2<valor>\n
#Tiempo<segundos>\n
```

**Protocolo de comandos (PC → Arduino):**
```
$LED1on / $LED1off
$LED2on / $LED2off
$VENT1on / $VENT1off
$VENT2on / $VENT2off
$PWM150        → PWM Q1 al 50%
$PWM275        → PWM Q2 al 75%
```

---

### `Serial/SerilaComunicacionControl` — ESP32 control por serie (un núcleo)

Versión para ESP32 del control por puerto serie. Mismo protocolo que el Arduino UNO, pero con sensores DS18B20 (digital) y PWM via `ledcWrite`. Corre en un `loop()` simple.

---

### `Serial/SerialComunicacionControlDobleNucleo` — ESP32 control por serie (doble núcleo)

Versión FreeRTOS del control serial con tres tareas:

| Tarea          | Núcleo | Prioridad | Función                                             |
|----------------|--------|-----------|-----------------------------------------------------|
| `SerialTask`   | 0      | 2 (alta)  | Recibe comandos y envía telemetría cada 1 segundo   |
| `SensorTask`   | 1      | 2 (alta)  | Lee temperatura (800 ms) y ADC (50 ms)              |
| `DisplayTask`  | 1      | 1 (baja)  | Actualiza OLED cada 300 ms                          |

---

### `pruebaMQTT` — Prueba con PubSubClient + SD + WiFiMulti

Sketch de prueba más completo que incluye:
- **`WiFiMulti`**: gestiona múltiples redes WiFi (cambia automáticamente a la disponible).
- **Lector de SD**: lee credenciales WiFi/MQTT desde archivos `.txt` en una tarjeta SD (pines SPI, CS en GPIO 15).
- **Conexión TLS opcional**: si el puerto no es 1883, carga certificados (`RootCA.pem`, `Cert.crt`, `Private.key`) desde la SD.
- **Librería MQTT:** `PubSubClient`

**Archivos esperados en la SD:**
```
/ssid.txt
/password.txt
/broker.txt
/port.txt
/mqtt_user.txt
/mqtt_pass.txt
/RootCA.pem      (solo si TLS)
/Cert.crt        (solo si TLS)
/Private.key     (solo si TLS)
```

---

## Librerías requeridas

Instálalas desde el Administrador de Librerías de Arduino IDE o copia las carpetas de `libraries/` en tu directorio de librerías local.

| Librería             | Uso                                           |
|----------------------|-----------------------------------------------|
| `PicoMQTT`           | Broker MQTT embebido en el ESP32              |
| `MQTT`               | Cliente MQTT liviano (`MQTTClient`)           |
| `PubSubClient`       | Cliente MQTT alternativo (`pruebaMQTT`)       |
| `DallasTemperature`  | Lectura de sensores DS18B20                   |
| `OneWire`            | Bus 1-Wire para DS18B20                       |
| `Adafruit_SSD1306`   | Control de pantalla OLED 128×64               |
| `Adafruit_GFX`       | Primitivas gráficas para la OLED              |

---

## Filtro IIR (suavizado de señales)

Todos los sketches aplican un filtro exponencial de primer orden a temperatura y corriente para reducir ruido:

$$y[n] = \alpha \cdot x[n] + (1-\alpha) \cdot y[n-1]$$

| Señal      | Alpha (`α`) | Efecto                        |
|------------|-------------|-------------------------------|
| Temperatura | `0.8`      | Respuesta rápida, poco filtro |
| Corriente   | `0.1`      | Respuesta lenta, mucho filtro |

Un α cercano a 1 sigue más la señal real; un α cercano a 0 suaviza más pero reacciona más lento.

---

## Protocolo MQTT — Referencia rápida

### Publicación (ESP32 → broker, cada 500–1000 ms)

| Tópico                        | Tipo    | Descripción            |
|-------------------------------|---------|------------------------|
| `test/sensor/temperatura1`    | `float` | Temperatura sensor 1 (°C) |
| `test/sensor/temperatura2`    | `float` | Temperatura sensor 2 (°C) |
| `test/sensor/corrienteQ1`     | `float` | Corriente Q1 (V ADC filtrado) |
| `test/sensor/corrienteQ2`     | `float` | Corriente Q2 (V ADC filtrado) |
| `test/sensor/tiempo`          | `uint32`| Segundos desde arranque |

### Suscripción (broker → ESP32)

| Tópico                  | Valores aceptados          | Acción                             |
|-------------------------|----------------------------|------------------------------------|
| `test/datos/pwm1`       | `0` – `100`                | PWM del transistor Q1 en %         |
| `test/datos/pwm2`       | `0` – `100`                | PWM del transistor Q2 en %         |
| `test/datos/led1`       | `"on"` / `"off"`           | Enciende o apaga LED 1             |
| `test/datos/led2`       | `"on"` / `"off"`           | Enciende o apaga LED 2             |
| `test/datos/ventilador1`| `"on"` / `"off"` / `0–100` | Ventilador 1: on/off o duty cycle  |
| `test/datos/ventilador2`| `"on"` / `"off"` / `0–100` | Ventilador 2: on/off o duty cycle  |

> [!TIP]
> Para probar el sistema sin la interfaz gráfica, puedes usar **MQTT Explorer** o el cliente de consola `mosquitto_pub`:
> ```bash
> mosquitto_pub -h 192.168.X.X -t "test/datos/pwm1" -m "50"
> mosquitto_pub -h 192.168.X.X -t "test/datos/led1" -m "on"
> ```
