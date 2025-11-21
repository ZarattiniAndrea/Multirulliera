#include "rulliera.h"
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

Rulliera::Rulliera(uint8_t sensorPin, uint8_t sensorPin2, uint8_t ledPin, ModbusTCPServer mbServer)
    : SENSOR_PIN(sensorPin), SENSOR_PIN_2(sensorPin2), LED_PIN(ledPin), modbusTCPServer(mbServer) {
        pinMode(LED_PIN, OUTPUT);
        pinMode(SENSOR_PIN, INPUT);
        pinMode(SENSOR_PIN_2, INPUT);
    }

void Rulliera::rilevamento_anteriore(uint8_t SENSOR_PIN_2, uint8_t LED_BUILTIN_2){
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

void Rulliera::rilevamento_posteriore(uint8_t SENSOR_PIN, uint8_t LED_BUILTIN_2){
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

void Rulliera::frontToggle(){
  front_state =! front_state;
}

void Rulliera::backToggle(){
  back_state =! back_state;
}

void Rulliera::WriteDebounce(unsigned long debounceDelay){
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

void Rulliera::blisterCounter(uint8_t FRONT_SENSOR, uint8_t REAR_SENSOR, uint8_t OUTPUT_LED, bool connectionType){
    if(connectionType == 0){ //ETHERNET
    EthernetClient client = ethServer.available();

    if(client){
      modbusTCPServer.accept(client); // "passo" il server Ethernet al server Modbus TCP
      while(client.connected()){
        //polling per le richieste Modbus mentre il client è connesso
        modbusTCPServer.poll();
        //aggiorno il registro coil frontale con lo stato del toggle
        modbusTCPServer.coilWrite(0x00, front_state);
        //aggiorno il registro coil posteriore con lo stato del toggle
        modbusTCPServer.coilWrite(0x01, back_state);
        //controllo i sensori
        rilevamento_anteriore(FRONT_SENSOR, OUTPUT_LED);
        rilevamento_posteriore(REAR_SENSOR, OUTPUT_LED);

        WebClient = WebServer.accept();

        // Qui gestisco il client WEB

        if(WebClient){
          Serial.println("Nuovo Client connesso");
          String request = "";
          boolean currentLineIsBlank = true; // Variabile per tenere traccia delle linee vuote
          while(WebClient.connected()){
              if(WebClient.available()){
                  char c = WebClient.read(); // Leggo un carattere dal client
                  // Fine dell'header HTTP è indicata da una linea vuota
                  request += c;
                  if (c == '\n' && currentLineIsBlank) {
                      // Invio della risposta HTTP
                      WebClient.println("HTTP/1.1 200 OK");
                      WebClient.println("Content-Type: text/html");
                      WebClient.println("Connection: close");
                      WebClient.println();
                      WebClient.println(generateHTML(debounceDelay));
                      break;
                  }
                  if (c == '\n') {
                      currentLineIsBlank = true; // Inizio di una nuova linea
                  } else if (c != '\r') {
                      currentLineIsBlank = false; // Carattere diverso da '\r', quindi la linea non è vuota
                  }
                  if(request.indexOf("GET /?debounce=") >= 0){
                    auto precdebounceDelay = debounceDelay;
                    int startIndex = request.indexOf("debounce=") + 9;
                    int endIndex = request.indexOf(" ", startIndex); //trovo lo spazio dopo il valore
                    String debounceValueStr = request.substring(startIndex, endIndex);
                    debounceDelay = debounceValueStr.toInt();
                    Serial.print("Nuovo valore di debounceDelay: ");
                    Serial.println(debounceDelay);
                    
                    if(debounceDelay != precdebounceDelay) WriteDebounce(debounceDelay);
                  }
              }
            }
          WebClient.stop();
        }
      }
      client.stop(); // Chiudo la connessione con il client Modbus
    }
  }

  if(connectionType == 1){ //WIFI
    if(!modbusClient || !modbusClient.connected()) {
      //Se non c'è un client modbus attivo, ne accetto uno nuovo
      WiFiClient newClient = wifiServer.accept();
      if(newClient){
        modbusClient.stop(); //chiudo eventuale client precedente
        modbusClient = newClient;
        modbusTCPServer.accept(modbusClient); // "passo" il client WiFi al server Modbus TCP
      }
    }
    modbusTCPServer.poll();
    modbusTCPServer.coilWrite(0x00, front_state);
    modbusTCPServer.coilWrite(0x01, back_state);
    rilevamento_anteriore(FRONT_SENSOR, OUTPUT_LED);
    rilevamento_posteriore(REAR_SENSOR, OUTPUT_LED);

    wifiWebClient = wifiWebServer.available();

    if(wifiWebClient){
      Serial.println("Nuovo Client connesso");
        String request = "";
        boolean currentLineIsBlank = true; // Variabile per tenere traccia delle linee vuote
        while(wifiWebClient.connected()){
            if(wifiWebClient.available()){
                char c = wifiWebClient.read(); // Leggo un carattere dal client
                // Fine dell'header HTTP è indicata da una linea vuota
                request += c;
                if (c == '\n' && currentLineIsBlank) {
                    // Invio della risposta HTTP
                    wifiWebClient.println("HTTP/1.1 200 OK");
                    wifiWebClient.println("Content-Type: text/html");
                    wifiWebClient.println("Connection: close");
                    wifiWebClient.println();
                    wifiWebClient.println(generateHTML(debounceDelay));
                    break;
                }
                if (c == '\n') {
                    currentLineIsBlank = true; // Inizio di una nuova linea
                } else if (c != '\r') {
                    currentLineIsBlank = false; // Carattere diverso da '\r', quindi la linea non è vuota
                }
                if(request.indexOf("GET /?debounce=") >= 0){
                  auto precdebounceDelay = debounceDelay;
                  int startIndex = request.indexOf("debounce=") + 9;
                  int endIndex = request.indexOf(" ", startIndex); //trovo lo spazio dopo il valore
                  String debounceValueStr = request.substring(startIndex, endIndex);
                  debounceDelay = debounceValueStr.toInt();
                  Serial.print("Nuovo valore di debounceDelay: ");
                  Serial.println(debounceDelay);
                  
                  if(debounceDelay != precdebounceDelay) WriteDebounce(debounceDelay);
                }
            }
        }
    }
  }
}