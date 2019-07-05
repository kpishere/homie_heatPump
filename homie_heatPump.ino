#include <Homie.h>
#include "src/SenvilleAURADisp.hpp"
#include "src/IRHVACLink.hpp"
#include "src/SenvilleAURA.hpp"
//#define DEBUG

#define FW_NAME "heatpump test"
#define FW_VERSION "0.0.2"

#define MAX_BUFFLEN 100
unsigned short maxBuffLen = MAX_BUFFLEN;

typedef enum UpdatePropertyE {
  None = 0x00, Display = 0x01, UpdateControl = 0x02, All = 0xFF
} UpdateProperty;

const int DEFAULT_UPDATE_INTERVAL = 180; // 3 min

IRHVACLink *irReceiver;
SenvilleAURA *senville;
SenvilleAURADisp *disp;
uint8_t byteMsgBuf[MSGSIZE_BYTES(MESSAGE_SAMPLES,MESSAGE_BITS)];
volatile char *controlBuff;
volatile char *displayBuff;
unsigned long lastUpdate;
volatile uint8_t updateFlags = UpdateProperty::None;

typedef enum PropertyE {
  Control
} Property;

HomieNode controlNode0("heatpump", "hvac");

HomieSetting<long> updateIntervalSetting("updateInterval", "The update interval in seconds");

void setupHandler() {
    String strVal;

    if(updateFlags & UpdateProperty::UpdateControl) {
      // Always get update to get sample time
      senville->toJsonBuff((char *)controlBuff);
  
      strVal = String((const char *)controlBuff);
      controlNode0.setProperty("control").send(strVal);      
    }
    if(updateFlags & UpdateProperty::Display) {
      // Always get update to get sample time
      disp->toBuff((char *)displayBuff);
      
      strVal = String((const char *)displayBuff);
      controlNode0.setProperty("display").send(strVal);
    }
    updateFlags = UpdateProperty::None;
}

void loopHandler() {
  unsigned long thisUpdate = millis();
  bool change = false;
  uint8_t *mem = NULL;

  if(lastUpdate == 0) {
    ESP.wdtDisable(); // now must be fed every 6 seconds, first loop can take time to connect etc.
  } else {
    ESP.wdtFeed();
  }
  
  // Check display hardware
  if(disp->hasUpdate()) {
#ifdef DEBUG
    disp->toBuff((char *)displayBuff);
    Serial.println((const char *)displayBuff);
#endif
    updateFlags |= UpdateProperty::Display;
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
#ifdef DEBUG
      senville->toJsonBuff((char *)controlBuff);
      Serial.println((char *)controlBuff);
#endif
      updateFlags |= UpdateProperty::UpdateControl;
    }
    irReceiver->listen();
  }
  
  // Update Homie properties
  if(thisUpdate - lastUpdate >= updateIntervalSetting.get() * 1000UL || lastUpdate == 0) {
    updateFlags = UpdateProperty::All;
  }
  if (updateFlags) {
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
#ifdef DEBUG
        Serial.print("Sending message : 0x");
        for(int i=0; i<MSGSIZE_BYTES(MESSAGE_SAMPLES,MESSAGE_BITS) ; i++)
          Serial.printf("%0X ",byteMsgBuf[i]);
        Serial.println();
#endif         
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
  ESP.wdtEnable( 30 * 1000 ); // Allow software watchdog
  lastUpdate = 0;
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
  updateFlags = UpdateProperty::All;
  setupHandler();
}

void loop() {
  Homie.loop();
}
