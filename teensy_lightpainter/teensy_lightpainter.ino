#include <SD.h>
#include <Adafruit_NeoPixel.h>
#include <SPI.h>
#include "image.h"

#define FRAMEDELAY 20 // msec between lines
int frameDelay = FRAMEDELAY;
int frameBlank = 0;

// datalink protocol variables
#define PImage     0
#define PBright    1
#define PFDelay    2
#define PFBlank    3

#define TAILPIN 2
#define LEDPIN 5
#define RXDAT  6  // chan 4
#define SWNAV  7  // chan 5 with RX switch to avoid interrupts
#define RXCLK  8  // chan 6
#define SW1    9
#define SDCS  10 // SD card SPI = pins 10-13
#define SW2   14
Adafruit_NeoPixel wing = Adafruit_NeoPixel(60, LEDPIN, NEO_GRB + NEO_KHZ800);
#define NTAIL 5
Adafruit_NeoPixel tail = Adafruit_NeoPixel(NTAIL, TAILPIN, NEO_RGB + NEO_KHZ400);

const uint32_t 
  RED     = (255<<16),
  GREEN   = (255<<8),
  BLUE    = 255,
  WHITE   = RED | GREEN | BLUE,
  PURPLE  = RED | BLUE,
  CLEAR   = 0;

Image *img = NULL;

int imgIndex = 0;
char fname[10];

int line;

void iRXDAT();
void iRXNAV();
void iRXCLK();
void parse(bool sync, int val);
void process(int ptype, int val);

volatile bool dataReady = false;
volatile bool dataOverflow = false;
volatile bool syncFlag = false;

volatile int rxData = 0;
volatile bool rxNav = true;
volatile int navRaw;
volatile bool invalidDAT, invalidNAV, invalidCLK;
volatile unsigned long lastNAV;

void loadImage() {
  if (img != NULL)
    delete(img);
  sprintf(fname, "%d.bmp", imgIndex);
  Serial.print("reading image "); Serial.println(fname);
  img = new Image(fname);
  if (img->lines == 0) {
    Serial.println("failed");
    return;
  } else {
    Serial.print("success: "); Serial.print(img->lines, DEC); Serial.println(" lines read");
  }
  line = 0;
}

void setup() {
  Serial.begin(115200);
  if (false) { // for debugging startup
    for (int i = 5; i > 0; i--) {
      Serial.println(i, DEC);
      delay(1000);
    }
  }
  
  wing.begin();
  wing.setBrightness(64);

  tail.begin();
  tail.setBrightness(255);
  
  pinMode(SW1, INPUT);
  pinMode(SW2, INPUT);

  pinMode(SWNAV, INPUT);
  *portConfigRegister(SWNAV) |= PORT_PCR_PE; //pull enable
  *portConfigRegister(SWNAV) &= ~PORT_PCR_PS; //pull down  

  pinMode(RXDAT, INPUT);
  pinMode(RXCLK, INPUT);
  attachInterrupt(RXDAT, iRXDAT, CHANGE);
  attachInterrupt(RXCLK, iRXCLK, CHANGE);

  Serial.print("\nInitializing SD card...");
  if (!SD.begin(SDCS)) {
    Serial.println("failed");
    while (1) {
      tailColor(RED);
      delay(1000);
      tailColor(WHITE);
      delay(1000);
    }
  } else {
    Serial.println("OK");
  }

  loadImage();
  
  delay(1000);
}

void writeTail() {
  // next servo pulse will be inaccurate
  // due to interupts being disabled by Neopixel library
  invalidDAT = true; invalidCLK = true; invalidNAV = true;
  tail.show();
}

void writeWing() {
  invalidDAT = true; invalidCLK = true; invalidNAV = true;
  wing.show();
}

void tailColor(uint32_t color) {
  for (int i = 0; i < NTAIL; i++) {
    tail.setPixelColor(i, color);
  }
  writeTail();
}

void nav() {
  for (int i=0; i < 25; i++) {
    wing.setPixelColor(i, RED);
  }
  for (int i=25; i < 35; i++) {
    wing.setPixelColor(i, WHITE);
  }
  for (int i=35; i < 60; i++) {
    wing.setPixelColor(i, GREEN);
  }
  tailColor(WHITE);
  writeWing();
}

int tailClearIn = 0;
void tailFlash(uint32_t color) {
  tailColor(color);
  tailClearIn = 5;
}

void blank() {
  for (int i = 0; i < 60; i++)
    wing.setPixelColor(i, 0);
  writeWing();
  tailColor(CLEAR);
}  

int countdownBlank = 0;
int navActive = 2;

void loop(void) {
  bool rxNav = !digitalRead(SWNAV); // nav active when switch is off

  if (rxNav != navActive) {
    navActive = rxNav;
    if (rxNav) {
      Serial.println("nav on");
      nav();
    } else {
      Serial.println("nav off");
      // back to the top, with a blank delay before starting again
      blank();
      countdownBlank = 20;
      line = 0;
    }
  }

  if (dataReady && navActive) { // only read while nav lights are up
    //Serial.print(rxData, DEC); Serial.print(" ");
    int val = (rxData-1000 + (1000/9/2))/(1000/9);
    parse(syncFlag, val);
    if (syncFlag) syncFlag = false;
    if (dataOverflow) {
      Serial.println("not reading fast enough");
      dataOverflow = false;
    }
    dataReady = false;
  }
  
  if (!navActive) {
    if (countdownBlank) {
      countdownBlank--;
    } else {
      img->writeLine(line, &wing);
      writeWing();
      line++;
      if (line == img->lines) {
        if (imgIndex == 5) // nyan tail loop
          line = 65;
        else if (imgIndex == 8) // pacman tail loop
          line = 428;
        else
          line = 0;
      }
    }
  }

  // housekeeping
  if (tailClearIn) {
    tailClearIn--;
    if (tailClearIn == 0)
      tailColor(navActive ? WHITE : CLEAR);
  }
  
  // frame delay
  if (navActive) {
    delay(50);
  } else {
    delay(frameDelay-frameBlank);
    if (frameBlank) {
      blank();
      delay(frameBlank);
    }
  }
}

// interrupt land
unsigned long rxDatStart, rxClkStart;
int pwData;

void iRXDAT() {
  if (digitalRead(RXDAT)) {
    rxDatStart = micros();
  } else if (invalidDAT) {
    invalidDAT = false;
  } else {
      pwData = micros()-rxDatStart;
  }
}

int lastClock = 2;
int syncCount = 0;
void iRXCLK() {
  if (digitalRead(RXCLK)) {
    rxClkStart = micros();
  } else if (invalidCLK) {
    invalidCLK = false;
  } else {
    int pw = micros()-rxClkStart;
    bool clk;
    if (pw >= 1400 && pw <= 1600) {
      syncCount++;
      if (syncCount > 3) {
        syncFlag = true;
        lastClock = 2;
      }
      return;
    } else
      syncCount = 0;
      
    if (pw >= 800 && pw <= 1200) {
      clk = false;
    } else if (pw >= 1800 && pw <= 2200) {
      clk = true;
    } else
      return; // invalid signal

    if (clk != lastClock) {
      rxData = pwData;
      if (dataReady) dataOverflow = true; // not being serviced often enough
      dataReady = true;
      lastClock = clk;
    }
  }
}

int pLen = 0;
int pType = -1;
int pVal = 0;
int pCksum = 0;

const int MAXTYPES = 4;

void parse(bool sync, int val) {
  if (sync) {
    Serial.println(""); Serial.print("sync ");
    pLen = -1;
    pCksum = 0;
    pVal = 0;
  }
  Serial.print(val, DEC); Serial.print(" ");

  pLen++;
  pCksum += val;

  if (pLen == 0) {
    pType = val;
  } else if (pLen < 3) {
    pVal *= 10;
    pVal += val;
  } else if (pLen == 3) {
    if ((pCksum%10) != 0) {
      Serial.print("cksumfail");
      tailFlash(RED);
    } else {
      Serial.print("RX: "); Serial.print(pType, DEC); Serial.print("="); 
      Serial.print(pVal, DEC);
      process(pType, pVal);

      pLen = 0;
      pVal = 0;
      pCksum = 0;
    }
  }
}

void process(int pType, int pVal) {
  int val;
  switch (pType) {
    case PImage:
      Serial.print("image="); Serial.print(val, DEC);
      imgIndex = pVal;
      loadImage();
      break;
    case PBright:
      val = pVal*10 / 4; // 0-99 to 0-247 (close enough)
      wing.setBrightness(val);
      tail.setBrightness(val);
      break;
    case PFDelay:
      frameDelay = pVal;
      break;
    case PFBlank:
      frameBlank = pVal;
      break;
    default:
      tailFlash(PURPLE); // received but not understood
      return;
  }
  tailFlash(GREEN); // aok
}

