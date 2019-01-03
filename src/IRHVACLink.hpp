//
//  IRHVACLink.hpp
//  

#ifndef IRHVACLink_hpp
#define IRHVACLink_hpp

#include <stdio.h>
#include "Arduino.h"
#include "IRHVACConfig.hpp"

// ring buffer size has to be large enough to fit
// data between two successive sync signals
#define RING_BUFFER_SIZE  550

#if defined(__AVR__)
#define IR_DDRPRT DDRA
#define IR_SENDPORT PORTA
#define IR_PIN PA3
#elif defined(ESP8266)
#define IR_PIN D1
#endif

typedef enum IRMsgStateE {Preamble, Message} IRMsgState;

class IRHVACLink {
public:
    IRHVACLink(const IRHVACConfig *_config);
    ~IRHVACLink();
    
    // NOTE: caller owns memory pointed to and it is presumed to have enough
    // valid data to satisfy the IRHVACConfig defintion of the message
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

    static const IRHVACConfig *config;
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

#endif /* IRHVACLink_hpp */
