# Sistema de Monitoreo Térmico y Control con ESP32 y MQTT

Este repositorio contiene el firmware desarrollado para el módulo **ESP32**. El sistema implementa comunicación en tiempo real mediante el protocolo MQTT para la adquisición de datos de temperatura (sensores DS18B20) y el control de actuadores (PWM para transistores de potencia)

## 📂 Estructura del Repositorio

El código está dividido en dos vertientes principales según su estado de desarrollo y estabilidad:

### 1. 🚀 Versión Estable e Implementada
Ubicada en las carpetas `prueba_mqtt_ds18b20` y `prueba_mqtt_ds18b20_sinOled`.
* **Estado:** Completamente testeada, validada y funcional.
* **Características:** Realiza la lectura periódica de variables físicas y atiende las suscripciones/publicaciones MQTT de forma secuencial en un ciclo estable.

### 2. 🔬 Versión Experimental (`ESP32BrokerTesisUnNucleo`)
Ubicada en la carpeta `ESP32BrokerTesisUnNucleo`.
* **Estado:** En fase de experimentación y optimización arquitectónica.
* **Objetivo técnico:** Migración obligatoria hacia una arquitectura de **Dos Núcleos (Dual Core)** utilizando las tareas en paralelo de FreeRTOS (`xTaskCreatePinnedToCore`), obliga usar la oled para conocer la dirección ip del ESP32.
* **Razón del cambio:** El procesamiento simultáneo de la lógica de control, las lecturas de hardware y la gestión de la red saturan el ciclo principal del microcontrolador cuando corre en un solo núcleo. Se exige pasar a dos núcleos para independizar y estabilizar por completo el envío masivo de datos sin interferir con las tareas de control de potencia.

---

## ⚠️ Alertas Críticas de Estabilidad e Interfaz

Al operar el dispositivo o trabajar con la versión experimental, se deben tener bajo estricta consideración las siguientes advertencias de red:

* **Pérdida Crítica del Broker en Reseteos:** Dado que la ESP32 está configurada para alojar de manera local el Broker MQTT principal de la red (`PicoMQTT`), **bajo ninguna circunstancia se debe reiniciar o resetear físicamente la placa mientras la interfaz gráfica de C# esté operando**.
* **Impacto en el Software (HMI):** Si la ESP32 se resetea, el broker deja de existir instantáneamente. Esto provoca excepciones de desconexión abrupta, desbordamiento de búferes de red y fallos por bloqueo (*timeouts*) en la interfaz de C# mientras se espera a que la placa se reinicie, vuelva a conectarse al Wi-Fi y cree el broker nuevamente.

---

## 📚 Librerías Utilizadas (Incluidas en el repositorio)
* `PicoMQTT`: Motor del Broker embebido en la placa.
* `MQTT`:Esp32 como cliente en la conexión del broker MQTT
* `DallasTemperature` & `OneWire`: Adquisición digital de los sensores de temperatura DS18B20.
* `Adafruit_SSD1306` & `Adafruit_GFX`: Control de la interfaz visual del display OLED.
