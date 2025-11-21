#ifndef RULLIERA_H
#define RULLIERA_H

#include <Arduino.h>
#include <WiFi.h>
#include <Ethernet.h>
#include <ArduinoModbus.h>
#include <ArduinoRS485.h>


class Rulliera {
    private: 
        unsigned long contatore = 0; 
        int front_sensorstate = LOW;
        int front_precsensorstate = LOW;
        int back_sensorstate = LOW;
        int back_precsensorstate = LOW;
        unsigned long lastDebounceTime = 0;
        unsigned long debounceDelay = 200;
        bool front_state = false;
        bool back_state = false;
        uint8_t SENSOR_PIN;
        uint8_t SENSOR_PIN_2;
        uint8_t LED_PIN;
        ModbusTCPServer modbusTCPServer;
        static WiFiClient modbusClient;
        EthernetServer ethServer = EthernetServer(502);
        EthernetServer WebServer = EthernetServer(80);
        EthernetClient WebClient = EthernetClient();
        WiFiServer wifiServer = WiFiServer(502);
        WiFiServer wifiWebServer = WiFiServer(80);
        WiFiClient wifiWebClient = WiFiClient();

        
    public: 
        Rulliera(uint8_t sensorPin, uint8_t sensorPin2, uint8_t ledPin, ModbusTCPServer mbServer);
        void rilevamento_anteriore(uint8_t SENSOR_PIN_2, uint8_t LED_BUILTIN_2);
        void rilevamento_posteriore(uint8_t SENSOR_PIN, uint8_t LED_BUILTIN_2);
        void backToggle();
        void frontToggle();
        void WriteDebounce(unsigned long debounceDelay);
        void blisterCounter(uint8_t FRONT_SENSOR, uint8_t REAR_SENSOR, uint8_t OUTPUT_LED, bool connectionType);
};

#endif // RULLIERA_H