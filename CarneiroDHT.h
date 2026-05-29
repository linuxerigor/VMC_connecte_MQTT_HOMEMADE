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
#include <esp_system.h>
#include <esp_task_wdt.h>

#include "config.h"

#define DHTPIN 23
#define DHTPINP 14
#define RELAYPIN 16
#define RELAYTOTALPIN 17

// WDT: 30s, largement suffisant pour couvrir les délais SSL/TLS et NTP
#define WDT_TIMEOUT 30

// Nombre maximum d'échecs DHT consécutifs avant de redémarrer
#define DHT_MAX_ERRORS 5

// Nombre maximum d'échecs MQTT consécutifs avant de redémarrer
#define MQTT_MAX_RETRIES 3

extern DHT dht;

extern int variacao_umidade;
extern int interval;
extern float tolerancia_anterior;
extern float umidadeAnterior;
extern unsigned long millisactuel;
extern unsigned long millisprecedent;
extern unsigned long millisprecedentvariation;

extern int dhtErrorCount;
extern int mqttRetryCount;

struct Tarefa {
  int minuto;
  int hora;
  int dia;
  int acao;
  bool executadaHoje;
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
void majhumiditeprecedent();
void readDHT22();
void restartDHT22();
void reconnectMQTT();
void callback(char* topic, byte* payload, unsigned int length);
