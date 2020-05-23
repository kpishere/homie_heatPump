#ifndef IRNECRemote_hpp
#define IRNECRemote_hpp

#include <stdio.h>
#include "Arduino.h"
#include "IRLink.hpp"

// Command message
#define MESSAGE_SAMPLES 1
#define MESSAGE_BITS 32 /*  Message is 4-bytes, first byte is Addr, second byte is Addr inverted,
                            third byte is Cmd, fourth byte is Cmd inverted.  This count includes spaces. */
#define MESSAGE_SYNC_BITS 2
#define SYNC_PREAMBLE_0  9000.0
#define SYNC_PREAMBLE_1  4560.0

#define SEP_LENGTH0  500.0
#define BIT0_LENGTH  650.0
#define BIT1_LENGTH 1730.0
#define EOT_LENGTH  40000.0

// Repeat message -- different preamble then an EOT pulse
#define SYNC_PREAMBLE_1A  2280.0

typedef struct irMsgS {
    uint16_t addr;
    uint8_t cmd;
    char *display(char *buf) {
        int pos = 0;
        sprintf(buf,"\nmsg.addr "); APND_CHARBUFF(pos,buf,"%0d ", addr)
        APND_CHARBUFF(pos,buf,"\nmsg.cmd ", ""); APND_CHARBUFF(pos,buf,"%0d ", cmd)
        pos = strlen(buf);
        return buf;
    }

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

    IRConfig *getIRConfig();
    
    // If msg is valid, it is copied locally into the class
    bool isValid(uint8_t *msg, bool setCRC = false);
    irMsg getMessage();
    void setMessage(irMsg m);

    uint8_t *rawMessage();

    char *toBuff(char *buf);
    static uint8_t reverse(uint8_t b);
};

#endif IRNECRemote_hpp
