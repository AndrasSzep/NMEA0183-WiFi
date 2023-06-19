/* 
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp32-web-server-websocket-sliders/
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.
  
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*/

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include "SPIFFS.h"
#include <Arduino_JSON.h>
#include  "config.h"
#include  "BoatData.h"
#include  "aux_functions.h"

static ETSTimer intervalTimer;
char nmeaLine[MAX_NMEA0183_MSG_BUF_LEN]; //NMEA0183 message buffer
size_t i=0, j=1;                          //indexers
uint8_t *pointer_to_int;                  //pointer to void *data (!)
int noOfFields = MAX_NMEA_FIELDS;  //max number of NMEA0183 fields
String nmeaStringData[MAX_NMEA_FIELDS + 1];

WiFiUDP udp;
sBoatData stringBD;   //Strings with BoatData
tBoatData BoatData;         // BoatData

int SourceID = UDPPort;
// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
// Create a WebSocket object
AsyncWebSocket ws("/ws");
// Set LED GPIO
const int ledPin1 = 12;
const int ledPin2 = 13;
const int ledPin3 = 14;

String message = "";
String sliderValue1 = "0";
String sliderValue2 = "0";
String sliderValue3 = "0";
String sliderValue4 = "0";
String sliderValue5 = "0";
String sliderValue6 = "0";
String sliderValue7 = "0";
String sliderValue8 = "0";
String sliderValue9 = "0";
String sliderValue10 = "0";
String sliderValue11 = "0";
String sliderValue12 = "0";
String sliderValue13 = "0";
String sliderValue14 = "0";
String sliderValue15 = "0";
String sliderValue16 = "0";

int dutyCycle1;
int dutyCycle2;
int dutyCycle3;

// setting PWM properties
const int freq = 5000;
const int ledChannel1 = 0;
const int ledChannel2 = 1;
const int ledChannel3 = 2;

const int resolution = 8;

//Json Variable to Hold Slider Values
JSONVar sliderValues;

//Get Slider Values
String getSliderValues(){
  sliderValues["sliderValue1"] = String(sliderValue1);
  sliderValues["sliderValue2"] = String(sliderValue2);
  sliderValues["sliderValue3"] = String(sliderValue3);  
  sliderValues["sliderValue4"] = String(sliderValue4);
  sliderValues["sliderValue5"] = String(sliderValue5);
  sliderValues["sliderValue6"] = String(sliderValue6);  
  sliderValues["sliderValue7"] = String(sliderValue7);
  sliderValues["sliderValue8"] = String(sliderValue8);
  sliderValues["sliderValue9"] = String(sliderValue9);  
  sliderValues["sliderValue10"] = String(sliderValue10);
  sliderValues["sliderValue11"] = String(sliderValue11);
  sliderValues["sliderValue12"] = String(sliderValue12);
  sliderValues["sliderValue13"] = String(sliderValue13);  
  sliderValues["sliderValue14"] = String(sliderValue14);
  sliderValues["sliderValue15"] = String(sliderValue15);
  sliderValues["sliderValue16"] = String(sliderValue16);

  String jsonString = JSON.stringify(sliderValues);
  return jsonString;
}

// Initialize SPIFFS
void initFS() {
  if (!SPIFFS.begin()) {
    Serial.println("An error has occurred while mounting SPIFFS");
  }
  else{
   Serial.println("SPIFFS mounted successfully");
  }
}

// Initialize WiFi
void initWiFi() {
#ifdef  STOREWIFI
  Serial.println("\nStoring WiFi credentials");
  storeString( String("/ssid.txt"), String(myWIFI));
  storeString( String("/password.txt"), String(myPWD));
#endif
  String ssid = retrieveString( String("/ssid.txt"));
  String password = retrieveString( String("/password.txt"));

  strcpy( storedWIFI, ssid.c_str());
  strcpy( storedPWD, password.c_str());

  WiFi.mode(WIFI_STA);
  WiFi.begin(storedWIFI, storedPWD);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
}

void notifyClients(String sliderValues) {
  ws.textAll(sliderValues);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    message = (char*)data;
    Serial.println(message);
    if (message.indexOf("1s") >= 0) {
      sliderValue1 = message.substring(2);
      dutyCycle1 = map(sliderValue1.toInt(), 0, 100, 0, 255);
      Serial.println(dutyCycle1);
      Serial.print(getSliderValues());
      notifyClients(getSliderValues());
    }
    if (message.indexOf("2s") >= 0) {
      sliderValue2 = message.substring(2);
      dutyCycle2 = map(sliderValue2.toInt(), 0, 100, 0, 255);
      Serial.println(dutyCycle2);
      Serial.print(getSliderValues());
      notifyClients(getSliderValues());
    }    
    if (message.indexOf("3s") >= 0) {
      sliderValue3 = message.substring(2);
      dutyCycle3 = map(sliderValue3.toInt(), 0, 100, 0, 255);
      Serial.println(dutyCycle3);
      Serial.print(getSliderValues());
      notifyClients(getSliderValues());
    }
    if (strcmp((char*)data, "getValues") == 0) {
      notifyClients(getSliderValues());
    }
  }
}
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}


void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n****UDP-to-WEB****");

  pinMode(ledPin1, OUTPUT);
  pinMode(ledPin2, OUTPUT);
  pinMode(ledPin3, OUTPUT);
  initFS();
  initWiFi();

  // configure LED PWM functionalitites
  ledcSetup(ledChannel1, freq, resolution);
  ledcSetup(ledChannel2, freq, resolution);
  ledcSetup(ledChannel3, freq, resolution);

  // attach the channel to the GPIO to be controlled
  ledcAttachPin(ledPin1, ledChannel1);
  ledcAttachPin(ledPin2, ledChannel2);
  ledcAttachPin(ledPin3, ledChannel3);

  // Initialize UDP
  udp.begin(UDPPort);

  Serial.print("UDP listening on: ");
  Serial.println(UDPPort);

  initWebSocket();
  
  // Web Server Root URL
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", "text/html");
  });
  
  server.serveStatic("/", SPIFFS, "/");

  // Start server
  server.begin();
  Serial.println("Server started");
  delay(100);
  if (MDNS.begin(servername)) {
    Serial.println("mDNS responder started");
  }
}

void loop() {
  int packetSize = udp.parsePacket();
  if (packetSize) {
    processPacket(packetSize);
  }

  ledcWrite(ledChannel1, dutyCycle1);
  ledcWrite(ledChannel2, dutyCycle2);
  ledcWrite(ledChannel3, dutyCycle3);

  ws.cleanupClients();
}

void processPacket(int packetSize) {
  char packetBuffer[4096];
  udp.read(packetBuffer, packetSize);
  packetBuffer[packetSize] = '\0';
//  Serial.println(packetBuffer);

  uint8_t *pointer_to_int;
  pointer_to_int = (uint8_t *) packetBuffer;
  if( packetSize == 1) {                  //in case just one byte was received for whatever reason
    nmeaLine[0] = pointer_to_int[0];
    j=1; 
    }
  else {
    for ( i=0; i<packetSize; i++) {
      nmeaLine[j++] = pointer_to_int[i];
      if( pointer_to_int[i-1] == 13 && pointer_to_int[i] == 10 ) {
        nmeaLine[j] = 0;
//        Serial.printf("%i: %s", j, nmeaLine);    //here we got the full line ending CRLF
        noOfFields = parseNMEA0183(nmeaLine, nmeaStringData);
//        Serial.print(noOfFields);
//        Serial.print(": ");
//act accodring to command
        String command = nmeaStringData[0];
        if(command == "RPM") {
          stringBD.RPM = String( int (nmeaStringData[3].toDouble()/10));
          sliderValue1 = stringBD.RPM; ;
        } else if (command == "DPT") {
          stringBD.WaterDepth = nmeaStringData[1];
          sliderValue2 = stringBD.WaterDepth;
        } else if (command == "RMC") {
          stringBD.SOG = nmeaStringData[7];
          sliderValue3 = stringBD.SOG;
        } else if (command == "HDT") {
          stringBD.COG = String( int(nmeaStringData[1].toDouble()));
          sliderValue4 = stringBD.COG;
        } else if (command == "MWD") {
          stringBD.WindSpeedK = nmeaStringData[5];
          sliderValue5 = stringBD.WindSpeedK;
        } else if (command == "MTW") {
          stringBD.WaterTemperature = nmeaStringData[1];
          sliderValue9 = stringBD.WaterTemperature;
        } else if (command == "RMC") {
          stringBD.SOG = nmeaStringData[7];
          sliderValue3 = stringBD.SOG;
          stringBD.Latitude = nmeaStringData[3] + nmeaStringData[4];
          sliderValue7 = nmeaStringData[3] + nmeaStringData[4];
          stringBD.Longitude = nmeaStringData[5] + nmeaStringData[6];
          sliderValue8 = nmeaStringData[5] + nmeaStringData[6];
          stringBD.UTC = nmeaStringData[1];
          stringBD.Date = nmeaStringData[9];
          sliderValue13 = nmeaStringData[9] + " " + nmeaStringData[1];
        } else if (command == "GLL") {
          stringBD.Latitude = convertGPString(nmeaStringData[1]) + nmeaStringData[2];
          sliderValue7 = stringBD.Latitude;
          stringBD.Longitude = convertGPString(nmeaStringData[3]) + nmeaStringData[4];
          sliderValue8 = stringBD.Longitude;
          stringBD.UTC = nmeaStringData[5];
          int hours = stringBD.UTC.substring(0, 2).toInt();
          int minutes = stringBD.UTC.substring(2, 4).toInt();
          int seconds = stringBD.UTC.substring(4, 6).toInt();
          stringBD.UTC = String(hours, DEC) + ":" + String(minutes, DEC) + ":" + String(seconds, DEC);
          sliderValue13 = stringBD.UTC;          
        } else if (command == "GGA") {
//
        } 
        else {
//          Serial.println("Unknown command");
        }
        notifyClients(getSliderValues()); //push new values
        j=0;
      }
    }
  }  
}
