#include "CarneiroDHT.h"

int variacao_umidade = 4;
int interval = 15000;
unsigned long millisactuel = 0;
unsigned long millisprecedent = 0;
unsigned long millisprecedentvariation = 0;

float tolerancia_anterior = 0.0;
float umidadeAnterior = 0.0;

int dhtErrorCount = 0;
int mqttRetryCount = 0;

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
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3600, 60000);
int timeZone = 1;

// ---------------------------------------------------------------------------

void ligadoturbo(int on) {
  ativar = on;
  estadoturbo = on;
  // Utilisation de printf au lieu de String() — évite la fragmentation heap
  Serial.printf("Rele ligado turbo = %d\n", on);
  digitalWrite(RELAYPIN, on);

  if (on == 1 && estadovmc == 1) {
    ligadovmc(0);
  }

  sendMQTT();
}

void ligadovmc(int on) {
  digitalWrite(RELAYTOTALPIN, on);
  estadovmc = on;
  Serial.printf("Rele ligado VMC = %d\n", on);

  if (on == 1 && estadoturbo == 1) {
    ligadoturbo(0);
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
    Serial.println("sendMQTT: client non connecte, message ignore");
    return;
  }

  char buffer[50];
  getdate(buffer, sizeof(buffer));

  char topic[60];
  snprintf(topic, sizeof(topic), "%s/data", mqttTopic);

  char message[150];
  snprintf(message, sizeof(message),
           "{\"h\":\"%d\",\"t\":\"%d\",\"estadovmc\":\"%d\",\"estadoturbo\":\"%d\",\"d\":\"%s\",\"heap\":\"%d\"}",
           (int)h, (int)t, estadovmc, estadoturbo, buffer, esp_get_free_heap_size());

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
  // Update NTP — non bloquant si déjà synchronisé (utilise le cache interne)
  timeClient.update();

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

  if (on == 0 || on == 1) {
    ligadovmc(on);
  }
  if (on == 2) {
    Serial.println("Redemarrage planifie");
    esp_restart();
  }
}

// ---------------------------------------------------------------------------

void majhumiditeprecedent() {
  if ((h - umidadeAnterior) > variacao_umidade) {
    Serial.printf("Variation brusque! (%.1f - %.1f) > %d\n", h, umidadeAnterior, variacao_umidade);
    ligadoturbo(1);
    ativarauto = 1;
    tolerancia_anterior = umidadeAnterior + 1.0;
    Serial.printf("tolerancia_anterior=%.1f umidadeAnterior=%.1f\n", tolerancia_anterior, umidadeAnterior);
  }
  umidadeAnterior = h;
}

void readDHT22() {
  // Petite pause recommandée par le datasheet DHT22 entre deux lectures
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
    return; // conserve les dernières valeurs valides
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

  // clientID statique — évite une allocation String à chaque reconnexion
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

void callback(char* topic, byte* payload, unsigned int length) {
  // Buffer local pour éviter de modifier le payload original
  char msg[8] = {0};
  size_t len = length < sizeof(msg) - 1 ? length : sizeof(msg) - 1;
  memcpy(msg, payload, len);

  Serial.printf("Message recu [%s] : %s\n", topic, msg);

  char topicTurbo[60], topicVMC[60];
  snprintf(topicTurbo, sizeof(topicTurbo), "%s/ligadoturbo", mqttTopic);
  snprintf(topicVMC,   sizeof(topicVMC),   "%s/ligadovmc",   mqttTopic);

  if (strcmp(topic, topicTurbo) == 0) {
    if (msg[0] == '1')      ligadoturbo(1);
    else if (msg[0] == '0') ligadoturbo(0);
  }

  if (strcmp(topic, topicVMC) == 0) {
    if (msg[0] == '1')      ligadovmc(1);
    else if (msg[0] == '0') ligadovmc(0);
  }
}
