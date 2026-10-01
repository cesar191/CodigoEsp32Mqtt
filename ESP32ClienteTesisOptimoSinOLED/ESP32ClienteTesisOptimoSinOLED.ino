// Librerías de conectividad y protocolo
  #include <WiFi.h>
  #include <WiFiMulti.h>
  #include <MQTT.h>
  #include "data.h"

// Librerías para sensores de temperatura DS18B20
  #include <OneWire.h>
  #include <DallasTemperature.h>
// CONFIGURACIÓN DE PINES (uint8_t = 1 byte)

  constexpr uint8_t PIN_TEMP_1          = 14;
  constexpr uint8_t PIN_TEMP_2          = 27;
  constexpr uint8_t PIN_ADC_CORRIENTE1  = 35;
  constexpr uint8_t PIN_ADC_CORRIENTE2  = 34;

  constexpr uint8_t PIN_LED1            = 26;
  constexpr uint8_t PIN_LED2            = 25;
  constexpr uint8_t PIN_VENTILADOR1     = 33;
  constexpr uint8_t PIN_VENTILADOR2     = 32;

  constexpr uint8_t PIN_PWM_Q1          = 13;
  constexpr uint8_t PIN_PWM_Q2          = 12;

// PARÁMETROS DE CONFIGURACIÓN
  constexpr uint32_t FREC_PWM_Q       = 10000;
  constexpr uint32_t FREC_PWM_VENT    = 100;
  constexpr uint8_t  RESOLUCION_PWM   = 8;
  constexpr uint8_t  RESOLUCION_TEMP  = 9;

  constexpr float ALPHA_TEMP          = 0.8f;
  constexpr float ALPHA_CORRIENTE     = 0.05f;

// INSTANCIAS DE OBJETOS
  WiFiMulti wifiMulti;
  WiFiClient netClient;
  MQTTClient clienteMqtt;

  OneWire wireTemp1(PIN_TEMP_1);
  OneWire wireTemp2(PIN_TEMP_2);
  DallasTemperature sensorTemp1(&wireTemp1);
  DallasTemperature sensorTemp2(&wireTemp2);

// VARIABLES GLOBALES
  uint32_t tiempoAnteriorMs   = 0;
  uint32_t tiempoAnterior2Ms  = 0;
  uint32_t segundos           = 0;
// Ciclos de trabajo (0 - 100%)
  uint8_t dutyCycleQ1     = 0;
  uint8_t dutyCycleQ2     = 0;
  uint8_t dutyCycleVent1  = 0;
  uint8_t dutyCycleVent2  = 0;
// Estados de actuadores
  String estadoLed1         = "off";
  String estadoLed2         = "off";
  String estadoVentilador1  = "off";
  String estadoVentilador2  = "off";

// Mediciones y estados de filtros
  float temp1         = 0.0f;
  float temp2         = 0.0f;
  float corrienteQ1   = 0.0f;
  float corrienteQ2   = 0.0f;

  float temp1Anterior       = 20.0f;
  float temp2Anterior       = 20.0f;
  float corriente1Anterior  = 0.065f;
  float corriente2Anterior  = 0.065f;

// FUNCIONES Y RUTINAS

void procesarMensajeMQTT(String topic, String payload) {
  if (topic == TOPIC_SUB_PWM1) {
    dutyCycleQ1 = payload.toInt();
    ledcWrite(PIN_PWM_Q1, dutyCycleQ1 * 2.55f);
  }
  else if (topic == TOPIC_SUB_PWM2) {
    dutyCycleQ2 = payload.toInt();
    ledcWrite(PIN_PWM_Q2, dutyCycleQ2 * 2.55f);
  }
  else if (topic == TOPIC_SUB_LED1) {
    estadoLed1 = payload;
    if (payload == "on") {
      digitalWrite(PIN_LED1, HIGH);
    } else if (payload == "off") {
      digitalWrite(PIN_LED1, LOW);
    }
  } 
  else if (topic == TOPIC_SUB_LED2) {
    estadoLed2 = payload;
    if (payload == "on") {
      digitalWrite(PIN_LED2, HIGH);
    } else if (payload == "off") {
      digitalWrite(PIN_LED2, LOW);
    }
  } 
  else if (topic == TOPIC_SUB_VENT1) {
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
  } 
  else if (topic == TOPIC_SUB_VENT2) {
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
  ledcAttach(PIN_PWM_Q1, FREC_PWM_Q, RESOLUCION_PWM);
  ledcAttach(PIN_PWM_Q2, FREC_PWM_Q, RESOLUCION_PWM);
  ledcAttach(PIN_VENTILADOR1, FREC_PWM_VENT, RESOLUCION_PWM);
  ledcAttach(PIN_VENTILADOR2, FREC_PWM_VENT, RESOLUCION_PWM);

  // Configuración de salidas discretas
  pinMode(PIN_LED1, OUTPUT);
  pinMode(PIN_LED2, OUTPUT);
}

void loop() {
  clienteMqtt.loop();

  // Lectura y filtrado exponencial de temperatura
  sensorTemp1.requestTemperatures();
  temp1 = (sensorTemp1.getTempCByIndex(0) * ALPHA_TEMP) + ((1.0f - ALPHA_TEMP) * temp1Anterior);
  temp1Anterior = temp1;

  sensorTemp2.requestTemperatures();
  temp2 = (sensorTemp2.getTempCByIndex(0) * ALPHA_TEMP) + ((1.0f - ALPHA_TEMP) * temp2Anterior);
  temp2Anterior = temp2;

  // Lectura y filtrado exponencial de corriente por ADC
  const int adc1 = analogRead(PIN_ADC_CORRIENTE1);
  corrienteQ1 = ((((adc1 * 3.3f) / 4096.0f) * 1.17f + 0.065f) * ALPHA_CORRIENTE) + ((1.0f - ALPHA_CORRIENTE) * corriente1Anterior);
  corriente1Anterior = corrienteQ1;

  const int adc2 = analogRead(PIN_ADC_CORRIENTE2);
  corrienteQ2 = ((((adc2 * 3.3f) / 4096.0f) * 1.17f + 0.065f) * ALPHA_CORRIENTE) + ((1.0f - ALPHA_CORRIENTE) * corriente2Anterior);
  corriente2Anterior = corrienteQ2;

  // Publicación de telemetría por MQTT
  if(millis()-tiempoAnterior2Ms>=1000){
    segundos = millis() / 1000.0f;
    tiempoAnterior2Ms=millis();

    clienteMqtt.publish(TOPIC_PUB_TEMP1, String(temp1));
    clienteMqtt.publish(TOPIC_PUB_TEMP2, String(temp2));
    clienteMqtt.publish(TOPIC_PUB_IQ1,   String(corrienteQ1));
    clienteMqtt.publish(TOPIC_PUB_IQ2,   String(corrienteQ2));
    clienteMqtt.publish(TOPIC_PUB_TIME,  String(segundos));
  }

}