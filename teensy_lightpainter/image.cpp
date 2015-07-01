#include "image.h"

bool Image::readHeader(const char *fname) {
  uint8_t  bmpDepth;              // Bit depth (currently must be 24)

  // Open requested file on SD card
  if ((file = SD.open(fname, FILE_READ)) == NULL) {
    Serial.print(F("loadImage: file not found: "));
    Serial.println(fname);
    return false;
  }

  // Parse BMP header
  if(read16() != 0x4D42) { // BMP signature
    Serial.println(F("loadImage: file doesn't look like a BMP"));
    return false;
  }
  
  Serial.print(F("File size: ")); Serial.println(read32());
  (void)read32(); // Read & ignore creator bytes
  bmpImageoffset = read32(); // Start of image data
  Serial.print(F("Image Offset: ")); Serial.println(bmpImageoffset, DEC);
  // Read DIB header
  Serial.print(F("Header size: ")); Serial.println(read32());
  bmpWidth  = read32();
  bmpHeight = read32();
  if(read16() != 1) { // # planes -- must be '1'
    Serial.println(F("loadImage: invalid n. of planes"));
    return false;
  }
  
  bmpDepth = read16(); // bits per pixel
  Serial.print(F("Bit Depth: ")); Serial.println(bmpDepth);
  if((bmpDepth != 24) || (read32() != 0)) { // 0 = uncompressed {
    Serial.println(F("loadImage: invalid pixel format"));
    return false;
  }

  Serial.print(F("Image size: "));
  Serial.print(bmpWidth);
  Serial.print('x');
  Serial.println(bmpHeight);

  // BMP rows are padded (if needed) to 4-byte boundary
  rowSize = (bmpWidth * 3 + 3) & ~3;

  // If bmpHeight is negative, image is in top-down order.
  // This is not canon but has been observed in the wild.
  if(bmpHeight < 0) {
    bmpHeight = -bmpHeight;
    flip      = false;
  }
  return true;
}

void Image::writeLine(int row, Adafruit_NeoPixel *strip) {
  int      w, col;
  uint8_t  r, g, b;
  uint32_t pos = 0;
  uint8_t  sdbuffer[3*64]; // pixel buffer (R+G+B per pixel)
  uint8_t  buffidx = sizeof(sdbuffer); // Current position in sdbuffer

  // Seek to start of scan line.
  if(flip) // Bitmap is stored bottom-to-top order (normal BMP)
    pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize;
  else     // Bitmap is stored top-to-bottom
    pos = bmpImageoffset + row * rowSize;
  if(file.position() != pos) { // Need seek?
    file.seek(pos);
    buffidx = sizeof(sdbuffer); // Force buffer reload
  }

  for (col=0; col<bmpWidth; col++) { // For each pixel...
    // Time to read more pixel data?
    if (buffidx >= sizeof(sdbuffer)) { // Indeed
      file.read(sdbuffer, sizeof(sdbuffer));
      buffidx = 0; // Set index to beginning
    }

    // Convert pixel from BMP to NeoPixel format
    b = sdbuffer[buffidx++];
    g = sdbuffer[buffidx++];
    r = sdbuffer[buffidx++];
    strip->setPixelColor(col, strip->Color(r, g, b));

    /*
    if (row == 0) {
      Serial.print("row "); Serial.print(row, DEC); Serial.print(" column "); Serial.print(col, DEC); 
      Serial.print(" = R"); Serial.print(r, DEC); 
      Serial.print(" G"); Serial.print(g, DEC);
      Serial.print(" B"); Serial.println(b, DEC);
    }
    */
  } // end pixel
}

Image::Image(const char *fname) {
  lines = 0;
  if (!readHeader(fname)) {
    Serial.println("Failed to read header");
    // lines==0 so image is invalid
    return;
  }
  
  lines = bmpHeight;
  if (bmpWidth != 60) {
    Serial.println("width is not 60, weirdness ahead");
  }
}

Image::~Image() {
  file.close();
}

// utility methods
uint16_t Image::read16() {
  uint16_t result;
  ((uint8_t *)&result)[0] = file.read(); // LSB
  ((uint8_t *)&result)[1] = file.read(); // MSB
  return result;
}

uint32_t Image::read32() {
  uint32_t result;
  ((uint8_t *)&result)[0] = file.read(); // LSB
  ((uint8_t *)&result)[1] = file.read();
  ((uint8_t *)&result)[2] = file.read();
  ((uint8_t *)&result)[3] = file.read(); // MSB
  return result;
}

