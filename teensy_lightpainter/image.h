#ifndef __LPIMAGE_H
#define __LPIMAGE_H

#include <Arduino.h>
#include <SD.h>
#include <Adafruit_NeoPixel.h>

class Image {
public:
  Image(const char *fname);
  ~Image();
  
  int lines;
  void writeLine(int line, Adafruit_NeoPixel *strip);
  
private:
  File file;
  int  bmpWidth, bmpHeight;
  uint32_t bmpImageoffset;        // Start of image data in file
  uint32_t rowSize;
  bool flip;
  
  bool readHeader(const char *fname);
  void readData();
  uint16_t read16();
  uint32_t read32();
};

#endif
