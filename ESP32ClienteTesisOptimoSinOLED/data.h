#ifndef DATA_H
#define DATA_H

// Redes Wi-Fi
constexpr char SSID_HOGAR[] = "_MERY_";
constexpr char PASS_HOGAR[] = "Integrado741";

constexpr char SSID_USCO[]  = "Usco_Ingenieria_Plus";
constexpr char PASS_USCO[]  = "";

constexpr char SSID_MOVIL[] = "Cesar191";
constexpr char PASS_MOVIL[] = "RocioAndres23";

// Servidor MQTT
constexpr char BROKER_MQTT[]     = "192.168.100.72";
constexpr uint16_t PUERTO_MQTT   = 1883;
constexpr char USER_MQTT[]       = "";
constexpr char PASS_MQTT[]       = "";
constexpr char ESP_CLIENT_NAME[] = "cesar";

// Tópicos MQTT - Publicación
constexpr char TOPIC_PUB_TEMP1[] = "test/sensor/temperatura1";
constexpr char TOPIC_PUB_TEMP2[] = "test/sensor/temperatura2";
constexpr char TOPIC_PUB_IQ1[]   = "test/sensor/corrienteQ1";
constexpr char TOPIC_PUB_IQ2[]   = "test/sensor/corrienteQ2";
constexpr char TOPIC_PUB_TIME[]  = "test/sensor/tiempo";

// Tópicos MQTT - Suscripción
constexpr char TOPIC_SUB_PWM1[]  = "test/datos/pwm1";
constexpr char TOPIC_SUB_PWM2[]  = "test/datos/pwm2";
constexpr char TOPIC_SUB_LED1[]  = "test/datos/led1";
constexpr char TOPIC_SUB_LED2[]  = "test/datos/led2";
constexpr char TOPIC_SUB_VENT1[] = "test/datos/ventilador1";
constexpr char TOPIC_SUB_VENT2[] = "test/datos/ventilador2";

#endif