
#include "CarneiroDHT.h"

int variacao_umidade = 4;               // Variação brusca (em porcentagem)
int intervaloLeituravariacao = 150000;  //60000; // Tempo entre leituras

float tolerancia_anterior = 0.0;  // umidade inicial
float umidadeAnterior = 0.0;      // Armazena a última leitura de umidade
unsigned long tempoAnteriorvariacao = 0;
unsigned long tempoAnteriorreadDHT22 = 0;


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
  snprintf(message, sizeof(message), "{\"h\":\"%d\",\"t\":\"%d\",\"estadovmc\":\"%d\",\"estadoturbo\":\"%d\",\"dt\":\"%s\"}", (int)h, (int)t, estadovmc, estadoturbo,buffer);
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
  if (on == 0||on == 1) {
     ligadovmc(on);
  }
  if (on == 2 ) {
     esp_restart(); 
  }
}

void readDHT22() {
  if (millis() - tempoAnteriorreadDHT22 > 2000) {

        verificarHorarioDesligarLiga();

        tempoAnteriorreadDHT22 = millis();
        float newT = dht.readTemperature(), newH = dht.readHumidity();
        if (!isnan(newT)) t = newT;
        if (!isnan(newH)) h = newH;
      
        if (isnan(newT)){
          digitalWrite(DHTPINP, LOW);  
          delay(1000);  
          digitalWrite(DHTPINP, HIGH);  
          delay(1000);  
          Serial.println("Error lendo DHT, reiniciando");
        }
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

  if (String(doc["chave"].as<String>()).equals("variacao_umidade")) {
    variacao_umidade = doc["valor"];
    Serial.printf("salvando config na memoria : %d\n", variacao_umidade);
  } else if (String(doc["chave"].as<String>()).equals("intervaloLeituravariacao")) {
    intervaloLeituravariacao = doc["valor"];
    Serial.printf("salvando config na memoria : %d\n", intervaloLeituravariacao);
  } else if (String(doc["chave"].as<String>()).equals("t")) {
    carregarTarefasJson(doc["valor"]);  // Carrega as tarefas iniciais
  }

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
