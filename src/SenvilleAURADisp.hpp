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
 *
 * Display Properties
 * ------------------
 * Normal mode :  Displays the Set Temperature
 *
 * Diagnostic mode : Send Option::Led three times {Instr:2, Opt:8} then send Option::Direct
 *   {Instr:2, Opt:1} three times. From there, send Option::Led to step forward in displaying
 *   parameters below (Option::Direct to step in reverse).  This mode is exited by non-activity
 *   timout period of a few seconds.
 *
 * Parameters :
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
 *
 *    T1 - Room temp air Intake (Example: 24, Range: -1F to 70)
 *    T2 - Evaporator temp. at mid-point of coil (Example: 10, Range: -1F to 70)
 *    T3 - Condensor temp at mid-point of coil (Example: 32, Range: -1F to 70)
 *    T4 - Ambiant air temperature (Example: 32, Range: -1F to 70)
 *    Tb - (T2B) Evaporator outlet temp? (not supported on all units? Example: -1F, Range: -1F to 70)
 *    TP - Compressor discharge temperature (Example: 69, Range: -1A to d0)
 *    TH - ? (Example: 00, Range: ?)
 *    FT - Targeted Frequency Compressor (Example: 27, Range: 00-F9, Meaning 0-159 Hz )
 *    Fr - Actual Frequency Compressor (Example: 26, Range: 00-F9, Meaning 0-159 Hz )
 * -- Meaning of values below : 00 - Off, Range: 1-low, 2-med, 3-high, 4-turbo (for non-inverter models?)
 * -- Meaning of values below : 00 - Off, Range: 14-FF hex value of RPM/10 (mult. by 10 for RPM) (for inverter models)
 *    1F - (IF) Indoor fan speed (Example: 40, Meaning 400 RPM)
 *    0F - (OF) Outdoor fan speed (Example: 55, Meaning 550 RPM)
 * -- Meaning of values below : Range: 00-B3 hex value of angle in 2 deg. increments (mult. by 2 for degrees)
 *    LA - TXV Opening angle (Example: 96, Meaning: 300 degrees)
 * -- Meaning of values below : Range: 00-FF hex value of minutes running
 *    CT - Compressor continuous running time (Example: FF)
 * -- Meaning of values below : Range: 00-99 meanings unknown, decimal values
 *    5T - (ST) Causes of compressor stop (Example: 07)
 * -- Meaning of values below : Range: 00-FF meanings unknown, hex values
 *    A0 - Reserve/unknown (Example: 00) (any ideas guys?  for which model?)
 *    A1 - Reserve/unknown (Example: 00)
 *    b0 - Reserve/unknown (Example: 00)
 *    b1 - Reserve/unknown (Example: 00)
 *    b2 - Reserve/unknown (Example: 00)
 *    b3 - Reserve/unknown (Example: 00)
 *    b4 - Reserve/unknown (Example: 00)
 *    b5 - Reserve/unknown (Example: 00)
 *    b6 - Reserve/unknown (Example: 00)
 *    dL - Reserve/unknown (Example: 17)
 *    Ac - Reserve/unknown (Example: AA)
 *    Uo - Reserve/unknown (Example: b2)
 *    Td - Reserve/unknown (Example: 5E)
 */
#ifndef SenvilleAURADisp_hpp
#define SenvilleAURADisp_hpp

#include <stdio.h>
#include "Arduino.h"

#define DISPLAY_BYTE_SIZE 3
#define LED_INTER D2
#define CLK_HSPI D5
#define DATA_MOSI D7

#define DISP_MAXSTRINGPERCODE 3
typedef struct displyMapAsciiS {
    uint8_t dispCode;
    char asciiVal[DISP_MAXSTRINGPERCODE];
    displyMapAsciiS(uint8_t code, char ascii[]) {
        dispCode = code;
        memcpy(asciiVal,ascii,DISP_MAXSTRINGPERCODE * sizeof(char));
    }
} DisplayMapAscii;

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
    static const DisplayMapAscii displayMap[];

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
