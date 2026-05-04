//red hogar
const char* ssid="_MERY_";
const char* pass="Integrado741";
//red usco
const char* ssid1="Usco_Ingenieria_Plus";
const char* pass1="";

//red movil
const char* ssid2="Cesar191";
const char* pass2="RocioAndres23";

//                                      //hogar        //usco          //movil
const char* broker="192.168.1.4";     //192.168.1.4    172.16.66.231   10.99.200.233
const int puerto=1883;
const char* user="";
const char* mq_pass="";
const char* esp_name="cesar";



// topicos de enviar datos
const char* data_temp1="test/sensor/temperatura1";
const char* data_temp2="test/sensor/temperatura2";
const char* data_IQ1="test/sensor/corrienteQ1";
const char* data_IQ2="test/sensor/corrienteQ2";
const char* data_time="test/sensor/tiempo";
//topicos de recibir información
const char* msg_pwm1="test/datos/pwm1";
const char* msg_pwm2="test/datos/pwm2";
const char* msg_led1="test/datos/led1";
const char* msg_led2="test/datos/led2";
const char* msg_ventilador1="test/datos/ventilador1";
const char* msg_ventilador2="test/datos/ventilador2";
