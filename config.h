/* 
by Dr.András Szép under GNU General Public License (GPL).
*/
#define DEBUG
#define STOREWIFI
#define ENVSENSOR       //environmental sensors connected

#define OTAPORT 8080    //OTA port
const char*   servername  = "nmea";     //nDNS servername - http://servername.local
unsigned int UDPPort = 10110; // 10110 is the default NMEA0183 port#
const char* ntpServer = "europe.pool.ntp.org";
const int timeZone = 0;  // Your time zone in hours

#define TIMEPERIOD 60.0

#define MAX_NMEA0183_MSG_BUF_LEN 4096
#define MAX_NMEA_FIELDS  64

#define SDA_PIN 26
#define SCL_PIN 32