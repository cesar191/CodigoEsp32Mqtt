//esp32 como broker
#include <PicoMQTT.h>

#define WIFI_SSID  "_MERY_"
#define WIFI_PASSWORD  "Integrado741"

class SecureBroker: public PicoMQTT::Server {
protected:
  PicoMQTT::ConnectReturnCode auth(const char* client_id, const char* username, const char* password)
  override{
    //1. regla para que el cliente que se conecte no sea un random
    //por ende se pide que el usuario sea mayor a 3 caracteres
    if(String(client_id).length()<3){
      return PicoMQTT::CRC_IDENTIFIER_REJECTED;
    }
    //2. regla para pedir usuario y conraseña
    if(!username || !password){
      return PicoMQTT::CRC_NOT_AUTHORIZED;
    }
    //3. regla de verificación de usuario y contraseña
    const bool ok=
          (String(username) == "usco" && String(password) == "12345678") ||
          (String(username) == "cesar" && String(password) == "1234567890");
    return ok ? PicoMQTT::CRC_ACCEPTED
              : PicoMQTT::CRC_BAD_USERNAME_OR_PASSWORD;

  }
};

SecureBroker mqtt;

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
