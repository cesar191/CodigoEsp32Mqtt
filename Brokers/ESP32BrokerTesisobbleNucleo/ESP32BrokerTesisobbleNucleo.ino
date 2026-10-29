// ─────────────────────────────────────────────────────────
//  ESP32 Broker MQTT + Control de Hardware (Doble Núcleo)
//  PicoMQTT::Server corre directamente en el ESP32
// ─────────────────────────────────────────────────────────
#include "data.h"
#include <PicoMQTT.h>
#include <WiFi.h>
#include <WiFiClient.h>

#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ─────────────────────────────────────────────────────────
//  BROKER MQTT SEGURO (con autenticación)
// ─────────────────────────────────────────────────────────
class SecureBroker : public PicoMQTT::Server {
protected:
  PicoMQTT::ConnectReturnCode auth(const char *client_id,
                                    const char *username,
                                    const char *password) override {
    // Rechazar client_id muy cortos
    if (String(client_id).length() < 3) {
      return PicoMQTT::CRC_IDENTIFIER_REJECTED;
    }
    // Requerir usuario y contraseña
    if (!username || !password) {
      return PicoMQTT::CRC_NOT_AUTHORIZED;
    }
    // Verificar credenciales
    const bool ok =
        (String(username) == "usco" && String(password) == "12345678") ||
        (String(username) == "cesar" && String(password) == "1234567890");
    return ok ? PicoMQTT::CRC_ACCEPTED
              : PicoMQTT::CRC_BAD_USERNAME_OR_PASSWORD;
  }
};

SecureBroker mqtt;

// ─────────────────────────────────────────────────────────
//  CONFIGURACIÓN DE PINES
// ─────────────────────────────────────────────────────────
constexpr uint8_t PIN_TEMP_1 = 14;
constexpr uint8_t PIN_TEMP_2 = 27;
constexpr uint8_t PIN_ADC_CORRIENTE1 = 35;
constexpr uint8_t PIN_ADC_CORRIENTE2 = 34;
constexpr uint8_t PIN_LED1 = 26;
constexpr uint8_t PIN_LED2 = 25;
constexpr uint8_t PIN_VENTILADOR1 = 33;
constexpr uint8_t PIN_VENTILADOR2 = 32;
constexpr uint8_t PIN_PWM_Q1 = 13;
constexpr uint8_t PIN_PWM_Q2 = 12;

// ─────────────────────────────────────────────────────────
//  PARÁMETROS DE CONFIGURACIÓN
// ─────────────────────────────────────────────────────────
constexpr uint32_t FREC_PWM_Q1 = 500;
constexpr uint32_t FREC_PWM_Q2 = 10000;
constexpr uint32_t FREC_PWM_VENT1 = 5;
constexpr uint32_t FREC_PWM_VENT2 = 5;
constexpr uint8_t RESOLUCION_PWM = 8;
constexpr uint8_t RESOLUCION_TEMP = 9;
constexpr float ALPHA_TEMP = 0.8f;
constexpr float ALPHA_CORRIENTE = 0.1f;

// Intervalos de tiempo (ms)
constexpr uint32_t INTERVALO_PUBLICACION = 1000;
constexpr uint32_t INTERVALO_OLED = 300;
constexpr uint32_t INTERVALO_SENSORES_TEMP = 800;
constexpr uint32_t INTERVALO_SENSORES_ADC = 50;
constexpr uint32_t TIMEOUT_WIFI = 15000;

// ─────────────────────────────────────────────────────────
//  INSTANCIAS DE OBJETOS
// ─────────────────────────────────────────────────────────
OneWire wireTemp1(PIN_TEMP_1);
OneWire wireTemp2(PIN_TEMP_2);
DallasTemperature sensorTemp1(&wireTemp1);
DallasTemperature sensorTemp2(&wireTemp2);
Adafruit_SSD1306 display(128, 64, &Wire);

// ─────────────────────────────────────────────────────────
//  ESTRUCTURA DE DATOS COMPARTIDOS (protegida por mutex)
// ─────────────────────────────────────────────────────────
struct DatosSensores {
  float temp1;
  float temp2;
  float corrienteQ1;
  float corrienteQ2;
  uint8_t dutyCycleQ1;
  uint8_t dutyCycleQ2;
  uint8_t dutyCycleVent1;
  uint8_t dutyCycleVent2;
  String estadoLed1;
  String estadoLed2;
  String estadoVentilador1;
  String estadoVentilador2;
  uint32_t segundos;
};

DatosSensores datos = {0.0f, 0.0f, 0.0f, 0.0f, 0, 0, 0, 0,
                       "off", "off", "off", "off", 0};

// Variables locales del filtro (solo usadas por SensorTask)
float temp1Anterior = 0.0f;
float temp2Anterior = 0.0f;
float corriente1Anterior = 0.0f;
float corriente2Anterior = 0.0f;

// ─────────────────────────────────────────────────────────
//  SINCRONIZACIÓN Y HANDLES DE TAREAS
// ─────────────────────────────────────────────────────────
SemaphoreHandle_t xMutexData = NULL;
TaskHandle_t BrokerTaskHandle = NULL;
TaskHandle_t SensorTaskHandle = NULL;
TaskHandle_t DisplayTaskHandle = NULL;

// ─────────────────────────────────────────────────────────
//  CALLBACK MQTT — Mensajes entrantes desde clientes
// ─────────────────────────────────────────────────────────
void procesarMensajeMQTT(const char *topicRaw, const char *payloadRaw) {
  String topic(topicRaw);
  String payload(payloadRaw);

  if (xSemaphoreTake(xMutexData, pdMS_TO_TICKS(50)) == pdTRUE) {
    if (topic == TOPIC_SUB_PWM1) {
      datos.dutyCycleQ1 = payload.toInt();
      ledcWrite(PIN_PWM_Q1, datos.dutyCycleQ1 * 2.55f);
    } else if (topic == TOPIC_SUB_PWM2) {
      datos.dutyCycleQ2 = payload.toInt();
      ledcWrite(PIN_PWM_Q2, datos.dutyCycleQ2 * 2.55f);
    } else if (topic == TOPIC_SUB_LED1) {
      datos.estadoLed1 = payload;
      digitalWrite(PIN_LED1, (payload == "on") ? HIGH : LOW);
    } else if (topic == TOPIC_SUB_LED2) {
      datos.estadoLed2 = payload;
      digitalWrite(PIN_LED2, (payload == "on") ? HIGH : LOW);
    } else if (topic == TOPIC_SUB_VENT1) {
      datos.estadoVentilador1 = payload;
      if (payload == "on") {
        ledcWrite(PIN_VENTILADOR1, 255);
      } else if (payload == "off") {
        ledcWrite(PIN_VENTILADOR1, 0);
      } else {
        datos.dutyCycleVent1 = payload.toInt();
        ledcWrite(PIN_VENTILADOR1, datos.dutyCycleVent1 * 2.55f);
      }
    } else if (topic == TOPIC_SUB_VENT2) {
      datos.estadoVentilador2 = payload;
      if (payload == "on") {
        ledcWrite(PIN_VENTILADOR2, 255);
      } else if (payload == "off") {
        ledcWrite(PIN_VENTILADOR2, 0);
      } else {
        datos.dutyCycleVent2 = payload.toInt();
        ledcWrite(PIN_VENTILADOR2, datos.dutyCycleVent2 * 2.55f);
      }
    }
    xSemaphoreGive(xMutexData);
  }
}

// ─────────────────────────────────────────────────────────
//  CONEXIÓN WiFi — Con timeout, no bloquea infinitamente
// ─────────────────────────────────────────────────────────
bool conectarWiFi() {
  if (WiFi.status() == WL_CONNECTED) return true;

  WiFi.disconnect(true);
  vTaskDelay(pdMS_TO_TICKS(100));
  WiFi.begin(SSID, PASS);

  uint32_t inicio = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - inicio > TIMEOUT_WIFI) {
      return false;
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
  return true;
}

// ─────────────────────────────────────────────────────────
//  TAREA 1: Broker MQTT (Núcleo 0 — PRIORIDAD MÁXIMA)
//
//  CRÍTICO: Esta es la tarea más importante.
//  El ESP32 ES el broker. Si mqtt.loop() no se ejecuta
//  frecuentemente, TODOS los clientes se desconectan.
//  NO poner nada bloqueante aquí: ni OLED, ni sensores.
// ─────────────────────────────────────────────────────────
void BrokerTask(void *parameter) {
  uint32_t ultimoEnvioPub = 0;

  for (;;) {
    // ── Loop del broker — DEBE correr lo más rápido posible ──
    // Atiende conexiones de clientes, enruta mensajes, mantiene keepalive
    mqtt.loop();

    // ── Publicación periódica de datos de sensores ──
    if (millis() - ultimoEnvioPub >= INTERVALO_PUBLICACION) {
      ultimoEnvioPub = millis();

      // Timeout corto: si no logra tomar el mutex, salta esta publicación
      // Es preferible perder 1 publicación que bloquear el broker
      if (xSemaphoreTake(xMutexData, pdMS_TO_TICKS(20)) == pdTRUE) {
        mqtt.publish(TOPIC_PUB_TEMP1, String(datos.temp1));
        mqtt.publish(TOPIC_PUB_TEMP2, String(datos.temp2));
        mqtt.publish(TOPIC_PUB_IQ1, String(datos.corrienteQ1));
        mqtt.publish(TOPIC_PUB_IQ2, String(datos.corrienteQ2));
        mqtt.publish(TOPIC_PUB_TIME, String(datos.segundos));
        xSemaphoreGive(xMutexData);
      }
    }

    // Delay mínimo — el broker necesita ser responsivo
    // 10ms = ~100 iteraciones/segundo del broker
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ─────────────────────────────────────────────────────────
//  TAREA 2: Lectura de sensores (Núcleo 1)
//  Lee temperaturas y corrientes sin interferir con el broker
// ─────────────────────────────────────────────────────────
void SensorTask(void *parameter) {
  uint32_t ultimoMuestreoTemp = 0;
  uint32_t ultimoMuestreoADC = 0;
  bool conversionSolicitada = false;

  // Solicitar primera conversión de temperatura
  sensorTemp1.requestTemperatures();
  sensorTemp2.requestTemperatures();
  conversionSolicitada = true;

  for (;;) {
    // ── Lectura de corrientes (ADC) — más frecuente ──
    if (millis() - ultimoMuestreoADC >= INTERVALO_SENSORES_ADC) {
      ultimoMuestreoADC = millis();

      const int adc1 = analogRead(PIN_ADC_CORRIENTE1);
      const int adc2 = analogRead(PIN_ADC_CORRIENTE2);

      if (xSemaphoreTake(xMutexData, pdMS_TO_TICKS(20)) == pdTRUE) {
        datos.segundos = millis() / 1000;

        datos.corrienteQ1 = (((adc1 * 3.3f) / 4096.0f) * ALPHA_CORRIENTE) +
                            ((1.0f - ALPHA_CORRIENTE) * corriente1Anterior);
        corriente1Anterior = datos.corrienteQ1;

        datos.corrienteQ2 = (((adc2 * 3.3f) / 4096.0f) * ALPHA_CORRIENTE) +
                            ((1.0f - ALPHA_CORRIENTE) * corriente2Anterior);
        corriente2Anterior = datos.corrienteQ2;

        xSemaphoreGive(xMutexData);
      }
    }

    // ── Lectura de temperaturas — cada 800ms ──
    if (millis() - ultimoMuestreoTemp >= INTERVALO_SENSORES_TEMP) {
      ultimoMuestreoTemp = millis();

      if (conversionSolicitada) {
        float t1_raw = sensorTemp1.getTempCByIndex(0);
        float t2_raw = sensorTemp2.getTempCByIndex(0);

        if (xSemaphoreTake(xMutexData, pdMS_TO_TICKS(20)) == pdTRUE) {
          if (t1_raw != DEVICE_DISCONNECTED_C) {
            datos.temp1 = t1_raw * ALPHA_TEMP + (1.0f - ALPHA_TEMP) * temp1Anterior;
            temp1Anterior = datos.temp1;
          }
          if (t2_raw != DEVICE_DISCONNECTED_C) {
            datos.temp2 = t2_raw * ALPHA_TEMP + (1.0f - ALPHA_TEMP) * temp2Anterior;
            temp2Anterior = datos.temp2;
          }
          xSemaphoreGive(xMutexData);
        }
      }

      // Solicitar nueva conversión para el próximo ciclo
      sensorTemp1.requestTemperatures();
      sensorTemp2.requestTemperatures();
      conversionSolicitada = true;
    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// ─────────────────────────────────────────────────────────
//  TAREA 3: Pantalla OLED (Núcleo 1)
//  Separada para que I2C no bloquee sensores ni broker
// ─────────────────────────────────────────────────────────
void DisplayTask(void *parameter) {
  for (;;) {
    if (xSemaphoreTake(xMutexData, pdMS_TO_TICKS(30)) == pdTRUE) {
      // Copiar datos localmente para soltar el mutex rápido
      float t1 = datos.temp1;
      float t2 = datos.temp2;
      float i1 = datos.corrienteQ1;
      float i2 = datos.corrienteQ2;
      uint8_t dq1 = datos.dutyCycleQ1;
      uint8_t dq2 = datos.dutyCycleQ2;
      String el1 = datos.estadoLed1;
      String el2 = datos.estadoLed2;
      String ev1 = datos.estadoVentilador1;
      String ev2 = datos.estadoVentilador2;
      uint32_t seg = datos.segundos;
      xSemaphoreGive(xMutexData);

      // Dibujar fuera del mutex (I2C es lento, ~80-120ms)
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);

      // Fila 0: IP Local
      display.setCursor(0, 0);
      display.print("IP: ");
      display.print(WiFi.localIP());

      // Fila 1: Temperaturas
      display.setCursor(0, 8);
      display.print("T1: ");
      display.print(t1, 1);
      display.print("C");
      display.setCursor(64, 8);
      display.print("T2: ");
      display.print(t2, 1);
      display.print("C");

      // Fila 2: Ciclos PWM
      display.setCursor(0, 16);
      display.print("Q1: ");
      display.print(dq1);
      display.print("%");
      display.setCursor(64, 16);
      display.print("Q2: ");
      display.print(dq2);
      display.print("%");

      // Fila 3: Estado LEDs
      display.setCursor(0, 24);
      display.print("L1: ");
      display.print(el1);
      display.setCursor(64, 24);
      display.print("L2: ");
      display.print(el2);

      // Fila 4: Estado Ventiladores
      display.setCursor(0, 32);
      display.print("V1: ");
      display.print(ev1);
      display.setCursor(64, 32);
      display.print("V2: ");
      display.print(ev2);

      // Fila 5: Corrientes
      display.setCursor(0, 40);
      display.print("I1: ");
      display.print(i1, 2);
      display.print("A");
      display.setCursor(64, 40);
      display.print("I2: ");
      display.print(i2, 2);
      display.print("A");

      // Fila 6: Tiempo
      display.setCursor(0, 48);
      display.print("Tiempo: ");
      display.print(seg);
      display.print(" Seg");

      // Fila 7: Estado del broker
      display.setCursor(0, 56);
      display.print("Broker: OK");

      display.display();
    }

    vTaskDelay(pdMS_TO_TICKS(INTERVALO_OLED));
  }
}

// ─────────────────────────────────────────────────────────
//  SETUP
// ─────────────────────────────────────────────────────────
void setup() {
  // Mutex — crear ANTES de cualquier tarea
  xMutexData = xSemaphoreCreateMutex();

  // Pines digitales
  pinMode(PIN_LED1, OUTPUT);
  pinMode(PIN_LED2, OUTPUT);

  // Sensores de temperatura
  sensorTemp1.begin();
  sensorTemp1.setResolution(RESOLUCION_TEMP);
  sensorTemp2.begin();
  sensorTemp2.setResolution(RESOLUCION_TEMP);
  analogReadResolution(12);

  // PWM
  ledcAttach(PIN_PWM_Q1, FREC_PWM_Q1, RESOLUCION_PWM);
  ledcAttach(PIN_PWM_Q2, FREC_PWM_Q2, RESOLUCION_PWM);
  ledcAttach(PIN_VENTILADOR1, FREC_PWM_VENT1, RESOLUCION_PWM);
  ledcAttach(PIN_VENTILADOR2, FREC_PWM_VENT2, RESOLUCION_PWM);

  // Pantalla OLED
  if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.display();
  }

  // ── Configuración WiFi robusta ──
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);          // Desactivar power-save (evita desconexiones)
  WiFi.setAutoReconnect(true);   // Reconexión automática a nivel de stack
  WiFi.persistent(false);        // No guardar en flash cada boot

  // Conectar WiFi (bloqueante solo en setup, con timeout)
  conectarWiFi();

  // ── Iniciar Broker MQTT ──
  // Suscribirse a tópicos de control
  mqtt.subscribe("test/datos/#", [](const char *topic, const char *payload) {
    procesarMensajeMQTT(topic, payload);
  });
  mqtt.begin();

  // ── Crear tareas ──

  // Núcleo 0: Broker MQTT (PRIORIDAD 2 — la más alta)
  // El broker es la tarea más crítica: si no corre, los clientes se desconectan
  xTaskCreatePinnedToCore(BrokerTask, "BrokerTask",
                          8192,  // Stack 8 KB (broker necesita más para manejar clientes)
                          NULL, 2, &BrokerTaskHandle,
                          0);    // Núcleo 0

  // Núcleo 1: Sensores (PRIORIDAD 2 — lectura crítica)
  xTaskCreatePinnedToCore(SensorTask, "SensorTask",
                          4096,  // Stack 4 KB
                          NULL, 2, &SensorTaskHandle,
                          1);    // Núcleo 1

  // Núcleo 1: Display OLED (PRIORIDAD 1 — no crítica)
  xTaskCreatePinnedToCore(DisplayTask, "DisplayTask",
                          4096,  // Stack 4 KB
                          NULL, 1, &DisplayTaskHandle,
                          1);    // Núcleo 1
}

void loop() {
  vTaskDelete(NULL);
}
