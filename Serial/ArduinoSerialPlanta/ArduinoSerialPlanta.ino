#include <Arduino.h>

// CONFIGURACIÓN DE PINES
constexpr uint8_t PIN_TEMP_1 = A1;
constexpr uint8_t PIN_TEMP_2 = A2;
constexpr uint8_t PIN_ADC_CORRIENTE1 = A3;
constexpr uint8_t PIN_ADC_CORRIENTE2 = A4;

constexpr uint8_t PIN_LED1 = 7;
constexpr uint8_t PIN_LED2 = 8;
constexpr uint8_t PIN_VENTILADOR1 = 5;
constexpr uint8_t PIN_VENTILADOR2 = 6;

constexpr uint8_t PIN_PWM_Q1 = 10; // PWM
constexpr uint8_t PIN_PWM_Q2 = 11; // PWM

// PARÁMETROS DE CONFIGURACIÓN
constexpr float ALPHA_TEMP = 0.8f;
constexpr float ALPHA_CORRIENTE = 0.1f;

// INTERVALOS DE TIEMPO (ms)
constexpr uint32_t INTERVALO_SENSORES = 200;
constexpr uint32_t INTERVALO_SERIAL   = 1000;
uint32_t segundos = 0;

// VARIABLES GLOBALES DE ESTADO Y MEDICIÓN
uint32_t tAnteriorSensores = 0;
uint32_t tAnteriorSerial   = 0;

uint8_t dutyCycleQ1 = 0;
uint8_t dutyCycleQ2 = 0;

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
void enviarTelemetriaSerial();

void setup() {
  Serial.begin(9600);

  pinMode(PIN_LED1, OUTPUT);
  pinMode(PIN_LED2, OUTPUT);
  pinMode(PIN_VENTILADOR1, OUTPUT);
  pinMode(PIN_VENTILADOR2, OUTPUT);
  pinMode(PIN_PWM_Q1, OUTPUT);
  pinMode(PIN_PWM_Q2, OUTPUT);

  digitalWrite(PIN_LED1, LOW);
  digitalWrite(PIN_LED2, LOW);
  digitalWrite(PIN_VENTILADOR1, LOW);
  digitalWrite(PIN_VENTILADOR2, LOW);
  analogWrite(PIN_PWM_Q1, 0);
  analogWrite(PIN_PWM_Q2, 0);
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

  // 3. Tarea de transmisión de telemetría por puerto serie
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
    digitalWrite(PIN_VENTILADOR1, HIGH);
  } else if (strcmp(cmd, "$VENT1off") == 0) {
    estadoVent1 = false;
    digitalWrite(PIN_VENTILADOR1, LOW);
  } else if (strcmp(cmd, "$VENT2on") == 0) {
    estadoVent2 = true;
    digitalWrite(PIN_VENTILADOR2, HIGH);
  } else if (strcmp(cmd, "$VENT2off") == 0) {
    estadoVent2 = false;
    digitalWrite(PIN_VENTILADOR2, LOW);
  } else if (strncmp(cmd, "$PWM1", 5) == 0) {
    int valor = atoi(&cmd[5]);
    dutyCycleQ1 = (uint8_t)constrain(valor, 0, 100);
    uint8_t pwmVal = (uint8_t)map(dutyCycleQ1, 0, 100, 0, 255);
    analogWrite(PIN_PWM_Q1, pwmVal);
  } else if (strncmp(cmd, "$PWM2", 5) == 0) {
    int valor = atoi(&cmd[5]);
    dutyCycleQ2 = (uint8_t)constrain(valor, 0, 100);
    uint8_t pwmVal = (uint8_t)map(dutyCycleQ2, 0, 100, 0, 255);
    analogWrite(PIN_PWM_Q2, pwmVal);
  }
}

void leerSensores() {
  // Lectura LM35 en A1 (conversión a °C con Vref = 5.0V) y filtro IIR
  const int adcT1 = analogRead(PIN_TEMP_1);
  float t1Raw = ((adcT1 * 500.0f) / 1024.0f)/4;
  temp1 = (t1Raw * ALPHA_TEMP) + ((1.0f - ALPHA_TEMP) * temp1Anterior);
  temp1Anterior = temp1;

  // Lectura LM35 en A2
  const int adcT2 = analogRead(PIN_TEMP_2);
  float t2Raw = ((adcT2 * 500.0f) / 1024.0f)/4;
  temp2 = (t2Raw * ALPHA_TEMP) + ((1.0f - ALPHA_TEMP) * temp2Anterior);
  temp2Anterior = temp2;

  // Lectura ADC Corriente en A3 (10 bits, 5.0V ref)
  const int adcI1 = analogRead(PIN_ADC_CORRIENTE1);
  float v1Raw = (adcI1 * 5.0f) / 1024.0f;
  corrienteQ1 = (v1Raw * ALPHA_CORRIENTE) + ((1.0f - ALPHA_CORRIENTE) * corriente1Anterior);
  corriente1Anterior = corrienteQ1;

  // Lectura ADC Corriente en A5
  const int adcI2 = analogRead(PIN_ADC_CORRIENTE2);
  float v2Raw = (adcI2 * 5.0f) / 1024.0f;
  corrienteQ2 = (v2Raw * ALPHA_CORRIENTE) + ((1.0f - ALPHA_CORRIENTE) * corriente2Anterior);
  corriente2Anterior = corrienteQ2;
}

void enviarTelemetriaSerial() {
  segundos = millis() / 1000;

  Serial.print(F("#Temp1"));
  Serial.println(temp1);

  Serial.print(F("#Temp2"));
  Serial.println(temp2);

  Serial.print(F("#Corri1"));
  Serial.println(corrienteQ1);

  Serial.print(F("#Corri2"));
  Serial.println(corrienteQ2);

  Serial.print(F("#Tiempo"));
  Serial.println(segundos);
}