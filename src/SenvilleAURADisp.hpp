//
//  SenvilleAURADisp.hpp
//  
/*
 * Replacement for Senville Display - NodeMCU v3 ESP8266-12E
 *
 * Connector CN201 - 8 Pins (NOTE: Electric circuit is isolated from ground, safe to hook up Oscilloscope)
 *
 *  GND  Black - GND
 *  5V   White - 5V
 *  IR   Red - IR pulse duration signal
 *  DATA Orange - DATA for LEDs
 *  LED1 Green - LED1 select w. 12 ms waveform
 *  LED2 Dk Blue - LED2 select w. 12 ms waveform
 *  LED  Purple - LED select w. 12 ms waveform
 *  CLK  Grey - CLK w. 8-bit bursts of 10 us tick each 4 ms
 *
 *  - IR needs interrupt handler on UP/DN transistions (see IRHVACLink.hpp)
 *  - CLK needs interrupt handler on UP transition, then read state of DATA
 *  - LED, LED1, LED2 need interrupt on DN transition to know whom next data byte is for
 *    only one is used for 'byte alignment' as they fire in repeating order
 *  - HCS or HSS is pin D8 on ESP8266 and must be set LOW.  SPI is always listening.
 */
#ifndef SenvilleAURADisp_hpp
#define SenvilleAURADisp_hpp

#include <stdio.h>
#include "Arduino.h"

#define DISPLAY_BYTE_SIZE 3
#define LED_INTER D2
#define CLK_HSPI D5
#define DATA_MOSI D7

class SenvilleAURADisp {
private:
    static volatile short bitPtr;
    static volatile uint8_t rdByte;
    static volatile bool byteReady;
    static volatile uint8_t displayPtr;
    static volatile uint8_t displayBuff[DISPLAY_BYTE_SIZE];
    static volatile uint8_t displayBuffLast[DISPLAY_BYTE_SIZE];
    static volatile bool printIt;
public:
    SenvilleAURADisp();
    ~SenvilleAURADisp();
    bool hasUpdate();
    char *toBuff(char *buf);
    void listen(); // pin is re-defined for listening
    void listenStop(); // Stops interrupts, important for serial communication etc.
    void handler();
    void handleSynch();
};

#endif /* SenvilleAURADisp_hpp */
