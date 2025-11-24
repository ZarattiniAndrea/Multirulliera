#include <Arduino.h>
#include <Ethernet.h>
#include <WiFi.h>
#include <ArduinoModbus.h>
#include <ArduinoRS485.h>
#include <SPI.h>
#include "rulliera.h"

// Definizione dei pin collegati (per tutte le linee)
// Prima rulliera
#define LED_BUILTIN PI_0 //led integrato sulla scheda
#define LED_BUILTIN_2 PI_1 //secondo led integrato sulla scheda
#define SENSOR_PIN A0 //input dal sensore POSTERIORE I1
#define SENSOR_PIN_2 A1// input dal sensore ANTERIORE I2

// Seconda rulliera 
// .....

// Terza rulliera
// .....

//variabili globali
bool connectionType; //true = WIFI, false = ETHERNET
int status = WL_IDLE_STATUS;

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
ModbusTCPServer modbusTCPServer; // Creazione del server Modbus TCP (ethServer invece è il server Ethernet di base)

//Creo oggetto rulliera
Rulliera Rulliera1 = Rulliera(SENSOR_PIN,SENSOR_PIN_2,LED_BUILTIN_2, modbusTCPServer, modbusClient);

bool wifiConnectionTest(char ssid[], char pass[]);
bool ethConnectionTest(byte mac[], IPAddress ip);

void setup() {
  // Inizializzo la comunicazionne seriale a 9600 baud
  Serial.begin(9600);
  Serial.print("Ciao! Inizializzo la connessione...");
  connectionType = true;
  if(connectionType){ // arduino OPTA (STM32H7 + mbed OS)
  /* non controlla il cavo o il link, ma solo se il driver Ethernet è disponibile a livello hardware 
    (cioè se il microcontrollore STM32 ha un controller MAC attivo e il PHY risponde via MDIO). 
    Su OPTA il controllore LAN8742A è sempre presente anche se il cavo non è collegato, motivo per cui 
    Ethernet.hardwareStatus() ritorna sempre. If entra sempre nel blocco.
  */
    if(wifiConnectionTest(ssid, pass)){
      Serial.println("Connessione WiFi riuscita!");
      connectionType = true;
    } else {
      Serial.println("Connessione WiFi fallita, provo con Ethernet...");
      connectionType = false;
    }
  }
  if(!connectionType){
    Serial.println("Connessione con Ethernet");
    Serial.println("Assicurarsi che il cavo Ethernet sia collegato...");
    delay(5000);
    if(ethConnectionTest(mac, ip)){
      Serial.println("Connessione Ethernet riuscita!");
      connectionType = false;
    } else {
      Serial.println("Connessione Ethernet fallita, non posso continuare.");
      while(1);
    }
  }

  Serial.print("Connesso alla rete!");

  if(!modbusTCPServer.begin()){ // Avvio il server Modbus TCP
    Serial.println("Impossibile avviare il server Modbus TCP!");
    Serial.println("Server Ethernet avviato!");
    while(1);
  }

  //configuro 2 registri di tipo Coils all'indirizzo 0
  if(!modbusTCPServer.configureCoils(0x00, 2)){
    Serial.println("Impossibile configurare i registri di tipo COILS!");
    while(1);
  }
  Serial.println("Stato connectionType (0=ETH, 1=WIFI): ");
  Serial.println(connectionType);
}

void loop() {
  Rulliera1.blisterCounter(SENSOR_PIN_2, SENSOR_PIN, LED_BUILTIN_2, connectionType);
}

bool wifiConnectionTest(char ssid[], char pass[]){
  Serial.println("Connessione con WiFi");
  int contatore = 5;
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
