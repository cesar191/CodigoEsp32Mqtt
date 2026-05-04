/*
  Pines que se usan sin problema del wifi
    13(o),14(o),16(x)x,17(x)x,18(o),19(o),21(o),22(o),23(o),25(o),26(o),27(o),32(o),33(o)
  pines PWM:
  - d13 para el Q1
  - d12 para el Q2-->16
  - d35 para el ventilador 1-->32
  - d34 para el ventilador  2-->33
  sensor:
  - D14 para Temperatura 1
  - D27 para Temperatura 2
  alarma:
  -d26 led1
  -d25 led2
  corriente analoga:
  -d35 Q1
  -d34 Q2
  I2C:
  -d21 SDA
  -d22 SCL
  SPI:
  -d23 MOSI
  -d19 MISO
  -d18 SCK
  -d15 SS-->17
  
*/
//librerias para el wifi y mqtt
  #include <WiFi.h>
  #include <WiFiMulti.h>
  #include <MQTT.h>
  #include "data.h"
//libreriras para ds18b20
  #include <OneWire.h>                  
  #include <DallasTemperature.h>
//cliente mqtt 
  WiFiMulti wifiMulti;
  WiFiClient net;
  MQTTClient clienteMQTT;

  unsigned long tiempoAnt = 0;

//establece los pines para los sensores
  OneWire ourWire1(14);                
  OneWire ourWire2(27);               
//se declara las variables para los sensores
  DallasTemperature sensors1(&ourWire1); 
  DallasTemperature sensors2(&ourWire2);
//corriente
  int corriente1=35;
  int corriente2=34;
//alarmas
  const int led1=26;
  const int led2=25;
  const int ventilador1=33;
  const int ventilador2=32;
  String estadoled1="off";
  String estadoled2="off";
  String estadoventilador1="off";
  String estadoventilador2="off";
//pwm pines solo tiene 0-15 canales de pwm
  const int q1=13;
  const int q2=12;
//parametros pwm
  const int frecuencia=100000;
  const int resolucion=8;
  int dutyCycle1=0;
  int dutyCycle2=0;
//parametros para el filtro de media exponencial
  float temp1;
  float temp2;
  int resolucionTemperatura=12;

  float corrienteQ1;
  float corrienteQ2;
  float segundos;

  float temperatura1Anterior=0;
  float temperatura2Anterior=0;
  float corriente1Anterior=0;
  float corriente2Anterior=0;
  const float alpha=0.8;
//

//funciones para mqtt
  void MensajeMQTT(String topic, String payload) {
    if(topic==msg_pwm1){
      dutyCycle1=payload.toInt();
    }
    if(topic==msg_pwm2){
      dutyCycle2=payload.toInt();
    }
    if(topic==msg_led1){
      estadoled1=payload;
      if(payload=="on"){
        digitalWrite(led1,HIGH);
      }else if(payload=="off"){
        digitalWrite(led1,LOW);
      }else{
        
      }
    } 
    if(topic==msg_led2){
      estadoled2=payload;
      if(payload=="on"){
        digitalWrite(led2,HIGH);
      }else if(payload=="off"){
        digitalWrite(led2,LOW);
      }
    } 
    if(topic==msg_ventilador1){
      estadoventilador1=payload;
      if(payload=="on"){
        digitalWrite(ventilador1,HIGH);
      }
      if(payload=="off"){
        digitalWrite(ventilador1,LOW);
      }
    } 
    if(topic==msg_ventilador2){
      estadoventilador2=payload;
      if(payload=="on"){
        digitalWrite(ventilador2,HIGH);
      }
      if(payload=="off"){
        digitalWrite(ventilador2,LOW);
      }
    } 
  }

  void conectar() {
    while (wifiMulti.run() != WL_CONNECTED) {
    delay(500);
    }
    // while (!client.connect(NombreESP, "public", "public")) {
    while (!clienteMQTT.connect(esp_name)) {
      delay(500);
    }
    clienteMQTT.subscribe("test/datos/#");
  }


void setup() {
    //Serial.begin(115200);
    wifiMulti.addAP(ssid, pass),
    //wifiMulti.addAP(ssid1, pass1);
    wifiMulti.addAP(ssid2, pass2);

    WiFi.mode(WIFI_STA);

    clienteMQTT.begin(broker, net);
    clienteMQTT.onMessage(MensajeMQTT);

    conectar();
  //iniciamos los sensores
    sensors1.begin();
    sensors1.setResolution(resolucionTemperatura);   
    sensors2.begin(); 
    sensors2.setResolution(resolucionTemperatura);
  // Configurar pwm
    ledcAttach(q1,frecuencia,resolucion);
    ledcAttach(q2,frecuencia,resolucion);
    ledcAttach(ventilador1,frecuencia,resolucion);
    ledcAttach(ventilador2,frecuencia,resolucion);
  //alarmas  
    pinMode(led1,OUTPUT);
    pinMode(led2,OUTPUT);
    pinMode(ventilador1,OUTPUT);
    pinMode(ventilador2,OUTPUT); 
  
}

void loop() {
  clienteMQTT.loop();
  delay(10);
  if (!clienteMQTT.connected()) {
    conectar();
  }

  //prueba
   //PWM
    ledcWrite(q1, dutyCycle1*2.55);
    ledcWrite(q2, dutyCycle2*2.55);
        
    //sensor
    //mediciones del los sensores
      sensors1.requestTemperatures();  
      temp1= sensors1.getTempCByIndex(0)*alpha+(1-alpha)*temperatura1Anterior;
      temperatura1Anterior=temp1; 

      sensors2.requestTemperatures();   //Se envía el comando para leer la temperatura
      temp2= sensors2.getTempCByIndex(0)*alpha+(1-alpha)*temperatura2Anterior; //Se obtiene la temperatura en ºC del sensor 2
      temperatura2Anterior=temp2; 
      
      segundos=millis()/1000;

      int Adc1=analogRead(corriente1);
      corrienteQ1=(((Adc1*3.3)/4096)+0.1)*alpha+(1-alpha)*corriente1Anterior;
      corriente1Anterior=corrienteQ1;

      int Adc2=analogRead(corriente2);
      corrienteQ2=(((Adc2*3.3)/4096)+0.1)*alpha+(1-alpha)*corriente2Anterior;
      corriente2Anterior=corrienteQ2;
  //fin prueba


  if (millis() - tiempoAnt >= 1000) {
    tiempoAnt = millis(); 
  //enviar datos a mqtt 
      clienteMQTT.publish(data_temp1, String(temp1));
      clienteMQTT.publish(data_temp2, String(temp2));
      clienteMQTT.publish(data_IQ1, String(corrienteQ1));
      clienteMQTT.publish(data_IQ2, String(corrienteQ2));
      clienteMQTT.publish(data_time, String(segundos));
    
  } 
}