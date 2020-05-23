//
//  SenvilleAURA.hpp
//  IRDecoder/Encoder
//

#ifndef SenvilleAURA_hpp
#define SenvilleAURA_hpp

#include <stdio.h>
#include "Arduino.h"
#include "IRLink.hpp"

/* Notes on waveform configured below :
 *  - There is no significance to high or low values, it is all about pulse durations.
 *  - Two transitions count one pulse width, possibly a bit, but also a space, mark, etc.
 *
 *  - In the configuration below, the waveform is roughly :
 
 |__|--|_|-|............|_|--- and the data repeats once but with reverse bit logic of the second frame
        | |
        | | Bit
        | Separator
 /     \ /             \/    \
    |        |            |
    |        |            | Bit separator and then Break between frames (or longer if end of message)
    |        |
    |        | Bit separator then data bit
    |
    | Two synch preamble pulses
 
 Some sample commands in JSON syntax using fromJsonBuff() :
 
 Turn AC on and set temp :  {Instr:1, IsOn:1, Mode:0, FanSpeed:0, SetTemp:24}
 
 */

#define MESSAGE_SAMPLES 2
#define MESSAGE_BITS 48 // Message is ten nibbles, fits into 5 bytes, plus CRC
#define MESSAGE_SYNC_BITS 2
#define SYNC_PREAMBLE_0  4100.0
#define SYNC_PREAMBLE_1  4320.0

#define TOLERANCE_PERCENT 0.21f
#define SEP_LENGTH0  500.0
#define BIT0_LENGTH  500.0
#define BIT1_LENGTH 1560.0
#define EOT_LENGTH  5120.0

#define TEMP_LOWEST 17

enum Instruction {Command = 0x01, InstrOption = 0x02, FollowMe = 0x04};
enum Mode {Cool = 0 , Dry = 1, ModeAuto = 2, Heat = 3, Fan = 4};
enum FanSpeed {FanAuto = 0, Low = 1, Med = 2, High = 3};
enum Option {Direct = 1, Swing = 2, Led = 8, Turbo = 9, SelfClean = 13, SilenceOn = 18, SilenceOff = 19, FP = 15};
enum FollowMeState {FmStart = 0xFF, FmUpdateTemp = 0x7F, FmStop = 0x3F};

class SenvilleAURA {
    // Sample index is 0 or 1
    #define MSG_CONST_STATE(samp) (0+6 * (samp))
    #define MSG_CMD_OPT(samp)     (1+6 * (samp))
    #define MSG_RUNMODE(samp)     (2+6 * (samp))
    #define MSG_TIMESTART(samp)   (3+6 * (samp))
    #define MSG_TIMESTOP(samp)    (4+6 * (samp))
    #define MSG_CRC(samp)         (5+6 * (samp))
    /* Message structure: [0][1][2][3][4][5][6][7] - bytes numbered from left to right
     *  [0] > 1010 constant; 0001 command, 0010 option, 0100 follow-me
     *  [1] > COMMAND: 0 Off, 1 on; 0 normal, 1 sleep; 0 fan mode disabled, 1 fan mode selectable;
     *        00 auto, 01 fan 0, 10 fan 1, 11 fan 2;
     *        000 cool mode, 001 dry, 010 auto, 011 heat, 100 fan
     *  [1] > OPTION: 000 constant; 00001 direct, 00010 swing, 01000 led, 01001 turbo, 01101 self clean,
     *        10010 silence on, 10011 silence off, 01111 FP
     *  [2] - 010 command, 111 option; 0 compressor enabled, 1 compressor off;
     *        **** set temp (+17), 0000 sleep mode or timmer command, 0100 follow-me, 1111 option command
     *  [3] - 11111111 command or option or stop follow me or on timmer set,
     *        00111111 start follow me,
     *        01111111 update follow me,
     *        1******1 off timmer set from 0 to 24h
     *  [4] - 11111111 command or option mode or off timmer set,
     *        ******** follow me mode is temp in degC
     *        1******1 on timmer set from 0 to 24h
     *  [5] > ******** CRC
     */
private:
    static IRConfig config;
    unsigned long sampleId;
    unsigned long lastSampleMs;
    unsigned short setTempDegC;
    short validSamplePtr;
    uint8_t message[MSGSIZE_BYTES(MESSAGE_SAMPLES,MESSAGE_BITS)];

    uint8_t calcCRC(short _sample);

    void setInstructionType(Instruction instr);

public:
    SenvilleAURA();
    
    IRConfig *getIRConfig();

    // If msg is valid, it is copied locally into the class
    bool isValid(uint8_t *msg, bool setCRC = false);
    uint8_t *getMessage();
    unsigned int getMessageDuration();

    char *toBuff(char *buf);
    char *toJsonBuff(char *buf);
    // Returns true if successfully parsed and sendBuf is populated for transmission
    bool fromJsonBuff(char *buf, uint8_t *sendBuf);

    // Control Options
    Instruction getInstructionType();
    
    bool getPowerOn();
    void setPowerOn(bool newState);
    
    Mode getMode();
    void setMode(Mode val);

    // No fan control in AUTO or DRY modes
    FanSpeed getFanSpeed();
    void     setFanSpeed(FanSpeed val);
    
    // Avalable in COOL, HEAT, or AUTO modes only
    // Cancels when Mode canged, FAN SPEED changed or ON/OFF pressed
    bool getSleepOn();
    void setSleepOn(bool newState);

    Option getOption();
    // Make option command - Creates a new command
    // Direct, Swing, Led, SilenceOn/SilenceOff - Cool, Dry, Heat, Fan, Auto
    // Turbo - Cool, Heat, Auto
    // Self Clean - Cool, Auto
    // FP - Only works when initially in heat mode, display shows 'FP' when active
    //      Cancels when On/Off, Sleep, FP, Mode, Fan speed, Up/Dn pressed
    static SenvilleAURA *optionCmd(Option val);

    // No temp control in fan mode
    uint8_t  getSetTemp();
    void setSetTemp(uint8_t val);

    uint8_t  getTimeOff();
    void setTimeOff(uint8_t hour);
    
    uint8_t  getTimeOn();
    void setTimeOn(uint8_t hour);
    
    uint8_t getFollowMeTemp();
    FollowMeState getFollowMeState();
    // Make option command - Follow Me, there are three states, new, update temp, and stop
    //   Note: Heat pump expects an update in measured temperature every 3 minutes
    static SenvilleAURA *followMeCmd(SenvilleAURA *fromState, FollowMeState newState,
                                     uint8_t measuredTemp);
    
    unsigned long  getOnTimeMs();
    unsigned long  getSeqId();
};
#endif /* SenvilleAURA_hpp */
