/* 

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

/*
*******************************************************************************
* Visit for more information: https://docs.m5stack.com/en/hat/hat_envIII
* Product: ENVIII_SHT30_QMP6988.  环境传感器
*******************************************************************************
*/
#ifdef ENVSENSOR
#include <M5StickC.h>
#include "M5_ENV.h"
SHT3X sht30(0x44, 1);
QMP6988 qmp6988;
float airTemp      = 0.0;
float airHumidity  = 0.0;
float airPressure = 0.0;
#endif

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


String message = "";
String timedate = "1957-04-28 10:01:02";
String rpm = "0";
String depth = "0";
String speed = "0";
String heading = "90";
String windspeed = "0";
String winddir = "0";
String longitude = "45º10'20\"N";
String latitude = "05º10'20\"E";
String watertemp = "18";
String humidity = "50";
String pressure = "760";
String airtemp = "21";

//Json Variable to Hold Slider Values
JSONVar sliderValues;

//Get Slider Values
String getSliderValues(){
  sliderValues["timedate"] = String(timedate);
  sliderValues["rpm"] = String(rpm);
  sliderValues["depth"] = String(depth);  
  sliderValues["speed"] = String(speed);
  sliderValues["heading"] = String(heading);
  sliderValues["windspeed"] = String(windspeed);  
  sliderValues["winddir"] = String(winddir);
  sliderValues["longitude"] = String(longitude);
  sliderValues["latitude"] = String(latitude);  
  sliderValues["watertemp"] = String(watertemp);
  sliderValues["humidity"] = String(humidity);
  sliderValues["pressure"] = String(pressure);
  sliderValues["airtemp"] = String(airtemp); 

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
#ifdef DEBUG
  Serial.println(sliderValues);
#endif
  ws.textAll(sliderValues);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    message = (char*)data;
#ifdef DEBUG
    Serial.println(message);
#endif
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
  Serial.println("\n**** NMEA0183 from UDP->->WEB ****");

#ifdef ENVSENSOR
    M5.begin();             // Init M5StickC.  初始化M5StickC
    Wire.begin(SDA_PIN, SCL_PIN);
    delay(100);
    qmp6988.init();
    Serial.println(F("ENVIII Hat(SHT30 and QMP6988) test"));
#endif

  initFS();
  initWiFi();

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
  /*use mdns for host name resolution*/
  if (!MDNS.begin(servername))  // http://ESP32OTA.local
    Serial.println("Error setting up MDNS responder!");   
  else
    Serial.printf("mDNS responder started = http://%s.local\n", servername);
}

void loop() {
#ifdef ENVSENSOR  
    airPressure = qmp6988.calcPressure()/133.322387415;
    if (sht30.get() == 0) {  // Obtain the data of shT30.  获取sht30的数据
        airTemp = sht30.cTemp;   // Store the temperature obtained from shT30.
        airHumidity = sht30.humidity;  // Store the humidity obtained from the SHT30.
    } else {
        airTemp = 0, airHumidity = 0;
    }
  airtemp = String( airTemp,1 );
  humidity = String( airHumidity,1);
  pressure = String( airPressure, 0);
#endif
  int packetSize = udp.parsePacket();
  if (packetSize) {
    processPacket(packetSize);
  }
  ws.cleanupClients();
}

void processPacket(int packetSize) {
  char packetBuffer[4096];
  udp.read(packetBuffer, packetSize);
  packetBuffer[packetSize] = '\0';

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
        noOfFields = parseNMEA0183(nmeaLine, nmeaStringData);
//act accodring to command
        String command = nmeaStringData[0];
        if(command == "APB") {
//          Serial.print("APB");    //autopilot
        } else if (command == "DBK") {
          Serial.print("DBK");
        } else if (command == "DBT") {
          stringBD.WaterDepth = nmeaStringData[3];
          depth = stringBD.WaterDepth;
        } else if (command == "DPT") {
          stringBD.WaterDepth = nmeaStringData[1];
          depth = stringBD.WaterDepth;
        } else if (command == "GGA") {
          stringBD.UTC = nmeaStringData[1];
          int hours = stringBD.UTC.substring(0, 2).toInt();
          int minutes = stringBD.UTC.substring(2, 4).toInt();
          int seconds = stringBD.UTC.substring(4, 6).toInt();
          stringBD.UTC = int2string(hours) + ":" + int2string(minutes) + ":" + int2string(seconds);
          timedate = stringBD.UTC;        
          stringBD.Latitude = convertGPString(nmeaStringData[2]) + nmeaStringData[3];
          latitude = stringBD.Latitude;
          stringBD.Longitude = convertGPString(nmeaStringData[4]) + nmeaStringData[5];
          longitude = stringBD.Longitude;              
        } else if (command == "GLL") {
          stringBD.Latitude = convertGPString(nmeaStringData[1]) + nmeaStringData[2];
          latitude = stringBD.Latitude;
          stringBD.Longitude = convertGPString(nmeaStringData[3]) + nmeaStringData[4];
          longitude = stringBD.Longitude;
          stringBD.UTC = nmeaStringData[5];
          int hours = stringBD.UTC.substring(0, 2).toInt();
          int minutes = stringBD.UTC.substring(2, 4).toInt();
          int seconds = stringBD.UTC.substring(4, 6).toInt();
          stringBD.UTC = int2string(hours) + ":" + int2string(minutes) + ":" + int2string(seconds);
          timedate = stringBD.UTC;          
        } else if (command == "HDG") {
          stringBD.Heading = nmeaStringData[1];
          heading = stringBD.Heading;
        } else if (command == "HDM") {
          stringBD.Heading = nmeaStringData[1];
          heading = stringBD.Heading;
        } else if (command == "HDT") {
          stringBD.COG = String( int(nmeaStringData[1].toDouble()));
          heading = stringBD.COG;
        } else if (command == "MTW") {
          stringBD.WaterTemperature = nmeaStringData[1];
          watertemp = stringBD.WaterTemperature;
        } else if (command == "MWD") {
          stringBD.WindDirectionT = nmeaStringData[1];
          winddir  = stringBD.WindDirectionT;
          stringBD.WindDirectionM = nmeaStringData[3];
          winddir  = stringBD.WindDirectionM;
          stringBD.WindSpeedK = nmeaStringData[5];
          windspeed = stringBD.WindSpeedK;
        } else if (command == "MWV") {
          stringBD.WindDirectionT = nmeaStringData[1];
          winddir = stringBD.WindDirectionT;
        } else if (command == "RMB") {
//          Serial.print("RMB");        //waypoint info
        } else if (command == "RMC") {
          stringBD.SOG = nmeaStringData[7];
          speed = stringBD.SOG;
          stringBD.Latitude = convertGPString(nmeaStringData[3]) + nmeaStringData[4];
          latitude = stringBD.Latitude;
          stringBD.Longitude = convertGPString(nmeaStringData[5]) + nmeaStringData[6];
          longitude = stringBD.Longitude;
          stringBD.UTC = nmeaStringData[1];
          int hours = stringBD.UTC.substring(0, 2).toInt();
          int minutes = stringBD.UTC.substring(2, 4).toInt();
          int seconds = stringBD.UTC.substring(4, 6).toInt();
          stringBD.UTC = int2string(hours) + ":" + int2string(minutes) + ":" + int2string(seconds);            
          stringBD.Date = "20"+ nmeaStringData[9].substring(4,6) + "-" + nmeaStringData[9].substring(2,4) + "-" + nmeaStringData[9].substring(0,2) ;
          timedate = stringBD.Date + " " + stringBD.UTC;   
        } else if (command == "RPM") {
          if (nmeaStringData[2] == "1") {     //engine no.1
            stringBD.RPM = String( int (nmeaStringData[3].toDouble()/10));
            rpm = stringBD.RPM; 
          }
        } else if (command == "ZDA") {
          stringBD.Date = nmeaStringData[4] + "-" + nmeaStringData[3] + "-" + nmeaStringData[2];
          stringBD.UTC = nmeaStringData[1];
          int hours = stringBD.UTC.substring(0, 2).toInt();
          int minutes = stringBD.UTC.substring(2, 4).toInt();
          int seconds = stringBD.UTC.substring(4, 6).toInt();
          stringBD.UTC = String(hours, DEC) + ":" + String(minutes, DEC) + ":" + String(seconds, DEC);
          timedate = stringBD.Date + " " + stringBD.UTC;     
        } 
        else {
          Serial.println(command);
        }
        notifyClients(getSliderValues()); //push new values
        j=0;
      }
    }
  }  
}
