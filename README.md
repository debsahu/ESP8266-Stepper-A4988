# ESP8266-Stepper-A4988

[![Build Status](https://travis-ci.com/debsahu/ESP8266-Stepper-A4988.svg?branch=master)](https://travis-ci.com/debsahu/ESP8266-Stepper-A4988) [![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](https://opensource.org/licenses/MIT) [![version](https://img.shields.io/badge/version-v1.0.0-blue.svg)](https://github.com/debsahu/ESP8266-Stepper-A4988/)

A web wrapper around Stepper motor driver A4988 to drive a stepper motor (17HS3401) up and down.

## Libraries Needed

[platformio.ini](https://github.com/debsahu/ESP8266-Stepper-A4988/blob/master/platformio.ini) is included, use [PlatformIO](https://platformio.org/platformio-ide) and it will take care of installing the following libraries.

| Library                   | Link                                                       |
|---------------------------|------------------------------------------------------------|
|ESPAsyncE131               |https://github.com/forkineye/ESPAsyncE131                   |
|ESPAsyncUDP                |https://github.com/me-no-dev/ESPAsyncUDP                    |
|ESPAsyncTCP                |https://github.com/me-no-dev/ESPAsyncTCP                    |
|ESPAsyncWiFiManager        |https://github.com/alanswx/ESPAsyncWiFiManager              |
|ESPAsyncDNSServer          |https://github.com/devyte/ESPAsyncDNSServer                 |
|ESP Async WebServer        |https://github.com/me-no-dev/ESPAsyncWebServer              |
|ArduinoJson                |https://github.com/bblanchon/ArduinoJson                    |
|StepperDriver              |https://github.com/laurb9/StepperDriver                     |
|AsyncMqttClient            |https://github.com/marvinroger/async-mqtt-client            |