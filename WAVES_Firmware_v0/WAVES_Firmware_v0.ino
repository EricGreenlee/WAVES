#include "memoryStorage.h"
#include "swarmModem.h"
#include "waterSensor.h"

#include "FS.h"
#include "SD.h"
#include "SPI.h"

#define uS_TO_S_FACTOR  1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP   60 * 5   /* Time ESP32 will go to sleep (in seconds) */

RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR int readingCount = 0;

//deep sleep wake stub may be an option to keep in mind

void setup() {

//General Setup and Increment Boot Count
  Serial.begin(115200);
  delay(1000);
  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));

//Initialize SWARM Modem, Serial2
  swarmSerialInit();

  Serial.println("Temperature: " + String(swarmGetTemperature()));
  Serial.println("Voltage: " + String(swarmGetVoltage()));

//Get GPS fix and current time
  swarmGpsInit();

//Take Sensor Reading - Log Distance and Time
  takeSensorReading(readingCount);
  logReadingTime(readingCount);
  readingCount++;

//Check if Enough Readings Have Been Collected
  if (readingCount >= 11) {

  //Attempt Data Transmission
    bool transmitSuccess = swarmTransmit(readingCount);
    if (transmitSuccess) {

    //Close Serial2
      swarmCloseSerial();

    //Save Data to SD Card in csv Format, Erase Data from the RTC Array
      saveReadingsToSd(readingCount);
      readingCount = 0;
    }
  }

/*
  if(lastTransmitTime.DD == NULL){ } else {
    long currTime,lastTime;
    currTime = (dateTime.hh * 60 * 60) + (dateTime.mm * 60) + dateTime.ss;
    lastTime = (lastTransmitTime.hh * 60 * 60) + (dateTime.mm * 60) + dateTime.ss;
    if(currTime - lastTime >= (15 * 60)){
    }
  }
*/

//Set Sleep Timer and Enter Deep Sleep Mode
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) + " Seconds");
  Serial.println("Going to sleep now");
  delay(1000);
  Serial.flush();
  esp_deep_sleep_start();
  Serial.println("This will never be printed");

} // END SETUP


//Loop Remains Unused

void loop() {
  // put your main code here, to run repeatedly:
} // END LOOP
