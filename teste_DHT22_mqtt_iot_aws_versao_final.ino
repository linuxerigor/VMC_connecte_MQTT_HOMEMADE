#include "CarneiroDHT.h"


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

  // Watchdog
  esp_task_wdt_init(&wdtConfig);
  esp_task_wdt_add(NULL);

  dht.begin();

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
  WiFi.setTxPower(WIFI_POWER_19_5dBm);

  ArduinoOTA.setTimeout(30000);
  ArduinoOTA.begin();

  startwebserver();

  adicionarTarefa(30, 23, -1, 1);
  adicionarTarefa(30, 6, -1, 0);
  adicionarTarefa(30, 9, -1, 2);
  adicionarTarefa(30, 15, -1, 2);
  Serial.println("adicionarTarefa()");

  // Inicializa o cliente NTP
  timeClient.begin();
  timeClient.setTimeOffset(timeZone * 3600);  // Define o fuso horário


  // Configura o certificado raiz SSL/TLS (ESP32)
  Serial.println("espClientForMQTT()");
  espClientForMQTT.setCACert(rootCACert);
  espClientForMQTT.setCertificate(clientCert); 
  espClientForMQTT.setPrivateKey(privateKey);   
                                               
  // Configura o servidor MQTT
  mqttClient.setServer(MQTTSERVER, MQTTPORT);
  mqttClient.setKeepAlive(60); 
  mqttClient.setSocketTimeout(10);
  mqttClient.setCallback(callback);

  readDHT22();
  umidadeAnterior = h;
  tolerancia_anterior = h;

  esp_task_wdt_reset(); // Watchdog
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

  mqttClient.loop();
  
  if (millis() - millisprecedent > interval) {
    
    millisprecedent = millis();
    Serial.println("run ...");

    readDHT22();
    
    verificarHorarioDesligarLiga();

    millisactuel = millis();
    if ((millisactuel - millisprecedentvariation) >= 150000) {
        millisprecedentvariation = millisactuel;
        majhumiditeprecedent();
    }


    if (ativarauto && h <= (tolerancia_anterior)) {
      Serial.println("Umidade retornour patamar anterior!");
      ligadoturbo(0);
      tolerancia_anterior = h;
      ativarauto = 0;
    }

    if (!mqttClient.connected()) {
      reconnectMQTT();
    }

    sendMQTT();

    Serial.println("fin ...");
  }

  server.handleClient();

  esp_task_wdt_reset(); // Watchdog

}