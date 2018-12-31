#include <FS.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson
#include <Hash.h>
#include <ESPAsyncTCP.h>       //https://github.com/me-no-dev/ESPAsyncTCP
#include <ESPAsyncWebServer.h> //https://github.com/me-no-dev/ESPAsyncWebServer
#include <SPIFFSEditor.h>
#include <ESPAsyncWiFiManager.h> //https://github.com/alanswx/ESPAsyncWiFiManager
#include <ESPAsyncDNSServer.h>   //https://github.com/devyte/ESPAsyncDNSServer
// #include <DNSServer.h>
#include <Ticker.h>
#include <AsyncMqttClient.h> //https://github.com/marvinroger/async-mqtt-client
#include <pgmspace.h>
#include "secrets.h"

#define SKETCH_VERSION "1.0.0"

#define MOTOR_STEPS 200 // Motor steps per revolution. Most steppers are 200 steps or 1.8 degrees/step
#define RPM 250
#define MICROSTEPS 16

#define ENABLE D1
#define DIR D2
#define STEP D5

#define HOSTNAME "ESP8266StepperA4988"
#define HTTP_PORT 80
#define DNS_PORT 53

#ifndef SECRET
char MQTT_HOST[64] = "192.168.0.xxx";
char MQTT_PORT[6] = "1883";
char MQTT_USER[32] = "";
char MQTT_PASS[32] = "";
#endif

char MQTT_PUB_TOPIC[] = "home/" HOSTNAME "/out";
char MQTT_SUB_TOPIC[] = "home/" HOSTNAME "/in";

#include "A4988.h"

A4988 stepper(MOTOR_STEPS, DIR, STEP, ENABLE);
int32_t rotateAngle = -25 * 360;
AsyncWebServer server(HTTP_PORT);
AsyncWebSocket ws("/ws");
AsyncDNSServer dns;
AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
Ticker wifiReconnectTimer;

#define favicon_ico_gz_len 603
const uint8_t favicon_ico_gz[] PROGMEM = {
    0x1f, 0x8b, 0x08, 0x08, 0x0c, 0x96, 0x29, 0x5c, 0x00, 0x03, 0x69, 0x63,
    0x6f, 0x6e, 0x5f, 0x6b, 0x43, 0x42, 0x5f, 0x69, 0x63, 0x6f, 0x6e, 0x2e,
    0x69, 0x63, 0x6f, 0x00, 0x9d, 0xd4, 0x4b, 0x88, 0x52, 0x61, 0x14, 0x07,
    0xf0, 0xef, 0xaa, 0xf9, 0xc2, 0xc7, 0x55, 0xab, 0x9d, 0x64, 0x2d, 0x34,
    0x83, 0x16, 0x03, 0x11, 0x86, 0x8b, 0x59, 0x28, 0x42, 0x81, 0x06, 0x8a,
    0x20, 0xa5, 0xa9, 0xe9, 0x42, 0xdd, 0x24, 0x63, 0xa0, 0xe1, 0xa2, 0x64,
    0x52, 0x7a, 0x10, 0x3d, 0xa8, 0x4d, 0xd0, 0xa6, 0x45, 0x8b, 0x88, 0xa0,
    0x82, 0x28, 0x66, 0x13, 0xf4, 0x82, 0xa8, 0x45, 0xd1, 0x83, 0x5a, 0xc8,
    0x2c, 0xa7, 0xa1, 0xa0, 0x36, 0x2d, 0x62, 0xe6, 0x7c, 0xfd, 0xef, 0xbd,
    0xc7, 0x72, 0x21, 0x05, 0x7d, 0x97, 0xdf, 0xbd, 0x97, 0xe3, 0x39, 0xf7,
    0x7b, 0xa2, 0x10, 0x0a, 0x2e, 0x55, 0x15, 0xb8, 0x87, 0x44, 0xc3, 0x22,
    0xc4, 0x66, 0x21, 0xc4, 0x76, 0x40, 0x08, 0x11, 0x23, 0xae, 0x37, 0xfc,
    0xf6, 0xd8, 0x63, 0x98, 0x6a, 0x1b, 0x61, 0x0f, 0xec, 0xb5, 0xdb, 0xed,
    0x4b, 0x1e, 0x8f, 0x67, 0xd9, 0xeb, 0xf5, 0x8e, 0xf1, 0x1c, 0x3b, 0x1c,
    0x8e, 0x31, 0x62, 0xfa, 0xbb, 0xdb, 0xed, 0x1e, 0x07, 0x02, 0x81, 0x65,
    0x8b, 0xc5, 0x72, 0x15, 0xb9, 0xf3, 0xb0, 0x0b, 0x9c, 0x70, 0x07, 0xbe,
    0xc2, 0xf7, 0x68, 0x34, 0xba, 0x3e, 0x1c, 0x0e, 0x65, 0xa7, 0xd3, 0x91,
    0xf9, 0x7c, 0x5e, 0xa6, 0x52, 0x29, 0x19, 0x8b, 0xc5, 0x64, 0xa1, 0x50,
    0x90, 0xbd, 0x5e, 0x4f, 0xe6, 0x72, 0x39, 0x69, 0xb5, 0x5a, 0x7f, 0x22,
    0xf7, 0x1b, 0xac, 0xc0, 0x49, 0x58, 0x03, 0xa9, 0x41, 0x3d, 0x0d, 0x06,
    0x03, 0xea, 0x76, 0xbb, 0x54, 0x2c, 0x16, 0xa9, 0x5a, 0xad, 0x52, 0xb9,
    0x5c, 0xa6, 0x52, 0xa9, 0x44, 0xed, 0x76, 0x9b, 0x32, 0x99, 0x0c, 0x99,
    0x4c, 0x26, 0x9a, 0xe4, 0xc3, 0x27, 0x58, 0x07, 0x2d, 0x46, 0x36, 0x9b,
    0x8d, 0x30, 0x56, 0x52, 0x55, 0xf5, 0x37, 0x9f, 0xcf, 0x47, 0x98, 0x0f,
    0x39, 0x9d, 0x4e, 0x72, 0xb9, 0x5c, 0x34, 0xc9, 0xe5, 0xfa, 0xf1, 0x74,
    0x7d, 0x28, 0x14, 0xa2, 0x56, 0xab, 0x45, 0x8d, 0x46, 0x83, 0x30, 0x66,
    0x4a, 0x26, 0x93, 0x94, 0x48, 0x24, 0xf4, 0xfe, 0xd3, 0xe9, 0x34, 0x85,
    0xc3, 0xe1, 0x49, 0xff, 0x33, 0xeb, 0xb5, 0xfe, 0xfa, 0xfd, 0x3e, 0x65,
    0xb3, 0x59, 0x6a, 0x36, 0x9b, 0x54, 0xa9, 0x54, 0xa8, 0x56, 0xab, 0xd1,
    0x68, 0x34, 0x92, 0xf5, 0x7a, 0x9d, 0xe2, 0xf1, 0x38, 0x99, 0xcd, 0xe6,
    0x59, 0xf5, 0xfa, 0x7c, 0x14, 0x45, 0x91, 0x18, 0xab, 0xc4, 0x9a, 0x4b,
    0xac, 0xb5, 0xf4, 0xfb, 0xfd, 0x32, 0x12, 0x89, 0xc8, 0x60, 0x30, 0x28,
    0x31, 0x2f, 0x3d, 0xa6, 0xe5, 0x4c, 0xcd, 0x5f, 0xab, 0xff, 0x01, 0x37,
    0x61, 0x00, 0x8b, 0x70, 0x82, 0xdf, 0xcf, 0xc0, 0x25, 0xb8, 0x08, 0xa7,
    0xa6, 0xe2, 0x8b, 0xfc, 0x7c, 0x02, 0xef, 0xe1, 0x23, 0x1c, 0xe1, 0xb3,
    0x60, 0xe2, 0x7d, 0x3d, 0x0d, 0x0f, 0xe0, 0x39, 0x3c, 0xe5, 0x3d, 0xee,
    0xc2, 0x36, 0xce, 0x53, 0xe0, 0x3a, 0xdc, 0x80, 0xdb, 0x70, 0x8d, 0xe3,
    0x87, 0x79, 0x4c, 0xf7, 0xe1, 0x18, 0x1c, 0x84, 0x32, 0x0c, 0xf9, 0x5b,
    0xaf, 0x21, 0x0e, 0x6e, 0x78, 0x06, 0x1d, 0xe8, 0xc3, 0x23, 0xd8, 0x02,
    0xaf, 0xe0, 0x38, 0x6c, 0x80, 0x00, 0x14, 0x21, 0x03, 0x76, 0x61, 0x1c,
    0xe7, 0x5b, 0xdc, 0xef, 0x0e, 0xf8, 0x00, 0x09, 0xd8, 0x07, 0xef, 0x60,
    0x0e, 0x96, 0xe0, 0x0a, 0x9f, 0xcb, 0x3c, 0x9f, 0xad, 0x15, 0xce, 0xdf,
    0x04, 0xf7, 0xe0, 0x32, 0x7f, 0xf3, 0x0d, 0xf7, 0xa9, 0xcd, 0xe9, 0x2d,
    0x3c, 0xe4, 0xf1, 0x69, 0x35, 0x77, 0xe1, 0xe8, 0x54, 0x7d, 0x1d, 0x5e,
    0xf2, 0x5e, 0x69, 0xeb, 0xf6, 0x82, 0x73, 0xb4, 0x71, 0x99, 0x61, 0x3f,
    0xaf, 0xb3, 0xe6, 0x02, 0xaf, 0xef, 0x02, 0xd7, 0x7f, 0x86, 0x43, 0x70,
    0x0e, 0xce, 0xf3, 0x9e, 0x9c, 0x85, 0xdd, 0xe2, 0xef, 0xed, 0x80, 0x30,
    0xce, 0xc9, 0x17, 0xd8, 0xf9, 0x8f, 0xdc, 0x59, 0x2d, 0x05, 0xab, 0x3c,
    0xcf, 0xad, 0xff, 0x51, 0xaf, 0xb7, 0x90, 0x5d, 0x4c, 0xfe, 0x3d, 0xfe,
    0x34, 0xec, 0xfe, 0xda, 0xbc, 0xe1, 0x17, 0x9d, 0x1a, 0xb4, 0xba, 0x7e,
    0x04, 0x00, 0x00};

File fsUploadFile;
bool shouldReboot = false;
const char update_html[] PROGMEM = R"=====(<!DOCTYPE html><html lang="en"><head><title>Firware Update</title><meta http-equiv="Content-Type" content="text/html; charset=utf-8"><meta name="viewport" content="width=device-width"><link rel="icon" href="favicon.ico" type="image/x-icon"><link rel="shortcut icon" href="favicon.ico" type="image/x-icon"></head><body><h3>Update Firmware</h3><br><form method="POST" action="/update" enctype="multipart/form-data"><input type="file" name="update"> <input type="submit" value="Update"></form></body></html>)=====";
const char root_html[] PROGMEM = R"=====(
<html><head><head><title>ESP8266 Stepper A4988</title><meta http-equiv="Content-Type" content="text/html; charset=utf-8"><meta name="viewport" content="width=device-width"><link rel="icon" href="favicon.ico" type="image/x-icon"><link rel="shortcut icon" href="favicon.ico" type="image/x-icon"><script>!function(e,n){"function"==typeof define&&define.amd?define([],n):"undefined"!=typeof module&&module.exports?module.exports=n():e.ReconnectingWebSocket=n()}(this,function(){function e(n,t,o){function c(e,n){var t=document.createEvent("CustomEvent");return t.initCustomEvent(e,!1,!1,n),t}var i={debug:!1,automaticOpen:!0,reconnectInterval:1e3,maxReconnectInterval:3e4,reconnectDecay:1.5,timeoutInterval:2e3};o||(o={});for(var r in i)this[r]="undefined"!=typeof o[r]?o[r]:i[r];this.url=n,this.reconnectAttempts=0,this.readyState=WebSocket.CONNECTING,this.protocol=null;var s,u=this,d=!1,a=!1,l=document.createElement("div");l.addEventListener("open",function(e){u.onopen(e)}),l.addEventListener("close",function(e){u.onclose(e)}),l.addEventListener("connecting",function(e){u.onconnecting(e)}),l.addEventListener("message",function(e){u.onmessage(e)}),l.addEventListener("error",function(e){u.onerror(e)}),this.addEventListener=l.addEventListener.bind(l),this.removeEventListener=l.removeEventListener.bind(l),this.dispatchEvent=l.dispatchEvent.bind(l),this.open=function(n){s=new WebSocket(u.url,t||[]),n||l.dispatchEvent(c("connecting")),(u.debug||e.debugAll)&&console.debug("ReconnectingWebSocket","attempt-connect",u.url);var o=s,i=setTimeout(function(){(u.debug||e.debugAll)&&console.debug("ReconnectingWebSocket","connection-timeout",u.url),a=!0,o.close(),a=!1},u.timeoutInterval);s.onopen=function(){clearTimeout(i),(u.debug||e.debugAll)&&console.debug("ReconnectingWebSocket","onopen",u.url),u.protocol=s.protocol,u.readyState=WebSocket.OPEN,u.reconnectAttempts=0;var t=c("open");t.isReconnect=n,n=!1,l.dispatchEvent(t)},s.onclose=function(t){if(clearTimeout(i),s=null,d)u.readyState=WebSocket.CLOSED,l.dispatchEvent(c("close"));else{u.readyState=WebSocket.CONNECTING;var o=c("connecting");o.code=t.code,o.reason=t.reason,o.wasClean=t.wasClean,l.dispatchEvent(o),n||a||((u.debug||e.debugAll)&&console.debug("ReconnectingWebSocket","onclose",u.url),l.dispatchEvent(c("close")));var i=u.reconnectInterval*Math.pow(u.reconnectDecay,u.reconnectAttempts);setTimeout(function(){u.reconnectAttempts++,u.open(!0)},i>u.maxReconnectInterval?u.maxReconnectInterval:i)}},s.onmessage=function(n){(u.debug||e.debugAll)&&console.debug("ReconnectingWebSocket","onmessage",u.url,n.data);var t=c("message");t.data=n.data,l.dispatchEvent(t)},s.onerror=function(n){(u.debug||e.debugAll)&&console.debug("ReconnectingWebSocket","onerror",u.url,n),l.dispatchEvent(c("error"))}},1==this.automaticOpen&&this.open(!1),this.send=function(n){if(s)return(u.debug||e.debugAll)&&console.debug("ReconnectingWebSocket","send",u.url,n),s.send(n);throw"INVALID_STATE_ERR : Pausing to reconnect websocket"},this.close=function(e,n){"undefined"==typeof e&&(e=1e3),d=!0,s&&s.close(e,n)},this.refresh=function(){s&&s.close()}}return e.prototype.onopen=function(){},e.prototype.onclose=function(){},e.prototype.onconnecting=function(){},e.prototype.onmessage=function(){},e.prototype.onerror=function(){},e.debugAll=!1,e.CONNECTING=WebSocket.CONNECTING,e.OPEN=WebSocket.OPEN,e.CLOSING=WebSocket.CLOSING,e.CLOSED=WebSocket.CLOSED,e})</script><script>function LoadBody(){wsc=new ReconnectingWebSocket("ws://"+window.location.hostname+"/ws"),wsc.timeoutInterval=3e3,wsc.onopen=function(n){console.log("WebSocketClient connected:",n)}}function ShadeUp(){var n={shade:"up"};wsc.send(JSON.stringify(n))}function ShadeDown(){var n={shade:"down"};wsc.send(JSON.stringify(n))}var wsc</script><style>#shadebt,body{text-align:center;margin:auto}#shadebt,a{color:#fff;text-decoration:none}#github a,#shadebt,a{text-decoration:none}body{width:100%;height:100%;background-color:#033}#wrapper{width:325px;height:150px;border:2px solid #407F7F;border-radius:8px;margin:25px auto auto;background-color:#699}#shadebt{background-color:#781424;border:none;padding:15px 32px;display:inline-block;font-size:18px;border-radius:8px}#github a{color:#ff69b4}</style></head><body onload="LoadBody()"><div id="wrapper"><h3>Window Shades</h3><input id="shadebt" type="button" value="Shades ▲" onclick="ShadeUp()">&nbsp;&nbsp;&nbsp;<input id="shadebt" type="button" value="Shades ▼" onclick="ShadeDown()"><br></div><br><a href="/update">Firmware Update</a><br><br><div id="github"><a href="https://github.com/debsahu/ESP8266-Stepper-A4988">ESP8266 Stepper A4988</a></div></body></head></html>
)=====";

bool processJson(String &message, bool mqttFlag = false)
{
    //const size_t bufferSize = JSON_OBJECT_SIZE(1) + 20;
    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.parseObject(message);

    if (!root.success())
    {
        Serial.println("parseObject() failed");
        return false;
    }

    if (root.containsKey("shade"))
    {
        const char *brighness_str = root["shade"];
        Serial.printf("Brighness JSON parsed: %s\n", brighness_str);
        if (strcmp(brighness_str, "up") == 0)
        {
            rotateAngle = -25 * 360;
            Serial.println("Moving Shades UP");
            if (mqttFlag)
                mqttClient.publish(MQTT_PUB_TOPIC, 0, false, "up");
            stepper.startRotate(rotateAngle); // or in degrees
        }
        else if (strcmp(brighness_str, "down") == 0)
        {
            rotateAngle = 25 * 360;
            Serial.println("Moving Shades DOWN");
            if (mqttFlag)
                mqttClient.publish(MQTT_PUB_TOPIC, 0, false, "down");
            stepper.startRotate(rotateAngle); // or in degrees
        }
    }
    jsonBuffer.clear();
    return true;
}

void moveShadesUp(void)
{
    rotateAngle = -25 * 360;
    Serial.println("Moving Shades UP");
    if (mqttClient.connected())
        mqttClient.publish(MQTT_PUB_TOPIC, 0, false, "up");
    stepper.startRotate(rotateAngle); // or in degrees
}

void moveShadesDown(void)
{
    rotateAngle = 25 * 360;
    Serial.println("Moving Shades DOWN");
    if (mqttClient.connected())
        mqttClient.publish(MQTT_PUB_TOPIC, 0, false, "down");
    stepper.startRotate(rotateAngle); // or in degrees
}

void connectToWifi()
{
    if (!WiFi.isConnected())
    {
        Serial.println("Connecting to Wi-Fi...");
        WiFi.begin();
    }
    else
    {
        Serial.println("WiFi is connected");
    }
}

void onWifiConnect(const WiFiEventStationModeGotIP &event)
{
    Serial.println("Connected to Wi-Fi.");
    connectToMqtt();
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected &event)
{
    Serial.println("Disconnected from Wi-Fi.");
    mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
    wifiReconnectTimer.once(2, connectToWifi);
}

void connectToMqtt()
{
    Serial.println("Connecting to MQTT...");
    mqttClient.connect();
}

void onMqttConnect(bool sessionPresent)
{
    Serial.println("Connected to MQTT.");
    Serial.print("Session present: ");
    Serial.println(sessionPresent);
    uint16_t packetIdSub = mqttClient.subscribe(MQTT_SUB_TOPIC, 2);
    Serial.printf("Subscribing [%s] at QoS 2, packetId: ", MQTT_SUB_TOPIC);
    Serial.println(packetIdSub);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
    Serial.print("Disconnected from MQTT, reason: ");
    if (reason == AsyncMqttClientDisconnectReason::TLS_BAD_FINGERPRINT)
    {
        Serial.println("Bad server fingerprint.");
    }
    else if (reason == AsyncMqttClientDisconnectReason::TCP_DISCONNECTED)
    {
        Serial.println("TCP Disconnected.");
    }
    else if (reason == AsyncMqttClientDisconnectReason::MQTT_UNACCEPTABLE_PROTOCOL_VERSION)
    {
        Serial.println("Bad server fingerprint.");
    }
    else if (reason == AsyncMqttClientDisconnectReason::MQTT_IDENTIFIER_REJECTED)
    {
        Serial.println("MQTT Identifier rejected.");
    }
    else if (reason == AsyncMqttClientDisconnectReason::MQTT_SERVER_UNAVAILABLE)
    {
        Serial.println("MQTT server unavailable.");
    }
    else if (reason == AsyncMqttClientDisconnectReason::MQTT_MALFORMED_CREDENTIALS)
    {
        Serial.println("MQTT malformed credentials.");
    }
    else if (reason == AsyncMqttClientDisconnectReason::MQTT_NOT_AUTHORIZED)
    {
        Serial.println("MQTT not authorized.");
    }
    else if (reason == AsyncMqttClientDisconnectReason::ESP8266_NOT_ENOUGH_SPACE)
    {
        Serial.println("Not enough space on esp8266.");
    }

    if (WiFi.isConnected())
    {
        mqttReconnectTimer.once(2, connectToMqtt);
    }
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos)
{
    Serial.print("Subscribe acknowledged: QoS: ");
    Serial.print(qos);
    Serial.print(", packetId: ");
    Serial.println(packetId);
}

void onMqttUnsubscribe(uint16_t packetId)
{
    Serial.println("Unsubscribe acknowledged.");
    Serial.print("  packetId: ");
    Serial.println(packetId);
}

void onMqttMessage(char *topic, char *payload_in, AsyncMqttClientMessageProperties properties, size_t length, size_t index, size_t total)
{
    Serial.print("MQTT: Recieved [");
    Serial.print(topic);
    char *payload = (char *)malloc(length + 1);
    memcpy(payload, payload_in, length);
    payload[length] = NULL;
    String payload_str = String(payload);
    free(payload);
    Serial.printf("]: %s\n", payload);

    Serial.println("Processing MQTT JSON Message: ");
    Serial.println(processJson(payload_str, true) ? "Success" : "Failed");
}

void onMqttPublish(uint16_t packetId)
{
    Serial.println("Publish acknowledged.");
    Serial.print("  packetId: ");
    Serial.println(packetId);
}

void setup()
{
    Serial.begin(115200);
    Serial.println();
    Serial.println();

    stepper.begin(RPM, MICROSTEPS);
    stepper.enable();

    if (SPIFFS.begin())
    {
        Dir dir = SPIFFS.openDir("/");
        while (dir.next())
        {
            String fileName = dir.fileName();
            size_t fileSize = dir.fileSize();
            Serial.printf("FS File: %s, size: %dB\n", fileName.c_str(), fileSize);
        }

        FSInfo fs_info;
        SPIFFS.info(fs_info);
        Serial.printf("FS Usage: %d/%d bytes\n\n", fs_info.usedBytes, fs_info.totalBytes);
    }

    char NameChipId[64] = {0}, chipId[7] = {0};
    snprintf(chipId, sizeof(chipId), "%06x", ESP.getChipId());
    snprintf(NameChipId, sizeof(NameChipId), "%s_%06x", HOSTNAME, ESP.getChipId());

    WiFi.mode(WIFI_STA);
    WiFi.hostname(const_cast<char *>(NameChipId));
    AsyncWiFiManager wifiManager(&server, &dns); //Local intialization. Once its business is done, there is no need to keep it around
    wifiManager.setConfigPortalTimeout(180);     //sets timeout until configuration portal gets turned off, useful to make it all retry or go to sleep in seconds

    AsyncWiFiManagerParameter custom_mqtt_host("host", "MQTT hostname", MQTT_HOST, 64);
    AsyncWiFiManagerParameter custom_mqtt_port("port", "MQTT port", MQTT_PORT, 6);
    AsyncWiFiManagerParameter custom_mqtt_user("user", "MQTT user", MQTT_USER, 32);
    AsyncWiFiManagerParameter custom_mqtt_pass("pass", "MQTT pass", MQTT_PASS, 32);

    wifiManager.addParameter(&custom_mqtt_host);
    wifiManager.addParameter(&custom_mqtt_port);
    wifiManager.addParameter(&custom_mqtt_user);
    wifiManager.addParameter(&custom_mqtt_pass);

    if (!wifiManager.autoConnect(NameChipId))
    {
        Serial.println("Failed to connect and hit timeout\nEntering Station Mode");
        WiFi.mode(WIFI_AP);
        WiFi.hostname(const_cast<char *>(NameChipId));
        IPAddress apIP(192, 168, 1, 1);
        WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
        WiFi.softAP(NameChipId);
        Serial.println("");
        Serial.print("HotSpt IP: ");
        Serial.println(WiFi.softAPIP());
        dns = AsyncDNSServer();
        dns.setTTL(300);
        dns.setErrorReplyCode(AsyncDNSReplyCode::ServerFailure);
        dns.start(DNS_PORT, "*", WiFi.softAPIP());
    }
    else
    {
        Serial.println("");
        Serial.print(F("Connected with IP: "));
        Serial.println(WiFi.localIP());

        strcpy(MQTT_HOST, custom_mqtt_host.getValue());
        strcpy(MQTT_PORT, custom_mqtt_port.getValue());
        strcpy(MQTT_USER, custom_mqtt_user.getValue());
        strcpy(MQTT_PASS, custom_mqtt_pass.getValue());

        wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
        wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);

        mqttClient.onConnect(onMqttConnect);
        mqttClient.onDisconnect(onMqttDisconnect);
        mqttClient.onSubscribe(onMqttSubscribe);
        mqttClient.onUnsubscribe(onMqttUnsubscribe);
        mqttClient.onMessage(onMqttMessage);
        mqttClient.onPublish(onMqttPublish);
        mqttClient.setServer(MQTT_HOST, atoi(MQTT_PORT));
        if (MQTT_USER != "" or MQTT_PASS != "")
            mqttClient.setCredentials(MQTT_USER, MQTT_PASS);
        mqttClient.setClientId(NameChipId);

        connectToMqtt();
    }

    ws.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
        if (type == WS_EVT_CONNECT)
        {
            Serial.printf("ws[%s][%u] connect\n", server->url(), client->id());
            client->ping();
        }
        else if (type == WS_EVT_DISCONNECT)
        {
            Serial.printf("ws[%s][%u] disconnect\n", server->url(), client->id());
        }
        else if (type == WS_EVT_ERROR)
        {
            Serial.printf("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t *)arg), (char *)data);
        }
        else if (type == WS_EVT_PONG)
        {
            Serial.printf("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len) ? (char *)data : "");
        }
        else if (type == WS_EVT_DATA)
        {
            AwsFrameInfo *info = (AwsFrameInfo *)arg;
            String msg = "";
            if (info->final && info->index == 0 && info->len == len)
            {
                if (info->opcode == WS_TEXT)
                {
                    for (size_t i = 0; i < info->len; i++)
                    {
                        msg += (char)data[i];
                    }
                    Serial.println(msg);
                    Serial.println(processJson(msg) ? "Success" : "Failed");
                }
            }
        }
    });
    server.addHandler(&ws);

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", root_html);
    });
    server.on("/up", HTTP_GET, [](AsyncWebServerRequest *request) {
        moveShadesUp();
        request->send(200, "text/plain", "up");
    });
    server.on("/down", HTTP_GET, [](AsyncWebServerRequest *request) {
        moveShadesDown();
        request->send(200, "text/plain", "down");
    });
    server.on("/version", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", SKETCH_VERSION);
    });
    server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse_P(200, "image/x-icon", favicon_ico_gz, favicon_ico_gz_len);
        response->addHeader("Content-Encoding", "gzip");
        request->send(response);
    });
    server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", update_html);
        request->send(response);
    });
    server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request) {
        shouldReboot = !Update.hasError();
        AsyncWebServerResponse *response = request->beginResponse(200, "text/html", shouldReboot ? "<META http-equiv='refresh' content='15;URL=/'>Update Success, rebooting..." : "FAIL");
        response->addHeader("Connection", "close");
        request->send(response); },
              [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
                  if (!filename.endsWith(".bin"))
                  {
                      return;
                  }
                  if (!index)
                  {
                      Serial.printf("Update Start: %s\n", filename.c_str());
                      Update.runAsync(true);
                      if (!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000))
                      {
                          Update.printError(Serial);
                      }
                  }
                  if (!Update.hasError())
                  {
                      if (Update.write(data, len) != len)
                      {
                          Update.printError(Serial);
                      }
                  }
                  if (final)
                  {
                      if (Update.end(true))
                          Serial.printf("Update Success: %uB\n", index + len);
                      else
                      {
                          Update.printError(Serial);
                      }
                  }
              });

    server.onNotFound([](AsyncWebServerRequest *request) {
        String filename = request->url();
        String ContentType = "text/plain";

        if (filename.endsWith(".htm"))
            ContentType = "text/html";
        else if (filename.endsWith(".html"))
            ContentType = "text/html";
        else if (filename.endsWith(".css"))
            ContentType = "text/css";
        else if (filename.endsWith(".js"))
            ContentType = "application/javascript";
        else if (filename.endsWith(".png"))
            ContentType = "image/png";
        else if (filename.endsWith(".gif"))
            ContentType = "image/gif";
        else if (filename.endsWith(".jpg"))
            ContentType = "image/jpeg";
        else if (filename.endsWith(".ico"))
            ContentType = "image/x-icon";
        else if (filename.endsWith(".xml"))
            ContentType = "text/xml";
        else if (filename.endsWith(".pdf"))
            ContentType = "application/x-pdf";
        else if (filename.endsWith(".zip"))
            ContentType = "application/x-zip";
        else if (filename.endsWith(".gz"))
            ContentType = "application/x-gzip";
        else if (filename.endsWith("ico.gz"))
            ContentType = "image/x-icon";

        if (SPIFFS.exists(filename + ".gz") || SPIFFS.exists(filename))
        {
            if (SPIFFS.exists(filename + ".gz"))
                filename += ".gz";
            AsyncWebServerResponse *response = request->beginResponse(SPIFFS, filename, ContentType);
            if (filename.endsWith(".gz"))
                response->addHeader("Content-Encoding", "gzip");
            request->send(response);
            return;
        }

        Serial.print("NOT_FOUND: ");
        if (request->method() == HTTP_GET)
            Serial.print("GET");
        else if (request->method() == HTTP_POST)
            Serial.print("POST");
        else if (request->method() == HTTP_DELETE)
            Serial.print("DELETE");
        else if (request->method() == HTTP_PUT)
            Serial.print("PUT");
        else if (request->method() == HTTP_PATCH)
            Serial.print("PATCH");
        else if (request->method() == HTTP_HEAD)
            Serial.print("HEAD");
        else if (request->method() == HTTP_OPTIONS)
            Serial.print("OPTIONS");
        else
            Serial.print("UNKNOWN");

        Serial.printf(" http://%s%s\n", request->host().c_str(), request->url().c_str());

        if (request->contentLength())
        {
            Serial.printf("_CONTENT_TYPE: %s\n", request->contentType().c_str());
            Serial.printf("_CONTENT_LENGTH: %u\n", request->contentLength());
        }

        int headers = request->headers();
        int i;
        for (i = 0; i < headers; i++)
        {
            AsyncWebHeader *h = request->getHeader(i);
            Serial.printf("_HEADER[%s]: %s\n", h->name().c_str(), h->value().c_str());
        }

        int params = request->params();
        for (i = 0; i < params; i++)
        {
            AsyncWebParameter *p = request->getParam(i);
            if (p->isFile())
            {
                Serial.printf("_FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
            }
            else if (p->isPost())
            {
                Serial.printf("_POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
            }
            else
            {
                Serial.printf("_GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
            }
        }

        request->send(404);
    });
    server.addHandler(new SPIFFSEditor("admin", "admin"));

    MDNS.setInstanceName(String(HOSTNAME " (" + String(chipId) + ")").c_str());
    if (MDNS.begin(HOSTNAME))
    {
        MDNS.addService("http", "tcp", HTTP_PORT);
        Serial.printf(">>> MDNS Started: http://%s.local/\n", NameChipId);
    }
    else
    {
        Serial.println(F(">>> Error setting up mDNS responder <<<"));
    }

    server.begin();
}

void loop()
{
    unsigned wait_time_micros = stepper.nextAction(); // motor control loop - send pulse and return how long to wait until next pulse

    if (wait_time_micros <= 0)
    {
        stepper.stop();
    }
    //else if (wait_time_micros > 100){}

    MDNS.update();

    if (shouldReboot)
        ESP.reset();
}