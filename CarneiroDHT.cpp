#include "CarneiroDHT.h"

int variacao_umidade = 4;                // Variation (en percentage)
int interval = 15000;                   // en millseconds
unsigned long millisactuel = 0;
unsigned long millisprecedent = 0;
unsigned long millisprecedentvariation = 0;

//--------------------------------------------------------------------------

float tolerancia_anterior = 0.0;  // umidade inicial
float umidadeAnterior = 0.0;      // Armazena a última leitura de umidade

// Vetor dinâmico para armazenar as tarefas
std::vector<Tarefa> tarefas;

// Uncomment the type of sensor in use:
//#define DHTTYPE    DHT11     // DHT 11
#define DHTTYPE DHT22  // DHT 22 (AM2302)
DHT dht(DHTPIN, DHTTYPE);


int ativar = 0;
int ativarauto = 0;

int estadovmc = 0;
int estadoturbo = 0;

// current temperature & humidity, updated in loop()
float t = 0.0;
float h = 0.0;


WiFiClientSecure espClientForMQTT;  // wIth SSL ESP32

PubSubClient mqttClient(espClientForMQTT);

WebServer server(80);

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

  sendMQTT();
}

void ligadovmc(int on) {
  digitalWrite(RELAYTOTALPIN, on);
  estadovmc = on;
  Serial.println("Rele ligado VMC = " + String(on));  // allume le relais

  if (on == 1 && estadoturbo == 1) {
    ligadoturbo(0);
  }

  sendMQTT();
}



void getdate(char* buffer, int tamanho) {
    timeClient.update(); // Atualiza o tempo do NTP

    unsigned long rawTime = timeClient.getEpochTime(); // Obtém o timestamp UNIX
    struct tm *timeInfo;
    time_t now = rawTime;
    timeInfo = localtime(&now); // Converte para formato de data e hora local

    strftime(buffer, tamanho, "%d/%m/%Y %H:%M:%S", timeInfo);

}

void sendMQTT() {
  char buffer[50];
  getdate(buffer, sizeof(buffer));

  // Publicar mensagem
  char topic[50];
  snprintf(topic, sizeof(topic), "%s/data", mqttTopic);
  char message[100];
  snprintf(message, sizeof(message), "{\"h\":\"%d\",\"t\":\"%d\",\"estadovmc\":\"%d\",\"estadoturbo\":\"%d\",\"d\":\"%s\"}", (int)h, (int)t, estadovmc, estadoturbo,buffer);
  mqttClient.publish(topic, message);
  Serial.println("publish " + String(topic) + " = " + String(message));

}


// Função para adicionar tarefas dinamicamente
void adicionarTarefa(int minuto, int hora, int dia, int acao) {
  Tarefa novaTarefa = { minuto, hora, dia, acao, false };
  tarefas.push_back(novaTarefa);  
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
  Serial.println("executarTarefaHorarioDesligarLiga");

  if (on == 0||on == 1) {
     ligadovmc(on);
  }
  if (on == 2 ) {
     esp_restart(); 
  }
  
}



void majhumiditeprecedent() {
      // Atualiza o valor anterior da umidade
    if ((h - umidadeAnterior) > variacao_umidade) {
      Serial.printf("Variação brusca detectada! (%f - %f) > %d \n", h, umidadeAnterior, variacao_umidade);
      ligadoturbo(1);
      ativarauto = 1;
      tolerancia_anterior = umidadeAnterior + 1.0 ; // *1.01;
      Serial.printf("(tolerancia_anterior = %f  umidadeAnterior = %f)\n", tolerancia_anterior, umidadeAnterior);
    }
    umidadeAnterior = h;
}

void readDHT22() {
        
    float newT = dht.readTemperature(), newH = dht.readHumidity();
      
    if (isnan(newT)){
      restartDHT22();
    }

    if (!isnan(newT)) t = newT;
    if (!isnan(newH)) h = newH;

}

void restartDHT22() {
    digitalWrite(DHTPINP, LOW);  
    delay(1000);  
    digitalWrite(DHTPINP, HIGH);  
    delay(1000);  
    Serial.println("Error lendo DHT, reiniciando");
  
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

void startwebserver(){
  Serial.println("startwebserver()");

  if (!MDNS.begin("esp32")) {
    Serial.println("Erro ao iniciar mDNS");
  }

  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html",
      "<form method='POST' action='/update' enctype='multipart/form-data'>"
      "<input type='file' name='update'>"
      "<input type='submit' value='Atualizar'>"
      "</form>");
  });

  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "Falha" : "Sucesso");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Update.begin();
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      Update.write(upload.buf, upload.currentSize);
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) {
        Serial.println("Atualização concluída");
      } else {
        Serial.println("Erro na atualização");
      }
    }
  });

  server.begin();


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


}