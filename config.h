//#define DEBUG
//#define STOREWIFI
char storedWIFI[80],storedPWD[80];
char* myWIFI        = "1114297_Trooli.uk";
char* myPWD    = "aZRkOKpL";
const char* servername  = "nmea0183";     //nDNS servername - http://servername.local
unsigned int UDPPort = 10110; // 10110 is the default NMEA0183 port#
#define MAX_NMEA0183_MSG_BUF_LEN 4096
String sentence = "$GPGGA,123456,S,12,34.5678,N,5678.12,34,A*12";
#define MAX_NMEA_FIELDS  64