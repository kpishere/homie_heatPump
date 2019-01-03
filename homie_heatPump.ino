#include <Homie.h>
#include "src/SenvilleAURADisp.hpp"
#include "src/IRHVACLink.hpp"
#include "src/SenvilleAURA.hpp"
#define DEBUG

#define FW_NAME "heatpump test"
#define FW_VERSION "0.0.1"

#define MAX_BUFFLEN 100
unsigned short maxBuffLen = MAX_BUFFLEN;

const int DEFAULT_UPDATE_INTERVAL = 180; // 3 min

IRHVACLink *irReceiver;
SenvilleAURA *senville;
SenvilleAURADisp *disp;
uint8_t byteMsgBuf[MSGSIZE_BYTES(MESSAGE_SAMPLES,MESSAGE_BITS)];
volatile char *controlBuff;
volatile char *displayBuff;
unsigned long lastUpdate = 0;

typedef enum PropertyE {
  Control
} Property;

HomieNode controlNode0("heatpump", "hvac");

HomieSetting<long> updateIntervalSetting("updateInterval", "The update interval in seconds");

void setupHandler() {
    String strVal = String((const char *)controlBuff);
    controlNode0.setProperty("control").send(strVal);
    strVal = String((const char *)displayBuff);
    controlNode0.setProperty("display").send(strVal);
}

void loopHandler() {
  unsigned long thisUpdate = millis();
  bool change = false;
  uint8_t *mem = NULL;

  // Check display hardware
  if(disp->hasUpdate()) {
    disp->toBuff((char *)displayBuff);
#ifdef DEBUG
    Serial.println((const char *)displayBuff);
#endif
    change = true;
    disp->listen();
  }

  // Check IR Link hardware
  mem = irReceiver->loop_chkMsgReceived();
  if(mem != NULL) {
#ifdef DEBUG
    Serial.print("Received message : 0x");
    for(int i=0; i<MSGSIZE_BYTES(MESSAGE_SAMPLES,MESSAGE_BITS) ; i++)
      Serial.printf("%0X ",mem[i]);
    Serial.println();
#endif 
    if(senville->isValid(mem)) {
#ifdef DEBUG
      Serial.print("Validated message : ");
      senville->toBuff((char *)controlBuff);
      Serial.println((char *)controlBuff);
#endif 
      senville->toJsonBuff((char *)controlBuff);
#ifdef DEBUG
      Serial.println((char *)controlBuff);
#endif
      change = change || true;
    }
    irReceiver->listen();
  }
  
  // Update Homie properties
  if (change || thisUpdate - lastUpdate >= updateIntervalSetting.get() * 1000UL || lastUpdate == 0) {
    setupHandler();
    lastUpdate = thisUpdate;
  }
}

bool propertyControlHandler(const HomieRange& range, const String& value) {
  return propertyHandler(Property::Control,range,value);
}

bool propertyHandler(Property prop, const HomieRange& range, const String& value) {
#ifdef DEBUG
        Serial.printf("propertyHandler '%d'\n",prop);
#endif
  switch(prop) {
    case Property::Control:
#ifdef DEBUG
        Serial.printf("Parsing '%s'\n",value.c_str());
#endif
      if(senville->fromJsonBuff((char *)value.c_str(), byteMsgBuf)) {
        irReceiver->send(byteMsgBuf,true);  // NOWait=true will cause 'echo' which is desired here, it gets written back to MQTT
        irReceiver->listen();
      } else {
#ifdef DEBUG
        Serial.printf("Failed to parse '%s'\n",value.c_str());
#endif
        return false;
      }
    break;
  }
  return true;
}

void setup() {    
  controlBuff = (char *)malloc(sizeof(char) * maxBuffLen);
  displayBuff = (char *)malloc(sizeof(char) * maxBuffLen);
  
#ifdef DEBUG  
  Serial.begin(115200);
  Serial.setDebugOutput(true);  
  Serial << endl << endl;
#endif

  // Homie setup
  Homie_setFirmware(FW_NAME, FW_VERSION);
  Homie.setSetupFunction(setupHandler).setLoopFunction(loopHandler);
#ifndef DEBUG  
  Homie.disableLogging(); 
#endif
  controlNode0.advertise("control").settable(propertyControlHandler);
  controlNode0.advertise("display");

  updateIntervalSetting.setDefaultValue(DEFAULT_UPDATE_INTERVAL).setValidator([] (long candidate) {
    return candidate > 0;
  });

  Homie.setup();
  
  // Hardware integration 
  disp = new SenvilleAURADisp();
  senville = new SenvilleAURA();
  irReceiver = new IRHVACLink(senville->getIRHVACConfig());
  irReceiver->listen();  
}

void loop() {
  Homie.loop();
}
