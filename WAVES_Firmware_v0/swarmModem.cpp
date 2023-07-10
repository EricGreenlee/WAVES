/*******************************************************
Includes
*******************************************************/
#include "arduino.h"
#include "SparkFun_Swarm_Satellite_Arduino_Library.h"
#include "swarmModem.h"
#include "waterSensor.h"

/*******************************************************
swarmModem Code
*******************************************************/
#define MAX_VOLTAGE     10  //Volts
#define MAX_TEMPERATURE 75  //Degrees C
#define MIN_TEMPERATURE 0   //Degrees C

SWARM_M138 mySwarm;

RTC_DATA_ATTR Swarm_M138_DateTimeData_t lastTransmitTime;
RTC_DATA_ATTR Swarm_M138_DateTimeData_t readingTimes[50];

Swarm_M138_GeospatialData_t geoInfo;
Swarm_M138_DateTimeData_t dateTime;  // Allocate memory for the Date/Time


void swarmSerialInit(){

  bool modemBegun = mySwarm.begin(Serial2);
  while (!modemBegun) {  // If the begin failed, keep trying to begin communication with the modem
    Serial.println(F("Could not communicate with the modem. It may still be booting..."));
    delay(500);
    modemBegun = mySwarm.begin(Serial2);
  }
  mySwarm.setTransmitDataCallback(&printMessageSent);

}

float swarmGetVoltage(){
  float voltage;
  Swarm_M138_Error_e err = mySwarm.getCPUvoltage(&voltage);
  
  if (err == SWARM_M138_SUCCESS) {
    return voltage;
  } else {
    Serial.print(F("Swarm communication error: "));
    Serial.print((int)err);
    Serial.print(F(" : "));
    Serial.println(mySwarm.modemErrorString(err)); // Convert the error into printable text
    return -1;
  }
}

float swarmGetTemperature(){
  float temperature;
  Swarm_M138_Error_e err = mySwarm.getTemperature(&temperature);
  
  if (err == SWARM_M138_SUCCESS) {
    return temperature;
  } else {
    Serial.print(F("Swarm communication error: "));
    Serial.print((int)err);
    Serial.print(F(" : "));
    Serial.println(mySwarm.modemErrorString(err)); // Convert the error into printable text
    return -1;
  }
}

void swarmGpsInit(){

  Swarm_M138_Error_e geo = mySwarm.getGeospatialInfo(&geoInfo);
  Swarm_M138_Error_e time = mySwarm.getDateTime(&dateTime);

  while (geo != SWARM_M138_SUCCESS) {
    Serial.print(F("Swarm communication error: "));
    Serial.print((int)geo);
    Serial.print(F(" : "));
    //Serial.println(mySwarm.modemErrorString(err)); // Convert the error into printable text
    Serial.println(F("The modem may not have acquired a valid GPS fix..."));
    delay(500);
    geo = mySwarm.getGeospatialInfo(&geoInfo);
    time = mySwarm.getDateTime(&dateTime);
  }

  Serial.print(F("getDateTime returned: "));
  Serial.print(dateTime.YYYY);
  Serial.print(F("/"));
  Serial.print(dateTime.MM);
  Serial.print(F("/"));
  Serial.print(dateTime.DD);
  Serial.print(F(" "));
  Serial.print(dateTime.hh);
  Serial.print(F(":"));
  Serial.print(dateTime.mm);
  Serial.print(F(":"));
  Serial.println(dateTime.ss);
}

void logReadingTime(int index){
  readingTimes[index] = dateTime;
}

bool swarmTransmit(int numReadings){
//PACKET ENCODING: {NUMBER OF READINGS, [READINGS], TIME OF MOST RECENT READING, CPU TEMPERATURE, CPU VOLTAGE}

  // length: 1 byte for number of readings, 2 bytes per reading, 3 bytes for hours, mins, secs, 2 bytes for temp and voltage)
  int length = 1 + (numReadings * 2) + 3 + 2;  
  uint8_t data[length];

  uint64_t id;
  data[0] = numReadings & 0xff;
  int j = 1;
  for (int i = 0; i < numReadings; i++) {  //pack the data array with 2 byte values 
    data[j] = (getReading(i) >> 8) & 0xff;
    data[j + 1] = getReading(i) & 0xff;
    j += 2;
  }
  data[j] = dateTime.hh;
  j++;
  data[j] = dateTime.mm;
  j++;
  data[j] = dateTime.ss;
  j++;

  //Converts the returned temperature from a float to an int between 0-255 so it can be sent as 1 byte
  //The 0-255 range is proportional to a temperature range of 0-75 degrees C 
  float currentTemp = swarmGetTemperature();
  int tempScalar;
  if (currentTemp > MAX_TEMPERATURE){ tempScalar = 255; } else if (currentTemp < MIN_TEMPERATURE){ tempScalar = 0; }
  else {
    tempScalar = (int)(currentTemp*255.0/MAX_TEMPERATURE);
  }
  data[j] = tempScalar;
  j++;

  //Converts the returned voltage from a float to an int between 0-255 so it can be sent as 1 byte
  //The 0-255 range is proportional to a voltage range of 0-10 V
  float currentVolt = swarmGetVoltage();
  int voltScalar;
  if (currentVolt > MAX_VOLTAGE){ voltScalar = 255; } else if (currentVolt < 0){ voltScalar = 0; }
  else {
    voltScalar = (int)(currentVolt*255.0/MAX_VOLTAGE);
  }
  data[j] = voltScalar;

  Swarm_M138_Error_e transmit;
  transmit = mySwarm.transmitBinary(data, length, &id);

  if (transmit == SWARM_M138_SUCCESS) {
      Serial.print(F("The message has been added to the transmit queue. The message ID is "));
      serialPrintUint64_t(id);
      return(true);
      //may want to change this later, for now we assume constant power to modem
      //so, if something is in the transmit queue it will eventually get sent
      //put the writing to the SD card here
      //when done empty readings
      //format: bootcount,val,time
      //        bootcount+1, val1, time1
      //        bootcount+2, val2, time2
  } else {
      Serial.print(F("Swarm communication error: "));
      Serial.print((int)transmit);
      Serial.print(F(" : "));
      Serial.print(mySwarm.modemErrorString(transmit));  // Convert the error into printable text
      if (transmit == SWARM_M138_ERROR_ERR)              // If we received a command error (ERR), print it
      {
        Serial.print(F(" : "));
        Serial.print(mySwarm.commandError);
        Serial.print(F(" : "));
        Serial.println(mySwarm.commandErrorString((const char *)mySwarm.commandError));
      } else
        Serial.println();
      return(false);
    }
}

void swarmCloseSerial(){
  Serial2.end();
  delay(2000);
}

String getFullTime(int index){
  String time = "";
  time.concat(readingTimes[index].DD);
  time.concat(",");
  time.concat(readingTimes[index].MM);
  time.concat(",");
  time.concat(readingTimes[index].YYYY);
  time.concat(",");
  time.concat(readingTimes[index].hh);
  time.concat(",");
  time.concat(readingTimes[index].mm);
  time.concat(",");
  time.concat(readingTimes[index].ss);
  return time;
}

void clearReadingTimes(){
  for (int i = 0; i < READINGS_ARRAY_LENGTH; i++) {
    readingTimes[i].DD = 0;
    readingTimes[i].hh = 0;
    readingTimes[i].MM = 0;
    readingTimes[i].YYYY = 0;
    readingTimes[i].mm = 0;
    readingTimes[i].ss = 0;
  }
}

// Callback: printMessageSent will be called when a new unsolicited $TD SENT message arrives.
void printMessageSent(const int16_t *rssi_sat, const int16_t *snr, const int16_t *fdev, const uint64_t *msg_id) {
  Serial.print(F("New $TD SENT message received:"));
  Serial.print(F("  RSSI = "));
  Serial.print(*rssi_sat);
  Serial.print(F("  SNR = "));
  Serial.print(*snr);
  Serial.print(F("  FDEV = "));
  Serial.print(*fdev);
  Serial.print(F("  Message ID: "));
  Serial.print(*msg_id);
  Serial.println();
}

void serialPrintUint64_t(uint64_t theNum) {
  // Convert uint64_t to string
  // Based on printLLNumber by robtillaart
  // https://forum.arduino.cc/index.php?topic=143584.msg1519824#msg1519824

  char rev[21];  // Char array to hold to theNum (reversed order)
  char fwd[21];  // Char array to hold to theNum (correct order)
  unsigned int i = 0;
  if (theNum == 0ULL)  // if theNum is zero, set fwd to "0"
  {
    fwd[0] = '0';
    fwd[1] = 0;  // mark the end with a NULL
  } else {
    while (theNum > 0) {
      rev[i++] = (theNum % 10) + '0';  // divide by 10, convert the remainder to char
      theNum /= 10;                    // divide by 10
    }
    unsigned int j = 0;
    while (i > 0) {
      fwd[j++] = rev[--i];  // reverse the order
      fwd[j] = 0;           // mark the end with a NULL
    }
  }

  Serial.print(fwd);
}

