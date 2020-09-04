#include <SmingCore.h>
#include <esp_spi_flash.h>
#include <Debug.h>
#include <Network/RbootHttpUpdater.h>

#include "SenvilleAURADisp.hpp"
#include "IRLink.hpp"
#include "SenvilleAURA.hpp"
#define DEBUG

// Property is two paths separated by a space to the URL to download rom and spiff bin files from
#define MQTT_OTA_ROM_SPIFFS    "hvac/heatpump/ota/rom_spiff"

// If you want, you can define WiFi settings globally in Eclipse Environment Variables
#ifndef WIFI_SSID
#define WIFI_SSID "PleaseEnterSSID" // Put you SSID and Password here
#define WIFI_PWD "PleaseEnterPass"
#endif

// For testing purposes, try a few different URL formats
#define MQTT_URL1 "mqtt://192.168.1.104:1883"
#define MQTT_URL2 "mqtts://attachix.com:8883" // (Need ENABLE_SSL)
#define MQTT_URL3 "mqtt://frank:fiddle@192.168.100.107:1883"

#ifdef ENABLE_SSL
#include <ssl/private_key.h>
#include <ssl/cert.h>
#define MQTT_URL MQTT_URL2
#else
#define MQTT_URL MQTT_URL1
#endif

#define DEFAULT_CONFIG "{IsOn:0 , Instr:1 , Mode:0 , FanSpeed:0 , IsSleepOn:0 , SetTemp:22}"
#define CONFIG_FILENAME "control.config"
#define OTA_FILENAME "ota.txt"
#define MQTT_DEVICE_NAME "esp8266_01"
#define MQTT_CONTROL_PATH "hvac/heatpump/control"
#define MQTT_STATUS_PATH "hvac/heatpump/status"
#define MQTT_PROPERTIES_PATH "hvac/heatpump/properties"
#define MQTT_DISPLAY_PATH "hvac/heatpump/display"
#define MQTT_DEBUG_PATH "hvac/heatpump/debug"

typedef enum UpdatePropertyE {
  None = 0x00, Display = 0x01, UpdateControl = 0x02, All = 0xFF
} UpdateProperty;

const int DEFAULT_UPDATE_INTERVAL = 180; // 3 min
#define PROPERTY_SCAN_AT_TIME 60 /* seconds */
#define WIFI_RESTART_INTERVAL 30 /* seconds */
#define DISPLAY_IR_SCAN_INTERVAL 200 /* 1e-3 seconds */

#define MAX_BUFFLEN 300

IRLink *irReceiver;
SenvilleAURA *senville;
SenvilleAURADisp *disp;
uint8_t byteMsgBuf[MSGSIZE_BYTES(MESSAGE_SAMPLES,MESSAGE_BITS)];
char controlBuff[MAX_BUFFLEN];
char displayBuff[MAX_BUFFLEN];
unsigned long lastUpdate, lastPropertyUpdate;
volatile uint8_t updateFlags = UpdateProperty::None;
boolean ready = false;

CStringArray PropertyLabels;
uint8_t capturePropertyIndex;
uint8_t initiatePropertyCapture;
unsigned long timeOfLabelCapture;
#define PROPERTY_MODE_COMMANDS 6
#define OPTION_CMD "{Instr:2, Opt:%d}"
Properties properties[DISP_PROPERTIES];

// Forward declarations
void startMqttClient();
void onMessageReceived(String topic, String message);

MqttClient *mqtt = nullptr;

Timer procTimer;

/// BEGIN OTA
//
RbootHttpUpdater* otaUpdater = 0;

void OtaUpdate_CallBack(RbootHttpUpdater& client, bool result)
{
	Serial.println("In callback...");
	if(result == true) {
		// success
		uint8 slot;
		slot = rboot_get_current_rom();
		if(slot == 0)
			slot = 1;
		else
			slot = 0;
		// set to boot new rom and then reboot
		Serial.printf("Firmware updated, rebooting to rom %d...\r\n", slot);
		rboot_set_current_rom(slot);
		System.restart();
	} else {
		// fail
		Serial.println("Firmware update failed!");
	}
}

void ShowInfo()
{
	Serial.printf("\r\nSDK: v%s\r\n", system_get_sdk_version());
	Serial.printf("Free Heap: %d\r\n", system_get_free_heap_size());
	Serial.printf("CPU Frequency: %d MHz\r\n", system_get_cpu_freq());
	Serial.printf("System Chip ID: %x\r\n", system_get_chip_id());
	Serial.printf("SPI Flash ID: %x\r\n", spi_flash_get_id());
	//Serial.printf("SPI Flash Size: %d\r\n", (1 << ((spi_flash_get_id() >> 16) & 0xff)));

	rboot_config conf;
	conf = rboot_get_config();

	debugf("Count: %0X", conf.count);
	debugf("ROM 0: %0X", conf.roms[0]);
	debugf("ROM 1: %0X", conf.roms[1]);
	debugf("ROM 2: %0X", conf.roms[2]);
	debugf("GPIO ROM: %0X", conf.gpio_rom);
  debugf("current ROM: %0X", conf.current_rom);
}
//
/// END OTA

// Check for MQTT Disconnection
void checkMQTTDisconnect(TcpClient& client, bool flag)
{
	if(flag == true) {
		debugf("MQTT Broker Disconnected.");
	} else {
		debugf("MQTT Broker Unreachable.");
	}
  ready = false;
  disp->listenStop();
	procTimer.initializeMs(WIFI_RESTART_INTERVAL * 1e3, startMqttClient).start(); // 1e-3 seconds
}

void onMessageDelivered(uint16_t msgId, int type)
{
	Serial.printf(_F("Message with id %d and QoS %d was delivered successfully."), msgId,
				  (type == MQTT_MSG_PUBREC ? 2 : 1));
}

void irSendFromMsgBuffer(uint8_t *msgBuffer) {
  #ifdef DEBUG
  Serial.print(_F("Sending message : 0x"));
  for(int i=0; i<MSGSIZE_BYTES(MESSAGE_SAMPLES,MESSAGE_BITS) ; i++)
    Serial.printf("%0X ",msgBuffer[i]);
  Serial.println();
  #endif
  irReceiver->send(msgBuffer,true);  // NOWait=true will cause 'echo' which is desired here, it gets written back to MQTT
  irReceiver->listen();
  lastUpdate = 0; // will trigger a publish event
}

void saveConfig(uint8_t *msgBuffer) {
  String strVal;

  file_t fd = fileOpen(_F(CONFIG_FILENAME), eFO_CreateNewAlways |  eFO_ReadWrite );
  #ifdef DEBUG
  Serial.printf(_F("save fileOpen(\"%s\") = %d\r\n"), _F(CONFIG_FILENAME), fd);
  #endif
  if(fd > 0) {
    if(senville->isValid(msgBuffer)) {
      senville->toJsonBuff((char *)controlBuff);
      strVal = String((const char *)controlBuff);
      #ifdef DEBUG
          Serial.printf("write: %s\n",strVal.c_str());
      #endif
      if (fileWrite(fd, (const void *)strVal.c_str(), strVal.length()) < 0) {
        #ifdef DEBUG
        printf("\twrite errno %i\n", fileLastError(fd));
        #endif
      }
    }
    fileClose(fd);
  }
}

void saveOTA(String msg) {
  file_t fd = fileOpen(_F(OTA_FILENAME), eFO_CreateNewAlways |  eFO_ReadWrite );
  #ifdef DEBUG
  Serial.printf(_F("save fileOpen(\"%s\") = %d\r\n"), _F(OTA_FILENAME), fd);
  #endif
  if(fd > 0) {
    #ifdef DEBUG
      Serial.printf("write: %s\n",msg.c_str());
    #endif
    if (fileWrite(fd, (const void *)msg.c_str(), msg.length() + 1) < 0) {
      #ifdef DEBUG
      printf("\twrite errno %i\n", fileLastError(fd));
      #endif
    }
    fileClose(fd);
  }
}


void loadConfig() {
  int readBytes = 0;
  String strVal;

  file_t fd = fileOpen(_F(CONFIG_FILENAME), eFO_ReadOnly);
  #ifdef DEBUG
  Serial.printf(_F("load fileOpen(\"%s\") = %d\r\n"), _F(CONFIG_FILENAME), fd);
  #endif

	if(fd > 0) {
    readBytes = fileRead(fd, controlBuff, MAX_BUFFLEN);
		if(readBytes > 0) {
      strVal = String((const char *)controlBuff);

      #ifdef DEBUG
          Serial.printf("Loaded: %s\n",controlBuff);
      #endif

      senville->fromJsonBuff((char *)strVal.c_str(), byteMsgBuf);
      irSendFromMsgBuffer(byteMsgBuf);
		}
    fileClose(fd);
  } else {
    // Set & Save default state
    senville->fromJsonBuff(_F(DEFAULT_CONFIG), byteMsgBuf);
    irSendFromMsgBuffer(byteMsgBuf);
  }
}

void publish() {
    String strVal;

    if(updateFlags & UpdateProperty::UpdateControl) {
      // Dont want to put forward option commands, they'll show up in the control property
      if( senville->getInstructionType() != Instruction::InstrOption ) {
        // Always get update to get sample time
        senville->toJsonBuff((char *)controlBuff);

  			strVal = String((const char *)controlBuff);
  			mqtt->publish(_F(MQTT_STATUS_PATH), strVal);
      }
    }

    if(updateFlags & UpdateProperty::Display) {
      // Always get update to get sample time
      disp->toBuff((char *)displayBuff);

      strVal = String((const char *)displayBuff);
      mqtt->publish(_F(MQTT_DISPLAY_PATH), strVal);

      sprintf(displayBuff,"{capturePropertyIndex: %d, lastPropertyUpdate:%ld, waitTime: %ld}"
      , capturePropertyIndex, lastPropertyUpdate, (long)(PROPERTY_SCAN_AT_TIME * 1e3));
      strVal = String((const char *)displayBuff);
      mqtt->publish(_F(MQTT_DEBUG_PATH), strVal);
    }
    // Publish values at same time
    if( capturePropertyIndex == 0 ) { // Publish only when not scanning
      sprintf(displayBuff,"{");
      for(int i = 0; i<DISP_PROPERTIES; i++ ) {
        int pos = strlen(displayBuff);
        if( String((char *)properties[i].key).length()>0) {
          sprintf(&(displayBuff)[(pos)],"%s:%d%s",(char *)properties[i].key,properties[i].value,( (i < DISP_PROPERTIES-1)?", ":""));
        }
      }
      int pos = strlen(displayBuff); sprintf(&(displayBuff)[(pos)],"}");
      strVal = String((const char *)displayBuff);
      mqtt->publish(_F(MQTT_PROPERTIES_PATH), strVal);

      lastPropertyUpdate = millis();
    }
    updateFlags = UpdateProperty::None;
}

// Publish our message
void scan()
{
	unsigned long thisUpdate = millis();
  uint8_t *mem = NULL;

  // Check display hardware
  if(disp->hasUpdate()) {
#ifdef DEBUG
    Serial.print("scan ");
    disp->toBuff((char *)displayBuff);
    Serial.println((const char *)displayBuff);
#endif
    if(initiatePropertyCapture == 0
      && capturePropertyIndex > 0
      && capturePropertyIndex < DISP_PROPERTIES
    ) {
      disp->asciiDisplay((char *)displayBuff);
      if(strcmp(displayBuff, String(PropertyLabels[capturePropertyIndex-1]).c_str()) == 0) {
        // Validate expected label
        properties[capturePropertyIndex-1] = PropertiesS(displayBuff,0);
        timeOfLabelCapture = millis();
      } else {
        String strVal;

        strVal = String((const char *)displayBuff);
        mqtt->publish(_F(MQTT_DEBUG_PATH), strVal);

        // Wait some time before reading value
        if( (millis() - timeOfLabelCapture) > DISPLAY_IR_SCAN_INTERVAL * 3 ) {
          strVal = String((const char *)displayBuff);
          mqtt->publish(_F(MQTT_DEBUG_PATH), strVal);
          // If not expected label, it is value (if not spaces), set it and increment to next property
          if(strcmp(displayBuff, _F("  ")) != 0) {
            properties[capturePropertyIndex-1] = PropertiesS((char *)String(PropertyLabels[capturePropertyIndex-1]).c_str()
              , displayBuff );
            capturePropertyIndex++;
            // Send command to increment to next
            sprintf(controlBuff,OPTION_CMD,Option::Led);
            senville->fromJsonBuff(controlBuff, byteMsgBuf);
            irSendFromMsgBuffer(byteMsgBuf);
          }
        }
      }
    } else {
      updateFlags |= UpdateProperty::Display;
    }
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
  if((thisUpdate - lastUpdate) >= (DEFAULT_UPDATE_INTERVAL * 1e3) || lastUpdate == 0) {
    updateFlags = UpdateProperty::All;
  }
  // Initiate property capture cycle
  if( (capturePropertyIndex == 0 || capturePropertyIndex >= DISP_PROPERTIES)
    && ( (thisUpdate - lastPropertyUpdate) >= (PROPERTY_SCAN_AT_TIME * 1e3) || lastPropertyUpdate == 0 )) {
    capturePropertyIndex = 1;
    initiatePropertyCapture = PROPERTY_MODE_COMMANDS;
    for(int i=0; i < DISP_PROPERTIES; i++) properties[i].value = 0; // clear out values
  }
  // Send command and advance to completion of sequence in each scan cycle
  switch(initiatePropertyCapture) {
    case 6: case 5: case 4:      sprintf(controlBuff,OPTION_CMD,Option::Led);      break;
    case 3: case 2: case 1:      sprintf(controlBuff,OPTION_CMD,Option::Direct);   break;
    default: break; // do nothing
  }
  if(initiatePropertyCapture > 0) {
    senville->fromJsonBuff(controlBuff, byteMsgBuf);
    irSendFromMsgBuffer(byteMsgBuf);
    initiatePropertyCapture--;
  }
  // Re-connect if needed and publish to MQTT
	if(mqtt != nullptr && mqtt->getConnectionState() != eTCS_Connected) {
		startMqttClient(); // Auto reconnect
	}
  if (updateFlags) {
#ifdef DEBUG
		Serial.print(_F("Memory free="));
		Serial.println(system_get_free_heap_size());
#endif
		publish();
    lastUpdate = thisUpdate;
  }

  disp->listen();
}

// Callback for messages, arrived from MQTT server
void onMessageReceived(String topic, String message)
{
	#ifdef DEBUG
	Serial.print(topic);
	Serial.print(": ");
	Serial.println(message);
	#endif
	if(topic == _F(MQTT_CONTROL_PATH)) {
    uint8_t currentMessage[MSGSIZE_BYTES(MESSAGE_SAMPLES,MESSAGE_BITS)];
    memcpy(currentMessage, senville->getMessage(), MSGSIZE_BYTES(MESSAGE_SAMPLES,MESSAGE_BITS) * sizeof(uint8_t));
    senville->fromJsonBuff((char *)message.c_str(), byteMsgBuf);
    if( senville->getInstructionType() == Instruction::Command ) {
      if(memcmp(currentMessage,byteMsgBuf,MSGSIZE_BYTES(MESSAGE_SAMPLES,MESSAGE_BITS) * sizeof(uint8_t)) != 0) {
        saveConfig(byteMsgBuf);
      } // Different
    }
    // Always want to transmit these messages
    irSendFromMsgBuffer(byteMsgBuf);
  }
  if(topic == _F(MQTT_OTA_ROM_SPIFFS)) {
    irReceiver->listenStop();  // don't want these HW interrupts happening
    disp->listenStop();
    mqtt->unsubscribe(_F(MQTT_CONTROL_PATH));
    mqtt->unsubscribe(_F(MQTT_OTA_ROM_SPIFFS));
    delete mqtt;  mqtt = nullptr;
    saveOTA(message);
    spiffs_unmount();
    System.restart(1e3);
  }
}

// Run MQTT client
void startMqttClient()
{
	procTimer.stop();

  if(mqtt == nullptr) return;

	// 1. [Setup]
	if(!mqtt->setWill(_F(MQTT_DISPLAY_PATH), F("{}"))) {
		debugf("Unable to set the last will and testament. Most probably there is not enough memory on the device.");
	}

	mqtt->setConnectedHandler([](MqttClient& client, mqtt_message_t* message) {
#ifdef DEBUG
		Serial.print(_F("Connected to "));
		Serial.println(client.getRemoteIp());
#endif
    ready = true;
    capturePropertyIndex = 0;
    initiatePropertyCapture = 0;

    // Start publishing loop
    procTimer.initializeMs(DISPLAY_IR_SCAN_INTERVAL, scan).start();

		return 0;
	});

	mqtt->setCompleteDelegate(checkMQTTDisconnect);
	mqtt->setCallback(onMessageReceived);

#ifdef ENABLE_SSL
	mqtt->setSslInitHandler([](Ssl::Session& session) {
		session.options.verifyLater = true;
		session.keyCert.assign(default_private_key, sizeof(default_private_key), default_certificate,
							   sizeof(default_certificate), nullptr);
	});
#endif

	// 2. [Connect]
	Url url(MQTT_URL);
#ifdef DEBUG
	Serial.print(_F("Connecting to "));
	Serial.println(url);
#endif
	mqtt->connect(url, _F(MQTT_DEVICE_NAME));
	mqtt->subscribe(_F(MQTT_CONTROL_PATH));
  mqtt->subscribe(_F(MQTT_OTA_ROM_SPIFFS));
}

void onConnected(IpAddress ip, IpAddress netmask, IpAddress gateway)
{
	// Run MQTT client
  uint8 slot;
	rboot_config bootconf;
  int readBytes = 0;
  String romUrl;

  if(fileExist(OTA_FILENAME)) {}
  file_t fd = fileOpen(_F(OTA_FILENAME), eFO_ReadOnly);
  #ifdef DEBUG
  Serial.printf(_F("load fileOpen(\"%s\") = %d\r\n"), _F(OTA_FILENAME), fd);
  #endif

  if(fd > 0) {
    readBytes = fileRead(fd, controlBuff, MAX_BUFFLEN);
    if(readBytes > 0) {
      romUrl = String((const char *)controlBuff);
      #ifdef DEBUG
          Serial.printf("Loaded: %s\n",controlBuff);
      #endif
    }
    fileClose(fd);
    fileDelete(OTA_FILENAME);
  } else {
    // not doing OTA, Normal mode
    mqtt = new MqttClient();
  	startMqttClient();
    return;
  }

#ifdef DEBUG
	Serial.printf("Updating...");
#endif

	// need a clean object, otherwise if run before and failed will not run again
	if(otaUpdater) {
    #ifdef DEBUG
    	Serial.printf("delete existing...");
    #endif
    delete otaUpdater;
  }
	otaUpdater = new RbootHttpUpdater();

	// select rom slot to flash
	bootconf = rboot_get_config();
	slot = bootconf.current_rom;
  #ifdef DEBUG
    Serial.printf("slot current %d...",slot);
  #endif
	if(slot == 0)
		slot = 1;
	else
		slot = 0;

  #ifdef DEBUG
    Serial.printf("slot new %d...",slot);
  #endif

	otaUpdater->addItem(bootconf.roms[slot], romUrl);

  #ifdef DEBUG
    Serial.printf("loaded %s...",romUrl.c_str());
  #endif

	// request switch and reboot on success
	otaUpdater->switchToRom(slot);
	// and/or set a callback (called on failure or success without switching requested)
	otaUpdater->setCallback(OtaUpdate_CallBack);
	// start update
	otaUpdater->start();
}

void GDB_IRAM_ATTR init()
{
  // Valid property labels of HeatPump display
  PropertyLabels = SENVILLEAURA_PROPERTY_LABELS;
#ifdef DEBUG
	Serial.begin(SERIAL_BAUD_RATE); // 115200 by default
	Serial.systemDebugOutput(true); // Debug output to serial
  Debug.setDebug(Serial);
  ShowInfo();
#endif
  spiffs_mount();
  delay(3000);

	// Hardware integration
	disp = new SenvilleAURADisp();
	senville = new SenvilleAURA();
	irReceiver = new IRLink(senville->getIRConfig());
	updateFlags = UpdateProperty::All;
	lastUpdate = 0;
  lastPropertyUpdate = 0;

  loadConfig();

	WifiStation.config(WIFI_SSID, WIFI_PWD);
	WifiStation.enable(true);

  irReceiver->listen();

	// Run our method when station was connected to AP (or not connected)
	WifiEvents.onStationGotIP(onConnected);
}
