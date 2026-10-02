// Librerías de conectividad y protocolo
#include "data.h"
#include <MQTT.h>
#include <WiFi.h>
#include <WiFiMulti.h>

// Librerías para sensores de temperatura DS18B20
#include <DallasTemperature.h>
#include <OneWire.h>

// Librerías para pantalla OLED y bus I2C/SPI
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>

// CONFIGURACIÓN DE PINES (uint8_t = 1 byte)

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

// PARÁMETROS DE CONFIGURACIÓN
constexpr uint32_t FREC_PWM_Q1 = 1000;
constexpr uint32_t FREC_PWM_Q2 = 100000;
constexpr uint32_t FREC_PWM_VENT1 = 10;
constexpr uint32_t FREC_PWM_VENT2 = 5;
constexpr uint8_t RESOLUCION_PWM = 8;
constexpr uint8_t RESOLUCION_TEMP = 9;

constexpr float ALPHA_TEMP = 0.8f;
constexpr float ALPHA_CORRIENTE = 0.05f;

// INSTANCIAS DE OBJETOS
WiFiMulti wifiMulti;
WiFiClient netClient;
MQTTClient clienteMqtt;

OneWire wireTemp1(PIN_TEMP_1);
OneWire wireTemp2(PIN_TEMP_2);
DallasTemperature sensorTemp1(&wireTemp1);
DallasTemperature sensorTemp2(&wireTemp2);

Adafruit_SSD1306 display(128, 64, &Wire);

// VARIABLES GLOBALES
uint32_t tiempoAnteriorMs = 0;
uint32_t tiempoAnterior2Ms = 0;
uint32_t segundos = 0;
// Ciclos de trabajo (0 - 100%)
uint8_t dutyCycleQ1 = 0;
uint8_t dutyCycleQ2 = 0;
uint8_t dutyCycleVent1 = 0;
uint8_t dutyCycleVent2 = 0;
// Estados de actuadores
String estadoLed1 = "off";
String estadoLed2 = "off";
String estadoVentilador1 = "off";
String estadoVentilador2 = "off";

// Mediciones y estados de filtros
float temp1 = 0.0f;
float temp2 = 0.0f;
float corrienteQ1 = 0.0f;
float corrienteQ2 = 0.0f;

float temp1Anterior = 0.0f;
float temp2Anterior = 0.0f;
float corriente1Anterior = 0.0f;
float corriente2Anterior = 0.0f;

// FUNCIONES Y RUTINAS

void procesarMensajeMQTT(String topic, String payload) {
  if (topic == TOPIC_SUB_PWM1) {
    dutyCycleQ1 = payload.toInt();
    ledcWrite(PIN_PWM_Q1, dutyCycleQ1 * 2.55f);
  } else if (topic == TOPIC_SUB_PWM2) {
    dutyCycleQ2 = payload.toInt();
    ledcWrite(PIN_PWM_Q2, dutyCycleQ2 * 2.55f);
  } else if (topic == TOPIC_SUB_LED1) {
    estadoLed1 = payload;
    if (payload == "on") {
      digitalWrite(PIN_LED1, HIGH);
    } else if (payload == "off") {
      digitalWrite(PIN_LED1, LOW);
    }
  } else if (topic == TOPIC_SUB_LED2) {
    estadoLed2 = payload;
    if (payload == "on") {
      digitalWrite(PIN_LED2, HIGH);
    } else if (payload == "off") {
      digitalWrite(PIN_LED2, LOW);
    }
  } else if (topic == TOPIC_SUB_VENT1) {
    estadoVentilador1 = payload;
    if (payload == "on") {
      ledcWrite(PIN_VENTILADOR1, 255);
    } else if (payload == "off") {
      digitalWrite(PIN_VENTILADOR1, LOW);
      ledcWrite(PIN_VENTILADOR1, 0);
    } else {
      dutyCycleVent1 = payload.toInt();
      ledcWrite(PIN_VENTILADOR1, dutyCycleVent1 * 2.55f);
    }
  } else if (topic == TOPIC_SUB_VENT2) {
    estadoVentilador2 = payload;
    if (payload == "on") {
      ledcWrite(PIN_VENTILADOR2, 255);
    } else if (payload == "off") {
      ledcWrite(PIN_VENTILADOR2, 0);
    } else {
      dutyCycleVent2 = payload.toInt();
      ledcWrite(PIN_VENTILADOR2, dutyCycleVent2 * 2.55f);
    }
  }
}

void conectarRedYBroker() {
  while (wifiMulti.run() != WL_CONNECTED) {
    // Espera conexión Wi-Fi
  }

  while (!clienteMqtt.connect(ESP_CLIENT_NAME)) {
    // Espera conexión al Broker MQTT
  }

  clienteMqtt.subscribe("test/datos/#");
}

void actualizarPantallaOled() {
  display.clearDisplay();
  display.setTextSize(1);

  // Fila 0: IP Local
  display.setCursor(0, 0);
  display.print("IP: ");
  display.print(WiFi.localIP());

  // Fila 1: Temperaturas
  display.setCursor(0, 8);
  display.print("T1: ");
  display.print(temp1);
  display.print("C");

  display.setCursor(64, 8);
  display.print("T2: ");
  display.print(temp2);
  display.print("C");

  // Fila 2: Ciclos PWM
  display.setCursor(0, 16);
  display.print("Q1: ");
  display.print(dutyCycleQ1);
  display.print("%");

  display.setCursor(64, 16);
  display.print("Q2: ");
  display.print(dutyCycleQ2);
  display.print("%");

  // Fila 3: Estado LEDs
  display.setCursor(0, 24);
  display.print("L1: ");
  display.print(estadoLed1);

  display.setCursor(64, 24);
  display.print("L2: ");
  display.print(estadoLed2);

  // Fila 4: Estado Ventiladores
  display.setCursor(0, 32);
  display.print("V1: ");
  display.print(estadoVentilador1);

  display.setCursor(64, 32);
  display.print("V2: ");
  display.print(estadoVentilador2);

  // Fila 5: Corrientes
  display.setCursor(0, 40);
  display.print("I1: ");
  display.print(corrienteQ1);
  display.print("A");

  display.setCursor(64, 40);
  display.print("I2: ");
  display.print(corrienteQ2);
  display.print("A");

  // Fila 6: Tiempo de ejecución
  display.setCursor(0, 48);
  display.print("tiempo: ");
  display.print(segundos);
  display.print(" Seg");

  display.display();
}

void setup() {

  wifiMulti.addAP(SSID_HOGAR, PASS_HOGAR);
  wifiMulti.addAP(SSID_MOVIL, PASS_MOVIL);
  WiFi.mode(WIFI_STA);

  clienteMqtt.begin(BROKER_MQTT, netClient);
  clienteMqtt.onMessage(procesarMensajeMQTT);

  conectarRedYBroker();

  // Inicialización de sensores de temperatura
  sensorTemp1.begin();
  sensorTemp1.setResolution(RESOLUCION_TEMP);
  sensorTemp2.begin();
  sensorTemp2.setResolution(RESOLUCION_TEMP);

  // Configuración PWM
  ledcAttach(PIN_PWM_Q1, FREC_PWM_Q1, RESOLUCION_PWM);
  ledcAttach(PIN_PWM_Q2, FREC_PWM_Q2, RESOLUCION_PWM);
  ledcAttach(PIN_VENTILADOR1, FREC_PWM_VENT1, RESOLUCION_PWM);
  ledcAttach(PIN_VENTILADOR2, FREC_PWM_VENT2, RESOLUCION_PWM);

  // Configuración de salidas discretas
  pinMode(PIN_LED1, OUTPUT);
  pinMode(PIN_LED2, OUTPUT);

  // Inicialización de pantalla OLED
  if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
  }
}

void loop() {
  clienteMqtt.loop();

  // Lectura y filtrado exponencial de temperatura
  sensorTemp1.requestTemperatures();
  temp1 = (sensorTemp1.getTempCByIndex(0) * ALPHA_TEMP) +
          ((1.0f - ALPHA_TEMP) * temp1Anterior);
  temp1Anterior = temp1;

  sensorTemp2.requestTemperatures();
  temp2 = (sensorTemp2.getTempCByIndex(0) * ALPHA_TEMP) +
          ((1.0f - ALPHA_TEMP) * temp2Anterior);
  temp2Anterior = temp2;

  // Lectura y filtrado exponencial de corriente por ADC
  const int adc1 = analogRead(PIN_ADC_CORRIENTE1);
  corrienteQ1 = ((((adc1 * 3.3f) / 4096.0f)) * ALPHA_CORRIENTE) +
                ((1.0f - ALPHA_CORRIENTE) * corriente1Anterior);
  corriente1Anterior = corrienteQ1;

  const int adc2 = analogRead(PIN_ADC_CORRIENTE2);
  corrienteQ2 = ((((adc2 * 3.3f) / 4096.0f)) * ALPHA_CORRIENTE) +
                ((1.0f - ALPHA_CORRIENTE) * corriente2Anterior);
  corriente2Anterior = corrienteQ2;

  // Publicación de telemetría por MQTT
  if (millis() - tiempoAnterior2Ms >= 1000) {
    segundos = millis() / 1000.0f;
    tiempoAnterior2Ms = millis();
    clienteMqtt.publish(TOPIC_PUB_TEMP1, String(temp1));
    clienteMqtt.publish(TOPIC_PUB_TEMP2, String(temp2));
    clienteMqtt.publish(TOPIC_PUB_IQ1, String(corrienteQ1));
    clienteMqtt.publish(TOPIC_PUB_IQ2, String(corrienteQ2));
    clienteMqtt.publish(TOPIC_PUB_TIME, String(segundos));
  }

  // Renderizado en pantalla OLED
  actualizarPantallaOled();

  // Limpieza periódica cada 5 segundos
  if (millis() - tiempoAnteriorMs >= 5000) {
    tiempoAnteriorMs = millis();
    if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
      display.clearDisplay();
    }
  }

  if (!clienteMqtt.connected()) {
    conectarRedYBroker();
  }
}