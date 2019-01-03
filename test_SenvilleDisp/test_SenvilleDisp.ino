#include "src/SenvilleAURADisp.hpp"



SenvilleAURADisp *disp;
char lineBuf[81];

void setup() {    
  Serial.begin(115200);
  Serial.setDebugOutput(true);    
  disp = new SenvilleAURADisp();
}

void loop() {
  if(disp->hasUpdate()) {
    disp->toBuff(lineBuf);
    Serial.println(lineBuf);
    disp->listen();
  }
}
