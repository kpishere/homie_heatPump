#include <SmingCore.h>
#include "SenvilleAURADisp.hpp"
#include "IRLink.hpp"
#include "SenvilleAURA.hpp"
//#define DEBUG

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

#define MQTT_DEVICE_NAME "esp8266_01"
#define MQTT_CONTROL_PATH "hvac/heatpump/control"
#define MQTT_DISPLAY_PATH "hvac/heatpump/display"

typedef enum UpdatePropertyE {
  None = 0x00, Display = 0x01, UpdateControl = 0x02, All = 0xFF
} UpdateProperty;

const int DEFAULT_UPDATE_INTERVAL = 180; // 3 min
#define WIFI_RESTART_INTERVAL 30 /* seconds */

IRLink *irReceiver;
SenvilleAURA *senville;
SenvilleAURADisp *disp;
uint8_t byteMsgBuf[MSGSIZE_BYTES(MESSAGE_SAMPLES,MESSAGE_BITS)];
volatile char *controlBuff;
volatile char *displayBuff;
unsigned long lastUpdate;
volatile uint8_t updateFlags = UpdateProperty::None;

#define MAX_BUFFLEN 100
unsigned short maxBuffLen = MAX_BUFFLEN;

// Forward declarations
void startMqttClient();
void onMessageReceived(String topic, String message);

MqttClient mqtt;

Timer procTimer;

// Check for MQTT Disconnection
void checkMQTTDisconnect(TcpClient& client, bool flag)
{
#ifdef DEBUG
	if(flag == true) {
		Serial.println(_F("MQTT Broker Disconnected."));
	} else {
		Serial.println(_F("MQTT Broker Unreachable."));
	}
#endif
	// Restart connection attempt after few seconds
	procTimer.initializeMs(WIFI_RESTART_INTERVAL * 1000, startMqttClient).start(); // 1e-3 seconds
}

void onMessageDelivered(uint16_t msgId, int type)
{
	Serial.printf(_F("Message with id %d and QoS %d was delivered successfully."), msgId,
				  (type == MQTT_MSG_PUBREC ? 2 : 1));
}

void publish() {
    String strVal;

    if(updateFlags & UpdateProperty::UpdateControl) {
      // Always get update to get sample time
      senville->toJsonBuff((char *)controlBuff);

			strVal = String((const char *)controlBuff);
			mqtt.publish(F(MQTT_CONTROL_PATH), strVal);
    }

    if(updateFlags & UpdateProperty::Display) {
      // Always get update to get sample time
      disp->toBuff((char *)displayBuff);

			strVal = String((const char *)displayBuff);
			mqtt.publish(F(MQTT_DISPLAY_PATH), strVal);
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
  if(thisUpdate - lastUpdate >= DEFAULT_UPDATE_INTERVAL * 1000UL || lastUpdate == 0) {
    updateFlags = UpdateProperty::All;
  }
	if(mqtt.getConnectionState() != eTCS_Connected) {
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
}

// Callback for messages, arrived from MQTT server
void onMessageReceived(String topic, String message)
{
	#ifdef DEBUG
	Serial.print(topic);
	Serial.print(": ");
	Serial.println(message);
	#endif
	if(topic == MQTT_CONTROL_PATH) {
  	if(senville->fromJsonBuff((char *)message.c_str(), byteMsgBuf)) {
	#ifdef DEBUG
	    Serial.print("Sending message : 0x");
	    for(int i=0; i<MSGSIZE_BYTES(MESSAGE_SAMPLES,MESSAGE_BITS) ; i++)
	      Serial.printf("%0X ",byteMsgBuf[i]);
	    Serial.println();
	#endif
      irReceiver->send(byteMsgBuf,true);  // NOWait=true will cause 'echo' which is desired here, it gets written back to MQTT
      irReceiver->listen();
			lastUpdate = 0; // will trigger a publish event
    } else {
#ifdef DEBUG
    	Serial.printf("\tFailed to parse.\n");
#endif
    }
	}
}

// Run MQTT client
void startMqttClient()
{
	procTimer.stop();

	// 1. [Setup]
	if(!mqtt.setWill(F(MQTT_DISPLAY_PATH), F("{}"))) {
		debugf("Unable to set the last will and testament. Most probably there is not enough memory on the device.");
	}

	mqtt.setConnectedHandler([](MqttClient& client, mqtt_message_t* message) {
#ifdef DEBUG
		Serial.print(_F("Connected to "));
		Serial.println(client.getRemoteIp());
#endif
		scan();
		// Start publishing loop
		procTimer.initializeMs(1000, scan).start(); // 1e-3 seconds
		return 0;
	});

	mqtt.setCompleteDelegate(checkMQTTDisconnect);
	mqtt.setCallback(onMessageReceived);

#ifdef ENABLE_SSL
	mqtt.setSslInitHandler([](Ssl::Session& session) {
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
	mqtt.connect(url, F(MQTT_DEVICE_NAME));
	mqtt.subscribe(F(MQTT_CONTROL_PATH));
}

void onConnected(IpAddress ip, IpAddress netmask, IpAddress gateway)
{
	// Run MQTT client
	startMqttClient();
}

void init()
{
#ifdef DEBUG
	Serial.begin(SERIAL_BAUD_RATE); // 115200 by default
	Serial.systemDebugOutput(true); // Debug output to serial
#endif

	// Hardware integration
	disp = new SenvilleAURADisp();
	senville = new SenvilleAURA();
	irReceiver = new IRLink(senville->getIRConfig());
	irReceiver->listen();
	updateFlags = UpdateProperty::All;
	lastUpdate = 0;
	controlBuff = (char *)malloc(sizeof(char) * maxBuffLen);
	displayBuff = (char *)malloc(sizeof(char) * maxBuffLen);

	WifiStation.config(WIFI_SSID, WIFI_PWD);
	WifiStation.enable(true);

	// Run our method when station was connected to AP (or not connected)
	WifiEvents.onStationGotIP(onConnected);
}
