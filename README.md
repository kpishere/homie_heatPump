#  Homie Heat Pump

Yet another IR Library for HVAC related systems.  

NOTE: This is by no means a complete project.  At best, it is fun experimentation only.  For instance, if the ESP8266 crashes, what is the default heat pump setting?  There isn't one right now.  Use on your equipment is entirely your responsibility.  

The project is GPL V2.0 which means I share, you share.  Pull requests are welcome.

## Overview

This library sports fully independent classes for physical layer communication and logical layer protocol of Send/Receive signals to/from a heat pump for the purposes of replacing the hand-held remote.  There are Arduino *.ino test/example files of using the physical layer classes on their own.  There are other test examples of using the physical+logical layers.  There are also other examples of using Homie (link: https://github.com/marvinroger/homie-esp8266 ) library and framework for home automation, MQTT communication, OTA management, and WiFi configuration management.

## Features

This library is independent of Homie, the example merely uses it and you'll have to check out the Homie project if you'd like to set up your development environment to use that too.  This library is written for and tested on ESP8266 and Atmel Mega 2650 (and likely works for other Atmel chips .. haven't tested and you're welcome to test/patch etc.)  It is written using interrupt driven events for reading data and an interrupt timer for sending data and thus is written to play nice with ESP8266 WiFi interrupt responsiveness demands.

## Wiring Suggestions

Specific to the Senville AURA brand of Heat Pump (this may also work for some Mr.Cool, Comfort-Aire Century, Danfoss, Pioneer, and Lennox heat pump models too as they all use the same remote controller and chip-set, *Sino Wealth SH66P51A*) there is also a class to replace the entire display board (you could fashion your own y-cable to keep the display but I intend to replace it with this project).  Check out file SenvilleAURADisp.hpp for detail of the wiring connections.  Note: If you are just looking for IR functionality, I recommend using CN301 rather than the documented CN101 connector.  From the CN301 connector, you'll sense any other IR signal sent to the unit and you won't have to modulate the signal if you send on this connector -- this sending code is written for an un-modulated IR signal.  Otherwise, with the CN101 connector, the receive line basically illuminates the infra-red LED that is on the board, right beside the receiving unit -- it is a cheap one-way optio-coupler!  If you really want to use that connection then perhaps use another pin with a PWM of the carrier and an external switching transistor connected to the IR output pin controlled from this code.

Trust me, with this class of heat pump, there is no serial link here.  I've searched deeply for it! (There is a programming header on the interior board but I just didn't want to go there.  Other than that connector, the only serial link is between the interior and the exterior boards .. something else I didn't want to disturb.)  To fully replace the display board with, for example, a NodeMcu demo board, you'd wire it as such :

CN301 --> NodeMcu
--------------------------
*  GND  Black --> GND 
*  5V   White --> 5V In
*  IR   Red - IR pulse duration signal --> D1
*  DATA Orange - DATA for LEDs  --> D7 (DATA_MOSI)
*  LED1 Green - LED1 select w. 12 ms waveform --> D2
*  LED2 Dk Blue - LED2 select w. 12 ms waveform --> Not connected
*  LED  Purple - LED select w. 12 ms waveform -- > Not connected
*  CLK  Grey - CLK w. 8-bit bursts of 10 us tick each 4 ms --> D5 (CLK_HSPI)

NOTE and NB! :  ESP8266 is said to be 5V tolerant on sensing and will only drive pins to 3.3V but you may want to use some level converting circuitry here, just in case.  I tested without level conversion but intend to use level conversion in the final project.  The signals are all 'input' really.  The IR signal is normally high and pulled low by the ESP8266.  I'm merely adding for sake of possible surges or power on/off spikes.

## Configuration

For configuring WiFI, create your own config.json file in your working project folder and use the bash script config.sh to upload the settings.  It is more handy and straightforward than the clever (but still awkward) html config page presented in the homie project web pages.

Example: 

{"name": "heatpump","wifi": {"ssid": "VeggiePlatter","password": "chicken"},"mqtt": {"host": "192.168.1.199","port": 1883,"auth": false},"ota": {"enabled": true},"device_id": "heatpump"}

## Upcoming

You'll notice that the display output is simply the three bytes used to set the display properties.  Decoding these values is still under development.  You may also notice that only one of the LED signal pins is used.  There is limited IO on the ESP8266 and it simply couldn't be fit.  Currently, it is in test form only to confirm that each byte is sent in a regular order from the heat pump CPU thereby requiring only one of these pins as a synchronization pin to decode all three bytes.  In code, the order will be assumed where on the physical display board, it requires all three pins to activate different parts of the circuit.  This presumption remains to be confirmed.
