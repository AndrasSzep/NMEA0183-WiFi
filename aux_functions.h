/* 
by Dr.András Szép under GNU General Public License (GPL).
*/

int parseNMEA0183( String sentence, String data[]);
void initFS();
void storeString(String path, String content);
String retrieveString(String path);
String convertGPString(String input) ;
String int2string(int number);
String readStoredData( char* filename);
String updateStoredData(char* filename, int newValue);
