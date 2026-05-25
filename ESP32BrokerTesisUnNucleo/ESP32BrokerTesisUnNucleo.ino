// librerias y variables
  //librerias para el wifi y mqtt
    #include <WiFi.h>
    #include <WiFiMulti.h>
    #include <PicoMQTT.h>
    #include "data.h"
  //libreriras para ds18b20
    #include <OneWire.h>                  
    #include <DallasTemperature.h>
  //libreias i2c y oled
    #include <SPI.h>//para cuando se usa mosi, miso, clk, ss en el caso del sd
    #include <Wire.h>
    #include <Adafruit_GFX.h>
    #include <Adafruit_SSD1306.h>
  //broker
    WiFiMulti wifiMulti;
    WiFiClient net;
    PicoMQTT::Server mqtt;
    unsigned long tiempoAnt = 0;
  //establece los pines para los sensores
    OneWire ourWire1(14);                
    OneWire ourWire2(27);              
  //se declara las variables para los sensores
    DallasTemperature sensors1(&ourWire1);
    DallasTemperature sensors2(&ourWire2);
  //se declara la pantalla oled
    Adafruit_SSD1306 display = Adafruit_SSD1306(128, 64, &Wire);
  //corriente
    int corriente1=35;
    int corriente2=34;
  //alarmas
    const int led1=26;
    const int led2=25;
    String estadoled1="off";
    String estadoled2="off";
  //ventiladores
    const int ventilador1=33;
    const int ventilador2=32;
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
    mqtt.subscribe("test/datos/#",[](const char* topic,const char* payload){
      MensajeMQTT(topic,payload);
    });
  }
//oled
void pantallaOled(){

    //imprimir mensajes oled poe renglon en el tamaño de letra minimo es de 8 pixeles
    display.clearDisplay();         // Borra la pantalla
    //mostrar ip
    display.setTextSize(1);        
    display.setCursor(0,0);       // (X,Y) . (Horizontal, Vertical)
    display.print("IP: ");  
    display.print(WiFi.localIP()); // es con al ip con la que conecta a la red, no la del servidor mqtt
    // mostrar mediciones del sensor
    display.setCursor(0,8);      
    display.print("T1: ");  
    display.print(temp1);
    display.setCursor(64,8);      
    display.print("T2: ");
    display.print(temp2);
    //mostrar PWM
    display.setCursor(0,16);       // (X,Y) . (Horizontal, Vertical)
    display.print("Q1: ");  
    display.print(dutyCycle1);
    display.setCursor(64,16);       // (X,Y) . (Horizontal, Vertical)
    display.print("Q2: ");  
    display.print(dutyCycle2);
    //mostar led
    display.setCursor(0,24);       // (X,Y) . (Horizontal, Vertical)
    display.print("L1: ");
    display.print(estadoled1);
    display.setCursor(64,24);       // (X,Y) . (Horizontal, Vertical)
    display.print("L2: ");  
    display.print(estadoled2);
    //mostar ventilador
    display.setCursor(0,32);       // (X,Y) . (Horizontal, Vertical)
    display.print("V1: ");  
    display.print(estadoventilador1);
    display.setCursor(64,32);       // (X,Y) . (Horizontal, Vertical)
    display.print("V2: ");  
    display.print(estadoventilador2);
    //mostar corriente
    display.setCursor(0,40);       // (X,Y) . (Horizontal, Vertical)
    display.print("I1: ");  
    display.print(corrienteQ1);
    display.setCursor(64,40);       // (X,Y) . (Horizontal, Vertical)
    display.print("I2: ");  
    display.print(corrienteQ2);
    //tiempo
    display.setCursor(0,48);       // (X,Y) . (Horizontal, Vertical)
    display.print("tiempo: ");  
    display.print(segundos);
    display.display();              // Muestra el Texto en la pantalla
}

void setup() {
  // put your setup code here, to run once:
  //Serial.begin(115200);
    wifiMulti.addAP(ssid, pass),
    //wifiMulti.addAP(ssid1, pass1);
    wifiMulti.addAP(ssid2, pass2);
    WiFi.mode(WIFI_STA);
  //
    conectar();
    mqtt.begin();
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
  //incialisamos la pantalla oled
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // Otra direccion es la 0x3D
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
}
void loop() {
  // put your main code here, to run repeatedly:
   mqtt.loop();
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
      mqtt.publish(data_temp1, String(temp1));
      mqtt.publish(data_temp2, String(temp2));
      mqtt.publish(data_IQ1, String(corrienteQ1));
      mqtt.publish(data_IQ2, String(corrienteQ2));
      mqtt.publish(data_time, String(segundos));
  }
  //imprimir datos en la pantalla oled
     pantallaOled();
} 