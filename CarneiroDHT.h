

#include <stdio.h>
#include <Arduino.h>
#include <DHT.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>  
#include <Adafruit_Sensor.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>  // Biblioteca para manipulação de JSON
#include <NTPClient.h>    // Biblioteca para sincronizar horário
#include <WiFiUdp.h>      // Biblioteca para UDP
#include <vector>         // Para vetor dinâmico
#include <ArduinoOTA.h>

#include "config.h"

#define DHTPIN 23          // Digital pin connected to the DHT sensor
#define DHTPINP 14          // Digital pin connected to the DHT sensor
#define RELAYPIN 16       // relais forte
#define RELAYTOTALPIN 17  // relais total

extern DHT dht;

extern int variacao_umidade;
extern int intervaloLeituravariacao;
extern float tolerancia_anterior;
extern float umidadeAnterior;
extern unsigned long tempoAnteriorvariacao;
extern unsigned long tempoAnteriorreadDHT22;
extern unsigned long tempoAnteriormqtt;
extern unsigned long intervalomqtt;


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
extern float hl;

extern WiFiClientSecure espClientForMQTT;

extern PubSubClient mqttClient;

extern WiFiUDP ntpUDP;
extern NTPClient timeClient;
extern int timeZone;


void ligadoturbo(int on);

void ligadovmc(int on);

void sendMQTT();

void getdate(char* buffer, int tamanho);

void MQTT();

// Função para imprimir as tarefas (debug)
void imprimirTarefas();


// Função para adicionar tarefas dinamicamente
void adicionarTarefa(int minuto, int hora, int dia, int acao);

// Função para limpar todas as tarefas
void limparTarefas();

// Função para carregar tarefas do JSON
void carregarTarefasJson(const String& jsonString);

void verificarHorarioDesligarLiga();

void executarTarefaHorarioDesligarLiga(int horaAtual, int minutoAtual, int on);

void readDHT22() ;


void reconnectMQTT();

void readconfig(String mensagem) ;

// Função de callback para tratar as mensagens recebidas
void callback(char* topic, byte* payload, unsigned int length);
