MQTT Client Homie-HeatPump
==========================

Copy `component.mk.orig` to `component.mk` and make necessary changes.

OTA over MQTT + HTTP
=====================

Code subscribes to the topic `hvac/heatpump/ota/rom_spiff` and expects that topic to contain URL to rom, for example:

```
http://192.168.1.104:8000/rom0.bin
```

To share the firmware files for HTTP hosting from command line, this works in MacOs :

- navigate in terminal window to the firmware folder (`./out/Esp8266/firmware`)
- execute the following in that folder
```
python -m SimpleHTTPServer 8000
```

Then, to initiate an OTA update: publish to the above topic with :

```
mosquitto_pub -t hvac/heatpump/ota/rom_spiff -m "http://192.168.1.104:8000/rom0.bin"
```
