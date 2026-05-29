#include "CarneiroDHT.h"

void setup() {
  pinMode(RELAYPIN, OUTPUT);
  pinMode(RELAYTOTALPIN, OUTPUT);

  digitalWrite(RELAYPIN, 0);      // turbo OFF
  digitalWrite(RELAYTOTALPIN, 0); // VMC OFF

  pinMode(DHTPINP, OUTPUT);
  digitalWrite(DHTPINP, 1); // Alimentation DHT active

  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== Demarrage ===");

  // Heap libre au démarrage — utile pour détecter les fuites mémoire
  Serial.printf("Heap libre: %d bytes\n", esp_get_free_heap_size());

  // Watchdog : 30 s pour couvrir les délais SSL/TLS et NTP
  esp_task_wdt_config_t wdtConfig = {
    .timeout_ms    = WDT_TIMEOUT * 1000,
    .idle_core_mask = 0,
    .trigger_panic  = true,
  };
  esp_task_wdt_init(&wdtConfig);
  esp_task_wdt_add(NULL);

  dht.begin();

  // Connexion WiFi
  Serial.print("Connexion WiFi");
  WiFi.begin(STASSID, STAPSK);
  while (WiFi.status() != WL_CONNECTED) {
    esp_task_wdt_reset();
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\nWiFi connecte, IP : %s\n", WiFi.localIP().toString().c_str());
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);

  // Tâches planifiées
  adicionarTarefa(30, 23, -1, 1); // VMC ON  à 23h30
  adicionarTarefa(30,  6, -1, 0); // VMC OFF à 6h30
  adicionarTarefa(30,  9, -1, 2); // Reboot  à 9h30
  adicionarTarefa(30, 12, -1, 2); // Reboot  à 12h30
  adicionarTarefa(30, 15, -1, 2); // Reboot  à 15h30
  Serial.println("Taches planifiees ajoutees");

  // NTP
  timeClient.begin();
  timeClient.setTimeOffset(timeZone * 3600);
  esp_task_wdt_reset();

  // SSL/TLS pour MQTT (AWS IoT)
  Serial.println("Configuration SSL...");
  espClientForMQTT.setCACert(rootCACert);
  espClientForMQTT.setCertificate(clientCert);
  espClientForMQTT.setPrivateKey(privateKey);
  esp_task_wdt_reset();

  // MQTT
  mqttClient.setServer(MQTTSERVER, MQTTPORT);
  mqttClient.setKeepAlive(60);
  mqttClient.setSocketTimeout(15);
  mqttClient.setCallback(callback);

  // Première lecture DHT
  readDHT22();
  umidadeAnterior    = h;
  tolerancia_anterior = h;

  esp_task_wdt_reset();
  Serial.printf("Setup termine. Heap libre: %d bytes\n", esp_get_free_heap_size());
}

void loop() {

  // --- Reconnexion WiFi non bloquante ---
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi perdu, reconnexion...");
    WiFi.disconnect();
    WiFi.begin(STASSID, STAPSK);
    for (int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++) {
      esp_task_wdt_reset();
      delay(500);
    }
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi toujours indisponible");
    }
    return;
  }

  // --- Reconnexion MQTT si nécessaire ---
  if (!mqttClient.connected()) {
    reconnectMQTT();
    esp_task_wdt_reset();
  }

  mqttClient.loop();

  // --- Cycle principal toutes les `interval` ms ---
  if (millis() - millisprecedent >= (unsigned long)interval) {
    millisprecedent = millis();

    // Affiche la heap à chaque cycle pour surveiller les fuites mémoire
    Serial.printf("--- Cycle  Heap libre: %d bytes ---\n", esp_get_free_heap_size());

    readDHT22();

    verificarHorarioDesligarLiga();
    esp_task_wdt_reset();

    // Contrôle de variation d'humidité toutes les 150 s
    millisactuel = millis();
    if ((millisactuel - millisprecedentvariation) >= 150000UL) {
      millisprecedentvariation = millisactuel;
      majhumiditeprecedent();
    }

    // Retour au niveau d'humidité normal → arrêt turbo automatique
    if (ativarauto && h <= tolerancia_anterior) {
      Serial.println("Humidite revenue, arret turbo");
      ligadoturbo(0);
      tolerancia_anterior = h;
      ativarauto = 0;
    }

    sendMQTT();
    Serial.println("--- Fin cycle ---");
  }

  esp_task_wdt_reset();
}
