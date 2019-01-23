//
//  SenvilleAURADisp.cpp
//  
//
//  Created by Kevin Peck on 2018-12-29.
//

#include "SenvilleAURADisp.hpp"
#include "Arduino.h"
//#define DEBUG

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
volatile bool SenvilleAURADisp::byteReady;

// Message values
volatile uint8_t SenvilleAURADisp::displayBuff[DISPLAY_BYTE_SIZE];
volatile uint8_t SenvilleAURADisp::displayBuffLast[DISPLAY_BYTE_SIZE];
volatile bool SenvilleAURADisp::printIt;
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
void ISRDispHandler() {
    if(lastInst) lastInst->handler();
}
void ISRSyncHandler() {
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
    printIt = false;
    displayPtr = 0;
    bitPtr = 0;
    rdByte = 0;
    byteReady = false;
    this->listen();
}
SenvilleAURADisp::~SenvilleAURADisp() {
    lastInst = 0;
}
void SenvilleAURADisp::listen() {
    noInterrupts();
    //define pin modes
    pinMode(CLK_HSPI, INPUT);
    pinMode(DATA_MOSI, INPUT);
    attachInterrupt(digitalPinToInterrupt(CLK_HSPI), ISRDispHandler, RISING);
    attachInterrupt(digitalPinToInterrupt(LED_INTER), ISRSyncHandler, RISING);
    interrupts();
}
void SenvilleAURADisp::listenStop() {
    noInterrupts();
    detachInterrupt(digitalPinToInterrupt(CLK_HSPI));
    detachInterrupt(digitalPinToInterrupt(LED_INTER));
    interrupts();
}
bool SenvilleAURADisp::hasUpdate() {
    bool newVal = true;
    uint8_t spaces = 0;
    // Collects bytes until display array is filled
    if(byteReady) {
        byteReady = false;
        bitPtr = 0;
        displayBuff[displayPtr % DISPLAY_BYTE_SIZE] = rdByte & DISPLAY_MASK;
        displayPtr++;
        if(displayPtr % DISPLAY_BYTE_SIZE == 0) {
            printIt = true;
        }
    }
    if(!printIt)    return false;
    else            printIt = false;
    // Test all display bytes for a change
    for(int i=0; i< DISPLAY_BYTE_SIZE; i++) {
        newVal = newVal && displayBuff[i] == displayBuffLast[i];
        displayBuffLast[i] = displayBuff[i];
        // Supress results with spaces -- due to flashing
        if(i < DISP_LEDS)
            spaces += (displayBuff[i] == DISPLAY_MASK ? 1 : 0);
    }
    if(spaces>0) return false; 
    if(!newVal) this->listenStop();
    return !newVal;
}
#define APND_CHARBUFF(pos,buf,arg0,arg1) (pos) = strlen(buf); sprintf(&(buf)[(pos)],arg0,arg1);
char *SenvilleAURADisp::toBuff(char *buf) {
    int pos = 0;
    sprintf(buf,"{"STAT_DISPRAW":0x");
    for(uint8_t ptr = 0; ptr < DISP_LEDS; ptr++) {
        APND_CHARBUFF(pos,buf,(displayBuff[ptr]<0x10?"0":""), "")
        APND_CHARBUFF(pos,buf,"%0X", displayBuff[ptr])
    }
    APND_CHARBUFF(pos,buf,", "STAT_DISP":\"%s", displayBytetoAscii(displayBuff[DISP_CHAR1]))
    APND_CHARBUFF(pos,buf,"%s\"", displayBytetoAscii(displayBuff[DISP_CHAR2]))
    APND_CHARBUFF(pos,buf,", "STAT_ONTME":%d }", millis())
    return buf;
}
// Read next bit - flag when full byte ready
void SenvilleAURADisp::handler() {
    bool bitVal;
    noInterrupts();

    if(!byteReady) {
        // process when gathering byte bits
        bitVal = digitalRead(DATA_MOSI);
        if(bitPtr==0) {
            rdByte = 0;
        }
        rdByte |= (bitVal << (bitPtr%BITSINBYTE));
        bitPtr++;
        if(bitPtr == BITSINBYTE) {
            byteReady = true;
        }
    }
    interrupts();
}
// Reset to first byte. reset bits for sure alignment
void SenvilleAURADisp::handleSynch() {
    noInterrupts();
    displayPtr = 0;
    bitPtr = 0;
#ifdef DEBUG
    Serial.println("LED Interrupt");
#endif
    interrupts();
}
