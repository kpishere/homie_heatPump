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

#define STAT_DISPRAW "disp"
#define STAT_ONTME  "OnTimeMs"

// Bit/Byte management
volatile short SenvilleAURADisp::bitPtr;
volatile uint8_t SenvilleAURADisp::rdByte;
volatile bool SenvilleAURADisp::byteReady;

// Message values
volatile uint8_t SenvilleAURADisp::displayBuff[DISPLAY_BYTE_SIZE];
volatile uint8_t SenvilleAURADisp::displayBuffLast[DISPLAY_BYTE_SIZE];
volatile bool SenvilleAURADisp::printIt;
volatile uint8_t SenvilleAURADisp::displayPtr;

// Only one instance of this class is supported, the last
// class to invoke listen() wins.  First class to exit disables interrupt.
SenvilleAURADisp *lastInst;
void ISRDispHandler() {
    if(lastInst) lastInst->handler();
}
void ISRSyncHandler() {
    if(lastInst) lastInst->handleSynch();
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
    if(!printIt) return false;
    // Test all display bytes for a change
    for(int i=0; i< DISPLAY_BYTE_SIZE; i++) {
        newVal = newVal && displayBuff[i] == displayBuffLast[i];
        displayBuffLast[i] = displayBuff[i];
    }
    printIt = false;
    if(!newVal) this->listenStop();
    return !newVal;
}
#define APND_CHARBUFF(pos,buf,arg0,arg1) (pos) = strlen(buf); sprintf(&(buf)[(pos)],arg0,arg1);
char *SenvilleAURADisp::toBuff(char *buf) {
    int pos = 0;
    sprintf(buf,"{"STAT_DISPRAW":0x");
    for(uint8_t ptr = 0; ptr < DISPLAY_BYTE_SIZE; ptr++) {
        APND_CHARBUFF(pos,buf,(displayBuff[ptr]<0x10?"0":""), "")
        APND_CHARBUFF(pos,buf,"%0X", displayBuff[ptr])
        
    }
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
