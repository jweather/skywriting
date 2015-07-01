#include <Wire.h>
#include <DigisparkOLED.h>
#include "TinyPpmGen.h"
#include <EEPROM.h>

// OLED on 0+2
// PPM output on 8
// indexes
#define UP    0
#define DN    1
#define LT    2
#define RT    3
#define OK    4
#define NJOY  5
const int JOY[NJOY] = {10, 11, 12, 5, 4}; // pins

#define LED   1

#define NCHAN 6
#define THR   1
#define DAT   4
#define NAV   5
#define CLK   6

#define NImages  10
char *imageNames[NImages]={"rainbow", "egb", "blucheck", "blugreen", "sines", 
  "nyan", "flitetest", "brcc", "pacman", "nav"};

#define PImage     0
#define PBright    1
#define PFDelay    2
#define PFBlank    3
#define PAirborne  4
#define NParams    5
char *PNames[NParams]={"image", "brightness", "frame delay", "frame blank", "airborne"};

int values[NParams];

int select = 0;
int dirty = 0;
#define DIRTY  100 // how many cycles before we write to EEPROM

void repaint();
void refreshPPM(); // update PPM output for selected line

void defaults() {
  values[PImage] = 0;
  values[PBright] = 50;
  values[PFDelay] = 20;
  values[PFBlank] = 0;
  values[PAirborne] = 0;
  dirty = 1;
}

void setup() {
  pinMode(LED, OUTPUT);
  for (int i=0; i<NJOY; i++)
    pinMode(JOY[i], INPUT);
  
  TinyPpmGen.begin(TINY_PPM_GEN_POS_MOD, 6, 22500);
  for (int i = 1; i < NCHAN; i++)
    TinyPpmGen.setChWidth_us(i, 1500);  // everything at midpoint
  TinyPpmGen.setChWidth_us(THR, 1000);  // throttle at minimum
  TinyPpmGen.setChWidth_us(NAV, 1000);  // nav lights on

  oled.begin();
  oled.fill(0xFF);
  delay(500);
  oled.clear();
  delay(500);
  oled.setCursor(0, 0); //top left
  oled.setFont(FONT6X8);

  oled.println(F("boot"));
  
  int boot = EEPROM.read(NParams);
  if (boot != 0x42) {
    oled.println("first boot!");
    EEPROM.write(NParams, 0x42);
    // defaults
    defaults();
  } else {
    oled.println("EEPROM ok");
    for (int i=0; i<NParams; i++)
      values[i] = EEPROM.read(i);
  }
  delay(1000);
  
  /* // button test
  while (true) {
    oled.setCursor(0, 0);
    for (int i=0; i < NJOY; i++) {
      int d = digitalRead(JOY[i]);
      oled.print(d);
    }
    delay(500);
    digitalWrite(LED, !digitalRead(LED));
  }
  */
  
  oled.clear();
  select = 0;
  refreshPPM();
  repaint();
}

void repaint() {
  for (int line=0; line<NParams; line++) {
    oled.setCursor(0, line);
    oled.print(line==select ? ">" : " ");
    oled.print(PNames[line]);
    oled.print(" = ");
    if (line == PImage) {
      oled.print(imageNames[values[line]]);
    } else {
      oled.print(values[line]);
    }      
    oled.print("      "); // blank rest of line
  }
}

bool dataValid = false;
int data[4];
int dataPos = 0;
int dataState = 0;

void refreshPPM() {
  TinyPpmGen.setChWidth_us(THR, values[PAirborne] ? 1500 : 1000); // throttle position
  if (select >= PAirborne) {
    dataValid = false; // no output
  } else {
    // send selected parameter
    data[0] = select;
    data[1] = values[select]/10;
    data[2] = values[select]%10;
    data[3] = 10 - ((data[0]+data[1]+data[2])%10);
    dataValid = true;
    dataState = 0;
  }
}

int rollover(int val, int m) {
  if (val < 0)
    val += m;
  if (val > m)
    val -= m;
  return val;
}

void delta(int line, int d) {
  int stepSize = 1, maxValue = 100;
  
  switch (line) {
    case PImage:
      maxValue = NImages-1;
      break;
    case PAirborne:
      values[PAirborne] = !values[PAirborne];
      return;
  }
  
  values[line] = rollover(values[line] + stepSize*d, maxValue);
  dirty = DIRTY;
  // safety -- can't turn brightness all the way down
  if (line == PBright && values[PBright] < 10) values[PBright] = 10;
}

void button(int b, int repeats) {
  switch (b) {
    case DN:
      select = (select<NParams-1) ? select+1 : select;
      break;
    case UP:
      select = (select==0) ? select : select-1;
      break;
    case LT:
      delta(select, -repeats);
      break;
    case OK:
    case RT:
      delta(select, repeats);
      break;
  }
  refreshPPM();
  repaint();
}

void clk(int w) {
  TinyPpmGen.setChWidth_us(CLK, w);
}

void dat(int w) {
  TinyPpmGen.setChWidth_us(DAT, w);
}
  
bool lastClock = false;
int repeats[NJOY];

void loop() {
  unsigned long tstart = millis();
  for (int i=0; i < NJOY; i++) {
    int sw = !digitalRead(JOY[i]); // active low
    if (sw) {
      repeats[i]++;
      if (repeats[i] == 1) {
        button(i, repeats[i]);
      } else if (repeats[i] < 3) {
        // ignore, wait to start repeating
      } else {
        button(i, repeats[i]-3);
      }
    } else {
      repeats[i] = 0;
    }
  }
  
  if (dirty) {
    dirty--;
    if (dirty == 0) {
      for (int i=0; i<NParams; i++)
        EEPROM.write(i, values[i]);
    }
  }
  
  /* // sweep servo values
  TinyPpmGen.setChWidth_us(1, 1000 + dataState*100);
  dataState++;
  if (dataState > 10) dataState = 0;
  */
  
  /* // sweep data values
  if ((dataState % 2) == 0) {
    int pw = 1000 + (dataState/2)*(1000/9);
    dat(pw);
  } else {
    lastClock = !lastClock;
    clk(lastClock ? 2000 : 1000);
  }
  dataState++;
  if (dataState == 20) dataState = 0;
  */

  // data output
  if (dataValid) {
    if (dataState == 0) {
      clk(1500);
      lastClock = false;
      dataState++;
      dataPos = 0;
    } else if (dataState == 1) {
      int pw = 1000 + data[dataPos]*(1000/9);
      dat(pw);
      dataPos++;
      dataState = 2;
    } else if (dataState == 2) {
      lastClock = !lastClock;
      clk(lastClock ? 2000 : 1000);
      if (dataPos == 4) 
        dataState = 0; // back to start
      else
        dataState = 1; // next byte
    }
  }
  
  digitalWrite(LED, !digitalRead(LED));
  unsigned long elapsed = millis()-tstart;
  if (elapsed < 0 || elapsed > 150) elapsed = 0;
  delay(150 - elapsed);
}
