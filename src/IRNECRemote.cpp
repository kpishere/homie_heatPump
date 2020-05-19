#include <stdlib.h>
#include "IRNECRemote.hpp"
#include <ArduinoJson.h>

IRConfig IRNECRemote::config;

IRNECRemote::IRNECRemote() {
    config.msgSamplesCnt = MESSAGE_SAMPLES;
    config.msgBitsCnt = MESSAGE_BITS;
    config.msgSyncCnt = MESSAGE_SYNC_BITS;
    config.syncLengths = (IRPulseLengthUs *)malloc(sizeof(IRPulseLengthUs *)*MESSAGE_SYNC_BITS);
    config.syncLengths[0] = IRPulseLengthUsS(SYNC_PREAMBLE_0);
    config.syncLengths[1] = IRPulseLengthUsS(SYNC_PREAMBLE_1);

    config.bitSeparatorLength = IRPulseLengthUsS(SEP_LENGTH0);
    config.bitZeroLength = IRPulseLengthUsS(BIT0_LENGTH);
    config.bitOneLength = IRPulseLengthUsS(BIT1_LENGTH);
    config.msgBreakLength = IRPulseLengthUsS(EOT_LENGTH);
};
const IRConfig *IRNECRemote::getIRConfig() {
    return (const IRConfig *)&config;
}

// Check that buffer pointer has valid data and copy locally if so
bool IRNECRemote::isValid(uint8_t *msg, bool setCRC) {
    // Test that each pair of bytes is inverse of the next
    if(msg[0] == ~msg[1] || msg[2] == msg[3]) {
        memmove(message, msg, MSGSIZE_BYTES(MESSAGE_SAMPLES,MESSAGE_BITS) * sizeof(uint8_t));
        return true;
    }
    return false;
}
// Return message content
irMsg IRNECRemote::getMessage() {
    irMsg v;
    v.addr = message[0];
    v.cmd = message[2];
    return v;
}
void IRNECRemote::setMessage(irMsg m) {
    message[0] = m.addr;
    message[1] = ~m.addr;
    message[2] = m.cmd;
    message[3] = ~m.cmd;
}
uint8_t *IRNECRemote::rawMessage() {
    return message;
}

#define APND_CHARBUFF(pos,buf,arg0,arg1) (pos) = strlen(buf); sprintf(&(buf)[(pos)],arg0,arg1);
char *IRNECRemote::toBuff(char *buf) {
    int pos = 0;
    sprintf(buf,"0x");
    APND_CHARBUFF(pos,buf,"%0X ", message[0])
    APND_CHARBUFF(pos,buf,"%0X ", message[2])
    pos = strlen(buf);
    return buf;
}
