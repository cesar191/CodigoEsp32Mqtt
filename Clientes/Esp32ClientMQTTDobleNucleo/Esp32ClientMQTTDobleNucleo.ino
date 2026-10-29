#include "data.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <DallasTemperature.h>
#include <MQTT.h>
#include <OneWire.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <Wire.h>

//  CONFIGURACIÓN DE PINES

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


//  PARÁMETROS DE CONFIGURACIÓN

constexpr uint32_t FREC_PWM_Q1 = 500;
constexpr uint32_t FREC_PWM_Q2 = 10000;
constexpr uint32_t FREC_PWM_VENT1 = 5000;
constexpr uint32_t FREC_PWM_VENT2 = 5000;
constexpr uint8_t RESOLUCION_PWM = 8;
constexpr uint8_t RESOLUCION_TEMP = 9;
constexpr float ALPHA_TEMP = 0.8f;
constexpr float ALPHA_CORRIENTE = 0.1f;

// Intervalos de tiempo (ms)
constexpr uint32_t INTERVALO_PUBLICACION = 500;
constexpr uint32_t INTERVALO_OLED = 250;
constexpr uint32_t INTERVALO_SENSORES = 500;
constexpr uint32_t TIMEOUT_WIFI = 15000;       // Timeout máximo para reconexión WiFi
constexpr uint32_t INTERVALO_RECONEXION = 5000; // Espera entre intentos de reconexión
constexpr uint32_t MQTT_KEEPALIVE = 15;         // Keepalive MQTT en segundos


//  INSTANCIAS DE OBJETOS

WiFiClient netClient;
MQTTClient clienteMqtt(1024); 
OneWire wireTemp1(PIN_TEMP_1);
OneWire wireTemp2(PIN_TEMP_2);
DallasTemperature sensorTemp1(&wireTemp1);
DallasTemperature sensorTemp2(&wireTemp2);
Adafruit_SSD1306 display(128, 64, &Wire, -1);

//  ESTRUCTURA DE DATOS COMPARTIDOS (protegida por mutex)

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

// Datos compartidos protegidos por mutex
DatosSensores datos = {0.0f, 0.0f, 0.0f, 0.0f, 0, 0, 0, 0,
                       "off", "off", "off", "off", 0};

// Variables locales del filtro (solo usadas por SensorTask, no necesitan mutex)
float temp1Anterior = 0.0f;
float temp2Anterior = 0.0f;
float corriente1Anterior = 0.0f;
float corriente2Anterior = 0.0f;

//  SINCRONIZACIÓN

SemaphoreHandle_t xMutexData = NULL;
TaskHandle_t SensorTaskHandle = NULL;
TaskHandle_t MQTTTaskHandle = NULL;
TaskHandle_t DisplayTaskHandle = NULL;

//  CALLBACK MQTT — Se ejecuta en el contexto de la tarea MQTT

void procesarMensajeMQTT(String &topic, String &payload) {
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


//  CONEXIÓN WiFi — No bloqueante, con timeout

bool conectarWiFi() {
  if (WiFi.status() == WL_CONNECTED) return true;

  WiFi.disconnect(true);  // Limpia estado anterior
  vTaskDelay(pdMS_TO_TICKS(100));
  WiFi.begin(SSID, PASS);

  uint32_t inicio = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - inicio > TIMEOUT_WIFI) {
      return false;  // Timeout — no bloquea infinitamente
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
  return true;
}

//  CONEXIÓN MQTT — Con timeout, re-suscribe tópicos

bool conectarMQTT() {
  if (clienteMqtt.connected()) return true;
  if (WiFi.status() != WL_CONNECTED) return false;
 // String EspClient=ESP_CLIENT_NAME+
  // Intento de conexión (máximo 3 reintentos rápidos)
  for (int i = 0; i < 3; i++) {
    if (clienteMqtt.connect(ESP_CLIENT_NAME, USER_MQTT, PASS_MQTT)) {
      // Re-suscribir a todos los tópicos tras reconexión
      clienteMqtt.subscribe("test/datos/#");
      return true;
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
  return false;
}

//  TAREA 1: Lectura de sensores (Núcleo 0)

void SensorTask(void *parameter) {
  for (;;) {
    
    sensorTemp1.requestTemperatures();
    sensorTemp2.requestTemperatures();

    float lectTemp1 = sensorTemp1.getTempCByIndex(0);
    float lectTemp2 = sensorTemp2.getTempCByIndex(0);

    // analogReadMilliVolts() retorna milivolts directamente
    int adc1 = analogRead(PIN_ADC_CORRIENTE1);
    int adc2 = analogRead(PIN_ADC_CORRIENTE2);

    if (xSemaphoreTake(xMutexData, pdMS_TO_TICKS(100)) == pdTRUE) {
      if (lectTemp1 != DEVICE_DISCONNECTED_C) {
        datos.temp1 = (lectTemp1 * ALPHA_TEMP) + ((1.0f - ALPHA_TEMP) * temp1Anterior);
        temp1Anterior = datos.temp1;
      }
      if (lectTemp2 != DEVICE_DISCONNECTED_C) {
        datos.temp2 = (lectTemp2 * ALPHA_TEMP) + ((1.0f - ALPHA_TEMP) * temp2Anterior);
        temp2Anterior = datos.temp2;
      }

      float voltaje1 = adc1*3.3f / 4096.0f;
      float voltaje2 = adc2*3.3f / 4096.0f;

      datos.corrienteQ1 = (voltaje1 * ALPHA_CORRIENTE) +
                          ((1.0f - ALPHA_CORRIENTE) * corriente1Anterior);
      corriente1Anterior = datos.corrienteQ1;

      datos.corrienteQ2 = (voltaje2 * ALPHA_CORRIENTE) +
                          ((1.0f - ALPHA_CORRIENTE) * corriente2Anterior);
      corriente2Anterior = datos.corrienteQ2;

      datos.segundos = millis() / 1000;

      xSemaphoreGive(xMutexData);
    }

    vTaskDelay(pdMS_TO_TICKS(INTERVALO_SENSORES));
  }
}

//  TAREA 2: WiFi + MQTT (Núcleo 0)

void MQTTTask(void *parameter) {
  uint32_t ultimoEnvioPub = 0;
  uint32_t ultimaReconexion = 0;

  // Conexión inicial
  conectarWiFi();
  conectarMQTT();

  for (;;) {
    // ── Mantener conexión WiFi y MQTT ──
    if (WiFi.status() != WL_CONNECTED || !clienteMqtt.connected()) {
      // Solo intentar reconexión cada INTERVALO_RECONEXION ms
      if (millis() - ultimaReconexion >= INTERVALO_RECONEXION) {
        ultimaReconexion = millis();
        if (!conectarWiFi()) {
          vTaskDelay(pdMS_TO_TICKS(100));
          continue;  // Reintentar en el siguiente ciclo
        }
        conectarMQTT();
      }
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    // ── Loop MQTT — CRÍTICO: debe llamarse frecuentemente ──
    clienteMqtt.loop();

    // ── Publicación periódica ──
    if (millis() - ultimoEnvioPub >= INTERVALO_PUBLICACION) {
      ultimoEnvioPub = millis();

      if (xSemaphoreTake(xMutexData, pdMS_TO_TICKS(50)) == pdTRUE) {
        clienteMqtt.publish(TOPIC_PUB_TEMP1, String(datos.temp1));
        clienteMqtt.publish(TOPIC_PUB_TEMP2, String(datos.temp2));
        clienteMqtt.publish(TOPIC_PUB_IQ1, String(datos.corrienteQ1));
        clienteMqtt.publish(TOPIC_PUB_IQ2, String(datos.corrienteQ2));
        clienteMqtt.publish(TOPIC_PUB_TIME, String(datos.segundos));
        xSemaphoreGive(xMutexData);
      }
    }

    // Delay corto para permitir que loop() se ejecute con frecuencia
    // pero sin saturar el CPU
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

//  TAREA 3: Pantalla OLED (Núcleo 1)

void DisplayTask(void *parameter) {
  for (;;) {
    if (xSemaphoreTake(xMutexData, pdMS_TO_TICKS(50)) == pdTRUE) {
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
      bool wifiOk = (WiFi.status() == WL_CONNECTED);
      xSemaphoreGive(xMutexData);

      // Dibujar fuera del mutex (I2C es lento)
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);

      display.setCursor(0, 0);
      if (wifiOk) {
        display.print("IP: ");
        display.print(WiFi.localIP());
      } else {
        display.print("WiFi: Desconectado");
      }

      display.setCursor(0, 8);
      display.print("T1: ");
      display.print(t1, 1);
      display.print("C");
      display.setCursor(64, 8);
      display.print("T2: ");
      display.print(t2, 1);
      display.print("C");

      display.setCursor(0, 16);
      display.print("Q1: ");
      display.print(dq1);
      display.print("%");
      display.setCursor(64, 16);
      display.print("Q2: ");
      display.print(dq2);
      display.print("%");

      display.setCursor(0, 24);
      display.print("L1: ");
      display.print(el1);
      display.setCursor(64, 24);
      display.print("L2: ");
      display.print(el2);

      display.setCursor(0, 32);
      display.print("V1: ");
      display.print(ev1);
      display.setCursor(64, 32);
      display.print("V2: ");
      display.print(ev2);

      display.setCursor(0, 40);
      display.print("I1: ");
      display.print(i1, 2);
      display.print("A");
      display.setCursor(64, 40);
      display.print("I2: ");
      display.print(i2, 2);
      display.print("A");

      display.setCursor(0, 48);
      display.print("Tiempo: ");
      display.print(seg);
      display.print(" s");

      // Indicador de MQTT
      display.setCursor(0, 56);
      display.print("MQTT: ");
      display.print(clienteMqtt.connected() ? "OK" : "---");

      display.display();
    }

    vTaskDelay(pdMS_TO_TICKS(INTERVALO_OLED));
  }
}

void setup() {
  // Configurar pines digitales
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
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for (;;)
      ; // Detiene ejecución si la pantalla falla
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.display();

  // Mutex
  xMutexData = xSemaphoreCreateMutex();

  // Configuración WiFi robusta
  WiFi.mode(WIFI_STA);              // Modo estación explícito
  WiFi.setSleep(false);             // Desactivar power-save WiFi (evita desconexiones)
  WiFi.setAutoReconnect(true);      // Reconexión automática a nivel de stack
  WiFi.persistent(false);           // No guardar credenciales en flash (evita desgaste)

  // MQTT
  clienteMqtt.begin(BROKER_MQTT, PUERTO_MQTT, netClient);
  clienteMqtt.onMessage(procesarMensajeMQTT);
  clienteMqtt.setKeepAlive(MQTT_KEEPALIVE);

  // ── Crear tareas en los núcleos ──
  // Núcleo 1: Sensores (prioridad 2 — más alta, lectura crítica)
  xTaskCreatePinnedToCore(SensorTask, "SensorTask",
                          4096,  // Stack 4 KB
                          NULL, 2, &SensorTaskHandle,
                          1);

  // Núcleo 0: MQTT (prioridad 1 — debe correr frecuentemente)
  xTaskCreatePinnedToCore(MQTTTask, "MQTTTask",
                          8192,  // Stack 8 KB (WiFi/MQTT usan bastante stack)
                          NULL, 1, &MQTTTaskHandle,
                          0);

  // Núcleo 1: Display OLED (prioridad 1 — no crítica)
  xTaskCreatePinnedToCore(DisplayTask, "DisplayTask",
                          4096,  // Stack 4 KB
                          NULL, 1, &DisplayTaskHandle,
                          1);
}

void loop() {
  vTaskDelete(NULL); // loop() no se usa, las tareas corren por separado
}
