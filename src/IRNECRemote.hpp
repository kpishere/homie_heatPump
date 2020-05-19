#ifndef IRNECRemote_hpp
#define IRNECRemote_hpp

#include <stdio.h>
#include "Arduino.h"
#include "IRLink.hpp"

// Command message
#define MESSAGE_SAMPLES 1
#define MESSAGE_BITS 32 /*  Message is 4-bytes, first byte is Addr, second byte is Addr inverted,
                            third byte is Cmd, fourth byte is Cmd inverted */
#define MESSAGE_SYNC_BITS 2
#define SYNC_PREAMBLE_0  9000.0
#define SYNC_PREAMBLE_1  4560.0

#define TOLERANCE_PERCENT 0.21f
#define SEP_LENGTH0  500.0
#define BIT0_LENGTH  650.0
#define BIT1_LENGTH 1730.0
#define EOT_LENGTH  500.0

// Repeat message -- different preamble then an EOT pulse
#define SYNC_PREAMBLE_1A  2280.0

typedef struct irMsgS {
    unsigned char addr;
    unsigned char cmd;
} irMsg;

// Listen for b, translate to a
//  4-byte message : B0, B1, B2, B3  >>  B0 (lo), B1 (hi) == Address, B2 == Command, B3 == Inverted Command
//  b - Address: B0: 0x86, B1: 0x6B
//  c - Address: B0: 0x00, B1: 0xFF
//
// NEC Framing :
//
//  ----|_____|---|_|-|_|--| ...  |_______|--|_|
//
//                                           /\-- bit separator 0.5ms
//                                        /-\-- 2.3 ms hi
//                                /-------\-- 9ms repeat
//            /-------------------\-- 95ms message slot
//                      /-\-- bit one 1.73ms
//                  /\-- bit zero 0.65ms
//                /\-- bit separator 0.5ms
//      /---------\
//           |
//         Preamble 9ms lo, 4.56ms hi
//
//  9ms AGC pulse low
//  4.6ms hi
//  ( 1.135ms bit 0  -OR-  2.246ms bit 1 ) * 8 [bits/byte] * 4 [bytes]
//
//

class IRNECRemote {
private:
    static IRConfig config;
    uint8_t message[MSGSIZE_BYTES(MESSAGE_SAMPLES,MESSAGE_BITS)];

public:
    IRNECRemote();

    const IRConfig *getIRConfig();
    
    // If msg is valid, it is copied locally into the class
    bool isValid(uint8_t *msg, bool setCRC = false);
    irMsg getMessage();
    void setMessage(irMsg m);

    uint8_t *rawMessage();

    char *toBuff(char *buf);
};

#endif IRNECRemote_hpp
