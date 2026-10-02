// Librerías y variables
#include <WiFi.h>
#include <WiFiMulti.h>
#include <PicoMQTT.h>
#include "data.h"

#include <OneWire.h>                  
#include <DallasTemperature.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Instancias de conectividad
WiFiMulti wifiMulti;
PicoMQTT::Server mqtt;

// Handle para las tareas de FreeRTOS
TaskHandle_t TareaConectividadHandle = NULL;
TaskHandle_t TareaControlHandle = NULL;

// Mutex para proteger variables compartidas entre núcleos
SemaphoreHandle_t xMutex = NULL;

// Pines sensores DS18B20
OneWire ourWire1(14);                
OneWire ourWire2(27);               
DallasTemperature sensors1(&ourWire1); 
DallasTemperature sensors2(&ourWire2);

// Pantalla OLED
Adafruit_SSD1306 display(128, 64, &Wire);

// Pines de hardware
const int corriente1 = 35;
const int corriente2 = 34;
const int led1 = 26;
const int led2 = 25;
const int ventilador1 = 33;
const int ventilador2 = 32;
const int q1 = 13;
const int q2 = 12;

// Parámetros PWM
const int frecuencia = 100000; 
const int resolucion = 8;

// Variables Globales Compartidas (Protegidas por Mutex)
int dutyCycle1 = 0;
int dutyCycle2 = 0;
float temp1 = 0;
float temp2 = 0;
float corrienteQ1 = 0;
float corrienteQ2 = 0;
float segundos = 0;
String estadoled1 = "off";
String estadoled2 = "off";
String estadoventilador1 = "off";
String estadoventilador2 = "off";

// Variables locales de control
const int resolucionTemperatura = 12;
const float alpha = 0.8;
float temperatura1Anterior = 0;
float temperatura2Anterior = 0;
float corriente1Anterior = 0;
float corriente2Anterior = 0;

// Callback de MQTT (Manejador de mensajes entrantes)
void MensajeMQTT(String topic, String payload) {
  if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE) {
    if (topic == msg_pwm1) dutyCycle1 = payload.toInt();
    if (topic == msg_pwm2) dutyCycle2 = payload.toInt();
    
    if (topic == msg_led1) {
      estadoled1 = payload;
      digitalWrite(led1, (payload == "on") ? HIGH : LOW);
    } 
    if (topic == msg_led2) {
      estadoled2 = payload;
      digitalWrite(led2, (payload == "on") ? HIGH : LOW);
    } 
    if (topic == msg_ventilador1) {
      estadoventilador1 = payload;
      ledcWrite(ventilador1, (payload == "on") ? 255 : 0);
    } 
    if (topic == msg_ventilador2) {
      estadoventilador2 = payload;
      ledcWrite(ventilador2, (payload == "on") ? 255 : 0);
    }
    xSemaphoreGive(xMutex);
  }
}

void conectar() {
  while (wifiMulti.run() != WL_CONNECTED) {
    vTaskDelay(pdMS_TO_TICKS(500)); 
  }
  // Suscribirse a los tópicos de control
  mqtt.subscribe("test/datos/#", [](const char* topic, const char* payload) {
    MensajeMQTT(String(topic), String(payload));
  });
}

void pantallaOled() {
  display.clearDisplay();         
  display.setTextSize(1);         
  display.setCursor(0,0);         
  display.print("IP: ");  
  display.print(WiFi.localIP()); 

  display.setCursor(0,8);       
  display.print("T1: ");  display.print(temp1, 1);
  display.setCursor(64,8);       
  display.print("T2: "); display.print(temp2, 1);

  display.setCursor(0,16);       
  display.print("Q1: ");  display.print(dutyCycle1);
  display.setCursor(64,16);       
  display.print("Q2: ");  display.print(dutyCycle2);

  display.setCursor(0,24);       
  display.print("L1: ");  display.print(estadoled1);
  display.setCursor(64,24);       
  display.print("L2: ");  display.print(estadoled2);

  display.setCursor(0,32);       
  display.print("V1: ");  display.print(estadoventilador1);
  display.setCursor(64,32);       
  display.print("V2: ");  display.print(estadoventilador2);

  display.setCursor(0,40);       
  display.print("I1: ");  display.print(corrienteQ1, 2);
  display.setCursor(64,40);       
  display.print("I2: ");  display.print(corrienteQ2, 2);

  display.setCursor(0,48);       
  display.print("Tiempo: "); display.print(segundos, 0);
  display.display();              
}

void setup() {
  // Crear el Mutex para comunicación segura entre núcleos
  xMutex = xSemaphoreCreateMutex();

  // Configurar Wi-Fi
  wifiMulti.addAP(ssid, pass);
  wifiMulti.addAP(ssid2, pass2);
  WiFi.mode(WIFI_STA);
  
  // Establecer conexión y preparar suscripciones MQTT
  conectar();
  mqtt.begin(); 

  // Sensores DS18B20
  sensors1.begin();
  sensors1.setResolution(resolucionTemperatura);   
  sensors1.setWaitForConversion(false); 
  
  sensors2.begin(); 
  sensors2.setResolution(resolucionTemperatura);
  sensors2.setWaitForConversion(false);

  // Configurar Periféricos PWM (LEDC en ESP32)
  ledcAttach(q1, frecuencia, resolucion);
  ledcAttach(q2, frecuencia, resolucion);
  ledcAttach(ventilador1, frecuencia, resolucion);
  ledcAttach(ventilador2, frecuencia, resolucion);

  // Pines Digitales Básicos
  pinMode(led1, OUTPUT);
  pinMode(led2, OUTPUT);

  // Inicializar pantalla OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Error al inicializar la pantalla OLED");
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Primer disparo de conversión de temperatura
  sensors1.requestTemperatures();
  sensors2.requestTemperatures();

  // Core 0: Gestión de Red, Tráfico MQTT y Conectividad
  xTaskCreatePinnedToCore(
    TareaConectividad,           
    "Tarea_WiFi_MQTT",           
    4096,                        
    NULL,                        
    1,                           
    &TareaConectividadHandle,    
    0                            // <--- CORE 0
  );

  // Core 1: Control de Hardware, ADC, PWM, Filtros y OLED
  xTaskCreatePinnedToCore(
    TareaControl,                
    "Tarea_Hardware_Control",    
    4096,                        
    NULL,                        
    2,                           // Mayor prioridad al lazo cerrado de control
    &TareaControlHandle,         
    1                            // <--- CORE 1
  );
}

// El loop principal se elimina para ceder todo el control del chip a FreeRTOS
void loop() {
  vTaskDelete(NULL); 
}

// ==================== DEFINICIÓN DE TAREAS ====================

void TareaConectividad(void * pvParameters) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xPeriodoMqtt = pdMS_TO_TICKS(500); // Frecuencia de envío: 500ms

  for(;;) {
    // Procesa las solicitudes del Broker y mantiene vivas las conexiones
    mqtt.loop();

    // Envío periódico de datos medidos hacia la interfaz externa
    if (xTaskGetTickCount() - xLastWakeTime >= xPeriodoMqtt) {
      xLastWakeTime = xTaskGetTickCount();

      if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        mqtt.publish(data_temp1, String(temp1, 2));
        mqtt.publish(data_temp2, String(temp2, 2));
        mqtt.publish(data_IQ1, String(corrienteQ1, 3));
        mqtt.publish(data_IQ2, String(corrienteQ2, 3));
        mqtt.publish(data_time, String(segundos, 1));
        xSemaphoreGive(xMutex);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(5)); // Pausa mínima para que el Stack de Wi-Fi interno respire
  }
}

void TareaControl(void * pvParameters) {
  unsigned long tiempoSensores = millis();
  int localDuty1 = 0;
  int localDuty2 = 0;

  for(;;) {
    // 1. Muestreo de Corrientes por ADC (Frecuencia de ciclo rápido)
    int Adc1 = analogRead(corriente1);
    int Adc2 = analogRead(corriente2);

    if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE) {
      segundos = millis() / 1000.0;
      
      // Filtros de Media Exponencial (Matemática flotante precisa con 4095.0)
      corrienteQ1 = (((Adc1 * 3.3) / 4095.0) + 0.1) * alpha + (1.0 - alpha) * corriente1Anterior;
      corriente1Anterior = corrienteQ1;

      corrienteQ2 = (((Adc2 * 3.3) / 4095.0) + 0.1) * alpha + (1.0 - alpha) * corriente2Anterior; 
      corriente2Anterior = corrienteQ2;

      // Lectura segura de los comandos del Broker antes de liberar el Mutex
      localDuty1 = dutyCycle1;
      localDuty2 = dutyCycle2;
      xSemaphoreGive(xMutex);
    }

    // Acción de Control sobre los Drivers/MOSFETs Q
    ledcWrite(q1, (int)(localDuty1 * 2.55));
    ledcWrite(q2, (int)(localDuty2 * 2.55));

    // 2. Muestreo de Temperatura Asíncrono (Cada 800ms)
    if (millis() - tiempoSensores >= 800) {
      tiempoSensores = millis();

      float t1_raw = sensors1.getTempCByIndex(0);
      float t2_raw = sensors2.getTempCByIndex(0);

      if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        if (t1_raw != DEVICE_DISCONNECTED_C) {
          temp1 = t1_raw * alpha + (1.0 - alpha) * temperatura1Anterior;
          temperatura1Anterior = temp1;
        }
        if (t2_raw != DEVICE_DISCONNECTED_C) {
          temp2 = t2_raw * alpha + (1.0 - alpha) * temperatura2Anterior;
          temperatura2Anterior = temp2;
        }
        xSemaphoreGive(xMutex);
      }
      
      // Solicitar inmediatamente la conversión en background para el próximo ciclo
      sensors1.requestTemperatures();
      sensors2.requestTemperatures();
    }

    // 3. Actualización de la Pantalla OLED local
    pantallaOled();

    // Delay de 30ms para estabilizar el paso del lazo de hardware (~33 iteraciones por segundo)
    vTaskDelay(pdMS_TO_TICKS(30)); 
  }
}