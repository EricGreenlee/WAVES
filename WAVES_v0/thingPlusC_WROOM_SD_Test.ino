/*
  Written for ESP32
  Just test to see if we can read/write to an SD card over SPI

  Be sure to set the CS pin correctly at top of code below

  Common:
  1048576 bytes read for 3268 ms
  1048576 bytes written for 6269 ms
*/

#ifdef noModemGeospatial

const char myLatitude[] =   "55.000";     //                  <-- Update this with your latitude if desired
const char myLongitude[] =  "-1.000";     //                  <-- Update this with your longitude if desired
const char myAltitude[] =   "100";        //                  <-- Update this with your altitude in m if desired

#else

#include "SparkFun_Swarm_Satellite_Arduino_Library.h" //Click here to get the library:  http://librarymanager/All#SparkFun_Swarm_Satellite

SWARM_M138 mySwarm;

//#if defined(ARDUINO_ESP32_DEV)
// If you are using the ESP32 Dev Module board definition, you need to create the HardwareSerial manually:
//#pragma message "Using HardwareSerial for M138 communication - on ESP32 Dev Module"
//HardwareSerial swarmSerial(2); //TX on 17, RX on 16
//#else
// Serial1 is supported by the new SparkFun ESP32 Thing Plus C board definition
//#pragma message "Using Serial1 for M138 communication"
//#define swarmSerial Serial1 // Use Serial1 to communicate with the modem. Change this if required.
//#endif

#endif

#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <SoftwareSerial.h>

#include "WiFi.h"
#include "HTTPClient.h"
//#include "secrets.h"

#include "Sgp4.h"

const int sd_cs = 5; //Thing Plus C

const char celestrakServer[] = "https://celestrak.org";

const char getSwarmTLE[] = "NORAD/elements/gp.php?GROUP=swarm&FORMAT=tle";

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// The Swarm Pass-Checker data can be downloaded from their server

const char swarmPassCheckerServer[] = "https://bumblebee.hive.swarm.space";

const char passCheckerAPI[] = "api/v1/passes?";

const char latPrefix[] = "lat=";
const char lonPrefix[] = "&lon=";
const char altPrefix[] = "&alt=";
const char mergeSuffix[] = "&merge=false";

// The pass checker data is returned in non-pretty JSON format:
#define startOfFirstStartPass 32  // 8000\r\n{"passes":[{"start_pass":"YYYY-MM-DDTHH:MM:SSZ
                                  // ----- - --------------------------^
#define startPassDateTimeLength 19
#define endOffset 15 // Offset from the start_pass "Z" to the start of the end_pass
#define elevationOffset 41 // Offset from the end_pass "Z" to the start of the max_elevation
#define startOffset 12 // Offset from the "s" of start_pass to the start of the year

// At the time or writing:
//  Swarm satellites are named: SPACEBEE-n or SPACEBEENZ-n
//  SPACEBEE numbers are 1 - 155 (8 and 9 are missing)
//  SPACEBEENZ numbers are 1 - 22
const int maxSats = 176;

// Stop checking when we find this many satellite duplications for a single satellite
// (When > 24 hours of passes have been processed)
#define satPassLimit 7

// Ignore any false positives (satellites with fewer than this many passes)
#define satPassFloor 2

// Check for a match: start_pass +/- 2.5 seconds (in Julian Days)
const double predictionStartError = 0.000029;
// Check for a match: end_pass +/- 2.5 seconds (in Julian Days)
const double predictionEndError = 0.000029;
// Check for a match on max_elevation. Accuracy is worst at Zenith, much better towards the horizon.
const double maxElevationError = 2.0;
const double elevationZenith = 70.0;
const double maxElevationErrorZenith = 5.0;

#define nzOffset 100000 // Add this to the satellite number to indicate SPACEBEENZ

void listDir(fs::FS &fs, const char * dirname, uint8_t levels) {
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

void createDir(fs::FS &fs, const char * path) {
  Serial.printf("Creating Dir: %s\n", path);
  if (fs.mkdir(path)) {
    Serial.println("Dir created");
  } else {
    Serial.println("mkdir failed");
  }
}

void removeDir(fs::FS &fs, const char * path) {
  Serial.printf("Removing Dir: %s\n", path);
  if (fs.rmdir(path)) {
    Serial.println("Dir removed");
  } else {
    Serial.println("rmdir failed");
  }
}

void writeBuffer(fs::FS &fs, const char *path, uint8_t *buff, int howMany){
  Serial.printf("Writing buffer to file %s\n",path);
  File file = fs.open(path);
  if(!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  file.write(buff,howMany);
}

void readFile(fs::FS &fs, const char * path) {
  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.print("Read from file: ");
  while (file.available()) {
    //Serial.println("**");
    Serial.write(file.read());
  }
  file.close();
}

void writeFile(fs::FS &fs, const char * path, const char * message) {
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

void appendFile(fs::FS &fs, const char * path, const char * message) {
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

void renameFile(fs::FS &fs, const char * path1, const char * path2) {
  Serial.printf("Renaming file %s to %s\n", path1, path2);
  if (fs.rename(path1, path2)) {
    Serial.println("File renamed");
  } else {
    Serial.println("Rename failed");
  }
}

void deleteFile(fs::FS &fs, const char * path) {
  Serial.printf("Deleting file: %s\n", path);
  if (fs.remove(path)) {
    Serial.println("File deleted");
  } else {
    Serial.println("Delete failed");
  }
}

void testFileIO(fs::FS &fs, const char * path) {
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


void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("SD test");

  if (!SD.begin(sd_cs)) {
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }

  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);

  //Serial2.begin(4000000);
  //Serial2.end();

  listDir(SD, "/", 0);
  createDir(SD, "/mydir");
  listDir(SD, "/", 0);
  removeDir(SD, "/mydir");
  listDir(SD, "/", 2);
  writeFile(SD, "/hello.txt", "Hello ");
  deleteFile(SD, "/swarmPP.txt");
  writeFile(SD, "/mySwmPP.txt", "Wow I think this program actually works! \n");
  deleteFile(SD, "/swarmTLE.txt");
  writeFile(SD, "/mySwmTLE.txt", "O");
  appendFile(SD, "/hello.txt", "World!\n");
  readFile(SD, "/hello.txt");
  deleteFile(SD, "/foo.txt");
  renameFile(SD, "/hello.txt", "/foo.txt");
  readFile(SD, "/foo.txt");
  testFileIO(SD, "/test.txt");
  //testFileIO(SD, "/mySwmPP.txt");
  readFile(SD, "/mySwmPP.txt");
  writeFile(SD, "/swarmTLEEE.txt", "I\n");
  readFile(SD, "/swarmTLEEE.txt");
  
 
  Serial.printf("Total space: %lluMB\n", SD.totalBytes() / (1024 * 1024));
  Serial.printf("Used space: %lluMB\n", SD.usedBytes() / (1024 * 1024));



  //Swarm 
  
  #ifdef swarmPowerEnablePin
  pinMode(swarmPowerEnablePin, OUTPUT); // Enable modem power 
  digitalWrite(swarmPowerEnablePin, HIGH);
  #endif

  delay(1000);
  
  Serial.begin(115200);
  Serial.println(F("Example : Swarm Two-Line Elements for your location"));

  while (Serial.available()) Serial.read(); // Empty the serial buffer
  Serial.println(F("Press any key to begin..."));
  while (!Serial.available()); // Wait for a keypress
  Serial.println();

  


  ////////////////////

  #ifndef noModemGeospatial
  //mySwarm.enableDebugging(); // Uncomment this line to enable debug messages on Serial

  //SoftwareSerial swarmserial = SoftwareSerial(16,17);

  //swarmserial.begin(9600);
  //swarmserial.setPins(16,17);
  //swarmserial.end();
  appendFile(SD, "/swarmTLEEE.txt", "very weird\n");
  readFile(SD, "/swarmTLEEE.txt");

  bool modemBegun = mySwarm.begin(Serial2); // Begin communication with the modem

  Serial2.end();
  
  appendFile(SD, "/swarmTLEEE.txt", "very weird\n");
  readFile(SD, "/swarmTLEEE.txt");

  
  
  while (!modemBegun) // If the begin failed, keep trying to begin communication with the modem
  {
    Serial.println(F("Could not communicate with the modem. It may still be booting..."));
    delay(2000);
    //modemBegun = mySwarm.begin(Serial2);
  }

  

  

  // Call getGeospatialInfo to request the most recent geospatial information
  Swarm_M138_GeospatialData_t info;
  
  Swarm_M138_Error_e err = mySwarm.getGeospatialInfo(&info);
  
  //while (err != SWARM_M138_SUCCESS)
  //{
    Serial.print(F("Swarm communication error: "));
    Serial.print((int)err);
    Serial.print(F(" : "));
    Serial.println(mySwarm.modemErrorString(err)); // Convert the error into printable text
    Serial.println(F("The modem may not have acquired a valid GPS fix..."));
    delay(2000);
    err = mySwarm.getGeospatialInfo(&info);
  //}

  Serial.print(F("getGeospatialInfo returned: "));
  Serial.print(info.lat, 4);
  Serial.print(F(","));
  Serial.print(info.lon, 4);
  Serial.print(F(","));
  Serial.println(info.alt);
#endif



  

  
  
  
}

void loop() {

}
