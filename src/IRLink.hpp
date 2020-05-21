//
//  IRLink.hpp
//  

#ifndef IRLink_hpp
#define IRLink_hpp

#include <stdio.h>
#include "Arduino.h"

// ring buffer size has to be large enough to fit
// data between two successive sync signals
#define RING_BUFFER_SIZE  550

#if defined(__AVR__)
#define IR_DDRPRT DDRA
#define IR_SENDPORT PORTA
#define IR_PINR PA3
#define IR_PINX PA3
#elif defined(ESP8266)
#define IR_PINR D1
#define IR_PINX D1
#endif

#define TOLERANCE_PERCENT 0.21f

// Memory allocation function for containing message sent/received
// Note: remainder test is for non-8-bit multiple message sizes
#define MSGSIZE_BYTES(samp,msgbits) ((samp) * ((msgbits) % BITS_IN_BYTE > 0 ? 1 : 0) + (samp) * (msgbits) / BITS_IN_BYTE )

#define CALC_LO(v) (unsigned long)(v * (1.0 - TOLERANCE_PERCENT ) )
#define CALC_HI(v) (unsigned long)(v * (1.0 + TOLERANCE_PERCENT ) )
#define diffRollSafeUnsignedLong(t1,t2) (t2>t1? t2-t1 : t2+4294967295-t1 )
#define BITS_IN_BYTE 8

// Hi/lo tolerance is used for pulse duration timing
typedef struct IRPulseLengthUsS {
    unsigned short lo;
    unsigned short val;
    unsigned short hi;
    IRPulseLengthUsS(unsigned short _val = 0.0) {
        lo = CALC_LO(_val);
        hi = CALC_HI(_val);
        val = _val;
    }
} IRPulseLengthUs;

// Message definition
typedef struct IRConfigS {
    uint8_t msgSamplesCnt;
    uint8_t msgBitsCnt;
    uint8_t msgSyncCnt;
    IRPulseLengthUs *syncLengths;
    IRPulseLengthUs bitSeparatorLength;
    IRPulseLengthUs bitZeroLength;
    IRPulseLengthUs bitOneLength;
    IRPulseLengthUs msgBreakLength;
} IRConfig;

typedef enum IRMsgStateE {Preamble, Message} IRMsgState;

class IRLink {
public:
    IRLink(const IRConfig *_config);
    ~IRLink();
    
    // NOTE: caller owns memory pointed to and it is presumed to have enough
    // valid data to satisfy the IRConfig defintion of the message
    // Wait is for message to be sent.  Otherwise, if you call listen() right away, you'll get a
    // feedback loop (good for memory leak testing!)
    void send(uint8_t *msg, bool noWait = false);
    
    void listen(); // pin is re-defined for listening
    void listenStop(); // Stops interrupts, important for serial communication etc.
    
    /// REturns NULL if no measurement otherwise memory buffer pointer to newly received message
    /// NOTE: DO NOT release this memory!  It is allocated once on class creation.
    /// (this is a change from prior code)
    uint8_t *loop_chkMsgReceived();
    void handler();

    static const IRConfig *config;
private:
    static volatile unsigned long timings[RING_BUFFER_SIZE];
    static volatile unsigned long lastTime;
    static volatile unsigned int ringIndex;
    static volatile unsigned int syncIndex1;  // index of the first sync signal
    static volatile unsigned int edgeCount;
    static volatile unsigned int edgeCount1; // Count of separate messages repeated
    static volatile bool received; // Receive a single message
    static volatile IRMsgState state;
    
    bool isSyncInMsg(unsigned int idx);
    bool isSync(unsigned int idx);
};

#endif /* IRLink_hpp */
