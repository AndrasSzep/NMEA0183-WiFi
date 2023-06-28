/* 
by Dr.András Szép v1.2 30.6.2023 GPL license.

This is an AI (chatGPT) assisted development for
 Arduino ESP32 code to display UDP-broadcasted NMEA0183 messages 
(like from a NMEA0183 simulator https://github.com/panaaj/nmeasimulator )
through a webserver to be seen on any mobile device for free.
Websockets used to autoupdate the data.
Environmental sensors incorporated and data for the last 24hours stored respectively
in the SPIFFS files /pressure, /temperature, /humidity.
The historical environmental data displayed in the background as charts.

Local WiFi attributes are stored at SPIFFS in files named /ssid and /password.
WPS never tested.

ToDo: check True / Magnetic heading wind directions
      True and Apparent wind speed and directions
      LED lights on M5Atom

incorporate NMEA2000 can bus connection to receive data along with UDP.

*/

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include "SPIFFS.h"
//#include <Arduino_JSON.h>
#include <ArduinoJson.h>
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
#define ENVINTERVAL 5000  // Interval in milliseconds (5 seconds)
#endif

unsigned long previousMillis = 0;  // Variable to store the previous time



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

double  lastTime = 0.0;
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
String pressurearray = "760,760,760,760,760,760,760,760,760,760,760,760,760,760,760,760,760,760,760,760,760,760,760,760,760";
//Json Variable to Hold Slider Values
DynamicJsonDocument sliderValues(1024);
DynamicJsonDocument jsonDoc(1024);

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
//  sliderValues["pressurearray"] = String(pressurearray);
  String jsonString;
  serializeJson(sliderValues, jsonString);
//  String jsonString = JSON.stringify(sliderValues);
  return jsonString;
}


// Initialize WiFi
void initWiFi() {

  String ssid = retrieveString( String("/ssid.txt"));
  String password = retrieveString( String("/password.txt"));

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
  Serial.print("MAC address: ");
  Serial.println(WiFi.macAddress());
}

void notifyClients() {
  String jsonString;
  serializeJson(jsonDoc, jsonString);
  // Send the JSON string as a response
//        request->send(200, "application/json", jsonString);
//        client->text(jsonString);
  if (jsonString.length() != 0 && jsonString != "null") {
#ifdef DEBUG
    Serial.print("notifyClients: ");
    Serial.println(jsonString);
#endif
    ws.textAll(jsonString);
    jsonDoc.clear();
  }
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    message = (char*)data;
#ifdef DEBUG
    Serial.print("handleWebSocketMessage: ");
    Serial.println(message);
#endif
    if (strcmp((char*)data, "getValues") == 0) {
//      notifyClients(getSliderValues());
    }
  }
}

void handleRequest(AsyncWebServerRequest *request) {
  if (request->url() == "/historicdata") {  
    // Create a JSON document
    DynamicJsonDocument jsonDoc(1024);

    //read data
    jsonDoc["histtemp"] = readStoredData("/temperature");
    jsonDoc["histhum"] = readStoredData("/humidity");
    jsonDoc["histpres"] = readStoredData("/pressure");
    jsonDoc["histwater"] = readStoredData("/water");
    
    // Serialize the JSON document to a string
    String jsonString;
    serializeJson(jsonDoc, jsonString);
    #ifdef DEBUG
    Serial.print("handleRequest: ");
    Serial.println(jsonString);
    #endif
    // Set the Content-Type header to application/json
    request->send(200, "application/json", jsonString);
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
  } else if (type == WS_EVT_DISCONNECT) {
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
  } else if (type == WS_EVT_DATA) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
      // Handle received text message
      data[len] = '\0'; // Null-terminate the received data
      Serial.print("Received message: ");
      Serial.println((char*)data);

      // Check if the request is '/historicdata'
      if (strcmp((char*)data, "/historicdata") == 0) {
        // Prepare JSON data
        DynamicJsonDocument jsonDoc(1024);
    //read data
    jsonDoc["histtemp"] = readStoredData("/temperature");
    jsonDoc["histhum"] = readStoredData("/humidity");
    jsonDoc["histpres"] = readStoredData("/pressure");
    jsonDoc["histwater"] = readStoredData("/water");
   
        // Serialize JSON to a string
        String jsonString;
        serializeJson(jsonDoc, jsonString);
    #ifdef DEBUG
    Serial.print("onEvent: ");
    Serial.println(jsonString);
    #endif
        // Send the JSON string as a response
//        request->send(200, "application/json", jsonString)
        client->text(jsonString);
      } else {
        // Handle other requests here
      }
    }
  }
}


void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n\n**** NMEA0183 from UDP->->WEB ****\n");
  delay(100);
#ifdef ENVSENSOR
    M5.begin();             // Init M5StickC.  初始化M5StickC
    Wire.begin(SDA_PIN, SCL_PIN);
    delay(100);
    qmp6988.init();
    Serial.println(F("ENVIII Hat(SHT30 and QMP6988) has initialized "));
#endif

  initFS();       //initialalize file system SPIFFS
  initWiFi();     //init WifI from SPIFFS or WPS

  // Initialize UDP
  udp.begin(UDPPort);

  Serial.print("UDP listening on: ");
  Serial.println(UDPPort);

  ws.onEvent(onEvent);
  server.addHandler(&ws);
  
  // Web Server Root URL
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", "text/html");
  });

  // Register the request handler
  server.on("/historicdata", HTTP_GET, handleRequest);

  server.serveStatic("/", SPIFFS, "/");

  // Start server
  server.begin();
  Serial.println("WebServer started");
  delay(100);
  /*use mdns for host name resolution*/
  if (!MDNS.begin(servername))  // http://ESP32OTA.local
    Serial.println("Error setting up MDNS responder!");   
  else
    Serial.printf("mDNS responder started = http://%s.local\n", servername);
}

void loop() {
#ifdef ENVSENSOR  
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= ENVINTERVAL) {
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
    jsonDoc.clear();
    jsonDoc["airtemp"] = airtemp;  
    jsonDoc["humidity"] = humidity;  
    jsonDoc["pressure"] = pressure;  
    notifyClients();  
    previousMillis = currentMillis;
  }
  /*
  if(BoatData.GPSTime >= lastTime+TIMEPERIOD ){
    pressurearray = updateStoredData("/pressure.txt", pressure.toInt());
    lastTime = BoatData.GPSTime;
  }
  */
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
#ifdef  DEBUG
Serial.println(packetBuffer);
#endif
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
//          Serial.print("DBK");
        } else if (command == "DBT") {
          stringBD.WaterDepth = nmeaStringData[3];
          depth = stringBD.WaterDepth;
          jsonDoc["depth"] = depth; 
          notifyClients();
        } else if (command == "DPT") {
          stringBD.WaterDepth = nmeaStringData[1];
          depth = stringBD.WaterDepth;
          jsonDoc["depth"] = depth; 
          notifyClients(); 
        } else if (command == "GGA") {
          stringBD.UTC = nmeaStringData[1];
          int hours = stringBD.UTC.substring(0, 2).toInt();
          int minutes = stringBD.UTC.substring(2, 4).toInt();
          int seconds = stringBD.UTC.substring(4, 6).toInt();
          BoatData.GPSTime = stringBD.UTC.toDouble();
          stringBD.UTC = int2string(hours) + ":" + int2string(minutes) + ":" + int2string(seconds);
          timedate = stringBD.UTC;  
          jsonDoc["timedate"] = timedate; 
//          Serial.printf("GGA %s", timedate);      
          stringBD.Latitude = convertGPString(nmeaStringData[2]) + nmeaStringData[3];
          latitude = stringBD.Latitude;
          jsonDoc["latitude"] = latitude; 
          stringBD.Longitude = convertGPString(nmeaStringData[4]) + nmeaStringData[5];
          longitude = stringBD.Longitude;
          jsonDoc["longitude"] = longitude;  
          notifyClients();         
        } else if (command == "GLL") {
          stringBD.Latitude = convertGPString(nmeaStringData[1]) + nmeaStringData[2];
          latitude = stringBD.Latitude;
          jsonDoc["latitude"] = latitude;
          stringBD.Longitude = convertGPString(nmeaStringData[3]) + nmeaStringData[4];
          longitude = stringBD.Longitude;
          jsonDoc["longitude"] = longitude;
          stringBD.UTC = nmeaStringData[5];
          int hours = stringBD.UTC.substring(0, 2).toInt();
          int minutes = stringBD.UTC.substring(2, 4).toInt();
          int seconds = stringBD.UTC.substring(4, 6).toInt();
          stringBD.UTC = int2string(hours) + ":" + int2string(minutes) + ":" + int2string(seconds);
          timedate = stringBD.UTC;  
          jsonDoc["timedate"] = timedate;  
          notifyClients(); 
//          Serial.printf("GLL %s", timedate);         
        } else if (command == "GSA") { //GPS Sat
        //
        } else if (command == "HDG") {
          stringBD.HeadingM = String( int(nmeaStringData[1].toDouble()));
          heading = stringBD.HeadingM + "m";  
          jsonDoc["heading"] = heading; 
          notifyClients();
        } else if (command == "HDM") {
          stringBD.HeadingM = String( int(nmeaStringData[1].toDouble()));
          heading = stringBD.HeadingM + "m";  
          jsonDoc["heading"] = heading; 
          notifyClients();
        } else if (command == "HDT") {
          stringBD.HeadingT = String( int(nmeaStringData[1].toDouble()));
          heading = stringBD.HeadingT + "t";  
          jsonDoc["heading"] = heading; 
          notifyClients();
        } else if (command == "MTW") {
          stringBD.WaterTemperature = nmeaStringData[1];
          watertemp = stringBD.WaterTemperature;  
          jsonDoc["watertemp"] = watertemp; 
          notifyClients();
        } else if (command == "MWD") {
          stringBD.WindDirectionT = String( int(nmeaStringData[1].toDouble()));
          winddir  = stringBD.WindDirectionT + "t";  
          jsonDoc["winddir"] = winddir;
          stringBD.WindDirectionM = String( int(nmeaStringData[3].toDouble()));
          winddir  = stringBD.WindDirectionM + "m";  
          jsonDoc["winddir"] = winddir;
          stringBD.WindSpeedK = String( int(nmeaStringData[5].toDouble()));
          windspeed = stringBD.WindSpeedK + nmeaStringData[6];  
          jsonDoc["windspeed"] = windspeed; 
          notifyClients();
        } else if (command == "MWV") { //wind speed and angle
          stringBD.WindDirectionT = String( int(nmeaStringData[1].toDouble()));
          winddir = stringBD.WindDirectionT + "t";  
          jsonDoc["winddir"] = winddir;
          stringBD.WindSpeedK = nmeaStringData[3];  
          jsonDoc["windspeed"] = windspeed;
          windspeed = stringBD.WindSpeedK + nmeaStringData[4]; 
          notifyClients();
        } else if (command == "RMB") {    //nav info
//          Serial.print("RMB");        //waypoint info
        } else if (command == "RMC") {    //nav info
          stringBD.SOG = nmeaStringData[7];
          speed = stringBD.SOG;  
          jsonDoc["speed"] = speed;
          stringBD.Latitude = convertGPString(nmeaStringData[3]) + nmeaStringData[4];
          latitude = stringBD.Latitude;
          jsonDoc["latitude"] = latitude;
          stringBD.Longitude = convertGPString(nmeaStringData[5]) + nmeaStringData[6];
          longitude = stringBD.Longitude;
          jsonDoc["longitude"] = longitude;
          stringBD.UTC = nmeaStringData[1];
          int hours = stringBD.UTC.substring(0, 2).toInt();
          int minutes = stringBD.UTC.substring(2, 4).toInt();
          int seconds = stringBD.UTC.substring(4, 6).toInt();
          stringBD.UTC = int2string(hours) + ":" + int2string(minutes) + ":" + int2string(seconds);            
          stringBD.Date = "20"+ nmeaStringData[9].substring(4,6) + "-" + nmeaStringData[9].substring(2,4) + "-" + nmeaStringData[9].substring(0,2) ;
          timedate = stringBD.Date + " " + stringBD.UTC;
          jsonDoc["timedate"] = timedate; 
          notifyClients(); 
//          Serial.printf("RMC %s", timedate);     
        } else if (command == "RPM") {  //engine RPM
          if (nmeaStringData[2] == "1") {     //engine no.1
            stringBD.RPM = String( int (nmeaStringData[3].toDouble()));
            rpm = stringBD.RPM;  
            jsonDoc["rpm"] = rpm;
            notifyClients();
          }
        } else if (command == "VBW") {  //dual ground/water speed longitudal/transverse
        //
        } else if (command == "VHW") {  //speed and Heading over water
          stringBD.HeadingT = String( int(nmeaStringData[1].toDouble()));
          heading = stringBD.HeadingT + "t";
          jsonDoc["heading"] = heading;
          stringBD.HeadingM = String( int(nmeaStringData[3].toDouble()));
          heading = stringBD.HeadingM + "m";
          jsonDoc["heading"] = heading;              
          stringBD.Speed = nmeaStringData[5];
          speed = stringBD.Speed;
          jsonDoc["speed"] = speed;   
          notifyClients();    
        //
        } else if (command == "VTG") {  //Track Made Good and Ground Speed
        //
        } else if (command == "ZDA") { //Date&Time
          stringBD.Date = nmeaStringData[4] + "-" + nmeaStringData[3] + "-" + nmeaStringData[2];
          stringBD.UTC = nmeaStringData[1];
          int hours = stringBD.UTC.substring(0, 2).toInt();
          int minutes = stringBD.UTC.substring(2, 4).toInt();
          int seconds = stringBD.UTC.substring(4, 6).toInt();
          stringBD.UTC = int2string(hours) + ":" + int2string(minutes) + ":" + int2string(seconds);
          timedate = stringBD.Date + " " + stringBD.UTC;
          jsonDoc["timedate"] = timedate;  
          notifyClients();
//          Serial.printf("ZDA %s", timedate);       
        } 
        else {
          Serial.println(command);
        }
        jsonDoc.clear();
        j=0;
      }
    }
  }  
}
