//
//  IRLink.cpp
//
//  Hardware layer implementation of IR pulse signaling
//
#include "IRLink.hpp"
#include <HardwareTimer.h>
#if defined(__AVR__)
#else // defined(ESP8266)
#include <pins_arduino.h>
#endif
//#define DEBUG
//#define DEBUG_BITS

#if defined(__AVR__)
    #if defined(__AVR_ATmega32U4__)
        // TICKS PER us
        #define IR_SEND_ADJ 2.069
    #else
        // TICKS PER us for MEGA 2560
        #define IR_SEND_ADJ 2.069
    #endif
#else //defined(ESP8266)
// Ticks per us for ESP8266
#define IR_SEND_ADJ 5.148
#endif

// Receive values of pointers and state variables
IRConfig *IRLink::config;
volatile unsigned long IRLink::lastTime = micros();
volatile unsigned int IRLink::ringIndex = 0;
volatile unsigned int IRLink::syncIndex1 = 0;
volatile unsigned int IRLink::edgeCount1 = 0;
volatile unsigned int IRLink::edgeCount = 0;
volatile bool IRLink::received = false;
volatile IRMsgState IRLink::state = Preamble;
volatile unsigned long IRLink::timings[RING_BUFFER_SIZE];
uint8_t IRLink::pinX, IRLink::pinR; // Assignable send/receive pins

uint8_t *msgReceivedPtr;
const unsigned char byteMask[8] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};

// Received message value pointers
#define MSGSIZE(samp,msgbits,sync,brk) ((samp) * (2 * (msgbits + ( (brk)>0 ? 1 : 0) ) + (sync)  ))

// Send values of pointer and memory location for pulse length times
int volatile tc1_ptr;
#if defined(__AVR__)
    unsigned short volatile *pulsesToSend;
#else // defined(ESP8266)
    uint32_t volatile *pulsesToSend;
#endif

// Only one instance of this class is supported, the last
// class to invoke listen() wins.  First class to exit disables interrupt.
IRLink *lastInstance;
void IRAM_ATTR ISRHandler() {
    if(lastInstance) lastInstance->handler();
}

// Timer compare A interrup service routine
#if defined(__AVR__)
ISR(TIMER1_COMPA_vect){
    cli();
    #if defined(__AVR_ATmega32U4__)
        // Toggle output value
        IR_SENDPORT ^= _BV( ATmega32U4_ProMicroWiring(IRLink::pinX));
    #else
        // Toggle output value
        IR_SENDPORT ^= _BV(IRLink::pinX);
    #endif
    // Set next timer value
    OCR1A = pulsesToSend[tc1_ptr];
    // Increment pointer in array
    tc1_ptr++;
    // If at end, stop
    if ( tc1_ptr >= MSGSIZE(IRLink::config->msgSamplesCnt,IRLink::config->msgBitsCnt
                            ,IRLink::config->msgSyncCnt
                            ,IRLink::config->msgBreakLength.val)) {
        // disable timer compare interrupt
        TIMSK1 &= ~_BV(OCIE1A);
    }
    sei();
}
#else // defined(ESP8266)
void ICACHE_RAM_ATTR onTimer1ISR(void *argptr){
    cli();
    // Toggle output value
    digitalWrite(IRLink::pinX,!(digitalRead(IRLink::pinX)));  //Toggle LED Pin
    // Set next timer value
    hw_timer1_write(pulsesToSend[tc1_ptr]);
    // Increment pointer in array
    tc1_ptr++;
    // If at end, stop
    if ( tc1_ptr >= MSGSIZE(IRLink::config->msgSamplesCnt,IRLink::config->msgBitsCnt
                            ,IRLink::config->msgSyncCnt
                            ,IRLink::config->msgBreakLength.val)) {
        // disable timer compare interrupt
        hw_timer1_disable();
        pinMode(IRLink::pinX,INPUT);
    }
    sei();
}
#endif

void configSend() {
    cli();//stop interrupts
#if defined(__AVR__)
    #if defined(__AVR_ATmega32U4__)
        // Setup X-mit pin
        // Set up pin for output
        IR_DDRPRT  |=  _BV( ATmega32U4_ProMicroWiring(IRLink::pinX) );
        // Set value high
        IR_SENDPORT |= _BV( ATmega32U4_ProMicroWiring(IRLink::pinX) );
    #else
        // Setup X-mit pin
        // Set up pin for output
        IR_DDRPRT  |= _BV(IRLink::pinX);
        // Set value high
        IR_SENDPORT |= _BV(IRLink::pinX);
    #endif

        // Set up timer counter
        TCCR1A = 0;// set entire TCCR1A register to 0
        TCCR1B = 0;// same for TCCR1B
        TCNT1  = 0;//initialize counter value to 0
        // turn on CTC mode
        TCCR1B |= _BV(WGM12);
        // Set CS11 bit for 8 prescaler (0.000,000,5s or 1/2 micro-second)
        TCCR1B |= _BV(CS11);
        // disable timer compare interrupt
        TIMSK1 &= ~_BV(OCIE1A);
#else // defined(ESP8266)
    hw_timer_init();
    pinMode(IRLink::pinX,OUTPUT);
    digitalWrite(IRLink::pinX,HIGH);
#endif
    sei();//allow interrupts
}


//////////
// Class methods
//////////
IRLink::IRLink(IRConfig *_config, uint8_t ppinX, uint8_t ppinR) {
    config = _config;
    pinX = ppinX;
    pinR = ppinR;
#if defined(__AVR__)
    pulsesToSend = (unsigned short *)malloc(sizeof(unsigned short)*MSGSIZE(config->msgSamplesCnt,config->msgBitsCnt,config->msgSyncCnt,config->msgBreakLength.val));
#else // defined(ESP8266)
    pulsesToSend = (uint32_t *)malloc(sizeof(uint32_t)*MSGSIZE(config->msgSamplesCnt,config->msgBitsCnt,config->msgSyncCnt,config->msgBreakLength.val));
#endif

    msgReceivedPtr = (uint8_t *)malloc(sizeof(uint8_t) * MSGSIZE_BYTES(config->msgSamplesCnt,config->msgBitsCnt));
#ifdef DEBUG
    Serial.print("IRLink::IRLink");
#endif
    if(pinX != pinR) {
        pinMode(pinX, OUTPUT);
        digitalWrite(pinX,HIGH);
    }
}
IRLink::~IRLink() {
    if(pulsesToSend) free((void *)pulsesToSend);
    if(msgReceivedPtr) free((void *)msgReceivedPtr);
    lastInstance = 0;
}
void IRLink::listen() {
    lastInstance = this;
    // clear buffer
    for(unsigned int i = 0; i<RING_BUFFER_SIZE; i++) timings[i] = 0;
    // Clear msgReceivedPtr
    if(msgReceivedPtr != NULL) for(int i=0; i<MSGSIZE_BYTES(config->msgSamplesCnt,config->msgBitsCnt); i++) msgReceivedPtr[i] = 0;
    attachInterrupt(digitalPinToInterrupt(pinR), ISRHandler, CHANGE);
    pinMode(pinR, INPUT);
}
void IRLink::listenStop() {
    detachInterrupt(digitalPinToInterrupt(pinR));
}
void IRLink::send(uint8_t *msg, bool noWait) {
    short ptr, slptr;
    unsigned int duration = 0;

    for(ptr = 0; ptr < MSGSIZE(config->msgSamplesCnt,config->msgBitsCnt,config->msgSyncCnt,config->msgBreakLength.val); ptr++ ) {
        // The synch pulses
        for(slptr = 0; slptr < config->msgSyncCnt; slptr++) {
            if(ptr % (MSGSIZE(1,config->msgBitsCnt,config->msgSyncCnt,config->msgBreakLength.val)) == slptr) {
                pulsesToSend[ptr] = config->syncLengths[slptr].val;
#ifdef DEBUG_BITS
                Serial.print("synch "); Serial.print(pulsesToSend[ptr]);
                Serial.print(" ptr "); Serial.print(ptr);
                Serial.print(" slptr "); Serial.println(slptr);
                Serial.print(" == "); Serial.println(MSGSIZE(1,config->msgBitsCnt,config->msgSyncCnt,config->msgBreakLength.val));
#endif
            }
        }
        // Msg body
        if( ptr % (MSGSIZE(1,config->msgBitsCnt,config->msgSyncCnt,config->msgBreakLength.val))
           >= config->msgSyncCnt
           && ptr % (MSGSIZE(1,config->msgBitsCnt,config->msgSyncCnt,config->msgBreakLength.val))
           < (MSGSIZE(1,config->msgBitsCnt,config->msgSyncCnt,0))
           ) {
            if(ptr%2) { // odd bit pulse
                short hptr = (short)(ptr/2 - (config->msgSyncCnt/2) * ( 1 + ptr / MSGSIZE(1,config->msgBitsCnt,config->msgSyncCnt,config->msgBreakLength.val)) - (ptr / MSGSIZE(1,config->msgBitsCnt,config->msgSyncCnt,config->msgBreakLength.val)) );
                pulsesToSend[ptr] = (msg[(short)(hptr / BITS_IN_BYTE)] & byteMask[(short)(hptr % BITS_IN_BYTE)]
                                     ? config->bitOneLength.val : config->bitZeroLength.val );
#ifdef DEBUG_BITS
                Serial.print("val "); Serial.print(pulsesToSend[ptr]);
                Serial.print(" ptr "); Serial.print(ptr);
                Serial.print(" hptr "); Serial.print(hptr);
                Serial.print(" byt "); Serial.print((short)(hptr / BITS_IN_BYTE));
                Serial.print(" bit "); Serial.println((short)(hptr % BITS_IN_BYTE));
#endif
            } else { // even is separator pulse
                pulsesToSend[ptr] = config->bitSeparatorLength.val;
#ifdef DEBUG_BITS
                Serial.print("spc "); Serial.print(pulsesToSend[ptr]);
                Serial.print(" ptr "); Serial.println(ptr);
#endif
            }
        }
        // Msg break
        if(config->msgBreakLength.val>0 && ptr > 0
           && ptr % (MSGSIZE(1,config->msgBitsCnt,config->msgSyncCnt,config->msgBreakLength.val))
           >= MSGSIZE(1,config->msgBitsCnt,config->msgSyncCnt,0)
           ) {
               if(ptr%2) { // odd bit pulse
                   pulsesToSend[ptr] = config->msgBreakLength.val;
#ifdef DEBUG
                   Serial.print("brk "); Serial.print(pulsesToSend[ptr]);
                   Serial.print(" ptr "); Serial.println(ptr);
#endif
               } else { // even is separator pulse
                   pulsesToSend[ptr] = config->bitSeparatorLength.val;
#ifdef DEBUG
                   Serial.print("spc "); Serial.print(pulsesToSend[ptr]);
                   Serial.print(" ptr "); Serial.println(ptr);
#endif
               }
        }
    }
    // Scale the values as per timer configuration
    for(ptr = 0; ptr < MSGSIZE(config->msgSamplesCnt,config->msgBitsCnt,config->msgSyncCnt,config->msgBreakLength.val); ptr++ ) {
        duration += pulsesToSend[ptr];
        pulsesToSend[ptr] = (pulsesToSend[ptr] * IR_SEND_ADJ) + 0.5;
    }
#ifdef DEBUG
    Serial.print("pulses "); Serial.println(MSGSIZE(config->msgSamplesCnt,config->msgBitsCnt,config->msgSyncCnt,config->msgBreakLength.val));
    Serial.print("pulse dur "); Serial.println(duration);
#endif
    configSend();
    cli();
    // Set array pointer to first byte
    tc1_ptr = 0;
#if defined(__AVR__)
    // Set first comparitor value to trigger in short time
    OCR1A =  (config->syncLengths[0].val * IR_SEND_ADJ) + 0.5;
    // enable timer compare interrupt
    TIMSK1 |= _BV(OCIE1A);
#else // defined(ESP8266)
    //Initialize Ticker every 5 ticks/us - 1677721.4 us max
    hw_timer1_attach_interrupt((hw_timer_source_type_t)0,onTimer1ISR, nullptr);
    // Set first comparitor value to trigger in short time & enable interrupt
    hw_timer1_enable(TIMER_CLKDIV_16, TIMER_EDGE_INT, TIMER_FRC1_SOURCE);
    hw_timer1_write((config->syncLengths[0].val * IR_SEND_ADJ) + 0.5);
#endif
    sei();
    if(!noWait) delay(duration / 100);
}

// detect if a sync signal is present
bool IRLink::isSync(unsigned int idx) {
    // Test for each expected preamble value
    for(unsigned int i= 0; i < this->config->msgSyncCnt; i++) {
        unsigned long v =  timings[(idx+RING_BUFFER_SIZE-this->config->msgSyncCnt+i+1) % RING_BUFFER_SIZE];
        if( v < this->config->syncLengths[i].lo || v > this->config->syncLengths[i].hi ) {
            return false;
        }
    }
    return true;
};

/* Interrupt handler */
void IRLink::handler() {
    unsigned long duration = 0;

    // ignore if we haven't processed the previous received signal
    if (received == true)  return;
    // calculating timing since last change
    unsigned long time = micros();
    duration = diffRollSafeUnsignedLong(lastTime,time);

    lastTime = time;

    // store data in ring buffer
    ringIndex = (ringIndex + 1) % RING_BUFFER_SIZE;
    timings[ringIndex] = duration;

    switch(state) {
        case Preamble:
            if(isSync(ringIndex)) {
                syncIndex1 = (ringIndex+RING_BUFFER_SIZE-this->config->msgSyncCnt+1+1) % RING_BUFFER_SIZE;
                state = Message;
                edgeCount1 = 0;
                edgeCount = this->config->msgSyncCnt;
                return;
            }
            break;
        case Message:
            edgeCount++;
            // A sync in message state is a second re-transmession of message
            if (edgeCount1 || isSync((ringIndex - 1) % RING_BUFFER_SIZE)) {
                edgeCount1++;
                if (edgeCount > (this->config->msgBitsCnt * 2 * config->msgSamplesCnt + this->config->msgSyncCnt) )
                {
                    // and wait for msg to be picked up
                    this->listenStop();
                    received = true;
                    return;
                }
            }
            break;
    }
};

uint8_t *IRLink::loop_chkMsgReceived() {
    byte *result = NULL;
    unsigned int bitInMsg = 0;

    if (received == true) {
#ifdef DEBUG
         Serial.print("preamble: ");
         for(unsigned int i= 0; i < config->msgSyncCnt; i++) {
             unsigned long v =  timings[(syncIndex1+RING_BUFFER_SIZE-config->msgSyncCnt+i+1) % RING_BUFFER_SIZE];
             Serial.print(v); Serial.print(" ");
         }
         Serial.print("edgeCount: ");
         Serial.print(edgeCount);
         Serial.print(" edgeCount1: ");
         Serial.println(edgeCount1);
#endif
        // Value output
        for(unsigned int i=config->msgSyncCnt; i<(edgeCount-config->msgSyncCnt); i+=2) {
            unsigned long t0 = timings[(syncIndex1+RING_BUFFER_SIZE-config->msgSyncCnt+i+1) % RING_BUFFER_SIZE]
                ,         t1 = timings[(syncIndex1+RING_BUFFER_SIZE-config->msgSyncCnt+i+1+1) % RING_BUFFER_SIZE];

#ifdef DEBUG
            Serial.print(" ");
            Serial.print(i);
            Serial.print(" t0 ");
            Serial.print(t0);
            Serial.print(" t1 ");
            Serial.print(t1);
            Serial.println("");
#endif
            if (t1>(this->config->bitZeroLength.lo) && t1<(this->config->msgBreakLength.hi)) { // Highest and lowest possible of all
                if (t1>this->config->bitZeroLength.lo && t1<this->config->bitZeroLength.hi) {
                    // Do nothing as buffer is initialized to zero
                    bitInMsg++;
                }
                if (t1>this->config->bitOneLength.lo && t1<this->config->bitOneLength.hi) {
                    msgReceivedPtr[(short)(bitInMsg / BITS_IN_BYTE)] |=
                        byteMask[(short)(bitInMsg % BITS_IN_BYTE)];
                    bitInMsg++;
                }
                // All sync durations are longer than
                if ((t1>this->config->msgBreakLength.lo && t1<this->config->msgBreakLength.hi) /* At synch signal == eot*/
                    || (i >= (edgeCount1 - 1)) /* at end of message length */
                ) {
                    if( bitInMsg == this->config->msgBitsCnt) {
                        result = msgReceivedPtr; // Set the return pointer, we got something
                        // Advance to next valid space pulse
                        i += config->msgSyncCnt;
                    }
                }
            } else { // Non-compliant message, reset
                bitInMsg = 0;
                result = NULL;
            }
        }
        received = false;
        state = Preamble;
        lastTime = micros();
    }
    return result;
}

uint8_t IRLink::reverse(uint8_t b) {
   b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
   b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
   b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
   return b;
}
