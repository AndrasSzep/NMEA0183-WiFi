#define DEBUG
//#define STOREWIFI
#define ENVSENSOR       //environmental sensors connected

const char*   servername  = "nmea0183";     //nDNS servername - http://servername.local
unsigned int UDPPort = 10110; // 10110 is the default NMEA0183 port#

#define TIMEPERIOD 60.0

#define MAX_NMEA0183_MSG_BUF_LEN 4096
#define MAX_NMEA_FIELDS  64

#define SDA_PIN 26
#define SCL_PIN 32