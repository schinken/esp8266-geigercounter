# Mightyohm.com Geigercounter connected to ESP8266

❗ **This project assumes that you know how to solder, flash an arduino and use MQTT. This repository not a tutorial - there are plenty of them around the internet. This repository serves the purpose to provide the code to anyone who wants to uses it or extend it yourself to your needs** ❗

## getting started

To compile this code, just clone or download the master branch as a zip and ramp up your arduino IDE.

* Needed external dependencies are PubSubClient and optionally ArduinoOTA.
* Rename `settings.h.example` to `settings.h`
* Change the configuration in settings.h to your needs
* Select Wemos D1 Mini clone in Platform


## how to connect the wires
Here's some code and schematic to connect your mightyohm.com geiger counter to the Internet of Things (IoT).

![Schematic](https://raw.githubusercontent.com/schinken/esp8266-geigercounter/master/images/schematic.png "How to connect")

*You can optionally connect PULSE to D2 of the Wemos D1 and enable PIN_PULSE to receive a mqtt event each count!*

## images

<p align="middle">
<img src="https://raw.githubusercontent.com/schinken/esp8266-geigercounter/master/images/img-1.jpg" width="32%">
<img src="https://raw.githubusercontent.com/schinken/esp8266-geigercounter/master/images/img-2.jpg" width="32%">
<img src="https://raw.githubusercontent.com/schinken/esp8266-geigercounter/master/images/img-3.jpg" width="32%">
</p>

## license and credits

This code is under MIT license (Christopher Schirner <mail+github@schinken.io>)


