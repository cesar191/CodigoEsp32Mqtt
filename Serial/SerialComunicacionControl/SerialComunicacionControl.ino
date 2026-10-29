#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// CONFIGURACIÓN DE PINES
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
constexpr uint32_t FREC_PWM_Q1 = 500;
constexpr uint32_t FREC_PWM_Q2 = 10000;
constexpr uint32_t FREC_PWM_VENT1 = 5;
constexpr uint32_t FREC_PWM_VENT2 = 5;
constexpr uint8_t  RESOLUCION_PWM = 8;
constexpr uint8_t  RESOLUCION_TEMP = 9;

constexpr float ALPHA_TEMP = 0.8f;
constexpr float ALPHA_CORRIENTE = 0.1f;

// INTERVALOS DE TIEMPO (ms)
constexpr uint32_t INTERVALO_SENSORES = 200;
constexpr uint32_t INTERVALO_OLED     = 250;
constexpr uint32_t INTERVALO_SERIAL   = 1000;
uint32_t segundos = 0;

// INSTANCIAS DE OBJETOS
OneWire wireTemp1(PIN_TEMP_1);
OneWire wireTemp2(PIN_TEMP_2);
DallasTemperature sensorTemp1(&wireTemp1);
DallasTemperature sensorTemp2(&wireTemp2);

Adafruit_SSD1306 display(128, 64, &Wire);

// VARIABLES GLOBALES DE ESTADO Y MEDICIÓN
uint32_t tAnteriorSensores = 0;
uint32_t tAnteriorOled     = 0;
uint32_t tAnteriorSerial   = 0;

uint8_t dutyCycleQ1 = 0;
uint8_t dutyCycleQ2 = 0;
uint8_t dutyCycleVent1 = 0;
uint8_t dutyCycleVent2 = 0;

bool estadoLed1 = false;
bool estadoLed2 = false;
bool estadoVent1 = false;
bool estadoVent2 = false;

float temp1 = 0.0f;
float temp2 = 0.0f;
float corrienteQ1 = 0.0f;
float corrienteQ2 = 0.0f;
float temp1Anterior = 0.0f;
float temp2Anterior = 0.0f;
float corriente1Anterior = 0.0f;
float corriente2Anterior = 0.0f;

// BUFFER SERIAL
constexpr uint8_t TAMANO_BUFFER_SERIAL = 32;
char rxBuffer[TAMANO_BUFFER_SERIAL];
uint8_t rxIndex = 0;

// DECLARACIÓN DE FUNCIONES
void leerPuertoSerial();
void procesarComando(const char* cmd);
void leerSensores();
void actualizarPantallaOled();
void enviarTelemetriaSerial();

void setup() {
  Serial.begin(115200);

  // Inicialización de sensores de temperatura
  sensorTemp1.begin();
  sensorTemp1.setResolution(RESOLUCION_TEMP);
  sensorTemp1.setWaitForConversion(false);

  sensorTemp2.begin();
  sensorTemp2.setResolution(RESOLUCION_TEMP);
  sensorTemp2.setWaitForConversion(false);

  analogReadResolution(12);

  // Configuración PWM ESP32
  ledcAttach(PIN_PWM_Q1, FREC_PWM_Q1, RESOLUCION_PWM);
  ledcAttach(PIN_PWM_Q2, FREC_PWM_Q2, RESOLUCION_PWM);
  ledcAttach(PIN_VENTILADOR1, FREC_PWM_VENT1, RESOLUCION_PWM);
  ledcAttach(PIN_VENTILADOR2, FREC_PWM_VENT2, RESOLUCION_PWM);

  // Configuración de salidas discretas
  pinMode(PIN_LED1, OUTPUT);
  pinMode(PIN_LED2, OUTPUT);
  digitalWrite(PIN_LED1, LOW);
  digitalWrite(PIN_LED2, LOW);

  // Inicialización de pantalla OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("ERROR: Fallo al inicializar SSD1306");
  } else {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.display();
  }
}

void loop() {
  uint32_t tActual = millis();

  // 1. Recepción y procesamiento serie continuo
  leerPuertoSerial();

  // 2. Tarea de lectura y filtrado de sensores
  if (tActual - tAnteriorSensores >= INTERVALO_SENSORES) {
    tAnteriorSensores = tActual;
    leerSensores();
  }

  // 3. Tarea de actualización de pantalla OLED
  if (tActual - tAnteriorOled >= INTERVALO_OLED) {
    tAnteriorOled = tActual;
    actualizarPantallaOled();
  }

  // 4. Tarea de transmisión de telemetría por puerto serie
  if (tActual - tAnteriorSerial >= INTERVALO_SERIAL) {
    tAnteriorSerial = tActual;
    enviarTelemetriaSerial();
  }
}

void leerPuertoSerial() {
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
}

void procesarComando(const char* cmd) {
  if (strcmp(cmd, "$LED1on") == 0) {
    estadoLed1 = true;
    digitalWrite(PIN_LED1, HIGH);
  } else if (strcmp(cmd, "$LED1off") == 0) {
    estadoLed1 = false;
    digitalWrite(PIN_LED1, LOW);
  } else if (strcmp(cmd, "$LED2on") == 0) {
    estadoLed2 = true;
    digitalWrite(PIN_LED2, HIGH);
  } else if (strcmp(cmd, "$LED2off") == 0) {
    estadoLed2 = false;
    digitalWrite(PIN_LED2, LOW);
  } else if (strcmp(cmd, "$VENT1on") == 0) {
    estadoVent1 = true;
    dutyCycleVent1 = 100;
    ledcWrite(PIN_VENTILADOR1, 255);
  } else if (strcmp(cmd, "$VENT1off") == 0) {
    estadoVent1 = false;
    dutyCycleVent1 = 0;
    ledcWrite(PIN_VENTILADOR1, 0);
  } else if (strcmp(cmd, "$VENT2on") == 0) {
    estadoVent2 = true;
    dutyCycleVent2 = 100;
    ledcWrite(PIN_VENTILADOR2, 255);
  } else if (strcmp(cmd, "$VENT2off") == 0) {
    estadoVent2 = false;
    dutyCycleVent2 = 0;
    ledcWrite(PIN_VENTILADOR2, 0); // CORREGIDO: Se apuntaba a PIN_VENTILADOR1
  } else if (strncmp(cmd, "$PWM1", 5) == 0) {
    int valor = atoi(&cmd[5]);
    dutyCycleQ1 = (uint8_t)constrain(valor, 0, 100);
    uint8_t pwmVal = (uint8_t)((dutyCycleQ1 * 255) / 100);
    ledcWrite(PIN_PWM_Q1, pwmVal);
  } else if (strncmp(cmd, "$PWM2", 5) == 0) {
    int valor = atoi(&cmd[5]);
    dutyCycleQ2 = (uint8_t)constrain(valor, 0, 100);
    uint8_t pwmVal = (uint8_t)((dutyCycleQ2 * 255) / 100);
    ledcWrite(PIN_PWM_Q2, pwmVal);
  }
}

void leerSensores() {
  // Lectura asíncrona de temperaturas
  sensorTemp1.requestTemperatures();
  float t1Raw = sensorTemp1.getTempCByIndex(0);
  if (t1Raw != DEVICE_DISCONNECTED_C) {
    temp1 = (t1Raw * ALPHA_TEMP) + ((1.0f - ALPHA_TEMP) * temp1);
  }

  sensorTemp2.requestTemperatures();
  sensorTemp1.requestTemperatures();
  temp1 = (sensorTemp1.getTempCByIndex(0) * ALPHA_TEMP) + ((1.0f - ALPHA_TEMP) * temp1Anterior);
  temp1Anterior = temp1;

  sensorTemp2.requestTemperatures();
  temp2 = (sensorTemp2.getTempCByIndex(0) * ALPHA_TEMP) + ((1.0f - ALPHA_TEMP) * temp2Anterior);
  temp2Anterior = temp2;

  // Lectura y filtrado exponencial de corriente por ADC
  const int adc1 = analogRead(PIN_ADC_CORRIENTE1);
  //const int adc1 = analogRead(PIN_ADC_CORRIENTE1);
  corrienteQ1 = ((((adc1 * 3.3f) / 4096.0f)) * ALPHA_CORRIENTE) + ((1.0f - ALPHA_CORRIENTE) * corriente1Anterior);
  corriente1Anterior = corrienteQ1;
  
  const int adc2 = analogRead(PIN_ADC_CORRIENTE2);
  //const int adc2 = analogRead(PIN_ADC_CORRIENTE2);
  corrienteQ2 = ((((adc2 * 3.3f) / 4096.0f)) * ALPHA_CORRIENTE) + ((1.0f - ALPHA_CORRIENTE) * corriente2Anterior);
  corriente2Anterior = corrienteQ2;
}

void actualizarPantallaOled() {
  display.clearDisplay();
  display.setTextSize(1);
  // Fila 0: IP Loca
  display.setCursor(0, 0);
  display.print("");
  display.print("");
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
  display.print(estadoVent1);
  display.setCursor(64, 32);
  display.print("V2: ");
  display.print(estadoVent2);
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

void enviarTelemetriaSerial() {
  // Transmisión estructurada tipo CSV por la interfaz serie
  segundos = millis() / 1000.0f;

  Serial.print("#Temp1");
  Serial.println(temp1);
  
  Serial.print("#Temp2");
  Serial.println(temp2);
  
  Serial.print("#Corri1");
  Serial.println(corrienteQ1);
  
  Serial.print("#Corri2");
  Serial.println(corrienteQ2);
  
  Serial.print("#Tiempo");
  Serial.println(segundos);

}