//
//  SenvilleAURA.cpp
//
#include <stdlib.h>
#include "SenvilleAURA.hpp"
#include <ArduinoJson.h>
#define SHOW_RAWDATA
//#define DEBUG

#define JSON_PARSEBUFFER 100

// Json command/stat strings
#define CMD_ISON    "IsOn"
#define CMD_INSTR   "Instr"
#define CMD_MODE    "Mode"
#define CMD_FSPD    "FanSpeed"
#define CMD_SLP     "IsSleepOn"
#define CMD_STMP    "SetTemp"
#define STAT_SMPLID "SampleId"
#define STAT_ONTME  "OnTimeMs"
#define CMD_MTMP    "MeasTemp"
#define CMD_STATE   "State"
#define CMD_MTMP    "MeasTemp"
#define CMD_MTMP    "MeasTemp"
#define CMD_OPT     "Opt"
#define STAT_RAW    "Unknown"

uint8_t calcChecksum(const uint8_t state[],
                                  const uint16_t length) {
    uint8_t checksum = 0;
#ifdef DEBUG
    Serial.print("len ");
    Serial.println(length);
#endif
    for (uint8_t i = 0; i < length; i++) {
            checksum += IRLink::reverse(state[i]);
#ifdef DEBUG
        Serial.printf("crc val %0X\n",state[i]);
#endif
    }
    return checksum - 0x01;
}

////////
//
////////
IRConfig SenvilleAURA::config;

uint8_t SenvilleAURA::calcCRC(short _sample) {
    return IRLink::reverse(~(calcChecksum(&message[MSG_CONST_STATE(_sample)], MSG_CONST_STATE(1)-1)));
}
SenvilleAURA::SenvilleAURA() {
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

    this->sampleId = 0;
    this->lastSampleMs = 0;
    this->setTempDegC = 0;
    this->validSamplePtr = 0;
};
IRConfig *SenvilleAURA::getIRConfig() {
    return (IRConfig *)&config;
}
bool SenvilleAURA::isValid(uint8_t *msg, bool setCRC) {
    this->validSamplePtr = 0;
    uint8_t calcCRC = 0;

    // Test msg constant for correct on/off to high/low bit logic
    if(!msg[MSG_CONST_STATE(this->validSamplePtr)] & 0xA0) {
        this->validSamplePtr = 1;
        // If next sample does not match logic, msg is not valid
        if(!msg[MSG_CONST_STATE(this->validSamplePtr)] & 0xA0)
            return false;
    }
#ifdef DEBUG
    Serial.print("cnt "); Serial.println(MSGSIZE_BYTES(MESSAGE_SAMPLES,MESSAGE_BITS) * sizeof(uint8_t) );
#endif
    memmove(message, msg, MSGSIZE_BYTES(MESSAGE_SAMPLES,MESSAGE_BITS) * sizeof(uint8_t));
#ifdef DEBUG
    Serial.print("ptr "); Serial.println(this->validSamplePtr);
#endif
    calcCRC = SenvilleAURA::calcCRC(this->validSamplePtr);
#ifdef DEBUG
    Serial.printf("crc %0X\n",calcCRC);
#endif
    if(setCRC || calcCRC == msg[MSG_CRC(this->validSamplePtr)]) {
        if(setCRC) message[MSG_CRC(this->validSamplePtr)] = calcCRC;
        this->sampleId++;
        this->lastSampleMs = millis();
        return true;
    }
    return false;
}
uint8_t *SenvilleAURA::getMessage() {
    // If second msg is being used, copy to first spot
    if(this->validSamplePtr) {
        memmove(message, &message[MSG_CONST_STATE(this->validSamplePtr)],
            MSG_CONST_STATE(1) * sizeof(uint8_t));
    }
    // copy from first to second spot and also reversing the bits
    for(int ptr=0; ptr < MSG_CONST_STATE(1); ptr++ )
        message[MSG_CONST_STATE(1)+ptr] = ~message[ptr];
    // Update the CRCs
    message[MSG_CRC(0)] = SenvilleAURA::calcCRC(0);
    message[MSG_CRC(1)] = ~message[MSG_CRC(0)];
    return message;
}
#define APND_CHARBUFF(pos,buf,arg0,arg1) (pos) = strlen(buf); sprintf(&(buf)[(pos)],arg0,arg1);
char *SenvilleAURA::toBuff(char *buf) {
    int pos = 0;
    sprintf(buf,"0x");
    for(uint8_t ptr = 0; ptr < MSG_CONST_STATE(1); ptr++) {
        APND_CHARBUFF(pos,buf,"%0X ", message[ptr])
    }
    pos = strlen(buf);
    sprintf(&buf[pos], "%d %d", this->sampleId, this->lastSampleMs);
    return buf;
}
char *SenvilleAURA::toJsonBuff(char *buf) {
    int pos = 0, i;
    sprintf(buf,"");
    switch(this->getInstructionType()) {
        case Instruction::Command:
            APND_CHARBUFF(pos,buf,"{"CMD_ISON":%d ", this->getPowerOn())
            APND_CHARBUFF(pos,buf,", "CMD_INSTR":%d ", this->getInstructionType())
            APND_CHARBUFF(pos,buf,", "CMD_MODE":%d ", this->getMode())
            APND_CHARBUFF(pos,buf,", "CMD_FSPD":%d ", this->getFanSpeed())
            APND_CHARBUFF(pos,buf,", "CMD_SLP":%d ", this->getSleepOn())
            APND_CHARBUFF(pos,buf,", "CMD_STMP":%d ", this->getSetTemp())
            APND_CHARBUFF(pos,buf,", "STAT_SMPLID":%d ", this->getSeqId())
            APND_CHARBUFF(pos,buf,", "STAT_ONTME":%d ", this->getOnTimeMs())
            break;
        case Instruction::FollowMe:
            APND_CHARBUFF(pos,buf,"{"CMD_ISON":%d ", this->getPowerOn())
            APND_CHARBUFF(pos,buf,", "CMD_INSTR":%d ", this->getInstructionType())
            APND_CHARBUFF(pos,buf,", "CMD_MODE":%d ", this->getMode())
            APND_CHARBUFF(pos,buf,", "CMD_FSPD":%d ", this->getFanSpeed())
            APND_CHARBUFF(pos,buf,", "CMD_SLP":%d ", this->getSleepOn())
            APND_CHARBUFF(pos,buf,", "CMD_MTMP":%d ", this->getFollowMeTemp())
            APND_CHARBUFF(pos,buf,", "CMD_STATE":%d ", this->getFollowMeState())
            APND_CHARBUFF(pos,buf,", "STAT_SMPLID":%d ", this->getSeqId())
            APND_CHARBUFF(pos,buf,", "STAT_ONTME":%d ", this->getOnTimeMs())
            break;
        case Instruction::InstrOption:
            APND_CHARBUFF(pos,buf,"{"CMD_INSTR":%d ", this->getInstructionType())
            APND_CHARBUFF(pos,buf,", "CMD_OPT":%d ", this->getOption())
            APND_CHARBUFF(pos,buf,", "STAT_SMPLID":%d ", this->getSeqId())
            APND_CHARBUFF(pos,buf,", "STAT_ONTME":%d ", this->getOnTimeMs())
            break;
        default: // Unsupported
#ifdef SHOW_RAWDATA
            APND_CHARBUFF(pos,buf,"{"STAT_RAW":0x", "")
            for(int i=0; i<MSGSIZE_BYTES(MESSAGE_SAMPLES,MESSAGE_BITS) ; i++) {
                APND_CHARBUFF(pos,buf,"%0X ",message[i])
            }
            APND_CHARBUFF(pos,buf,", "STAT_SMPLID":%d ", this->getSeqId())
            APND_CHARBUFF(pos,buf,", "STAT_ONTME":%d ", this->getOnTimeMs())
#endif
            break;
    }
    APND_CHARBUFF(pos,buf,"}", "")
    return buf;
}
bool SenvilleAURA::fromJsonBuff(char *buf, uint8_t *sendBuf) {
    bool isOn, slp;
    Mode mde;
    FanSpeed fsp;
    SenvilleAURA *sFlw, *sOpt;
    uint8_t mTmp;
    FollowMeState fms;
    Option opt;
    Instruction thisInstr;
    DynamicJsonBuffer  jsonBuffer(JSON_PARSEBUFFER);
    JsonObject& root = jsonBuffer.parseObject(buf);
    
    // Test if parsing succeeds.
    if (!root.success()) {
#ifdef DEBUG
        Serial.println("parseObject() failed");
#endif
        return false;
    }
    
    if(root.containsKey(CMD_INSTR)) {
        thisInstr = static_cast<Instruction>(root[CMD_INSTR].as<uint8_t>());
        this->setInstructionType(Instruction::Command);
        switch(thisInstr) {
            case Instruction::Command:
            case Instruction::FollowMe:
                if(root.containsKey(CMD_ISON)) {
                    isOn = root[CMD_ISON].as<bool>();
                    this->setPowerOn(isOn);
                }
                if(root.containsKey(CMD_MODE)) {
                    mde = static_cast<Mode>(root[CMD_MODE].as<uint8_t>());
                    this->setMode(mde);
                }
                if(root.containsKey(CMD_FSPD)) {
                    fsp = static_cast<FanSpeed>(root[CMD_FSPD].as<uint8_t>());
                    this->setFanSpeed(fsp);
                }
                if(root.containsKey(CMD_SLP)) {
                    slp = root[CMD_SLP].as<bool>();
                    this->setSleepOn(slp);
                }
                if(thisInstr == Instruction::Command) {
                    if(root.containsKey(CMD_STMP)) {
                        uint8_t tmp = static_cast<Option>(root[CMD_STMP].as<uint8_t>());
                        this->setSetTemp(tmp);
                    }
                    memmove(sendBuf, this->getMessage(), MSGSIZE_BYTES(MESSAGE_SAMPLES,MESSAGE_BITS) * sizeof(uint8_t));
                    return true;
                } else {
                    if(root.containsKey(CMD_MTMP) && root.containsKey(CMD_STATE)) {
                        mTmp = root[CMD_MTMP].as<uint8_t>();
                        fms = static_cast<FollowMeState>(root[CMD_STATE].as<uint8_t>());
                        sFlw = this->followMeCmd(this, fms, mTmp);
                        memmove(sendBuf, sFlw->getMessage(), MSGSIZE_BYTES(MESSAGE_SAMPLES,MESSAGE_BITS) * sizeof(uint8_t));
                        return true;
                    }
                }
                break;
            default: // An InstrOption
                if(root.containsKey(CMD_OPT)) {
                    opt = static_cast<Option>(root[CMD_OPT].as<uint8_t>());
                    sOpt = this->optionCmd(opt);
                    memmove(sendBuf, sOpt->getMessage(), MSGSIZE_BYTES(MESSAGE_SAMPLES,MESSAGE_BITS) * sizeof(uint8_t));
                    return true;
                }
                break;
        }
    }
    return false;
}

/////////
//
////////
Instruction SenvilleAURA::getInstructionType() {
    return static_cast<Instruction>(message[MSG_CONST_STATE(this->validSamplePtr)] & 0x07);
}
void SenvilleAURA::setInstructionType(Instruction instr) {
    message[MSG_CONST_STATE(this->validSamplePtr)] = 0xA0 | ((uint8_t)instr);
}
bool SenvilleAURA::getPowerOn() {
    return 0x80 & message[MSG_CMD_OPT(this->validSamplePtr)];
}
void SenvilleAURA::setPowerOn(bool newState) {
    message[MSG_CMD_OPT(this->validSamplePtr)]
    = (newState? message[MSG_CMD_OPT(this->validSamplePtr)] | 0x80
       : message[MSG_CMD_OPT(this->validSamplePtr)] & 0x7F );
}
Mode SenvilleAURA::getMode() {
    return static_cast<Mode>((message[MSG_CMD_OPT(this->validSamplePtr)] & 0x07));
}
void SenvilleAURA::setMode(Mode val) {
    message[MSG_CMD_OPT(this->validSamplePtr)] &= 0xF8;
    message[MSG_CMD_OPT(this->validSamplePtr)] |= (0x07 & (uint8_t)val);
}
FanSpeed SenvilleAURA::getFanSpeed() {
    return static_cast<FanSpeed>((message[MSG_CMD_OPT(this->validSamplePtr)] & 0x18) >> 3);
}
void SenvilleAURA::setFanSpeed(FanSpeed val) {
    Mode m = this->getMode();
    if( m == Mode::ModeAuto || m == Mode::Dry ) val = FanSpeed::FanAuto;
    message[MSG_CMD_OPT(this->validSamplePtr)] &= 0xE7;
    message[MSG_CMD_OPT(this->validSamplePtr)] |= ((val << 3) & 0x18);
}
bool SenvilleAURA::getSleepOn(){
    return message[MSG_CMD_OPT(this->validSamplePtr)] & 0x40;
}
void SenvilleAURA::setSleepOn(bool newState) {
    Mode m = this->getMode();
    if( m == Mode::Fan || m == Mode::Dry || !this->getPowerOn() ) return;
    message[MSG_CMD_OPT(this->validSamplePtr)]
        = (newState? message[MSG_CMD_OPT(this->validSamplePtr)]
           | 0x40 : message[MSG_CMD_OPT(this->validSamplePtr)] & 0xBF );
}
Option SenvilleAURA::getOption() {
    return static_cast<Option>(message[MSG_CMD_OPT(this->validSamplePtr)] & 0x1F);
}
SenvilleAURA *SenvilleAURA::optionCmd(Option val) {
    SenvilleAURA *obj = new SenvilleAURA();
    uint8_t *msg = obj->getMessage();
    msg[MSG_CONST_STATE(0)] = 0xA0 | (uint8_t)Instruction::InstrOption;
    msg[MSG_CMD_OPT(0)] = val;
    msg[MSG_RUNMODE(0)] = 0xff;
    msg[MSG_TIMESTART(0)] = 0xff;
    msg[MSG_TIMESTOP(0)] = 0xff;
    obj->isValid(msg, true);
    return obj;
}
uint8_t  SenvilleAURA::getSetTemp() {
    return (message[MSG_RUNMODE(this->validSamplePtr)] & 0x0f) + TEMP_LOWEST;
}
void SenvilleAURA::setSetTemp(uint8_t val) {
    Mode m = this->getMode();
    if( m == Mode::Fan ) return;
    message[MSG_RUNMODE(this->validSamplePtr)] &= 0xF0;
    message[MSG_RUNMODE(this->validSamplePtr)] |= (val-TEMP_LOWEST) & 0x0F;
}
uint8_t  SenvilleAURA::getTimeOff() {
    return 0; // Not implemented
}
void SenvilleAURA::setTimeOff(uint8_t hour) {}
uint8_t  SenvilleAURA::getTimeOn() {
    return 0; // Not implemented
}
void SenvilleAURA::setTimeOn(uint8_t hour) {}

uint8_t SenvilleAURA::getFollowMeTemp() {
    return message[MSG_TIMESTOP(this->validSamplePtr)];
}
FollowMeState SenvilleAURA::getFollowMeState() {
    return static_cast<FollowMeState>(message[MSG_TIMESTART(this->validSamplePtr)]);
}
//   Note: Heat pump expects an update in measured temperature every 3 minutes
SenvilleAURA *SenvilleAURA::followMeCmd(SenvilleAURA *fromState, FollowMeState newState,
                                               uint8_t measuredTemp) {
    uint8_t *msg = fromState->getMessage();
    msg[MSG_CONST_STATE(0)] = 0xA0 | (uint8_t)Instruction::FollowMe;
    msg[MSG_CMD_OPT(0)] &= 0x10;
    msg[MSG_RUNMODE(0)] |= 0x44;
    msg[MSG_TIMESTART(0)] = newState;
    msg[MSG_TIMESTOP(0)] = measuredTemp;
    fromState->isValid(msg, true);
    return fromState;
}
unsigned long  SenvilleAURA::getOnTimeMs() {
    return this->lastSampleMs;
}
unsigned long  SenvilleAURA::getSeqId() {
    return this->sampleId;
}
