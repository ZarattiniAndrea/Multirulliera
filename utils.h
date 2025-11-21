#ifndef UTILS_H
#define UTILS_H
#include <Arduino.h>
#include <Ethernet.h>
#include <WiFi.h>
#include <ArduinoModbus.h>
#include <ArduinoRS485.h>
#include <SPI.h>
#include "FlashIAPLimits.h"
#include <FlashIAP.h>
#include <FlashIAPBlockDevice.h>
#include "webpage.h"

bool wifiConnectionTest(char* ssid, char* pass);
bool ethConnectionTest(byte mac[], IPAddress ip);
void rilevamento_posteriore(uint8_t SENSOR_PIN, uint8_t LED_BUILTIN_2);
void rilevamento_anteriore(uint8_t SENSOR_PIN_2, uint8_t LED_BUILTIN_2);
void backToggle();
void frontToggle();
void WriteDebounce(unsigned long debounceDelay);
void blisterCounter(uint8_t FRONT_SENSOR, uint8_t REAR_SENSOR, uint8_t OUTPUT_LED);

//variabili globali
unsigned long contatore = 0; //variabile per il conteggio degli impulsi del sensore
int front_sensorstate = LOW;
int front_precsensorstate = LOW;
int back_sensorstate = LOW;
int back_precsensorstate = LOW;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 200; // tempo di debouncing in ms
bool front_state = false; //variabile di toggle
bool back_state = false; //variabile di toggle
int status = WL_IDLE_STATUS; // variabile per lo stato della connessione. WL_IDLE_STATUS significa che non è connesso



//ETHERNET
//Definisco l'indirizzo IP del server Modbus
IPAddress ip(192, 168, 200, 170); // Indirizzo IP statico del server Modbus
byte mac[] = {0xA0, 0xCD, 0xF3, 0xB1, 0xEC, 0x1E};
EthernetServer ethServer(502); // Porta standard Modbus TCP è 502
EthernetServer WebServer(80);  // Server Web (porta 80)
EthernetClient WebClient; //client Web Ethernet

//WIFI
char ssid[] = "Galaxy S25 711A";
char pass[] = "HakunaMatata";
WiFiServer wifiServer(502); // Porta standard Modbus TCP è 502
WiFiServer wifiWebServer(80);  // Server Web (porta 80)
WiFiClient wifiWebClient; //client Web WiFi
static WiFiClient modbusClient;



bool wifiConnectionTest(char ssid[], char pass[]){
  Serial.println("Connessione con WiFi");
  contatore = 5;
    while(status != WL_CONNECTED){ //finchè non sono connesso alla rete
      Serial.print("Tentativo di connessione alla rete: ");
      Serial.println(ssid);
      status = WiFi.begin(ssid, pass); //avvio la connessione WiFi (con DHCP abilitato)
      contatore--;
      if(contatore == 0){
        return false;
      }
      delay(1000);
    }
    wifiServer.begin(); //Avvio il server WiFi
    wifiWebServer.begin(); //Avvio il Web Server WiFi
    return true;
}

bool ethConnectionTest(byte mac[], IPAddress ip){
  Serial.println(" INSERIRE CAVO DI RETE E ATTENDERE...");

  // Inizializzo la connessione Ethernet con l'IP statico
  Ethernet.begin(mac, ip);

  Serial.println("Cavo inserito!");
  unsigned long start = millis();
  //Provo a stabilire la connessione per 3 secondi
  while(millis() - start < 3000){
    if(Ethernet.localIP() != IPAddress(0,0,0,0)){
      // Se otteniamo un IP valido, la connessione Ethernet è attiva
      Serial.print("Ethernet connessa, IP: ");
      Serial.println(Ethernet.localIP());
      //avvio il server ethrenet
      ethServer.begin();
      //avvio il server Web ethernet
      WebServer.begin();
      return true;
    }
    delay(200);
  }
  //Se dopo 3 secondi non abbiamo ottenuto un IP valido, la connessione fallisce
  return false;
}

void WriteDebounce(unsigned long debounceDelay){
    auto [flashSize, startAddress, iapSize] = getFlashIAPLimits();
    //Creo un block device nella memoria flash disponibile
    FlashIAPBlockDevice blockDevice(startAddress, iapSize);
    //Prima di poterlo usare si deve inizializzare 
    blockDevice.init();

    const auto eraseBlockSize = blockDevice.get_erase_size(); // dimensione del blocco di cancellazione
    const auto programBlockSize = blockDevice.get_program_size(); // dimensione del blocco di programmazione

    //Calcolo il numero di bytes necessari per salvare il debounceDelay
    const auto messageSize = sizeof(debounceDelay);
    const unsigned int requiredEraseBlocks = ceil(messageSize / (float)eraseBlockSize); //numero di blocchi di cancellazione necessari
    const unsigned int requiredProgramBlocks = ceil(messageSize / (float) programBlockSize); //numero di blocchi di programmazione necessari
    const auto dataSize = requiredProgramBlocks * programBlockSize; //dimensione totale dei dati da scrivere
    unsigned long buffer[dataSize]; //buffer per i dati

    Serial.println("Lettura del valore precedente di debounceDelay dalla memoria flash...");
    blockDevice.read(buffer, 0, dataSize);
    Serial.println(*(unsigned long*)buffer);

    //Cancello un blocco iniziando iniziando dall'indirizzo 0 relativamente all'inizio del block device
    blockDevice.erase(0, requiredEraseBlocks * eraseBlockSize);
    Serial.println("Cancellazione del blocco di memoria completata.");

    //Scrivo il nuovo valore di debounceDelay nella memoria flash
    Serial.println("Scrittura del nuovo valore di debounceDelay nella memoria flash...");
    blockDevice.program((char*)&debounceDelay, 0, dataSize);
    Serial.println("Scrittura completata.");

    // Leggo di nuovo per verificare
    blockDevice.read(buffer, 0, dataSize);
    Serial.println("Valore letto dalla memoria flash:");
    Serial.println(*(unsigned long*)buffer);

    // Deinizializzo il block device
    blockDevice.deinit();
    Serial.println("FATTO!");
}

void frontToggle(){
  front_state = !front_state;
}

void backToggle(){
  back_state = !back_state;
}

void rilevamento_anteriore(uint8_t SENSOR_PIN_2, uint8_t LED_BUILTIN_2){
  int lettura = digitalRead(SENSOR_PIN_2); //leggo lo stato del sensore
  if (lettura != front_precsensorstate){ //se lo stato corrente del sensore è diverso rispetto all'ultimo stato registrato
    lastDebounceTime = millis();   //resetto il timer
  }
  
  //se è passato abbastanza tempo senza cambiamenti
  if ((millis() - lastDebounceTime) > debounceDelay){
    //aggiorno lo stato solo se è cambiato davvero
    if(lettura != front_sensorstate){
      front_sensorstate = lettura;
        if (front_sensorstate == HIGH) { //rilevo solo i fronti di salita
        contatore ++;
        //Serial.print("Impulso rilevato, conteggio: ");
        //Serial.print(contatore);
        digitalWrite(LED_BUILTIN_2, HIGH); //accende il led
        } else {
          digitalWrite(LED_BUILTIN_2, LOW); //spegne il led
          frontToggle();
        }
    }
  }
  front_precsensorstate = lettura; 
}

void rilevamento_posteriore(uint8_t SENSOR_PIN, uint8_t LED_BUILTIN_2){
  int lettura = digitalRead(SENSOR_PIN); //leggo lo stato del sensore
  if (lettura != back_precsensorstate){ //se lo stato corrente del sensore è diverso rispetto all'ultimo stato registrato
    lastDebounceTime = millis();   //resetto il timer
  }
  
  //se è passato abbastanza tempo senza cambiamenti
  if ((millis() - lastDebounceTime) > debounceDelay){
    //aggiorno lo stato solo se è cambiato davvero
    if(lettura != back_sensorstate){
      back_sensorstate = lettura;
        if (back_sensorstate == HIGH) { //rilevo solo i fronti di salita
        contatore ++;
        //Serial.print("Impulso rilevato, conteggio: ");
        //Serial.print(contatore);
        digitalWrite(LED_BUILTIN_2, HIGH); //accende il led
        } else {
          digitalWrite(LED_BUILTIN_2, LOW); //spegne il led
          backToggle();
        }
    }
  }
  back_precsensorstate = lettura; 
}

#endif // UTILS_H