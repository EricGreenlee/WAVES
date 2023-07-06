//*******************************************************
//memoryStorage.h
//*******************************************************
#ifndef MEMORY_STORAGE_H
#define MEMORY_STORAGE_H

#include "FS.h"

void saveReadingsToSd(int);

void listDir(fs::FS &, const char *, uint8_t);
void createDir(fs::FS &, const char *);
void removeDir(fs::FS &, const char *);
void readFile(fs::FS &, const char *);
void writeFile(fs::FS &, const char *, const char *);
void appendFile(fs::FS &, const char *, const char *);
void renameFile(fs::FS &, const char *, const char *);
void deleteFile(fs::FS &, const char *);
void testFileIO(fs::FS &, const char *);

#endif