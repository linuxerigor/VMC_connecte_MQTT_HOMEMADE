#include <stdio.h>
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
#include <vector>         // Para vetor dinâmico

#include "config.h"

#define DHTPIN 4          // Digital pin connected to the DHT sensor
#define RELAYPIN 16       // relais forte
#define RELAYTOTALPIN 17  // relais total

int variacao_umidade = 4;               // Variação brusca (em porcentagem)
int intervaloLeituravariacao = 150000;  //60000; // Tempo entre leituras

float tolerancia_anterior = 0.0;  // umidade inicial
float umidadeAnterior = 0.0;      // Armazena a última leitura de umidade
unsigned long tempoAnteriorvariacao = 0;

// Estrutura para as tarefas agendadas
struct Tarefa {
  int minuto;          // Minuto (0-59 ou -1 para todos os minutos)
  int hora;            // Hora (0-23 ou -1 para todas as horas)
  int dia;             // Dia do mês (1-31 ou -1 para todos os dias)
  int acao;            // Função a ser executada
  bool executadaHoje;  // Controle para evitar execuções repetidas
};

// Vetor dinâmico para armazenar as tarefas
std::vector<Tarefa> tarefas;

// Uncomment the type of sensor in use:
//#define DHTTYPE    DHT11     // DHT 11
#define DHTTYPE DHT22  // DHT 22 (AM2302)
DHT dht(DHTPIN, DHTTYPE);


unsigned long tempoAnteriormqtt = 0;  // Variável para armazenar o último tempo registrado
unsigned long intervalomqtt = 30000;  // Intervalo de 1 segundo (1000 milissegundos) send MQTT

int ativar = 0;
int ativarauto = 0;

int estadovmc = 0;
int estadoturbo = 0;

// current temperature & humidity, updated in loop()
float t = 0.0;
float h = 0.0;
float hl = 0.0;

WiFiClientSecure espClientForMQTT;  // wIth SSL ESP32

PubSubClient mqttClient(espClientForMQTT);

// Configuração do cliente NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3600, 60000);  // 3600 = UTC+1
int timeZone = 1; // Configuração do fuso horário

void ligadoturbo(int on) {
  ativar = on;
  estadoturbo = on;
  Serial.println("Rele ligado turbo =" + String(on));
  digitalWrite(RELAYPIN, on);  // allume le relais

  if (on == 1 && estadovmc == 1) {
    ligadovmc(!on);
  }

  MQTT();
}

void ligadovmc(int on) {
  digitalWrite(RELAYTOTALPIN, on);
  estadovmc = on;
  Serial.println("Rele ligado VMC = " + String(on));  // allume le relais

  if (on == 1 && estadoturbo == 1) {
    ligadoturbo(0);
  }

  MQTT();
}

void sendMQTT() {
  if (millis() - tempoAnteriormqtt >= intervalomqtt) {
    // Ação a ser realizada após o intervalo (1 segundo)
    tempoAnteriormqtt = millis();  // Atualiza o último tempo registrado
    if (hl != h) {
      hl = h;
      MQTT();
    }
  }
}

void getdate(char* buffer, int tamanho) {
    timeClient.update(); // Atualiza o tempo do NTP

    unsigned long rawTime = timeClient.getEpochTime(); // Obtém o timestamp UNIX
    struct tm *timeInfo;
    time_t now = rawTime;
    timeInfo = localtime(&now); // Converte para formato de data e hora local

    strftime(buffer, tamanho, "%d/%m/%Y %H:%M:%S", timeInfo);

}

void MQTT() {

  char buffer[50];
  getdate(buffer, sizeof(buffer));

  // Publicar mensagem
  char topic[50];
  snprintf(topic, sizeof(topic), "%s/data", mqttTopic);
  char message[100];
  snprintf(message, sizeof(message), "{\"h\":\"%d\",\"t\":\"%d\",\"estadovmc\":\"%d\",\"estadoturbo\":\"%d\",\"datetime\":\"%s\"}", (int)h, (int)t, estadovmc, estadoturbo,buffer);
  mqttClient.publish(topic, message);
  Serial.println("publicado " + String(topic) + " = " + String(message));
}

// Função para imprimir as tarefas (debug)
void imprimirTarefas() {
  char topic[50];
  snprintf(topic, sizeof(topic), "%s/data", mqttTopic);
  char message[100];

  Serial.println("Lista de Tarefas:");
  for (int i = 0; i < tarefas.size(); i++) {
    Serial.print("Tarefa ");
    Serial.print(i);
    Serial.print(": Hora = ");
    Serial.print(tarefas[i].hora);
    Serial.print(", Minuto = ");
    Serial.print(tarefas[i].minuto);
    Serial.print(", Dia = ");
    Serial.println(tarefas[i].dia);
    // Publicar mensagem

    snprintf(message, sizeof(message), "Tarefa : Hora = %d, Minuto = %d, Dia = %d, Acao = %d", tarefas[i].hora, tarefas[i].minuto, tarefas[i].dia, tarefas[i].acao);
    mqttClient.publish(topic, message);
    Serial.println("publicado " + String(topic) + " = " + String(message));
  }
}


// Função para adicionar tarefas dinamicamente
void adicionarTarefa(int minuto, int hora, int dia, int acao) {
  Tarefa novaTarefa = { minuto, hora, dia, acao, false };
  tarefas.push_back(novaTarefa);  // Adiciona ao vetor
}

// Função para limpar todas as tarefas
void limparTarefas() {
  tarefas.clear();  // Remove todas as tarefas do vetor
}

// Função para carregar tarefas do JSON
void carregarTarefasJson(const String& jsonString) {
  // Cria o buffer JSON
  StaticJsonDocument<1024> doc;

  // Analisa o JSON
  DeserializationError erro = deserializeJson(doc, jsonString);
  if (erro) {
    Serial.print("Erro ao processar JSON: ");
    Serial.println(erro.c_str());
    return;
  }

  // Limpa as tarefas atuais
  limparTarefas();

  // Percorre o array JSON
  JsonArray array = doc["t"].as<JsonArray>();
  for (JsonObject obj : array) {
    int hora = obj["h"];
    int minuto = obj["m"];
    int dia = obj["d"];
    int acao = obj["a"];  // Define qual ação será executada
    adicionarTarefa(minuto, hora, dia, acao);
  }
  Serial.println("Tarefas carregadas!");
  imprimirTarefas();
}

void setup() {

  pinMode(RELAYPIN, OUTPUT);       // définit la broche du relais en sortie
  pinMode(RELAYTOTALPIN, OUTPUT);  // définit la broche du relais en sortie

  digitalWrite(RELAYPIN, 0);       // 0 = rele desligado, turbo desligado
  digitalWrite(RELAYTOTALPIN, 0);  // 0 = rele desligado, VMC LIGADO

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



  //GET config EEPROM
  EEPROM.get(0, variacao_umidade);                          // Lê o valor da EEPROM
  EEPROM.get((0 + sizeof(int)), intervaloLeituravariacao);  // Lê o valor da EEPROM

  Serial.println("\nRecuperando config EEPROM.\n");
  Serial.printf("variacao_umidade: %d\n", variacao_umidade);
  Serial.printf("intervaloLeituravariacao: %d\n\n", intervaloLeituravariacao);

  if (variacao_umidade == -1 || variacao_umidade == 0) {
    Serial.println("Valor inválido na EEPROM. Salvando o valor padrão.");
    variacao_umidade = 20;
    EEPROM.put(0, variacao_umidade);
  }

  if (intervaloLeituravariacao == -1 || intervaloLeituravariacao == 0) {
    Serial.println("Valor inválido na EEPROM. Salvando o valor padrão.");
    intervaloLeituravariacao = 60000;
    EEPROM.put((0 + sizeof(int)), intervaloLeituravariacao);
  }


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


void verificarHorarioDesligarLiga() {

  timeClient.update();

  // Obtém o horário atual
  int horaAtual = timeClient.getHours();
  int minutoAtual = timeClient.getMinutes();
  int diaAtual = timeClient.getDay();  // Pode ser ajustado para data completa se necessário

  // Verifica as tarefas agendadas
  for (int i = 0; i < tarefas.size(); i++) {
    Tarefa& t = tarefas[i];  // Referência para a tarefa atual

    // Verifica se o horário corresponde à tarefa
    bool horaValida = (t.hora == -1 || t.hora == horaAtual);
    bool minutoValido = (t.minuto == -1 || t.minuto == minutoAtual);
    bool diaValido = (t.dia == -1 || t.dia == diaAtual);

    if (horaValida && minutoValido && diaValido && !t.executadaHoje) {
      executarTarefaHorarioDesligarLiga(horaAtual, minutoAtual, t.acao);
      t.executadaHoje = true;  // Marca como executada
    }

    // Reinicia o controle após passar do horário
    if (!horaValida || !minutoValido) {
      t.executadaHoje = false;
    }
  }
}

void executarTarefaHorarioDesligarLiga(int horaAtual, int minutoAtual, int on) {
  // Exibe o horário no monitor serial
  Serial.print("Horário atual: ");
  Serial.print(horaAtual);
  Serial.print(":");
  Serial.println(minutoAtual);
  ligadovmc(on);
}

void readDHT22() {

  // Read temperature as Celsius (the default)
  float newT = dht.readTemperature();
  if (isnan(newT)) {
    Serial.println("Failed to read from DHT sensor!");
  } else {
    t = newT;
  }
  // Read Humidity
  float newH = dht.readHumidity();
  // if humidity read failed, don't change h value
  if (isnan(newH)) {
    Serial.println("Failed to read from DHT sensor!");
  } else {
    h = newH;
  }
}


void reconnectMQTT() {
  // Tenta reconectar ao broker MQTT
  Serial.print("Tentando conexão MQTT...");
  // Cria um ID aleatório para o cliente
  String clientID = "ESP8266-" + String(WiFi.macAddress());
  //if (mqttClient.connect(clientID.c_str(),MQTTUSER,MQTTPASS)) {
  if (mqttClient.connect(clientID.c_str())) {  // with SSL
    Serial.println("Conectado ao broker MQTT");
    // Se conecta a um tópico
    char topic[50];
    snprintf(topic, sizeof(topic), "%s/ligadoturbo", mqttTopic);
    mqttClient.subscribe(topic);
    snprintf(topic, sizeof(topic), "%s/ligadovmc", mqttTopic);
    mqttClient.subscribe(topic);
    snprintf(topic, sizeof(topic), "%s/config", mqttTopic);
    mqttClient.subscribe(topic);
  } else {
    Serial.print("Falha, rc=");
    Serial.println(mqttClient.state());
    delay(5000);
  }
}

void readconfig(String mensagem) {

  Serial.print("Config recebido no tópico  ");
  Serial.print("Payload: ");
  Serial.println(mensagem);

  // Faz o parse do JSON
  StaticJsonDocument<200> doc;  // Define tamanho máximo do JSON
  DeserializationError error = deserializeJson(doc, mensagem);

  if (error) {
    Serial.print("Erro ao interpretar JSON: ");
    Serial.println(error.c_str());
    return;  // Sai da função em caso de erro
  }

  if (String(doc["chave"]).equals("variacao_umidade")) {
    variacao_umidade = doc["valor"];
    EEPROM.put(0, variacao_umidade);  // Salva na EEPROM
    Serial.printf("salvando config na memoria : %d\n", variacao_umidade);
  } else if (String(doc["chave"]).equals("intervaloLeituravariacao")) {
    intervaloLeituravariacao = doc["valor"];
    EEPROM.put((0 + sizeof(int)), intervaloLeituravariacao);  // Salva na EEPROM
    Serial.printf("salvando config na memoria : %d\n", intervaloLeituravariacao);
  } else if (String(doc["chave"]).equals("t")) {
    carregarTarefasJson(doc["valor"]);  // Carrega as tarefas iniciais
  }

  //GET config EEPROM
  EEPROM.get(0, variacao_umidade);                          // Lê o valor da EEPROM
  EEPROM.get((0 + sizeof(int)), intervaloLeituravariacao);  // Lê o valor da EEPROM

  Serial.println("\nRecuperando config EEPROM.\n");
  Serial.printf("variacao_umidade: %d\n", variacao_umidade);
  Serial.printf("intervaloLeituravariacao: %d\n\n", intervaloLeituravariacao);
}

// Função de callback para tratar as mensagens recebidas
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensagem recebida em [");
  Serial.print(topic);
  Serial.print("] ");

  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Se receber uma mensagem, executa alguma ação (exemplo: ligar uma luz)
  if (String(topic) == String(mqttTopic) + "/ligadoturbo") {
    if ((char)payload[0] == '1') {

      ligadoturbo(1);

    } else if ((char)payload[0] == '0') {

      ligadoturbo(0);
    }
  }

  if (String(topic) == String(mqttTopic) + "/ligadovmc") {
    if ((char)payload[0] == '1') {

      ligadovmc(1);

    } else if ((char)payload[0] == '0') {

      ligadovmc(0);
    }
  }


  if (String(topic) == String(mqttTopic) + "/config") {
    String mensagem = "";
    for (int i = 0; i < length; i++) {
      mensagem += (char)payload[i];
    }

    readconfig(mensagem);
  }
}
