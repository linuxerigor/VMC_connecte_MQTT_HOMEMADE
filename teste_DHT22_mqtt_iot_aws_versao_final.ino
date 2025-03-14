#include "CarneiroDHT.h"
#include "esp_task_wdt.h"

#define WDT_TIMEOUT 5  // Tempo limite do watchdog (em segundos)

void setup() {

  pinMode(RELAYPIN, OUTPUT);       // définit la broche du relais en sortie
  pinMode(RELAYTOTALPIN, OUTPUT);  // définit la broche du relais en sortie
  
  digitalWrite(RELAYPIN, 0);       // 0 = rele desligado, turbo desligado
  digitalWrite(RELAYTOTALPIN, 0);  // 0 = rele desligado, VMC LIGADO

  pinMode(DHTPINP, OUTPUT);
  digitalWrite(DHTPINP, 1);  

  Serial.begin(115200);

    // Configura o Watchdog Timer (WDT)
    esp_task_wdt_config_t wdtConfig = {
        .timeout_ms = WDT_TIMEOUT * 1000,  // Converte segundos para milissegundos
        .idle_core_mask = 0,  // Desabilita monitoramento de núcleos ociosos
        .trigger_panic = true,  // Reinicia o ESP32 se travar
    };

    // Inicializa o watchdog corretamente
    esp_task_wdt_init(&wdtConfig);
    esp_task_wdt_add(NULL);  // Adiciona a tarefa principal ao watchdog

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
      {"h": 6, "m": 30, "d": -1, "a": 0},
      {"h": 10, "m": 00, "d": -1, "a": 2}
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
    delay(5000);
    return;
  }

  ArduinoOTA.handle();

  readDHT22();

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

  esp_task_wdt_reset(); // Watchdog
}
