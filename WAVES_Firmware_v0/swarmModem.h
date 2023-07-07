/*******************************************************
swarmModem.h
*******************************************************/
#ifndef SWARM_MODEM_H
#define SWARM_MODEM_H

String getFullTime(int);

void swarmSerialInit();
void swarmGpsInit();
void logReadingTime(int);
bool swarmTransmit(int);
void swarmCloseSerial();
void clearReadingTimes();

void printMessageSent(const int16_t *, const int16_t *, const int16_t *, const uint64_t *);
void serialPrintUint64_t(uint64_t);

#endif