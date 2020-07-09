#include <stdlib.h>
#include "IRNECRemote.hpp"
#include <ArduinoJson.h>


IRConfig IRNECRemote::config;

IRNECRemote::IRNECRemote() {
    config.msgSamplesCnt = MESSAGE_SAMPLES;
    config.msgBitsCnt = MESSAGE_BITS;
    config.msgSyncCnt = MESSAGE_SYNC_BITS;
    config.syncLengths[0] = IRPulseLengthUsS(SYNC_PREAMBLE_0);
    config.syncLengths[1] = IRPulseLengthUsS(SYNC_PREAMBLE_1);

    config.bitSeparatorLength = IRPulseLengthUsS(SEP_LENGTH0);
    config.bitZeroLength = IRPulseLengthUsS(BIT0_LENGTH);
    config.bitOneLength = IRPulseLengthUsS(BIT1_LENGTH);
    config.msgBreakLength = IRPulseLengthUsS(EOT_LENGTH);
};
IRConfig *IRNECRemote::getIRConfig() {
    return (IRConfig *)&config;
}

// Check that buffer pointer has valid data and copy locally if so
bool IRNECRemote::isValid(uint8_t *msg, bool setCRC) {
    // Test that each pair of bytes is inverse of the next
    if( (msg[2] & 0xFF) == (~msg[3] & 0xff)) {
        memmove(message, msg, MSGSIZE_BYTES(MESSAGE_SAMPLES,MESSAGE_BITS) * sizeof(uint8_t));
        message[0] = IRLink::reverse(message[0]);
        message[1] = IRLink::reverse(message[1]);
        return true;
    }
    return false;
}
// Return message content
irMsg IRNECRemote::getMessage() {
    irMsg v;
    v.addr = (((uint16_t)message[1]) << 8) | message[0];
    v.cmd = message[2];
    return v;
}
void IRNECRemote::setMessage(irMsg m) {
    message[0] = IRLink::reverse(m.addr & 0xff);
    message[1] = IRLink::reverse((m.addr >> 8) & 0xff);
    message[2] = m.cmd;
    message[3] = ~m.cmd;
}
uint8_t *IRNECRemote::rawMessage() {
    return message;
}

char *IRNECRemote::toBuff(char *buf) {
    irMsg m = this->getMessage();
    int pos = 0;
    sprintf(buf,"0x");
    APND_CHARBUFF(pos,buf,"%04hX ", m.addr)
    APND_CHARBUFF(pos,buf,"%02hX ", m.cmd)
    pos = strlen(buf);
    return buf;
}
