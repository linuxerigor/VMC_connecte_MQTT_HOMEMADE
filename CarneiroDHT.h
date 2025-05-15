#include <stdio.h>
#include <Arduino.h>
#include <DHT.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>  
#include <Adafruit_Sensor.h>
#include <PubSubClient.h>
#include <NTPClient.h>    
#include <WiFiUdp.h>     
#include <vector>        
#include <ArduinoOTA.h>
#include <esp_system.h>
#include <esp_task_wdt.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>

#include "config.h"

#define DHTPIN 23          // Digital pin connected to the DHT sensor
#define DHTPINP 14          // Digital pin connected to the DHT sensor
#define RELAYPIN 16       // relais forte
#define RELAYTOTALPIN 17  // relais total

#define WDT_TIMEOUT 5

extern DHT dht;

extern int variacao_umidade;
extern int interval;
extern float tolerancia_anterior;
extern float umidadeAnterior;
extern unsigned long millisactuel;
extern unsigned long millisprecedent;
extern unsigned long millisprecedentvariation;

// Estrutura para as tarefas agendadas
struct Tarefa {
  int minuto;          // Minuto (0-59 ou -1 para todos os minutos)
  int hora;            // Hora (0-23 ou -1 para todas as horas)
  int dia;             // Dia do mês (1-31 ou -1 para todos os dias)
  int acao;            // Função a ser executada
  bool executadaHoje;  // Controle para evitar execuções repetidas
};

extern std::vector<Tarefa> tarefas;


extern int ativar;
extern int ativarauto;
extern int estadovmc;
extern int estadoturbo;
extern float t;
extern float h;

extern WiFiClientSecure espClientForMQTT;

extern PubSubClient mqttClient;

extern WebServer server;

extern WiFiUDP ntpUDP;

extern NTPClient timeClient;

extern int timeZone;

void ligadoturbo(int on);

void ligadovmc(int on);

void sendMQTT();

void getdate(char* buffer, int tamanho);

void adicionarTarefa(int minuto, int hora, int dia, int acao);

void verificarHorarioDesligarLiga();

void executarTarefaHorarioDesligarLiga(int horaAtual, int minutoAtual, int on);

void majhumiditeprecedent() ;

void readDHT22() ;

void restartDHT22();

void reconnectMQTT();

void startwebserver();

void callback(char* topic, byte* payload, unsigned int length);