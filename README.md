# **Projet de Contrôleur VMC IoT avec ESP32 Fait maison**

## **Introduction**

Avec l'avancement de la technologie et la démocratisation de l'Internet des Objets (IoT), de nombreuses solutions innovantes ont vu le jour pour optimiser et automatiser les processus quotidiens. Un exemple notable est le contrôle à distance des systèmes de ventilation, tels que la Ventilation Mécanique Contrôlée (VMC). Ce projet propose un contrôleur VMC DIY (fait maison) utilisant le microcontrôleur ESP32, qui est largement utilisé dans les projets IoT grâce à sa connectivité Wi-Fi et Bluetooth intégrée.

Le système, intitulé *VMC_connecte_MQTT_HOMEMADE*, permet de surveiller et de contrôler un système de ventilation à distance. En utilisant le protocole MQTT, largement adopté dans les solutions IoT, ce projet facilite le contrôle intelligent de la ventilation en fonction des paramètres environnementaux tels que la température et l'humidité. Le code C développé pour le contrôle a été implémenté avec la plateforme Arduino IDE, facilement accessible et largement utilisée par les hobbyistes et les développeurs.

## **Objectif du Projet**

L'objectif principal de ce projet est de créer une solution personnalisée de VMC permettant le contrôle à distance et automatisé du système de ventilation d'un environnement. En utilisant l'ESP32 comme contrôleur, le projet permet l'intégration avec le protocole MQTT pour envoyer et recevoir des données, ce qui permet le contrôle via des applications mobiles.

Ce système est particulièrement utile dans les endroits où un contrôle efficace de la ventilation est nécessaire, que ce soit pour améliorer la qualité de l'air, économiser de l'énergie ou maintenir un environnement confortable. De plus, étant basé sur l'ESP32 et MQTT, le projet est hautement évolutif et peut être étendu pour inclure d'autres capteurs ou dispositifs si nécessaire.

### **Composants Principaux**

#### **ESP32**
Le microcontrôleur ESP32 est le cœur du projet. Il est choisi pour sa capacité de communication via Wi-Fi et Bluetooth, ce qui le rend idéal pour les solutions IoT. L'ESP32 offre une excellente combinaison de faible coût, de performance et de connectivité, ce qui en fait un choix populaire pour les projets d'automatisation domestique et de contrôle à distance.

#### **Capteur de Température et d'Humidité**
Pour optimiser la ventilation en fonction des conditions environnementales, le système utilise un capteur de température et d'humidité (comme le DHT11 ou DHT22). Ces capteurs capturent les données sur l'environnement en temps réel, permettant au système d'ajuster automatiquement la ventilation pour atteindre des conditions de confort optimales.

#### **MQTT**
Le protocole MQTT (Message Queuing Telemetry Transport) est utilisé pour la communication entre l'ESP32 et la plateforme de contrôle. MQTT est un protocole léger, idéal pour les appareils IoT, permettant une communication efficace même dans des réseaux à faible bande passante. Dans ce projet, il permet à l'ESP32 d'envoyer et de recevoir des données sur les conditions environnementales et l'état du ventilateur à distance.

#### **Application IoT MQTT Panel**
Pour le contrôle à distance, l'application **IoT MQTT Panel** pour Android est utilisée. Cette application facilite la création d'une interface de contrôle où l'utilisateur peut visualiser les données des capteurs, telles que la température et l'humidité, et contrôler la vitesse de ventilation. Avec l'application MQTT Panel, il est possible d'envoyer des commandes directement à l'ESP32, ajustant ainsi la ventilation de manière simple et efficace.

---

## **Structure du Système et Fonctionnement**

Le système est composé de trois composants principaux : le microcontrôleur ESP32, le capteur de température et d'humidité, et l'application pour le contrôle à distance. La communication entre ces composants est réalisée via le protocole MQTT, ce qui garantit la flexibilité et l'évolutivité du projet.

1. **Lecture des Données Environnementales**
   Le capteur de température et d'humidité, connecté à l'ESP32, collecte les données sur l'environnement en temps réel. Ces données sont traitées et, si nécessaire, l'ESP32 ajuste le fonctionnement du ventilateur pour optimiser le confort.

2. **Envoi des Données via MQTT**
   Les données des capteurs sont envoyées à un serveur MQTT (broker). Ce serveur peut être situé sur un serveur local ou dans le cloud, en fonction de la configuration du système. L'ESP32 envoie des informations telles que la température et l'humidité de l'environnement, permettant à l'utilisateur de surveiller la situation à distance.

3. **Contrôle à Distance**
   L'application **IoT MQTT Panel** pour Android se connecte au broker MQTT et permet à l'utilisateur de visualiser les données en temps réel. Depuis l'application, il est possible d'ajuster la vitesse du ventilateur ou même de programmer le fonctionnement du système en fonction des lectures des capteurs. Le contrôle à distance se fait de manière simple et intuitive, rendant le système extrêmement flexible.

4. **Automatisation**
   L'un des grands avantages de ce système est l'automatisation. En fonction des données de température et d'humidité, il est possible de configurer des conditions qui déclenchent automatiquement le ventilateur, créant ainsi un environnement contrôlé sans avoir besoin d'intervention constante de l'utilisateur.

---

## **Explication du Code**

Le code du projet a été écrit en langage C, en utilisant la plateforme Arduino IDE pour le développement et le téléchargement du programme dans l'ESP32. Le code est structuré pour accomplir les fonctions suivantes :

1. **Connexion Wi-Fi et MQTT**
   L'ESP32 se connecte d'abord à un réseau Wi-Fi local, puis établit une connexion avec le broker MQTT. La bibliothèque PubSubClient est utilisée pour faciliter la communication MQTT.

2. **Lecture du Capteur de Température et d'Humidité**
   Le code utilise la bibliothèque DHT pour lire les données du capteur de température et d'humidité. À chaque intervalle, le système met à jour les lectures et envoie ces informations au broker MQTT.

3. **Contrôle du Ventilateur**
   En fonction des lectures des capteurs, le code détermine si le ventilateur doit être activé ou ajusté. Le contrôle du ventilateur est effectué via un relais, contrôlé directement par l'ESP32.

4. **Intégration avec l'Application**
   L'ESP32 envoie des informations telles que la température, l'humidité et l'état du ventilateur au broker MQTT, permettant à l'application **IoT MQTT Panel** de recevoir ces informations et de contrôler le ventilateur à distance.

Le code complet est disponible dans le dépôt GitHub du projet et peut être consulté et modifié selon les besoins. Le dépôt est open-source, ce qui permet à d'autres développeurs de contribuer ou d'utiliser la solution dans leurs propres projets.

```c
// Exemple de code simple pour se connecter au Wi-Fi et envoyer des données MQTT
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

// Définition du Wi-Fi
const char* ssid = "votre_ssid";
const char* password = "votre_mot_de_passe_wifi";

// Définition du MQTT
const char* mqtt_server = "broker.mqtt.com";
WiFiClient espClient;
PubSubClient client(espClient);

// Définition du capteur DHT
#define DHTPIN 4
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// Fonction de reconnexion MQTT
void reconnect() {
  while (!client.connected()) {
    if (client.connect("ESP32Client")) {
      client.subscribe("ventilation/control");
    } else {
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
  }

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  dht.begin();
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    return;
  }

  char tempStr[8];
  dtostrf(t, 1, 2, tempStr);
  client.publish("sensor/temperature", tempStr);
  
  char humStr[8];
  dtostrf(h, 1, 2, humStr);
  client.publish("sensor/humidity", humStr);

  delay(2000);
}
```

### **Disponibilité du Code**

Le code complet du projet est disponible sur le dépôt GitHub [VMC_connecte_MQTT_HOMEMADE](https://github.com/linuxerigor/VMC_connecte_MQTT_HOMEMADE). Le dépôt est open-source, ce qui permet à toute personne d'accéder, de modifier et d'adapter le code à ses besoins. Cela favorise la collaboration et l'amélioration continue de la solution.

---

## **Conclusion**

Le projet *VMC_connecte_MQTT_HOMEMADE* constitue une excellente solution pour ceux qui souhaitent créer un système de ventilation intelligent et contrôlé à distance. En utilisant l'ESP32 comme contrôleur central et MQTT pour la communication, le système est hautement flexible et évolutif, permettant de contrôler les ventilateurs en fonction des paramètres environnementaux en temps réel.

De plus, la possibilité de personnaliser le code et l'utilisation de l'application **IoT MQTT Panel** pour le contrôle à distance rendent le système facile à utiliser et efficace. En étant disponible en open-source sur GitHub, le projet offre également une excellente opportunité d'apprentissage pour les développeurs qui souhaitent en savoir plus sur l'IoT et l'automatisation domestique.

Le contrôle intelligent de la ventilation permet non seulement d'améliorer le confort de l'environnement, mais aussi d'économiser de l'énergie et d'améliorer la qualité de l'air. Ce projet est un excellent exemple de la manière dont la technologie peut être appliquée de manière pratique et accessible au grand public.

---

Cet article a été rédigé dans le but de fournir une explication claire et détaillée sur le fonctionnement et la mise en œuvre du contrôleur VMC IoT utilisant l'ESP32. La communauté des développeurs peut bénéficier de l'accès au code et de la possibilité de modifier le projet selon ses besoins spécifiques.

