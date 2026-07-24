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
  Serial.printf("Heap libre au boot: %d bytes\n", esp_get_free_heap_size());

  // Watchdog 30s
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
  Serial.printf("\nWiFi connecte IP: %s\n", WiFi.localIP().toString().c_str());
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);

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
  Serial.printf("=== Setup termine. Heap: %d bytes ===\n", esp_get_free_heap_size());
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
      Serial.println("WiFi indisponible, prochain cycle");
    }
    return;
  }

  // --- Reconnexion MQTT si nécessaire ---
  if (!mqttClient.connected()) {
    reconnectMQTT();
    esp_task_wdt_reset();
    return; // on attend le prochain tour de loop pour envoyer
  }

  // --- Traitement des actions MQTT reçues (hors callback) ---
  // Doit être appelé AVANT mqttClient.loop() pour traiter le cycle précédent,
  // et APRÈS pour traiter le cycle actuel — on le met après pour simplicité
  mqttClient.loop();
  processPendingActions();  // ← ici, on est hors de mqttClient.loop(), pas de récursion

  // --- Cycle principal toutes les `interval` ms ---
  if (millis() - millisprecedent >= (unsigned long)interval) {
    millisprecedent = millis();

    Serial.printf("--- Cycle  Heap: %d bytes ---\n", esp_get_free_heap_size());

    readDHT22();
    esp_task_wdt_reset();

    // Contrôle variation humidité toutes les 150s
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
