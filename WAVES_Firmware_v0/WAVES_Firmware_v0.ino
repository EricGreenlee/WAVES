#include "batteryHandler.cpp"
#include "memoryStorage.cpp"
#include "power.cpp"
#include "receiveHandler.cpp"
#include "satelliteModem.cpp"
#include "solarHandler.cpp"
#include "transmitHandler.cpp"
#include "waterSensor.cpp"
#include "SPI.h"
#include "SparkFun_Swarm_Satellite_Arduino_Library.h"


SWARM_M138 mySwarm;

#include "FS.h"
#include "SD.h"
#include "SPI.h"

const int sd_cs = 5;

#define uS_TO_S_FACTOR 1000000 /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP 5       /* Time ESP32 will go to sleep (in seconds) */

RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR Swarm_M138_DateTimeData_t lastTransmitTime;
RTC_DATA_ATTR int readingCount = 0;
RTC_DATA_ATTR unsigned int readings[50];
RTC_DATA_ATTR Swarm_M138_DateTimeData_t readingTimes[50];

//deep sleep wake stub may be an option to keep in mind



void setup() {
  Serial.begin(115200);
  delay(1000);
  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));
  bool modemBegun = mySwarm.begin(Serial2);
  while (!modemBegun)  // If the begin failed, keep trying to begin communication with the modem
  {
    Serial.println(F("Could not communicate with the modem. It may still be booting..."));
    delay(500);
    modemBegun = mySwarm.begin(Serial2);
  }



  mySwarm.setTransmitDataCallback(&printMessageSent);

  Swarm_M138_GeospatialData_t info;

  Swarm_M138_Error_e geo = mySwarm.getGeospatialInfo(&info);

  Swarm_M138_DateTimeData_t dateTime;  // Allocate memory for the Date/Time

  Swarm_M138_Error_e time = mySwarm.getDateTime(&dateTime);

  while (geo != SWARM_M138_SUCCESS) {
    Serial.print(F("Swarm communication error: "));
    Serial.print((int)geo);
    Serial.print(F(" : "));
    //Serial.println(mySwarm.modemErrorString(err)); // Convert the error into printable text
    Serial.println(F("The modem may not have acquired a valid GPS fix..."));
    delay(500);
    geo = mySwarm.getGeospatialInfo(&info);
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


  unsigned int val = analogRead(A0);
  Serial.printf("Sensor Value: %d\n", val);
  readings[readingCount] = val;
  readingTimes[readingCount] = dateTime;
  readingCount++;

  if (readingCount >= 11) {
    //transmit
    uint64_t id;
    int length = readingCount * 2;  //2 bytes per reading
    uint8_t data[length];
    for (int i = 0; i < readingCount; i++) {  //pack the data array with 4 byte values 
    //NEED TO DO PACKET ENCODING(NUMBER OF READINGS, [READINGS], TIME OF MOST RECENT READING)
      data[i] = (readings[i] >> 8) & 0xff;
      data[i + 1] = readings[i] & 0xff;
    }
    Swarm_M138_Error_e transmit;
    transmit = mySwarm.transmitBinary(data, length, &id);

    if (transmit == SWARM_M138_SUCCESS) {
      Serial.print(F("The message has been added to the transmit queue. The message ID is "));
      serialPrintUint64_t(id);
      Serial.println();
      //may want to change this later, for now we assume constant power to modem
      //so, if something is in the transmit queue it will eventually get sent
      //put the writing to the SD card here
      //when done empty readings
      //format: bootcount,val,time
      //        bootcount+1, val1, time1
      //        bootcount+2, val2, time2
      String csvLine = "";
      for (int i = 0; i < readingCount; i++) {
        //csvLine.concat(String(bootCount).concat(","));
        
        csvLine.concat(readings[i]);
        csvLine.concat(",");
        csvLine.concat(readingTimes[i].DD);
        csvLine.concat(",");
        csvLine.concat(readingTimes[i].MM);
        csvLine.concat(",");
        csvLine.concat(readingTimes[i].YYYY);
        csvLine.concat(",");
        csvLine.concat(readingTimes[i].hh);
        csvLine.concat(",");
        csvLine.concat(readingTimes[i].mm);
        csvLine.concat(",");
        csvLine.concat(readingTimes[i].ss);
        csvLine.concat("\n");
        //Serial.println(csvLine);
      }
      char csv_arry[csvLine.length()+1];
      Serial.println(csvLine);
      strcpy(csv_arry, csvLine.c_str());
      //Serial.println(csv_arry);
      Serial2.end();
      delay(2000);
      while (!SD.begin(sd_cs)) {
        Serial.println("Card Mount Failed! Freezing...");
        delay(500);
        while(1) {}
      }

      uint8_t cardType = SD.cardType();

      if (cardType == CARD_NONE) {
        Serial.println("No SD card attached");
        while(1) {}
      }

      writeFile(SD, "/bloop.txt", "hello");
      appendFile(SD, "/water_level.csv", csv_arry);
      //clear RTC Memory
      for (int i = 0; i < 50; i++) {
        readings[i] = 0;
        readingTimes[i].DD = 0;
        readingTimes[i].hh = 0;
        readingTimes[i].MM = 0;
        readingTimes[i].YYYY = 0;
        readingTimes[i].mm = 0;
        readingTimes[i].ss = 0;
      }
      readingCount = 0;
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
    }
  }

  /*
  if(lastTransmitTime.DD == NULL){

  } else {
    long currTime,lastTime;

    currTime = (dateTime.hh * 60 * 60) + (dateTime.mm * 60) + dateTime.ss;
    lastTime = (lastTransmitTime.hh * 60 * 60) + (dateTime.mm * 60) + dateTime.ss;

    if(currTime - lastTime >= (15 * 60)){

    }

  }
*/
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) + " Seconds");



  Serial.println("Going to sleep now");
  delay(1000);
  Serial.flush();
  esp_deep_sleep_start();
  Serial.println("This will never be printed");
}

void loop() {
  // put your main code here, to run repeatedly:
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
  serialPrintUint64_t(*msg_id);
  Serial.println();
}



void listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if (!root) {
    Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels) {
        listDir(fs, file.name(), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

void createDir(fs::FS &fs, const char *path) {
  Serial.printf("Creating Dir: %s\n", path);
  if (fs.mkdir(path)) {
    Serial.println("Dir created");
  } else {
    Serial.println("mkdir failed");
  }
}

void removeDir(fs::FS &fs, const char *path) {
  Serial.printf("Removing Dir: %s\n", path);
  if (fs.rmdir(path)) {
    Serial.println("Dir removed");
  } else {
    Serial.println("rmdir failed");
  }
}

void readFile(fs::FS &fs, const char *path) {
  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.print("Read from file: ");
  while (file.available()) {
    Serial.write(file.read());
  }
  file.close();
}

void writeFile(fs::FS &fs, const char *path, const char *message) {
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

void appendFile(fs::FS &fs, const char *path, const char *message) {
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  if (file.print(message)) {
    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}

void renameFile(fs::FS &fs, const char *path1, const char *path2) {
  Serial.printf("Renaming file %s to %s\n", path1, path2);
  if (fs.rename(path1, path2)) {
    Serial.println("File renamed");
  } else {
    Serial.println("Rename failed");
  }
}

void deleteFile(fs::FS &fs, const char *path) {
  Serial.printf("Deleting file: %s\n", path);
  if (fs.remove(path)) {
    Serial.println("File deleted");
  } else {
    Serial.println("Delete failed");
  }
}

void testFileIO(fs::FS &fs, const char *path) {
  File file = fs.open(path);
  static uint8_t buf[512];
  size_t len = 0;
  uint32_t start = millis();
  uint32_t end = start;
  if (file) {
    len = file.size();
    size_t flen = len;
    start = millis();
    while (len) {
      size_t toRead = len;
      if (toRead > 512) {
        toRead = 512;
      }
      file.read(buf, toRead);
      len -= toRead;
    }
    end = millis() - start;
    Serial.printf("%u bytes read for %u ms\n", flen, end);
    file.close();
  } else {
    Serial.println("Failed to open file for reading");
  }


  file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }

  size_t i;
  start = millis();
  for (i = 0; i < 2048; i++) {
    file.write(buf, 512);
  }
  end = millis() - start;
  Serial.printf("%u bytes written for %u ms\n", 2048 * 512, end);
  file.close();
}
