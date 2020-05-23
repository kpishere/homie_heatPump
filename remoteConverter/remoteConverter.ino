#include "src/IRLink.hpp"
#include "src/IRNECRemote.hpp"
//#define DEBUG

typedef struct necIrCmdMapS {
    unsigned char a;
    unsigned char b;
    void init(char _a, char _b) {
        a = _a;
        b = _b;
    }
} necIrCmdMap;

const uint16_t LMViewAddr = 0xFF00;
const uint16_t otherAddr = 0x6B86;

necIrCmdMap lmViewToAnon[] = {
    { 0x0C,0xF8}, { 0x0D,0xC0}, { 0x09,0x18}, { 0x05,0x58}, { 0x01,0xD8}, { 0x4F,0xDA}
    , { 0x4B,0x38}, { 0x47,0x68}, { 0x43,0xE8}, { 0x4E,0xA0}, { 0x4A,0x08}, { 0x46,0x48}
    , { 0x42,0xC8}, { 0x4D,0x6A}, { 0x49,0xF0}, { 0x41,0x30}, { 0x44,0x40}, { 0x03,0xA8}
    , { 0x4C,0x20}, { 0x06,0x80}, { 0x40,0xE0}, { 0x0B,0xC2}, { 0x4B,0x60}, { 0x07,0x22}
    , { 0x57,0x88}, { 0x08,0x7A}, { 0x04,0xBA}, { 0x5B,0xB0}, { 0x5F,0xFA}, { 0x0A,0x3A}
    , { 0x10,0x98}, { 0x14,0x52}, { 0x18,0xD2}, { 0x1C,0x22}, { 0x54,0x92}, { 0x58,0x32}
    , { 0x5C,0x10}, { 0x51,0x8A}, { 0x55,0xE2}, { 0x59,0x02}, { 0x5D,0x38}, { 0x52,0xB2}
    , { 0x56,0x9A}, { 0x5A,0x5A}, { 0x5E,0x00}
};

IRLink *irReceiver;
IRNECRemote *rmt;
char outputBuff[100];

irMsg translateB2A(uint16_t in_b0, irMsg in,
    uint16_t out_b0) {
    irMsg out = in;
    int items = sizeof(lmViewToAnon) / sizeof(necIrCmdMap);

    if(in.addr == in_b0) {
        for(int i = 0; i < items; i++) {
            if(in.cmd == lmViewToAnon[i].b) {
                out.addr = out_b0;
                out.cmd = lmViewToAnon[i].a;
                exit;
            }
        }
    }
    return out;
}

void setup() {
  char buff[800];
  IRConfig *cnf;
#ifdef DEBUG
  Serial.begin(115200);
  Serial.println("\nStarted.");
#endif
  rmt = new IRNECRemote();
  cnf = rmt->getIRConfig();
#ifdef DEBUG
  Serial.println("Signal configuration:");
  Serial.println(cnf->display(buff));
#endif
  irReceiver = new IRLink(cnf, D2, D1);
  irReceiver->listen();  
}
void loop() {
  uint8_t *mem = irReceiver->loop_chkMsgReceived();
  if(mem != NULL) {
#ifdef DEBUG   
    Serial.print("Received message : 0x");
    for(int i=0; i<MSGSIZE_BYTES(MESSAGE_SAMPLES,MESSAGE_BITS) ; i++) {
      Serial.print(mem[i], HEX); Serial.print(" "); 
    }
    Serial.println();
 #endif
    if(rmt->isValid(mem)) {
#ifdef DEBUG
      Serial.print("Validated message : ");
      rmt->toBuff(outputBuff);
      Serial.println(outputBuff);
#endif
      // Translate
      rmt->setMessage( translateB2A(otherAddr, rmt->getMessage(), LMViewAddr) );
#ifdef DEBUG
      // Test sending value
      Serial.println("Check sent message on scope etc.");

      Serial.print("Sending message : 0x");
      mem = rmt->rawMessage();
      for(int i=0; i<MSGSIZE_BYTES(MESSAGE_SAMPLES,MESSAGE_BITS) ; i++) {
        Serial.print(mem[i], HEX); Serial.print(" "); 
      }
      Serial.println();
#endif 
      irReceiver->send(rmt->rawMessage());   
   
#ifdef DEBUG
      Serial.println("listening...");
#endif
    }
    irReceiver->listen();
  }  
}
