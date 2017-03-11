#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <SoftwareSerial.h>
#include <ArduinoOTA.h>
#include <SimpleTimer.h>

#include "settings.h"

WiFiClient wifiClient;
PubSubClient mqttClient;
SoftwareSerial geigerCounterSerial(PIN_UART_RX, PIN_UART_TX);
SimpleTimer timer;

String serialInput = "";
char serialInputHelper[RECV_LINE_SIZE];

int lastCPM = 0, currentCPM = 0;
float lastuSv = 0, currentuSv = 0;

void setup() {

  Serial.begin(115200);
  geigerCounterSerial.begin(BAUD_GEIGERCOUNTER);
  delay(10);

  WiFi.hostname(WIFI_HOSTNAME);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  mqttClient.setClient(wifiClient);
  mqttClient.setServer(MQTT_HOST, 1883);
  
  ArduinoOTA.setHostname(WIFI_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.begin();

  timer.setInterval(UPDATE_INTERVAL_SECONDS * 1000L, updateRadiationValues);
}

void updateRadiationValues() {


  char tmp[8];

  if (currentCPM != lastCPM) {
    String(currentCPM).toCharArray(tmp, 8);
    Serial.print("Sending CPM");
    Serial.println(tmp);
    mqttClient.publish(MQTT_TOPIC_CPM, tmp, true);
  }

  if (currentuSv != lastuSv) {
    String(currentuSv).toCharArray(tmp, 8);
    Serial.print("Sending uSv");
    Serial.println(tmp);
    mqttClient.publish(MQTT_TOPIC_USV, tmp, true);
  }

  lastCPM = currentCPM;
  lastuSv = currentuSv;
}

void connectMqtt() {

  while (!mqttClient.connected()) {
    if (mqttClient.connect(WIFI_HOSTNAME, MQTT_TOPIC_LAST_WILL, 1, true, "disconnected")) {
      mqttClient.publish(MQTT_TOPIC_LAST_WILL, "connected", true);
    } else {
      delay(1000);
    }
  }

}

void parseReceivedLine(char* input) {

  char segment = 0;
  char *token;

  float uSv = 0;
  float cpm = 0;

  token = strtok(input, delimiter);

  while (token != NULL) {

    switch (segment) {

      // This is just for validation
      case IDX_CPS_KEY: if (strcmp(token, "CPS") != 0) return;  break;
      case IDX_CPM_KEY: if (strcmp(token, "CPM") != 0) return;  break;
      case IDX_uSv_KEY: if (strcmp(token, "uSv/hr") != 0) return;  break;

      case IDX_CPM:
        Serial.printf("Current CPM: %s\n", token);
        cpm = String(token).toInt();
        break;

      case IDX_uSv:
        Serial.printf("Current uSv/hr: %s\n", token);
        uSv = String(token).toFloat();
        break;
    }

    if (segment > 7) {
      // Invalid! There should be no more than 7 segments
      return;
    }

    token = strtok(NULL, delimiter);
    segment++;
  }

  currentuSv = uSv;
  currentCPM = cpm;
}

void loop() {

  connectMqtt();
  timer.run();
  mqttClient.loop();

  if (geigerCounterSerial.available()) {
    char in = (char) geigerCounterSerial.read();

    serialInput += in;

    if (in == '\n') {
      serialInput.toCharArray(serialInputHelper, RECV_LINE_SIZE);
      parseReceivedLine(serialInputHelper);

      serialInput = "";
    }

    // Just in case the buffer gets to big, start from scratch
    if (serialInput.length() > RECV_LINE_SIZE + 10) {
      serialInput = "";
    }

    Serial.write(in);
  }

  ArduinoOTA.handle();
}
