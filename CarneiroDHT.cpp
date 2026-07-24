#include "CarneiroDHT.h"

int variacao_umidade = 4;
int interval = 15000;
unsigned long millisactuel = 0;
unsigned long millisprecedent = 0;
unsigned long millisprecedentvariation = 0;
unsigned long millisLastNTP = 0;   // Timestamp de la dernière mise à jour NTP réussie

float tolerancia_anterior = 0.0;
float umidadeAnterior = 0.0;

int dhtErrorCount = 0;
int mqttRetryCount = 0;

// Actions différées depuis le callback — évite la récursion dans mqttClient.loop()
volatile int pendingTurbo = -1;
volatile int pendingVMC   = -1;

std::vector<Tarefa> tarefas;

#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

int ativar = 0;
int ativarauto = 0;
int estadovmc = 0;
int estadoturbo = 0;

float t = 0.0;
float h = 0.0;

WiFiClientSecure espClientForMQTT;
PubSubClient mqttClient(espClientForMQTT);

WiFiUDP ntpUDP;
// Intervalle NTP mis à 0 ici — on gère nous-mêmes la fréquence de mise à jour
// pour éviter les blocages UDP imprévisibles
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3600, 0);
int timeZone = 1;

// ---------------------------------------------------------------------------

void ligadoturbo(int on) {
  ativar = on;
  estadoturbo = on;
  Serial.printf("Rele ligado turbo = %d\n", on);
  digitalWrite(RELAYPIN, on);

  // Si turbo ON et VMC ON → éteindre VMC (sans récursion : appel direct)
  if (on == 1 && estadovmc == 1) {
    digitalWrite(RELAYTOTALPIN, 0);
    estadovmc = 0;
    Serial.println("VMC eteint pour turbo");
  }

  sendMQTT();
}

void ligadovmc(int on) {
  digitalWrite(RELAYTOTALPIN, on);
  estadovmc = on;
  Serial.printf("Rele ligado VMC = %d\n", on);

  // Si VMC ON et turbo ON → éteindre turbo (sans récursion : appel direct)
  if (on == 1 && estadoturbo == 1) {
    digitalWrite(RELAYPIN, 0);
    ativar = 0;
    estadoturbo = 0;
    Serial.println("Turbo eteint pour VMC");
  }

  sendMQTT();
}

// ---------------------------------------------------------------------------

void getdate(char* buffer, int tamanho) {
  unsigned long rawTime = timeClient.getEpochTime();
  time_t now = (time_t)rawTime;
  struct tm* timeInfo = localtime(&now);
  strftime(buffer, tamanho, "%d/%m/%Y %H:%M:%S", timeInfo);
}

void sendMQTT() {
  if (!mqttClient.connected()) {
    Serial.println("sendMQTT: non connecte, ignore");
    return;
  }


  char topic[60];
  snprintf(topic, sizeof(topic), "%s/data", mqttTopic);

  char message[150];
  snprintf(message, sizeof(message),
    "{\"h\":\"%d\",\"t\":\"%d\",\"estadovmc\":\"%d\",\"estadoturbo\":\"%d\"}",
    (int)h, (int)t, estadovmc, estadoturbo);

  if (mqttClient.publish(topic, message)) {
    Serial.printf("publish %s = %s\n", topic, message);
  } else {
    Serial.println("Echec publish MQTT");
  }
}

// ---------------------------------------------------------------------------

void adicionarTarefa(int minuto, int hora, int dia, int acao) {
  Tarefa novaTarefa = { minuto, hora, dia, acao, false };
  tarefas.push_back(novaTarefa);
}

void verificarHorarioDesligarLiga() {
  // Mise à jour NTP toutes les NTP_UPDATE_INTERVAL_MS seulement
  // pour éviter un blocage UDP aléatoire à chaque cycle de 15s
  unsigned long now = millis();
  if (now - millisLastNTP >= NTP_UPDATE_INTERVAL_MS || millisLastNTP == 0) {
    bool ok = timeClient.forceUpdate();  // timeout interne de 1s
    if (ok) {
      millisLastNTP = now;
      Serial.println("NTP mis a jour");
    } else {
      Serial.println("NTP timeout, on garde l heure precedente");
      // Pas de blocage : on continue avec l'heure mémorisée
    }
  }

  int horaAtual   = timeClient.getHours();
  int minutoAtual = timeClient.getMinutes();
  int diaAtual    = timeClient.getDay();

  for (size_t i = 0; i < tarefas.size(); i++) {
    Tarefa& tarefa = tarefas[i];

    bool horaValida   = (tarefa.hora   == -1 || tarefa.hora   == horaAtual);
    bool minutoValido = (tarefa.minuto == -1 || tarefa.minuto == minutoAtual);
    bool diaValido    = (tarefa.dia    == -1 || tarefa.dia    == diaAtual);

    if (horaValida && minutoValido && diaValido && !tarefa.executadaHoje) {
      executarTarefaHorarioDesligarLiga(horaAtual, minutoAtual, tarefa.acao);
      tarefa.executadaHoje = true;
    }

    if (!horaValida || !minutoValido) {
      tarefa.executadaHoje = false;
    }
  }
}

void executarTarefaHorarioDesligarLiga(int horaAtual, int minutoAtual, int on) {
  Serial.printf("executarTarefa hora=%d min=%d acao=%d\n", horaAtual, minutoAtual, on);
  if (on == 0 || on == 1) ligadovmc(on);
  if (on == 2) { Serial.println("Redemarrage planifie"); esp_restart(); }
}

// ---------------------------------------------------------------------------

void majhumiditeprecedent() {
  if ((h - umidadeAnterior) > variacao_umidade) {
    Serial.printf("Variation brusque! (%.1f - %.1f) > %d\n", h, umidadeAnterior, variacao_umidade);
    ligadoturbo(1);
    ativarauto = 1;
    tolerancia_anterior = umidadeAnterior + 1.0;
  }
  umidadeAnterior = h;
}

void readDHT22() {
  delay(100);
  float newT = dht.readTemperature();
  float newH = dht.readHumidity();

  if (isnan(newT) || isnan(newH)) {
    dhtErrorCount++;
    Serial.printf("Erreur DHT22 (%d/%d)\n", dhtErrorCount, DHT_MAX_ERRORS);
    restartDHT22();
    if (dhtErrorCount >= DHT_MAX_ERRORS) {
      Serial.println("Trop d erreurs DHT, redemarrage");
      esp_restart();
    }
    return;
  }

  dhtErrorCount = 0;
  t = newT;
  h = newH;
  Serial.printf("DHT OK  T=%.1f  H=%.1f\n", t, h);
}

void restartDHT22() {
  digitalWrite(DHTPINP, LOW);
  delay(500);
  digitalWrite(DHTPINP, HIGH);
  delay(500);
  Serial.println("Alimentation DHT cyclee");
}

// ---------------------------------------------------------------------------

void reconnectMQTT() {
  if (mqttRetryCount >= MQTT_MAX_RETRIES) {
    Serial.println("Trop d echecs MQTT, redemarrage");
    esp_restart();
  }

  Serial.printf("Tentative MQTT (%d/%d)...\n", mqttRetryCount + 1, MQTT_MAX_RETRIES);

  char clientID[30];
  snprintf(clientID, sizeof(clientID), "ESP32-%llX", ESP.getEfuseMac());

  if (mqttClient.connect(clientID)) {
    Serial.println("Connecte au broker MQTT");
    mqttRetryCount = 0;

    char topic[60];
    snprintf(topic, sizeof(topic), "%s/ligadoturbo", mqttTopic);
    mqttClient.subscribe(topic);
    snprintf(topic, sizeof(topic), "%s/ligadovmc", mqttTopic);
    mqttClient.subscribe(topic);
    snprintf(topic, sizeof(topic), "%s/config", mqttTopic);
    mqttClient.subscribe(topic);
  } else {
    mqttRetryCount++;
    Serial.printf("Echec MQTT rc=%d (%d/%d)\n", mqttClient.state(), mqttRetryCount, MQTT_MAX_RETRIES);
  }
}

// ---------------------------------------------------------------------------
// Traitement différé des actions reçues par MQTT callback
// Appelé dans loop(), HORS de mqttClient.loop() → pas de récursion possible
void processPendingActions() {
  if (pendingTurbo >= 0) {
    int val = pendingTurbo;
    pendingTurbo = -1;  // reset avant l'appel pour éviter double exécution
    ligadoturbo(val);
  }
  if (pendingVMC >= 0) {
    int val = pendingVMC;
    pendingVMC = -1;
    ligadovmc(val);
  }
}

// Le callback ne fait QUE mémoriser l'action demandée — aucun appel réseau ici
void callback(char* topic, byte* payload, unsigned int length) {
  char msg[8] = {0};
  size_t len = length < sizeof(msg) - 1 ? length : sizeof(msg) - 1;
  memcpy(msg, payload, len);

  Serial.printf("Message recu [%s] : %s\n", topic, msg);

  char topicTurbo[60], topicVMC[60];
  snprintf(topicTurbo, sizeof(topicTurbo), "%s/ligadoturbo", mqttTopic);
  snprintf(topicVMC,   sizeof(topicVMC),   "%s/ligadovmc",   mqttTopic);

  if (strcmp(topic, topicTurbo) == 0) {
    if      (msg[0] == '1') pendingTurbo = 1;
    else if (msg[0] == '0') pendingTurbo = 0;
  }

  if (strcmp(topic, topicVMC) == 0) {
    if      (msg[0] == '1') pendingVMC = 1;
    else if (msg[0] == '0') pendingVMC = 0;
  }
}