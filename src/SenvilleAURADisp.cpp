//
//  SenvilleAURADisp.cpp
//
//
//  Created by Kevin Peck on 2018-12-29.
//

#include "SenvilleAURADisp.hpp"
#define DEBUG

#if defined(__AVR__)
#else // defined(ESP8266)
#include <pins_arduino.h>
#define 	ESP_MAX_INTERRUPTS   16
#define 	digitalPinToInterrupt(p)   ( (p) < ESP_MAX_INTERRUPTS ? (p) : -1 )
#endif

#define BITSINBYTE 8
// The LSB of the display toggles between 1/0 with each scan but isn't connected to output
#define DISPLAY_MASK 0xFE

#define STAT_DISPRAW    "dispRaw"
#define STAT_DISP       "disp"
#define STAT_ONTME      "OnTimeMs"

#define DISP_CHAR1 0
#define DISP_CHAR2 1
#define DISP_LEDS  2

// Bit/Byte management
volatile short SenvilleAURADisp::bitPtr;
volatile uint8_t SenvilleAURADisp::rdByte;

// Message values
volatile uint8_t SenvilleAURADisp::displayBuff[DISPLAY_BYTE_SIZE];
volatile uint8_t SenvilleAURADisp::displayBuffLast[DISPLAY_BYTE_SIZE];
volatile uint8_t SenvilleAURADisp::displayPtr;

const DisplayMapAscii SenvilleAURADisp::displayMap[] = {
      displyMapAsciiS(0xFE, " ")
    , displyMapAsciiS(0x9C, "-1")
    , displyMapAsciiS(0xFC, "-")
    , displyMapAsciiS(0x02, "0")
    , displyMapAsciiS(0x9E, "1")
    , displyMapAsciiS(0x24, "2")
    , displyMapAsciiS(0x0C, "3")
    , displyMapAsciiS(0x98, "4")
    , displyMapAsciiS(0x48, "5")
    , displyMapAsciiS(0x40, "6")
    , displyMapAsciiS(0x1E, "7")
    , displyMapAsciiS(0x00, "8")
    , displyMapAsciiS(0x08, "9")
    , displyMapAsciiS(0x10, "A")
    , displyMapAsciiS(0x62, "C")
    , displyMapAsciiS(0x60, "E")
    , displyMapAsciiS(0x70, "F")
    , displyMapAsciiS(0x90, "H")
    , displyMapAsciiS(0xF2, "I")
    , displyMapAsciiS(0xE2, "L")
    , displyMapAsciiS(0x30, "P")
    , displyMapAsciiS(0x48, "S")
    , displyMapAsciiS(0x72, "T")
    , displyMapAsciiS(0x82, "U")
    , displyMapAsciiS(0xC0, "b")
    , displyMapAsciiS(0xE4, "c")
    , displyMapAsciiS(0x84, "d")
    , displyMapAsciiS(0x20, "e")
    , displyMapAsciiS(0x9E, "i")
    , displyMapAsciiS(0xC4, "o")
    , displyMapAsciiS(0xF4, "r")
    , displyMapAsciiS(0xC6, "u")
};

// Only one instance of this class is supported, the last
// class to invoke listen() wins.  First class to exit disables interrupt.
SenvilleAURADisp *lastInst;
void IRAM_ATTR ISRDispHandler() {
    if(lastInst) lastInst->handler();
}
void IRAM_ATTR ISRSyncHandler() {
    if(lastInst) lastInst->handleSynch();
}
// return display value as char* of 7bit ascii string
const char *displayBytetoAscii(uint8_t b) {
    for(int i=0; i<( sizeof(SenvilleAURADisp::displayMap)/sizeof(DisplayMapAscii) ); i++ ) {
        if(b == SenvilleAURADisp::displayMap[i].dispCode)
            return SenvilleAURADisp::displayMap[i].asciiVal;
    }
    return "?"; // Unmapped values are equated to a Question mark
}
//////
// Class methods
//////
SenvilleAURADisp::SenvilleAURADisp() {
    lastInst = this;
    displayPtr = 0;
    bitPtr = 0;
    rdByte = 0;
    pinMode(CLK_HSPI, INPUT);
    pinMode(LED_INTER, INPUT);
    pinMode(DATA_MOSI, INPUT);
    #ifdef DEBUG
    pinMode(DEBUG_PIN, OUTPUT);
    #endif
    this->listen();
}
SenvilleAURADisp::~SenvilleAURADisp() {
    lastInst = 0;
}

void SenvilleAURADisp::listen() {
    //define pin modes
    attachInterrupt(digitalPinToInterrupt(CLK_HSPI), ISRDispHandler, RISING);
    attachInterrupt(digitalPinToInterrupt(LED_INTER), ISRSyncHandler, RISING);
    displayPtr = 0;
    #ifdef DEBUG
    digitalWrite(DEBUG_PIN,HIGH);
    #endif
}
void SenvilleAURADisp::listenStop() {
    detachInterrupt(digitalPinToInterrupt(CLK_HSPI));
    detachInterrupt(digitalPinToInterrupt(LED_INTER));
    #ifdef DEBUG
    digitalWrite(DEBUG_PIN,LOW);
    #endif
}
bool SenvilleAURADisp::hasUpdate() {
    bool newVal = true;
    uint8_t spaces = 0;
    if( displayPtr >= DISPLAY_BYTE_SIZE ) {
      // Test all display bytes for a change
      for(int i=0; i< DISPLAY_BYTE_SIZE; i++) {
          newVal = newVal && displayBuff[i] == displayBuffLast[i];
          displayBuffLast[i] = displayBuff[i];
          // Supress results with spaces -- due to flashing
          if(i < DISP_LEDS)
              spaces += (displayBuff[i] == DISPLAY_MASK ? 1 : 0);
      }
      if(spaces>0) return false;
    }
    return !newVal;
}
#define APND_CHARBUFF(pos,buf,arg0,arg1) (pos) = strlen(buf); sprintf(&(buf)[(pos)],arg0,arg1);
char *SenvilleAURADisp::toBuff(char *buf) {
    int pos = 0;
    sprintf(buf,"{" STAT_DISPRAW ":0x");
    for(uint8_t ptr = 0; ptr < DISP_LEDS; ptr++) {
        APND_CHARBUFF(pos,buf,(displayBuff[ptr]<0x10?"0%s":"%s"), "")
        APND_CHARBUFF(pos,buf,"%0X", displayBuff[ptr])
    }
    APND_CHARBUFF(pos,buf,", " STAT_DISP ":\"%s", displayBytetoAscii(displayBuff[DISP_CHAR1]))
    APND_CHARBUFF(pos,buf,"%s\"", displayBytetoAscii(displayBuff[DISP_CHAR2]))
    APND_CHARBUFF(pos,buf,", " STAT_ONTME ":%ld }", millis())
    return buf;
}
char *SenvilleAURADisp::asciiDisplay(char *buf) {
  int pos = 0;
  sprintf(buf,"%s", displayBytetoAscii(displayBuff[DISP_CHAR1]));
  APND_CHARBUFF(pos,buf,"%s", displayBytetoAscii(displayBuff[DISP_CHAR2]))
  return buf;
}
//
/*
  * -- Meaning of values below : (specialized semi-hex display scheme that 'cheats' three characters for neg & 100's)
  *          -1F, -1E, -1d, -1c, -1b, -1A are similarly meaning -25, -24, -23, -22, -21, and -20 in deg.C
  *          -19 to -10 are in deg.C
  *          -9 to -1 are in deg.C
  *          00 to 99 are in deg.C
  *          A0 to A9 are 100-109 in deg.C
  *          b0 to b9 are 110-119 in deg.C
  *          c0 to c9 are 120-129 in deg.C
  *          d0 to d9 are 130-139 in deg.C
  *          E0 to E9 are 140-149 in deg.C
  *          F0 to F9 are 150-159 in deg.C
 */
int SenvilleAURADisp::alphaToInt(char *value) {
  int values[DISP_MAXSTRINGPERCODE];
  int i = strlen(value);
  int level = 0, sign = 1;
  for(; i > 0; i--) {
    switch(value[i-1]) {
      case 'a': case 'A': values[level] = 10 * (level==1?10:1); break;
      case 'b': case 'B': values[level] = 11 * (level==1?10:1); break;
      case 'c': case 'C': values[level] = 12 * (level==1?10:1); break;
      case 'd': case 'D': values[level] = 13 * (level==1?10:1); break;
      case 'e': case 'E': values[level] = 14 * (level==1?10:1); break;
      case 'f': case 'F': values[level] = 15 * (level==1?10:1); break;
      case '-': sign = -1; values[level] = 0; break;
      case ' ': values[level] = 0 * (level==1?10:1); break;
      case '0': values[level] = 0 * (level==1?10:1); break;
      case '1': values[level] = 1 * (level==1?10:1); break;
      case '2': values[level] = 2 * (level==1?10:1); break;
      case '3': values[level] = 3 * (level==1?10:1); break;
      case '4': values[level] = 4 * (level==1?10:1); break;
      case '5': values[level] = 5 * (level==1?10:1); break;
      case '6': values[level] = 6 * (level==1?10:1); break;
      case '7': values[level] = 7 * (level==1?10:1); break;
      case '8': values[level] = 8 * (level==1?10:1); break;
      case '9': values[level] = 9 * (level==1?10:1); break;
    }
    // Debug statement
    //Serial.printf("#:%d %c %d\n",i,value[i-1], values[level]);
    //
    level++;
  }
  return sign * (values[0] + (level > 1? values[1] : 0) + (level > 2 ? values[2]:0) );
}
// Read next bit - flag when full byte ready
void SenvilleAURADisp::handler() {
    bool bitVal;
    cli();

    // process when gathering byte bits
    bitVal = digitalRead(DATA_MOSI);
    if(bitPtr==0) {
        rdByte = 0;
    }
    rdByte |= (bitVal << (bitPtr%BITSINBYTE));
    bitPtr++;
    if(bitPtr == BITSINBYTE) {
      bitPtr = 0;
      displayBuff[displayPtr % DISPLAY_BYTE_SIZE] = rdByte & DISPLAY_MASK;
      displayPtr++;
      if(displayPtr % DISPLAY_BYTE_SIZE == 0) {
        this->listenStop(); // end when we've got 3 bytes
      }
    }
    sei();
}
// Reset to first byte. reset bits for sure alignment
void SenvilleAURADisp::handleSynch() {
    cli();
    displayPtr = 0;
    bitPtr = 0;
    #ifdef DEBUG
    digitalWrite(DEBUG_PIN,!digitalRead(DEBUG_PIN));
    digitalWrite(DEBUG_PIN,!digitalRead(DEBUG_PIN));
    #endif
    sei();
}
