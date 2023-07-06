/*******************************************************
waterSensor.h
//*******************************************************/
#ifndef WATER_SENSOR_H
#define WATER_SENSOR_H

#define READINGS_ARRAY_LENGTH 50

void takeSensorReading(int);
unsigned int getReading(int);
void clearSensorReadings();

#endif