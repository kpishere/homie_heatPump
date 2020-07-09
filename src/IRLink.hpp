//
//  IRLink.hpp
//  

#ifndef IRLink_hpp
#define IRLink_hpp

#include <stdio.h>
#include "Arduino.h"

#if defined(__AVR__)
    // ring buffer size has to be large enough to fit
    // data between two successive sync signals
    #if defined(__AVR_ATmega32U4__)
        #define RING_BUFFER_SIZE  100 /* NOT MUCH ROOM! */
        #define ATmega32U4_ProMicroWiring(p) ( (p==0?2:(p==1?3:(p==2?1:(p==3?0:4)))) )
        #define IR_DDRPRT DDRD
        #define IR_SENDPORT PORTD
        #define IR_PINR 2
        #define IR_PINX 2
    #else
        #define RING_BUFFER_SIZE  550
        #define IR_DDRPRT DDRA
        #define IR_SENDPORT PORTA
        #define IR_PINR PA3
        #define IR_PINX PA3
    #endif
#elif defined(ESP8266)
    #define RING_BUFFER_SIZE  550
    #define IR_PINR D1
    #define IR_PINX D1
#endif

#define TOLERANCE_PERCENT 0.25f

#define MAX_SYNCS 2

// Memory allocation function for containing message sent/received
// Note: remainder test is for non-8-bit multiple message sizes
#define MSGSIZE_BYTES(samp,msgbits) ((samp) * ((msgbits) % BITS_IN_BYTE > 0 ? 1 : 0) + (samp) * (msgbits) / BITS_IN_BYTE )

#define CALC_LO(v) (unsigned long)(v * (1.0 - TOLERANCE_PERCENT ) )
#define CALC_HI(v) (unsigned long)(v * (1.0 + TOLERANCE_PERCENT ) )
#define diffRollSafeUnsignedLong(t1,t2) (t2>t1? t2-t1 : t2+4294967295-t1 )
#define BITS_IN_BYTE 8

// Hi/lo tolerance is used for pulse duration timing
#define APND_CHARBUFF(pos,buf,arg0,arg1) (pos) = strlen(buf); sprintf(&(buf)[(pos)],arg0,arg1);
typedef struct IRPulseLengthUsS {
    unsigned short lo;
    unsigned short val;
    unsigned short hi;
    IRPulseLengthUsS(unsigned short _val = 0.0) {
        lo = CALC_LO(_val);
        hi = CALC_HI(_val);
        val = _val;
    }
    char *display(char *buf, int &pos) {
        APND_CHARBUFF(pos,buf,"%d ", val)
        APND_CHARBUFF(pos,buf,"%d ", lo)
        APND_CHARBUFF(pos,buf,"%d ", hi)
        pos = strlen(buf);
        return buf;
    }
} IRPulseLengthUs;

// Message definition
typedef struct IRConfigS {
    uint8_t msgSamplesCnt;
    uint8_t msgBitsCnt;
    uint8_t msgSyncCnt;
    IRPulseLengthUs syncLengths[MAX_SYNCS];
    IRPulseLengthUs bitSeparatorLength;
    IRPulseLengthUs bitZeroLength;
    IRPulseLengthUs bitOneLength;
    IRPulseLengthUs msgBreakLength;
    char *display(char *buf) {
        int pos = 0;
        sprintf(buf,"\nsamples "); APND_CHARBUFF(pos,buf,"%0d ", msgSamplesCnt)
        APND_CHARBUFF(pos,buf,"\nbits ", ""); APND_CHARBUFF(pos,buf,"%0d ", msgBitsCnt)
        APND_CHARBUFF(pos,buf,"\nsync ", ""); APND_CHARBUFF(pos,buf,"%0d ", msgSyncCnt)
        for(int i=0; i< msgSyncCnt; i++) {
            APND_CHARBUFF(pos,buf,"\n ", ""); syncLengths[i].display(buf, pos);
        }
        APND_CHARBUFF(pos,buf,"\nsep ", "");bitSeparatorLength.display(buf, pos);
        APND_CHARBUFF(pos,buf,"\nzro ", "");bitZeroLength.display(buf, pos);
        APND_CHARBUFF(pos,buf,"\none ", "");bitOneLength.display(buf, pos);
        APND_CHARBUFF(pos,buf,"\nbrk ", "");msgBreakLength.display(buf, pos);
        pos = strlen(buf);
        return buf;
    }
} IRConfig;

typedef enum IRMsgStateE {Preamble, Message} IRMsgState;

class IRLink {
public:
    IRLink(IRConfig *_config, uint8_t ppinX = IR_PINX, uint8_t ppinR = IR_PINR);
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

    static IRConfig *config;
    static uint8_t pinX, pinR; // Assignable send/receive pins
    
    // Utillity methods
    static uint8_t reverse(uint8_t b);
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
