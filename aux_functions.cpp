
#include <Arduino.h>  //necessary for the String variables
#include <SPIFFS.h>

int parseNMEA0183( String sentence, String data[]) {
  int noOfFields = 0;
  // Check if the sentence starts with a valid NMEA0183 header
  if (!sentence.startsWith("$")) {
    return -1;
  }

  // Find the asterisk marking the end of the sentence
  int endIdx = sentence.indexOf('*');
  if (endIdx == -1) {
    return -1;
  }

  // Check if the sentence length is sufficient for the checksum
  int sentenceLen = sentence.length();
  if (endIdx + 3 > sentenceLen) {
    return -1;
  }
  //count the number of fields
  int count = 0;
  for (size_t i = 0; i < sentence.length(); i++) {
    if (sentence.charAt(i) == ',') {
      count++;
    }
  }
  noOfFields = count;
  // Extract the data between the header and the asterisk
  String sentenceData = sentence.substring(3, endIdx+3);
  
  // Split the sentence data into fields using the comma delimiter
  int fieldIdx = 0;
  int startPos = 0;
  int commaIdx = sentenceData.indexOf(',', startPos);
  while (commaIdx != -1 && fieldIdx < noOfFields) {
    data[fieldIdx++] = sentenceData.substring(startPos, commaIdx);
    startPos = commaIdx + 1;
    commaIdx = sentenceData.indexOf(',', startPos);
  }
  // Add the remaining field (checksum) if there is room in the array
  data[fieldIdx] = sentence.substring(endIdx-1,endIdx);
  data[fieldIdx+1] = sentence.substring(endIdx+1,endIdx+3);
  return noOfFields+1;
}


void storeString(String path, String content) {
  File file = SPIFFS.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if (file.print(content)) {
    Serial.printf("String %s stored at %s successfully\n", content.c_str(), path.c_str());
  } else {
    Serial.println("String store failed");
  }
  file.close();
}

String retrieveString(String path) {
  File file = SPIFFS.open(path, FILE_READ);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return "";
  }
  String content = file.readString();
  file.close();
  Serial.printf("String %s from %s retrieved succesfully\n", content.c_str() , path.c_str());
  return content;
}

String convertGPString(String input) {
  // Extract degrees, minutes, and seconds from the input string
  int idx = input.indexOf(".");
  int degrees = input.substring(0, (idx-2)).toInt();
  int minutes = input.substring( (idx-2), idx).toInt();
  int seconds = int(input.substring( idx, idx+5).toFloat()*100.0*0.60);
  // Format the converted values into the desired format
  String output = String(degrees) + "º" + String(minutes) + "'" +  String(seconds) + "\"";
  return output;
}
