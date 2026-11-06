
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

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
constexpr uint32_t FREC_PWM_VENT1 = 5;
constexpr uint32_t FREC_PWM_VENT2 = 5;
constexpr uint8_t RESOLUCION_PWM = 8;
constexpr uint8_t RESOLUCION_TEMP = 9;
constexpr float ALPHA_TEMP = 0.8f;
constexpr float ALPHA_CORRIENTE = 0.1f;

// Intervalos de tiempo (ms)
constexpr uint32_t INTERVALO_ENVIO_SERIAL = 1000;
constexpr uint32_t INTERVALO_OLED = 300;
constexpr uint32_t INTERVALO_SENSORES_TEMP = 800;
constexpr uint32_t INTERVALO_SENSORES_ADC = 50;


//  INSTANCIAS DE OBJETOS

OneWire wireTemp1(PIN_TEMP_1);
OneWire wireTemp2(PIN_TEMP_2);
DallasTemperature sensorTemp1(&wireTemp1);
DallasTemperature sensorTemp2(&wireTemp2);
Adafruit_SSD1306 display(128, 64, &Wire);

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

DatosSensores datos = {0.0f, 0.0f, 0.0f, 0.0f, 0, 0, 0, 0,
                       "off", "off", "off", "off", 0};

// Variables locales del filtro (solo usadas por SensorTask)
float temp1Anterior = 0.0f;
float temp2Anterior = 0.0f;
float corriente1Anterior = 0.0f;
float corriente2Anterior = 0.0f;


//  SINCRONIZACIÓN Y HANDLES

SemaphoreHandle_t xMutexData = NULL;
TaskHandle_t SerialTaskHandle = NULL;
TaskHandle_t SensorTaskHandle = NULL;
TaskHandle_t DisplayTaskHandle = NULL;

//  BUFFER DE RECEPCIÓN SERIAL (char[] — más eficiente que String)

constexpr uint8_t TAMANO_BUFFER_SERIAL = 32;
char rxBuffer[TAMANO_BUFFER_SERIAL];
uint8_t rxIndex = 0;

//  PROCESAMIENTO DE COMANDOS SERIALES
//  Se ejecuta en la tarea Serial cuando llega un comando

void procesarComando(const char *cmd) {
  if (xSemaphoreTake(xMutexData, pdMS_TO_TICKS(50)) == pdTRUE) {
    if (strcmp(cmd, "$LED1on") == 0) {
      datos.estadoLed1 = "on";
      digitalWrite(PIN_LED1, HIGH);
    } else if (strcmp(cmd, "$LED1off") == 0) {
      datos.estadoLed1 = "off";
      digitalWrite(PIN_LED1, LOW);
    } else if (strcmp(cmd, "$LED2on") == 0) {
      datos.estadoLed2 = "on";
      digitalWrite(PIN_LED2, HIGH);
    } else if (strcmp(cmd, "$LED2off") == 0) {
      datos.estadoLed2 = "off";
      digitalWrite(PIN_LED2, LOW);
    } else if (strcmp(cmd, "$VENT1on") == 0) {
      datos.estadoVentilador1 = "on";
      datos.dutyCycleVent1 = 100;
      ledcWrite(PIN_VENTILADOR1, 255);
    } else if (strcmp(cmd, "$VENT1off") == 0) {
      datos.estadoVentilador1 = "off";
      datos.dutyCycleVent1 = 0;
      ledcWrite(PIN_VENTILADOR1, 0);
    } else if (strcmp(cmd, "$VENT2on") == 0) {
      datos.estadoVentilador2 = "on";
      datos.dutyCycleVent2 = 100;
      ledcWrite(PIN_VENTILADOR2, 255);
    } else if (strcmp(cmd, "$VENT2off") == 0) {
      datos.estadoVentilador2 = "off";
      datos.dutyCycleVent2 = 0;
      ledcWrite(PIN_VENTILADOR2, 0);
    } else if (strncmp(cmd, "$PWM1", 5) == 0) {
      int valor = atoi(&cmd[5]);
      datos.dutyCycleQ1 = (uint8_t)constrain(valor, 0, 100);
      ledcWrite(PIN_PWM_Q1, (uint8_t)((datos.dutyCycleQ1 * 255) / 100));
    } else if (strncmp(cmd, "$PWM2", 5) == 0) {
      int valor = atoi(&cmd[5]);
      datos.dutyCycleQ2 = (uint8_t)constrain(valor, 0, 100);
      ledcWrite(PIN_PWM_Q2, (uint8_t)((datos.dutyCycleQ2 * 255) / 100));
    }
    xSemaphoreGive(xMutexData);
  }
}

//  TAREA 1: Comunicación Serial (Núcleo 0)

void SerialTask(void *parameter) {
  uint32_t ultimoEnvio = 0;

  for (;;) {
    // ── Recepción: leer bytes disponibles ──
    while (Serial.available() > 0) {
      char inChar = (char)Serial.read();

      if (inChar == '\n' || inChar == '\r') {
        if (rxIndex > 0) {
          rxBuffer[rxIndex] = '\0';
          procesarComando(rxBuffer);
          rxIndex = 0;
        }
      } else if (rxIndex < TAMANO_BUFFER_SERIAL - 1) {
        rxBuffer[rxIndex++] = inChar;
      }
    }

    // ── Transmisión periódica de telemetría ──
    if (millis() - ultimoEnvio >= INTERVALO_ENVIO_SERIAL) {
      ultimoEnvio = millis();

      if (xSemaphoreTake(xMutexData, pdMS_TO_TICKS(20)) == pdTRUE) {
        // Copiar datos localmente para soltar el mutex rápido
        float t1 = datos.temp1;
        float t2 = datos.temp2;
        float i1 = datos.corrienteQ1;
        float i2 = datos.corrienteQ2;
        uint32_t seg = datos.segundos;
        xSemaphoreGive(xMutexData);

        // Enviar fuera del mutex (Serial.print puede ser lento)
        Serial.print("#Temp1");
        Serial.println(t1);

        Serial.print("#Temp2");
        Serial.println(t2);

        Serial.print("#Corri1");
        Serial.println(i1);

        Serial.print("#Corri2");
        Serial.println(i2);

        Serial.print("#Tiempo");
        Serial.println(seg);
      }
    }

    // Delay corto para no saturar CPU pero responder rápido a RX
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ─────────────────────────────────────────────────────────
//  TAREA 2: Lectura de sensores (Núcleo 1)
//  Lee temperaturas y corrientes, actualiza datos compartidos
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

      // Solicitar nueva conversión para el próximo ciclo (no bloqueante)
      sensorTemp1.requestTemperatures();
      sensorTemp2.requestTemperatures();
      conversionSolicitada = true;
    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// ─────────────────────────────────────────────────────────
//  TAREA 3: Pantalla OLED (Núcleo 1)
//  Separada para que I2C no bloquee la lectura de sensores
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

      // Fila 0: Título
      display.setCursor(0, 0);
      display.print("Serial Control");

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

      display.display();
    }

    vTaskDelay(pdMS_TO_TICKS(INTERVALO_OLED));
  }
}

// ─────────────────────────────────────────────────────────
//  SETUP
// ─────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  // Mutex — crear ANTES de cualquier tarea
  xMutexData = xSemaphoreCreateMutex();

  // Pines digitales
  pinMode(PIN_LED1, OUTPUT);
  pinMode(PIN_LED2, OUTPUT);
  digitalWrite(PIN_LED1, LOW);
  digitalWrite(PIN_LED2, LOW);

  // Sensores de temperatura (modo no bloqueante)
  sensorTemp1.begin();
  sensorTemp1.setResolution(RESOLUCION_TEMP);
  sensorTemp1.setWaitForConversion(false);

  sensorTemp2.begin();
  sensorTemp2.setResolution(RESOLUCION_TEMP);
  sensorTemp2.setWaitForConversion(false);

  analogReadResolution(12);

  // PWM
  ledcAttach(PIN_PWM_Q1, FREC_PWM_Q1, RESOLUCION_PWM);
  ledcAttach(PIN_PWM_Q2, FREC_PWM_Q2, RESOLUCION_PWM);
  ledcAttach(PIN_VENTILADOR1, FREC_PWM_VENT1, RESOLUCION_PWM);
  ledcAttach(PIN_VENTILADOR2, FREC_PWM_VENT2, RESOLUCION_PWM);

  // Pantalla OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("ERROR: Fallo al inicializar SSD1306");
  } else {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.display();
  }

  // ── Crear tareas ──

  // Núcleo 0: Serial RX + TX (PRIORIDAD 2 — responsividad)
  // Todo el acceso a Serial.read/Serial.print ocurre SOLO aquí
  xTaskCreatePinnedToCore(SerialTask, "SerialTask",
                          4096,  // Stack 4 KB
                          NULL, 2, &SerialTaskHandle,
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
