#ifndef CONFIG_H
#define CONFIG_H


#ifndef STASSID
#define STASSID "xxxxxxx"
#define STAPSK "xxxxx"
#endif


// Endereço IP do broker MQTT
#define MQTTSERVER "xxxxxxxx.iot.us-east-1.amazonaws.com"  // "44.211.26.171"
#define MQTTPORT 8883                                                //1883
#define MQTTUSER "admin"
#define MQTTPASS "xxxxx"

// Certificados e chave (copie os certificados para cá como string)
const char* rootCACert = R"EOF(
-----BEGIN CERTIFICATE-----
-----END CERTIFICATE-----
)EOF";

const char* clientCert = R"KEY(
-----BEGIN CERTIFICATE-----
-----END CERTIFICATE-----
)KEY";

const char* privateKey = R"KEY(
-----BEGIN RSA PRIVATE KEY-----
-----END RSA PRIVATE KEY-----
)KEY";

const char* mqttTopic = "xxxxxxx/vmc";  // Tópico MQTT

#endif // CONFIG_H