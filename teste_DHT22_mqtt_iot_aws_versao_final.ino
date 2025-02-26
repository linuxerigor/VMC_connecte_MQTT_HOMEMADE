#include "CarneiroDHT.h"

void setup() {

  pinMode(RELAYPIN, OUTPUT);       // définit la broche du relais en sortie
  pinMode(RELAYTOTALPIN, OUTPUT);  // définit la broche du relais en sortie
  
  digitalWrite(RELAYPIN, 0);       // 0 = rele desligado, turbo desligado
  digitalWrite(RELAYTOTALPIN, 0);  // 0 = rele desligado, VMC LIGADO

  pinMode(DHTPINP, OUTPUT);
  digitalWrite(DHTPINP, 1);  

  Serial.begin(115200);
  dht.begin();

  Serial.println();
  Serial.println();
  Serial.println();

 

  WiFi.begin(STASSID, STAPSK);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  ArduinoOTA.begin();


  Serial.printf("variacao_umidade: %d\n", variacao_umidade);
  Serial.printf("intervaloLeituravariacao: %d\n\n", intervaloLeituravariacao);

  // Inicializa o cliente NTP
  timeClient.begin();
  timeClient.setTimeOffset(timeZone * 3600);  // Define o fuso horário


  //#########################
  // Configura o cliente MQTT (ESP8266)
  //  espClientForMQTT.setTrustAnchors(new BearSSL::X509List(rootCACert));         // CA Root
  //  espClientForMQTT.setClientRSACert(new BearSSL::X509List(clientCert),     // Certificado do cliente
  //                       new BearSSL::PrivateKey(privateKey));  // Chave privada

  // Configura o certificado raiz SSL/TLS (ESP32)
  espClientForMQTT.setCACert(rootCACert);
  espClientForMQTT.setCertificate(clientCert);  // Certificado do cliente
  espClientForMQTT.setPrivateKey(privateKey);   // Chave privada do cliente
                                                //#########################

  // Configura o servidor MQTT
  mqttClient.setServer(MQTTSERVER, MQTTPORT);

  mqttClient.setKeepAlive(60);      // Tempo em segundos
  mqttClient.setSocketTimeout(10);  // 10 segundos

  // Configura o tópico para assinatura
  mqttClient.setCallback(callback);

  // Exemplo de JSON para inicialização
  String json = R"(
  {
    "t": [
      {"h": 23, "m": 30, "d": -1, "a": 1},
      {"h": 6, "m": 30, "d": -1, "a": 0}
    ]
  })";
  carregarTarefasJson(json);  // Carrega as tarefas iniciais

  readDHT22();
  umidadeAnterior = h;
  tolerancia_anterior = h;
}

void loop() {
  // wait for WiFi connection
  

  if ((WiFi.status() != WL_CONNECTED)) {
    Serial.println("Reconnecting to WIFI network");
    WiFi.disconnect();
    WiFi.reconnect();
    delay(2000);
    return;
  }

  ArduinoOTA.handle();

  verificarHorarioDesligarLiga();

  readDHT22();
  // Verifica se a leitura é válida
  if (isnan(h)) {
    Serial.println("Falha na leitura do sensor DHT!");
    return;  // Sai da função loop() até a próxima leitura
  }

  // Atualiza o valor anterior da umidade
  if (millis() - tempoAnteriorvariacao > intervaloLeituravariacao) {
    Serial.println("Verifica variação brusca ...");
    tempoAnteriorvariacao = millis();
    if ((h - umidadeAnterior) > variacao_umidade) {
      Serial.printf("Variação brusca detectada! (%f - %f) > %d \n", h, umidadeAnterior, variacao_umidade);
      ligadoturbo(1);
      ativarauto = 1;
      tolerancia_anterior = umidadeAnterior;  // * 1.01;
    }
    umidadeAnterior = h;
  }


  if (ativarauto && h <= (tolerancia_anterior)) {
    Serial.println("Umidade retornour patamar anterior!");
    ligadoturbo(0);
    tolerancia_anterior = h;
    ativarauto = 0;
  }
  ////////////////////////////////////////////////////////

  // Conecta-se ao broker MQTT
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  // Processa mensagens MQTT
  mqttClient.loop();

  sendMQTT();
}
