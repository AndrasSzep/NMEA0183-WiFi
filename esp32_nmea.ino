/* 
by Dr.András Szép v1.3 3.7.2023 GNU General Public License (GPL).
*/

/*
This is an AI (chatGPT) assisted development for
Arduino ESP32 code to display UDP-broadcasted NMEA0183 messages 
(like from a NMEA0183 simulator https://github.com/panaaj/nmeasimulator )
through a webserver to be seen on any mobile device for free.
Websockets used to autoupdate the data.
Environmental sensors incorporated and data for the last 24hours stored respectively
in the SPIFFS files /pressure, /temperature, /humidity.
The historical environmental data displayed in the background as charts.

Local WiFi attributes are stored at SPIFFS in files named /ssid and /password.
WPS never been tested but assume working.

Implemented OverTheAir update of the data files as well as the code itself on port 8080
(i.e. http://nmea2000.local:8080 ) see config.h . 
*** Arduino IDE 2.0 does not support file upload, this makes much simplier uploading updates 
especially in the client and stored data files.

ToDo: 
      LED lights on M5Atom. Still need some ideas of colors and blinking signals

      incorporate NMEA2000 can bus connection to receive data along with NMEA0183 on UDP.

*/
#define LITTLE_FS 0
#define SPIFFS_FS 1
#define FILESYSTYPE SPIFFS_FS
//const char *otahost = "OTASPIFFS";
#define FILEBUFSIZ 4096

#ifndef WM_PORTALTIMEOUT
#define WM_PORTALTIMEOUT 180
#endif

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <WiFiServer.h>
#include <NTPClient.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "SPIFFS.h"
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
float airTemp      = 20.0;
float airHumidity  = 50.0;
float airPressure = 760.0;
#define ENVINTERVAL 10000  // Interval in milliseconds 
#define STOREINTERVAL 3600000 // store env data in SPIFFS ones/hour
#endif
#define UDPINTERVAL 1000  //ignore UDP received within 1 second

unsigned long previousMillis = 0;  // Variable to store the previous time
unsigned  long  storedMillis  = 0;  //time of last stored env.data
unsigned long previousPacketMillis = 0;  // Variable to store the previous packet reception time

char nmeaLine[MAX_NMEA0183_MSG_BUF_LEN]; //NMEA0183 message buffer
size_t i=0, j=1;                          //indexers
uint8_t *pointer_to_int;                  //pointer to void *data (!)
int noOfFields = MAX_NMEA_FIELDS;  //max number of NMEA0183 fields
String nmeaStringData[MAX_NMEA_FIELDS + 1];

WiFiUDP udp;
NTPClient timeClient(udp, ntpServer, timeZone);
time_t utcTime = 0;
sBoatData stringBD;   //Strings with BoatData
tBoatData BoatData;         // BoatData

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
// Create a WebSocket object
AsyncWebSocket ws("/ws");

WebServer servOTA(OTAPORT);    //webserver for OnTheAir update on port 8080
bool fsFound = false;
#include "webpages.h"            // include after declaring - WebServer servOTA(OTAPORT) 
#include "filecode.h"

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

DynamicJsonDocument jsonDoc(1024);

String getDT() {
  time_t now = time(nullptr); // Get current time as time_t
  struct tm* currentTime = gmtime(&now); // Convert time_t to struct tm in UTC

  String dateTimeString;
  dateTimeString += String(currentTime->tm_year + 1900) + "-";
  dateTimeString += String(currentTime->tm_mon + 1) + "-";
  dateTimeString += String(currentTime->tm_mday) + " ";
  dateTimeString += String(currentTime->tm_hour) + ":";
  dateTimeString += String(currentTime->tm_min) + ":";
  dateTimeString += String(currentTime->tm_sec);

  return dateTimeString;
}


// Initialize WiFi
void initWiFi() {

  String ssid = retrieveString( String("/ssid.txt"));
  String password = retrieveString( String("/password.txt"));

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  Serial.print("Connecting to WiFi ...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
  Serial.print("MAC address: ");
  Serial.println(WiFi.macAddress());
    // Initialize the NTP client
  timeClient.begin();
  stringBD.UTC = getDT(); //store UTC
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

void handleRequest(AsyncWebServerRequest *request) {
  if (request->url() == "/historicdata") {  
    // Create a JSON document
    DynamicJsonDocument jsonDoc(1024);
    //read data
    jsonDoc["histtemp"] = readStoredData("/temperature");
    jsonDoc["histhum"] = readStoredData("/humidity");
    jsonDoc["histpres"] = readStoredData("/pressure");
    jsonDoc["histwater"] = readStoredData("/water");
    notifyClients();  
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
        notifyClients();  
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

  initFS(false ,false);       //initialalize file system SPIFFS
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

  servOTA.on("/", HTTP_GET, handleMain);

  // upload file to FS. Three callbacks
  servOTA.on(
      "/update", HTTP_POST, []()
      {
    servOTA.sendHeader("Connection", "close");
    servOTA.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart(); },
      []()
      {
        HTTPUpload &upload = servOTA.upload();
        if (upload.status == UPLOAD_FILE_START)
        {
          Serial.printf("Update: %s\n", upload.filename.c_str());
          if (!Update.begin(UPDATE_SIZE_UNKNOWN))
          { // start with max available size
            Update.printError(Serial);
          }
        }
        else if (upload.status == UPLOAD_FILE_WRITE)
        {
          /* flashing firmware to ESP*/
          if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
          {
            Update.printError(Serial);
          }
        }
        else if (upload.status == UPLOAD_FILE_END)
        {
          if (Update.end(true))
          { // true to set the size to the current progress
            Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
          }
          else
          {
            Update.printError(Serial);
          }
        }
      });

  servOTA.on("/delete", HTTP_GET, handleFileDelete);
  servOTA.on("/main", HTTP_GET, handleMain); // JSON format used by /edit
  // second callback handles file uploads at that location
  servOTA.on(
      "/edit", HTTP_POST, []()
      { servOTA.send(200, "text/html", "<meta http-equiv='refresh' content='1;url=/main'>File uploaded. <a href=/main>Back to list</a>"); },
      handleFileUpload);
  servOTA.onNotFound([]()
                     {if(!handleFileRead(servOTA.uri())) servOTA.send(404, "text/plain", "404 FileNotFound"); });

  servOTA.begin();
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
    airtemp = String( airTemp,1 ) + "°C";
    humidity = String( airHumidity,1) + "%";
    pressure = String( airPressure, 0) + "mm";
    jsonDoc.clear();
    jsonDoc["airtemp"] = airtemp;  
    jsonDoc["humidity"] = humidity;
    jsonDoc["pressure"] = pressure;  
    if (currentMillis - storedMillis >= STOREINTERVAL) {  //update stored value once/hour
      pressurearray = updateStoredData("/temperature.txt", airtemp.toInt());
      pressurearray = updateStoredData("/humidity.txt", humidity.toInt());
      pressurearray = updateStoredData("/pressure.txt", pressure.toInt());
      pressurearray = updateStoredData("/time.txt", utcTime);
    }
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
//    if (currentMillis - previousPacketMillis >= UDPINTERVAL) {
      processPacket(packetSize);
//      previousPacketMillis = currentMillis;
//    } else {
//      udp.flush();
//    }
  }
    servOTA.handleClient();
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
/*
          stringBD.WaterDepth = nmeaStringData[1];
          depth = stringBD.WaterDepth;
          jsonDoc["depth"] = depth; 
          notifyClients(); 
*/
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
          heading = stringBD.HeadingM + "°";  
          jsonDoc["heading"] = heading;
          notifyClients();
        } else if (command == "HDM") {
          stringBD.HeadingM = String( int(nmeaStringData[1].toDouble()));
          heading = stringBD.HeadingM + "°";  
          jsonDoc["heading"] = heading; 
          notifyClients();
        } else if (command == "HDT") {
          stringBD.HeadingT = String( int(nmeaStringData[1].toDouble()));
          heading = stringBD.HeadingT + "°";  
          jsonDoc["heading"] = heading; 
          notifyClients();
        } else if (command == "MTW") {
          stringBD.WaterTemperature = nmeaStringData[1];
          watertemp = stringBD.WaterTemperature + "°C";  
          jsonDoc["watertemp"] = watertemp; 
          notifyClients();
        } else if (command == "MWD") {
          stringBD.WindDirectionT = String( int(nmeaStringData[1].toDouble()));
          winddir  = stringBD.WindDirectionT + " true";  
          jsonDoc["winddir"] = winddir;
          stringBD.WindDirectionM = String( int(nmeaStringData[3].toDouble()));
//          winddir  = stringBD.WindDirectionM + "m";  
//          jsonDoc["winddir"] = winddir;
          stringBD.WindSpeedK = String( int(nmeaStringData[5].toDouble()));
          windspeed = stringBD.WindSpeedK + " true";  
          jsonDoc["windspeed"] = windspeed; 
          notifyClients();
        } else if (command == "MWV") { //wind speed and angle
          stringBD.WindDirectionT = String( int(nmeaStringData[1].toDouble()));
          winddir = stringBD.WindDirectionT + " app";  
          jsonDoc["winddir"] = winddir;
          stringBD.WindSpeedK = nmeaStringData[3];  
          windspeed = stringBD.WindSpeedK;  // + nmeaStringData[4];
          jsonDoc["windspeed"] = windspeed + " app"; 
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
        } else if (command == "VDO") {  
        //
        } else if (command == "VDM") {  
        //
        } else if (command == "APB") {  
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
          Serial.println("unsupported NMEA0183 sentence");
        }
        jsonDoc.clear();
        j=0;
      }
    }
  }  
}
