#include "src/IRHVACLink.hpp"
#include "src/SenvilleAURA.hpp"

IRHVACLink *irReceiver;
SenvilleAURA *senville;
char outputBuff[100];

void setup() {
  Serial.begin(115200);
  Serial.println("Started.");
  senville = new SenvilleAURA();
  irReceiver = new IRHVACLink(senville->getIRHVACConfig());
  irReceiver->listen();
}

void loop() {
  uint8_t *mem = irReceiver->loop_chkMsgReceived();
  if(mem != NULL) {
    Serial.print("Received message : 0x");
    for(int i=0; i<MSGSIZE_BYTES(MESSAGE_SAMPLES,MESSAGE_BITS) ; i++)
      Serial.printf("%0X ",mem[i]);
    Serial.println();
 
    if(senville->isValid(mem)) {
      Serial.print("Validated message : ");
      senville->toBuff(outputBuff);
      Serial.println(outputBuff);
      senville->toJsonBuff(outputBuff);
      Serial.println(outputBuff);

      // Test sending value
      delay(5000);
      Serial.println("Check sent message on scope etc.");
      irReceiver->send(senville->getMessage());      
      Serial.println("listening...");
    }
    irReceiver->listen();
  }
}
