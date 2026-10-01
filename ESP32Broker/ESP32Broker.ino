//esp32 como broker
#include <PicoMQTT.h>

#define WIFI_SSID  "_MERY_"
#define WIFI_PASSWORD  "Integrado741"

PicoMQTT::Server mqtt;

void setup() {
 //serial
Serial.begin(115200);
//conexion a red
WiFi.mode(WIFI_STA);
WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
while(WiFi.status() != WL_CONNECTED) { delay(1000); }
Serial.printf("Wifi conectado, IP: %s\n",WiFi.localIP().toString().c_str());

//broker que recibe todos los topicos
mqtt.subscribe("#",[](const char* topic, const char* payload){
  Serial.printf("Mensaje recibido topico '%s': %s\n",topic,payload);
});
mqtt.begin();

}



void loop() {
  // put your main code here, to run repeatedly:
  mqtt.loop();
  if(random(1000)==0){
    mqtt.publish("picomqtt/welcome","Hola desde un broker en ESP32");
  }
}
