#include <Arduino.h>
#include <EEPROM.h>
#include <DHT.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>  
#include <Adafruit_Sensor.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>  // Biblioteca para manipulação de JSON
#include <NTPClient.h>    // Biblioteca para sincronizar horário
#include <WiFiUdp.h>      // Biblioteca para UDP

#include "config.h"

#define DHTPIN 23          // Digital pin connected to the DHT sensor
#define RELAYPIN 16       // relais forte
#define RELAYTOTALPIN 17  // relais total

#define MAX_TAREFAS 10

int variacao_umidade = 4;               // Variação brusca (em porcentagem)
int intervaloLeituravariacao = 150000;  //60000; // Tempo entre leituras

float tolerancia_anterior = 0.0;  // umidade inicial
float umidadeAnterior = 0.0;      // Armazena a última leitura de umidade
unsigned long tempoAnteriorVariacao = 0;

// Estrutura para as tarefas agendadas
struct Tarefa {
  int minuto;          // Minuto (0-59 ou -1 para todos os minutos)
  int hora;            // Hora (0-23 ou -1 para todas as horas)
  int dia;             // Dia do mês (1-31 ou -1 para todos os dias)
  int acao;            // Função a ser executada
  bool executadaHoje;  // Controle para evitar execuções repetidas
} tarefas[MAX_TAREFAS];


DHT dht(DHTPIN, DHT22);


unsigned long tempoAnteriormqtt = 0;  // Variável para armazenar o último tempo registrado
const unsigned long intervalomqtt = 30000;  // Intervalo de 1 segundo (1000 milissegundos) send MQTT

int ativar = 0, ativarauto = 0, estadovmc = 0, estadoturbo = 0;
float t = 0.0, h = 0.0, hl = 0.0;

WiFiClientSecure espClientForMQTT;  // wIth SSL ESP32

PubSubClient mqttClient(espClientForMQTT);

// Configuração do cliente NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3600, 60000);  // 3600 = UTC+1

void ligarRele(int pin, int estado) {
  digitalWrite(pin, estado);
  Serial.println("ligarRele =" + String(estado));
}

void ligarTurbo(int on) {
  ativar = estadoturbo = on;
  ligarRele(RELAYPIN, on);
  if (on && estadovmc) ligarRele(RELAYTOTALPIN, 0);
  publicarMQTT();
}

void ligarVMC(int on) {
  estadovmc = on;
  ligarRele(RELAYTOTALPIN, on);
  if (on && estadoturbo) ligarTurbo(0);
  publicarMQTT();
}


void publicarMQTT() {
  char buffer[50], message[100], topic[50];
  timeClient.update();
  snprintf(buffer, sizeof(buffer), "%d/%m/%Y %H:%M:%S", timeClient.getEpochTime());

  snprintf(topic, sizeof(topic), "%s/data", mqttTopic);
  snprintf(message, sizeof(message), "{\"h\":%d,\"t\":%d,\"estadovmc\":%d,\"estadoturbo\":%d,\"datetime\":\"%s\"}", 
           (int)h, (int)t, estadovmc, estadoturbo, buffer);
  mqttClient.publish(topic, message);
  Serial.println("publicarMQTT = " + String(message)); 
}

void adicionarTarefa(int minuto, int hora, int dia, int acao) {
  for (int i = 0; i < MAX_TAREFAS; i++) {
    if (tarefas[i].hora == 0 && tarefas[i].minuto == 0) {
      tarefas[i] = {minuto, hora, dia, acao, false};
      break;
    }
  }
}

void carregarTarefasJson(const String& jsonString) {
  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, jsonString)) return;
  
  memset(tarefas, 0, sizeof(tarefas));
  for (JsonObject obj : doc["t"].as<JsonArray>()) {
    adicionarTarefa(obj["m"], obj["h"], obj["d"], obj["a"]);
  }
}

void verificarHorarioDesligarLiga() {
  timeClient.update();
  int horaAtual = timeClient.getHours(), minutoAtual = timeClient.getMinutes(), diaAtual = timeClient.getDay();

  for (Tarefa &t : tarefas) {
    if ((t.hora == -1 || t.hora == horaAtual) && 
        (t.minuto == -1 || t.minuto == minutoAtual) &&
        (t.dia == -1 || t.dia == diaAtual) &&
        !t.executadaHoje) {
      ligarVMC(t.acao);
      t.executadaHoje = true;
    }
    if (t.hora != horaAtual || t.minuto != minutoAtual) t.executadaHoje = false;
  }
}

void readDHT22() {
  float newT = dht.readTemperature(), newH = dht.readHumidity();
  if (!isnan(newT)) t = newT;
  if (!isnan(newH)) h = newH;
}

void reconnectMQTT() {
  Serial.print("Tentando conexão MQTT...");
  if (mqttClient.connect(("ESP32_" + WiFi.macAddress()).c_str())) {
    Serial.println("Conectado!");
    char topic[50];
    snprintf(topic, sizeof(topic), "%s/ligadoturbo", mqttTopic);
    mqttClient.subscribe(topic);
    snprintf(topic, sizeof(topic), "%s/ligadovmc", mqttTopic);
    mqttClient.subscribe(topic);
    snprintf(topic, sizeof(topic), "%s/config", mqttTopic);
    mqttClient.subscribe(topic);
  } else {
    Serial.print("Falha reconnectMQTT: ");
    Serial.println(mqttClient.state());
  }
}


void readconfig(String mensagem) {
  Serial.print("Config recebido no tópico. Payload: ");
  Serial.println(mensagem);

  // Faz o parse do JSON
  StaticJsonDocument<200> doc;  // Define tamanho máximo do JSON
  DeserializationError error = deserializeJson(doc, mensagem);

  if (error) {
    Serial.print("Erro ao interpretar JSON: ");
    Serial.println(error.c_str());
    return;  // Sai da função em caso de erro
  }

  // Obtém a chave e o valor do JSON
  const char* chave = doc["chave"];
  int valor = doc["valor"];

  if (!chave) {
    Serial.println("Erro: chave inválida no JSON.");
    return;
  }

  if (strcmp(chave, "variacao_umidade") == 0) {
    if (variacao_umidade != valor) {  // Só salva se o valor for diferente
      variacao_umidade = valor;
      EEPROM.put(0, variacao_umidade);  
      Serial.printf("Salvando config na memória: %d\n", variacao_umidade);
    }
  } 
  else if (strcmp(chave, "intervaloLeituravariacao") == 0) {
    if (intervaloLeituravariacao != valor) {  // Só salva se o valor for diferente
      intervaloLeituravariacao = valor;
      EEPROM.put(0 + sizeof(int), intervaloLeituravariacao);  
      Serial.printf("Salvando config na memória: %d\n", intervaloLeituravariacao);
    }
  } 
  else if (strcmp(chave, "t") == 0) {
    carregarTarefasJson(doc["valor"]);  // Chama a função se a chave for "t"
  } 
  else {
    Serial.println("Chave desconhecida. Nenhuma ação realizada.");
    return;
  }

  // Recupera os valores da EEPROM após a atualização
  EEPROM.get(0, variacao_umidade);
  EEPROM.get(0 + sizeof(int), intervaloLeituravariacao);

  Serial.println("\nRecuperando config EEPROM:");
  Serial.printf("variacao_umidade: %d\n", variacao_umidade);
  Serial.printf("intervaloLeituravariacao: %d\n\n", intervaloLeituravariacao);
}

// Função de callback otimizada para tratar as mensagens recebidas
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensagem recebida em [");
  Serial.print(topic);
  Serial.print("] ");

  String mensagem = "";
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    mensagem += (char)payload[i];
  }
  Serial.println();

  // Verifica o tópico e executa a ação correspondente
  if (strncmp(topic, (String(mqttTopic) + "/ligadoturbo").c_str(), strlen(topic)) == 0) {
    ligarTurbo(payload[0] == '1' ? 1 : 0);
  } 
  else if (strncmp(topic, (String(mqttTopic) + "/ligadovmc").c_str(), strlen(topic)) == 0) {
    ligarVMC(payload[0] == '1' ? 1 : 0);
  } 
  else if (strncmp(topic, (String(mqttTopic) + "/config").c_str(), strlen(topic)) == 0) {
    readconfig(mensagem);
  }
}

void setup() {
  pinMode(RELAYPIN, OUTPUT);
  pinMode(RELAYTOTALPIN, OUTPUT);
  ligarRele(RELAYPIN, 0);
  ligarRele(RELAYTOTALPIN, 0);
  
  Serial.begin(115200);
  dht.begin();
  WiFi.begin(STASSID, STAPSK);
  while (WiFi.status() != WL_CONNECTED) delay(500);

  Serial.println("Conectado! IP: " + WiFi.localIP().toString());

  EEPROM.get(0, variacao_umidade);
  EEPROM.get(sizeof(int), intervaloLeituravariacao);

  timeClient.begin();
  timeClient.setTimeOffset(3600);

  espClientForMQTT.setCACert(rootCACert);
  espClientForMQTT.setCertificate(clientCert);
  espClientForMQTT.setPrivateKey(privateKey);

  mqttClient.setServer(MQTTSERVER, MQTTPORT);
  mqttClient.setKeepAlive(60);
  mqttClient.setCallback(callback);

  String json = R"({"t":[{"h":23,"m":30,"d":-1,"a":1},{"h":6,"m":30,"d":-1,"a":0}]})";
  carregarTarefasJson(json);
  
  readDHT22();
  umidadeAnterior = h;
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect();
    WiFi.reconnect();
    delay(2000);
    return;
  }

  verificarHorarioDesligarLiga();
  readDHT22();
  
  if (millis() - tempoAnteriorVariacao > intervaloLeituravariacao) {
    tempoAnteriorVariacao = millis();
    if ((h - umidadeAnterior) > variacao_umidade) {
      ligarTurbo(1);
      ativarauto = 1;
      tolerancia_anterior = umidadeAnterior;
    }
    umidadeAnterior = h;
  }

  if (ativarauto && h <= tolerancia_anterior) {
    ligarTurbo(0);
    ativarauto = 0;
  }

  if (!mqttClient.connected()) reconnectMQTT();
  mqttClient.loop();
  
  if (millis() - tempoAnteriormqtt >= intervalomqtt) {
    tempoAnteriormqtt = millis();
    if (hl != h) {
      hl = h;
      publicarMQTT();
    }
  }
}