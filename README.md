# Sistema de Monitoreo Térmico y Control con ESP32 y MQTT

Este repositorio contiene el firmware desarrollado para el módulo **ESP32**. El sistema implementa comunicación en tiempo real mediante el protocolo MQTT para la adquisición de datos de temperatura (sensores DS18B20) y el control de actuadores (PWM para transistores de potencia).

## 📂 Estructura del Repositorio

Para un mejor manejo de los códigos y casos de uso, el proyecto se ha organizado en dos carpetas principales:

### 1. 🚀 `Clientes/` (Versión Estable)
Contiene el código donde el ESP32 actúa **únicamente como cliente MQTT**, conectándose a un broker externo.
* `ESP32ClienteTesisOptimo/`: Versión completa con pantalla OLED.
* `ESP32ClienteTesisOptimoSinOLED/`: Versión ligera sin pantalla.

**Características de los Clientes:**
* Completamente testeada, validada y funcional.
* Realiza la lectura periódica de variables físicas y atiende MQTT de forma estable.
* **Manejo de PWM:** El ESP32 puede trabajar 4 canales de PWM a diferentes frecuencias.

### 2. 🔬 `Brokers/` (Versión Experimental Dual)
Contiene el código donde el ESP32 aloja su propio **Broker MQTT local** (`PicoMQTT`) y actúa como servidor.
* `ESP32BrokerTesisUnNucleo/`: Versión base (un solo núcleo) optimizada temporalmente.
* `ESP32BrokerTesisobbleNucleo/`: Versión migrada a **Dos Núcleos (Dual Core)** con FreeRTOS.
* `esp32BrokerUserPass/`: Pruebas de autenticación.

**Características de los Brokers Locales:**
* **Objetivo:** Uso de `xTaskCreatePinnedToCore` para evitar cuellos de botella al procesar WiFi, lecturas físicas y el broker en un solo núcleo.
* Requiere usar la OLED para conocer la dirección IP del servidor ESP32.

---

## 🔒 Manejo Seguro de Datos
Para garantizar la seguridad de las credenciales de red (WiFi) y parámetros de conexión:
* Se ha implementado un archivo `.gitignore` en la raíz del proyecto.
* **Importante:** Todos los archivos llamados `data.h`, `secrets.h` o `credentials.h` serán **ignorados** por Git. 
* **Acción requerida del usuario:** Si clonas este repositorio, asegúrate de no subir tus contraseñas. Debes crear localmente tu propio archivo `data.h` con las credenciales necesarias.

### Configuración del archivo `data.h`
Para que la programación del ESP32 compile y funcione correctamente, tu archivo local `data.h` debe declarar las redes Wi-Fi y los siguientes tópicos MQTT (puedes adaptarlos según la interfaz de C#):

```cpp
// Redes Wi-Fi (Configura tus propias credenciales)
const char* ssid = "SSID-WIFI";
const char* pass = "PASSWORD-WIFI";
const char* broker = "BROKER-MQTT";


// Tópicos MQTT para ENVIAR datos (Publicación)
const char* data_temp1 = "test/sensor/temperatura1";
const char* data_temp2 = "test/sensor/temperatura2";
const char* data_IQ1   = "test/sensor/corrienteQ1";
const char* data_IQ2   = "test/sensor/corrienteQ2";
const char* data_time  = "test/sensor/tiempo";

// Tópicos MQTT para RECIBIR información (Suscripción)
const char* msg_pwm1        = "test/datos/pwm1";
const char* msg_pwm2        = "test/datos/pwm2";
const char* msg_led1        = "test/datos/led1";
const char* msg_led2        = "test/datos/led2";
const char* msg_ventilador1 = "test/datos/ventilador1";
const char* msg_ventilador2 = "test/datos/ventilador2";
```


---

## ⚠️ Alertas Críticas de Estabilidad e Interfaz

Al operar el dispositivo o trabajar con la versión experimental, se deben tener bajo estricta consideración las siguientes advertencias de red:

* **Pérdida Crítica del Broker en Reseteos:** Dado que la ESP32 está configurada para alojar de manera local el Broker MQTT principal de la red (`PicoMQTT`), **bajo ninguna circunstancia se debe reiniciar o resetear físicamente la placa mientras la interfaz gráfica de C# esté operando**.
* **Impacto en el Software (HMI):** Si la ESP32 se resetea, el broker deja de existir instantáneamente. Esto provoca excepciones de desconexión abrupta, desbordamiento de búferes de red y fallos por bloqueo (*timeouts*) en la interfaz de C# mientras se espera a que la placa se reinicie, vuelva a conectarse al Wi-Fi y cree el broker nuevamente.

---

## 📚 Librerías Utilizadas (Incluidas en el repositorio)
* `PicoMQTT`: Motor del Broker embebido en la placa.
* `MQTT`: ESP32 como cliente en la conexión del broker MQTT.
* `DallasTemperature` & `OneWire`: Adquisición digital de los sensores de temperatura DS18B20.
* `Adafruit_SSD1306` & `Adafruit_GFX`: Control de la interfaz visual del display OLED.

