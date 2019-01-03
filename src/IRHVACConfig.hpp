//
// A shared message configuration structure for physical definition of
// IR signal (after carrier de-modulation)
//

#ifndef IRHVACConfig_hpp
#define IRHVACConfig_hpp

#define TOLERANCE_PERCENT 0.21f
#define CALC_LO(v) (unsigned long)(v * (1.0 - TOLERANCE_PERCENT ) )
#define CALC_HI(v) (unsigned long)(v * (1.0 + TOLERANCE_PERCENT ) )
#define diffRollSafeUnsignedLong(t1,t2) (t2>t1? t2-t1 : t2+4294967295-t1 )
#define BITS_IN_BYTE 8

// Memory allocation function for containing message sent/received
// Note: remainder test is for non-8-bit multiple message sizes
#define MSGSIZE_BYTES(samp,msgbits) ((samp) * ((msgbits) % BITS_IN_BYTE > 0 ? 1 : 0) + (samp) * (msgbits) / BITS_IN_BYTE )

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
typedef struct IRHVACConfigS {
    uint8_t msgSamplesCnt;
    uint8_t msgBitsCnt;
    uint8_t msgSyncCnt;
    IRPulseLengthUs *syncLengths;
    IRPulseLengthUs bitSeparatorLength;
    IRPulseLengthUs bitZeroLength;
    IRPulseLengthUs bitOneLength;
    IRPulseLengthUs msgBreakLength;
} IRHVACConfig;

#endif IRHVACConfig_hpp
