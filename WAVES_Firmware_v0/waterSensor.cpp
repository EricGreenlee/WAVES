/*******************************************************
Includes
*******************************************************/
#include "arduino.h"
#include "waterSensor.h"
#include "swarmModem.h"
#include "memoryStorage.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"

/*******************************************************
waterSensor Code
*******************************************************/
RTC_DATA_ATTR unsigned int readings[READINGS_ARRAY_LENGTH]; // **move declaration to .h file eventually (same with swarmModem RTC data)

void takeSensorReading(int index){
  unsigned int sensorReading = analogRead(A0); // **replace with #define
  Serial.printf("Sensor Value: %d\n", sensorReading);
  readings[index] = sensorReading;
}

unsigned int getReading(int index){
  return(readings[index]);
}

void clearSensorReadings(){
  for (int i = 0; i < READINGS_ARRAY_LENGTH; i++){
    readings[i] = 0;
  }
}
