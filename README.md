# Mightyohm.com Geigercounter connected to ESP8266

Here's some code and schematic to connect your mightyohm.com geiger counter to the Internet of Things (IoT).

![Schematic](https://raw.githubusercontent.com/b4ckspace/esp8266-geigercounter/master/schematic.png "How to connect")

**If you use the external 3,3 volts, you have to remove the batteries from the geiger counter!**

## compile

The firmware can be built and flashed using the Arduino IDE.

For this, you will need to add ESP8266 support to it by [using the Boards Manager](https://github.com/esp8266/Arduino#installing-with-boards-manager).

Furthermore, you will also need to install the following libraries using the Library Manager:

* ArduinoJSON 6.10.1
* PubSubClient 2.8.0
* WiFiManager 0.15.0

## settings

Since this project is using the WifiManager library, the ESP8266 will open up a WiFi Access Point for its initial configuration
or if it is unable to connect to the previously configured WiFi.

The library pretends that said WiFi AP requires a captive portal which triggers a notification on recent android phones.
Simply connect to the AP with your phone, tap the "Login required"-notification and you should be able to configure everything.

## usage

Since we're using the Home Assistant Autodiscovery feature, everything should just work™.

## license and credits

This code is under MIT license (Christopher Schirner <schinken@bamberg.ccc.de>)
The geiger counter is used at our hackerspace in bamberg (https://hackerspace-bamberg.de)


