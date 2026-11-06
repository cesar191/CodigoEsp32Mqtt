// librerias
    #include "data.h"

    #include <Adafruit_GFX.h>
    #include <Adafruit_SSD1306.h>
    
    #include <DallasTemperature.h>
    #include <OneWire.h>
    
    #include <PubSubClient.h>
    #include <WiFi.h>
    #include <WiFiClient.h>
    #include <WiFiClientSecure.h>
    #include <WiFiMulti.h>
    #include <Wire.h>

    #include <SD.h>
    #include <SPI.h>
    
// Configuración actual en uso
    String current_ssid = SSID_HOGAR;
    String current_password = PASS_HOGAR;
    String current_broker = BROKER_MQTT;
    int current_port = PUERTO_MQTT;
    String current_mqtt_user = USER_MQTT;
    String current_mqtt_pass = PASS_MQTT;
    bool is_secure_connection = false;

// Certificados (si se requiere TLS)
    String amazonRootCA = "";
    String certificatePem = "";
    String privateKey = "";

// Redes
    WiFiMulti wifiMulti;
    WiFiClient espClientPlano;
    WiFiClientSecure espClientSeguro;
    PubSubClient mqttClient;

// Pines 
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

    constexpr uint32_t FREC_PWM_Q1 = 1000;
    constexpr uint32_t FREC_PWM_Q2 = 100000;
    constexpr uint32_t FREC_PWM_VENT1 = 10;
    constexpr uint32_t FREC_PWM_VENT2 = 5;
    constexpr uint8_t RESOLUCION_PWM = 8;
    constexpr uint8_t RESOLUCION_TEMP = 9;

    constexpr float ALPHA_TEMP = 0.8f;
    constexpr float ALPHA_CORRIENTE = 0.05f;
// variables sensor y SPI/I2C
    OneWire wireTemp1(PIN_TEMP_1);
    OneWire wireTemp2(PIN_TEMP_2);
    DallasTemperature sensorTemp1(&wireTemp1);
    DallasTemperature sensorTemp2(&wireTemp2);

    const int SD_CS_PIN = 15;
    Adafruit_SSD1306 display(128, 64, &Wire);
    bool oledConectada = false;

// Variables de estado
    uint32_t tiempoAnteriorMs = 0;
    uint32_t tiempoAnterior2Ms = 0;
    uint32_t segundos = 0;
    uint32_t ultimaOledUpdate = 0;

    uint8_t dutyCycleQ1 = 0;
    uint8_t dutyCycleQ2 = 0;
    uint8_t dutyCycleVent1 = 0;
    uint8_t dutyCycleVent2 = 0;

    String estadoLed1 = "off";
    String estadoLed2 = "off";
    String estadoVentilador1 = "off";
    String estadoVentilador2 = "off";

    float temp1 = 0.0f, temp2 = 0.0f, corrienteQ1 = 0.0f, corrienteQ2 = 0.0f;
    float temp1Anterior = 0.0f, temp2Anterior = 0.0f, corriente1Anterior = 0.0f,
        corriente2Anterior = 0.0f;

// Función utilitaria leer archivo SD
    String readFile(fs::FS &fs, const char *path) {
    File file = fs.open(path);
    if (!file || file.isDirectory()) {
        return String();
    }
    String fileContent;
    while (file.available()) {
        fileContent += String((char)file.read());
    }
    file.close();
    fileContent.trim();
    return fileContent;
    }
// Callback MQTT
    void mqttCallback(char *topicStr, byte *payloadBytes, unsigned int length) {
    String topic = String(topicStr);
    String payload = "";
    for (unsigned int i = 0; i < length; i++) {
        payload += (char)payloadBytes[i];
    }
    //PWM
    if (topic == TOPIC_SUB_PWM1) {
        dutyCycleQ1 = payload.toInt();
        ledcWrite(PIN_PWM_Q1, dutyCycleQ1 * 2.55f);
    } else if (topic == TOPIC_SUB_PWM2) {
        dutyCycleQ2 = payload.toInt();
        ledcWrite(PIN_PWM_Q2, dutyCycleQ2 * 2.55f);
    } 
    //LED
    else if (topic == TOPIC_SUB_LED1) {
        estadoLed1 = payload;
        if(payload=="on"){
            digitalWrite(PIN_LED1,HIGH);
        }else if(payload=="off"){
            digitalWrite(PIN_LED1,LOW);
        }
        
    } else if (topic == TOPIC_SUB_LED2) {
        estadoLed2 = payload;
        if(payload=="on"){
            digitalWrite(PIN_LED2,HIGH);
        }else if(payload=="off"){
            digitalWrite(PIN_LED2,LOW);
        }
    } else if (topic == TOPIC_SUB_VENT1) {
        estadoVentilador1 = payload;
        if (payload == "on"){
            ledcWrite(PIN_VENTILADOR1, 255);
        }
        else if (payload == "off"){
            ledcWrite(PIN_VENTILADOR1, 0);
        }
        else {
            dutyCycleVent1 = payload.toInt();
            ledcWrite(PIN_VENTILADOR1, dutyCycleVent1 * 2.55f);
        }
    } else if (topic == TOPIC_SUB_VENT2) {
        estadoVentilador2 = payload;
        if (payload == "on"){
            ledcWrite(PIN_VENTILADOR2, 255);
        }
        else if (payload == "off"){
            ledcWrite(PIN_VENTILADOR2, 0);
        }
        else {
            dutyCycleVent2 = payload.toInt();
            ledcWrite(PIN_VENTILADOR2, dutyCycleVent2 * 2.55f);
        }
    }
    }

// Función leer configuración SD
    void leerConfiguracionSD() {
    if (SD.begin(SD_CS_PIN)) {   
        String sd_ssid = readFile(SD, "/ssid.txt");
        String sd_pass = readFile(SD, "/password.txt");
        String sd_broker = readFile(SD, "/broker.txt");
        String sd_port = readFile(SD, "/port.txt");
        String sd_muser = readFile(SD, "/mqtt_user.txt");
        String sd_mpass = readFile(SD, "/mqtt_pass.txt");
        if (sd_ssid.length() > 0)
        current_ssid = sd_ssid;
        if (sd_pass.length() > 0)
        current_password = sd_pass;
        if (sd_broker.length() > 0)
        current_broker = sd_broker;
        if (sd_port.length() > 0)
        current_port = sd_port.toInt();
        if (sd_muser.length() > 0)
        current_mqtt_user = sd_muser;
        if (sd_mpass.length() > 0)
        current_mqtt_pass = sd_mpass;

        if (current_port != 1883) {
        
        is_secure_connection = true;
        amazonRootCA = readFile(SD, "/RootCA.pem");
        certificatePem = readFile(SD, "/Cert.crt");
        privateKey = readFile(SD, "/Private.key");
        } else {
        is_secure_connection = false;
        }
    } else {
        current_port = PUERTO_MQTT;
        current_broker = BROKER_MQTT;
        is_secure_connection = false;
    }
    }

void setup() {
  
  // Intentar iniciar OLED 
  if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    oledConectada = true;
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print("Iniciando...");
    display.display();
  }

  // Configuración hardware
  sensorTemp1.begin();
  sensorTemp1.setResolution(RESOLUCION_TEMP);
  sensorTemp2.begin();
  sensorTemp2.setResolution(RESOLUCION_TEMP);

  ledcAttach(PIN_PWM_Q1, FREC_PWM_Q1, RESOLUCION_PWM);
  ledcAttach(PIN_PWM_Q2, FREC_PWM_Q2, RESOLUCION_PWM);
  ledcAttach(PIN_VENTILADOR1, FREC_PWM_VENT1, RESOLUCION_PWM);
  ledcAttach(PIN_VENTILADOR2, FREC_PWM_VENT2, RESOLUCION_PWM);
  pinMode(PIN_LED1, OUTPUT);
  pinMode(PIN_LED2, OUTPUT);

  // Leer y aplicar configuración
  leerConfiguracionSD();

  // Configurar WiFi
  wifiMulti.addAP(current_ssid.c_str(), current_password.c_str());
  wifiMulti.addAP(SSID_HOGAR, PASS_HOGAR);
  wifiMulti.addAP(SSID_MOVIL, PASS_MOVIL);

  // Configurar cliente MQTT
  if (is_secure_connection) {
    espClientSeguro.setCACert(amazonRootCA.c_str());
    espClientSeguro.setCertificate(certificatePem.c_str());
    espClientSeguro.setPrivateKey(privateKey.c_str());
    mqttClient.setClient(espClientSeguro);
  } else {
    mqttClient.setClient(espClientPlano);
  }

  mqttClient.setServer(current_broker.c_str(), current_port);
  mqttClient.setCallback(mqttCallback);
}

// Función no bloqueante para conectar a WiFi y MQTT
void mantenerConexion() {
  if (wifiMulti.run() != WL_CONNECTED) {
    return; // Aún no hay WiFi
  }

  if (!mqttClient.connected()) {
    if (mqttClient.connect(ESP_CLIENT_NAME, current_mqtt_user.c_str(), current_mqtt_pass.c_str())) {
      mqttClient.subscribe("test/datos/#");
    }
  }
}
// oled
void actualizarOLED() {
  if (!oledConectada){
    return;
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Mostrar vista de red por 1 segundo cada 5 segundos
  if (millis() % 5000 < 2000) {
    display.setCursor(0, 8);
    display.print("--- RED Y BROKER ---");
    display.setCursor(0, 16);
    if (WiFi.status() == WL_CONNECTED) {
      display.print("WiFi: ");
      display.println(current_ssid);

      display.setCursor(0, 24);
      display.print("IP: ");
      display.println(current_broker);

      display.setCursor(0, 32);
      display.print("Puerto: ");
      display.println(current_port == 1883 ? "1883 (Plano)"
                                           : String(current_port) + " (TLS)");
    } else {
      display.print("Estado: SIN CONEXION");
    }
  } else {
    // Vista de sensores (Igual que el repositorio original)
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
  }

  display.display();
}

void loop() {
  // Manejo de conexiones

  if (mqttClient.connected()) {
    mqttClient.loop();
  }else{
    mantenerConexion();
  }

  // Lecturas
  sensorTemp1.requestTemperatures();
  temp1 = (sensorTemp1.getTempCByIndex(0) * ALPHA_TEMP) + ((1.0f - ALPHA_TEMP) * temp1Anterior);
  temp1Anterior = temp1;

  sensorTemp2.requestTemperatures();
  temp2 = (sensorTemp2.getTempCByIndex(0) * ALPHA_TEMP) + ((1.0f - ALPHA_TEMP) * temp2Anterior);
  temp2Anterior = temp2;

  const int adc1 = analogRead(PIN_ADC_CORRIENTE1);
  corrienteQ1 = (((adc1 * 3.3f) / 4096.0f) * ALPHA_CORRIENTE) + ((1.0f - ALPHA_CORRIENTE) * corriente1Anterior);
  corriente1Anterior = corrienteQ1;

  const int adc2 = analogRead(PIN_ADC_CORRIENTE2);
  corrienteQ2 = (((adc2 * 3.3f) / 4096.0f) * ALPHA_CORRIENTE) + ((1.0f - ALPHA_CORRIENTE) * corriente2Anterior);
  corriente2Anterior = corrienteQ2;
  // Envío cada 1 segundo
  if (millis() - tiempoAnterior2Ms >= 1000) {
    segundos = millis() / 1000.0f;
    tiempoAnterior2Ms = millis();

    if (mqttClient.connected()) {
      mqttClient.publish(TOPIC_PUB_TEMP1, String(temp1).c_str());
      mqttClient.publish(TOPIC_PUB_TEMP2, String(temp2).c_str());
      mqttClient.publish(TOPIC_PUB_IQ1, String(corrienteQ1).c_str());
      mqttClient.publish(TOPIC_PUB_IQ2, String(corrienteQ2).c_str());
      mqttClient.publish(TOPIC_PUB_TIME, String(segundos).c_str());
    }
  }

  // Actualizar OLED si está conectada (cada 2 segundo)
  if (millis() - ultimaOledUpdate >= 1000) {
    ultimaOledUpdate = millis();
    actualizarOLED();
  }
}
