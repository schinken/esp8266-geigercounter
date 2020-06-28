#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <SoftwareSerial.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>

#include "dep/pubsubclient-2.7/src/PubSubClient.cpp"
#include "dep/WiFiManager-0.15.0/WiFiManager.cpp"
#include "dep/SimpleTimer-schinken/SimpleTimer.cpp"


#include "settings.h"

#ifdef USE_HA_AUTODISCOVERY
  #define FIRMWARE_PREFIX "esp8266-geigercounter"
  char MQTT_TOPIC_LAST_WILL[128];
  char MQTT_TOPIC_CPM_MEASUREMENT[128];
  char MQTT_TOPIC_USV_MEASUREMENT[128];
#endif

WiFiClient wifiClient;
PubSubClient mqttClient;
SoftwareSerial geigerCounterSerial(PIN_UART_RX, PIN_UART_TX);
SimpleTimer timer;

uint8_t mqttRetryCounter = 0;
String serialInput = "";
char serialInputHelper[RECV_LINE_SIZE];

int lastCPM = 0, currentCPM = 0;
float lastuSv = 0, currentuSv = 0;

char hostname[16];

void setup() {
  Serial.begin(115200);
  Serial.println("\n");
  Serial.println("Hello from esp8266-geigercounter");

  geigerCounterSerial.begin(BAUD_GEIGERCOUNTER);

  // Power up wait
  delay(2000);

  WiFiManager wifiManager;
  int32_t chipid = ESP.getChipId();

  Serial.print("MQTT_MAX_PACKET_SIZE: ");
  Serial.println(MQTT_MAX_PACKET_SIZE);


#ifdef HOSTNAME
  hostname = HOSTNAME;
#else
  snprintf(hostname, 24, "GEIGERCTR-%X", chipid);
#endif

#ifdef USE_HA_AUTODISCOVERY
  snprintf(MQTT_TOPIC_LAST_WILL, 127, "%s/%s/presence", FIRMWARE_PREFIX, hostname);
  snprintf(MQTT_TOPIC_CPM_MEASUREMENT, 127, "%s/%s/%s_%s/state", FIRMWARE_PREFIX, hostname, hostname, "cpm");
  snprintf(MQTT_TOPIC_USV_MEASUREMENT, 127, "%s/%s/%s_%s/state", FIRMWARE_PREFIX, hostname, hostname, "uSv");
#endif

#ifdef CONF_WIFI_PASSWORD
  wifiManager.autoConnect(hostname, CONF_WIFI_PASSWORD);
#else
  wifiManager.autoConnect(hostname);
#endif

  WiFi.hostname(hostname);
  mqttClient.setClient(wifiClient);
  mqttClient.setServer(MQTT_HOST, 1883);

  ArduinoOTA.setHostname(hostname);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.begin();

  Serial.print("Hostname: ");
  Serial.println(hostname);

  Serial.println("-- Current GPIO Configuration --");
  Serial.print("PIN_UART_RX: ");
  Serial.println(PIN_UART_RX);

  mqttConnect();
  timer.setInterval(UPDATE_INTERVAL_SECONDS * 1000L, updateRadiationValues);
}

void mqttConnect() {
  while (!mqttClient.connected()) {

    bool mqttConnected = false;
    if (MQTT_USERNAME && MQTT_PASSWORD) {
      mqttConnected = mqttClient.connect(hostname, MQTT_USERNAME, MQTT_PASSWORD, MQTT_TOPIC_LAST_WILL, 1, true, MQTT_LAST_WILL_PAYLOAD_DISCONNECTED);
    } else {
      mqttConnected = mqttClient.connect(hostname, MQTT_TOPIC_LAST_WILL, 1, true, MQTT_LAST_WILL_PAYLOAD_DISCONNECTED);
    }

    if (mqttConnected) {
      Serial.println("Connected to MQTT Broker");
      mqttClient.publish(MQTT_TOPIC_LAST_WILL, MQTT_LAST_WILL_PAYLOAD_CONNECTED, true);
      mqttRetryCounter = 0;

      #ifdef USE_HA_AUTODISCOVERY
        setupHAAutodiscovery();
      #endif

    } else {
      Serial.println("Failed to connect to MQTT Broker");

      if (mqttRetryCounter++ > MQTT_MAX_CONNECT_RETRY) {
        Serial.println("Restarting uC");
        ESP.restart();
      }

      delay(2000);
    }
  }
}


#ifdef USE_HA_AUTODISCOVERY
#define AUTOCONFIG_PAYLOAD_TPL_USV  "{\
\"stat_t\":\"%s/%s/%s_uSv/state\",\
\"avty_t\":\"%s/%s/presence\",\
\"unique_id\":\"%s_uSv\",\
\"name\":\"%s uSv\",\
\"unit_of_meas\":\"µSv/h\",\
\"dev\": {\
\"identifiers\":\"%s\",\
\"name\":\"%s\",\
\"manufacturer\":\"MightyOhm LLC\",\
\"model\":\"Geiger Counter\"\
}\
}"
#define AUTOCONFIG_PAYLOAD_TPL_CPM  "{\
\"stat_t\":\"%s/%s/%s_cpm/state\",\
\"avty_t\":\"%s/%s/presence\",\
\"unique_id\":\"%s_cpm\",\
\"name\":\"%s CPM\",\
\"unit_of_meas\":\"CPM\",\
\"dev\": {\
\"identifiers\":\"%s\",\
\"name\":\"%s\",\
\"manufacturer\":\"MightyOhm LLC\",\
\"model\":\"Geiger Counter\"\
}\
}"

void setupHAAutodiscovery() {
  char autoconfig_topic_cpm[128];
  char autoconfig_payload_cpm[1024];
  char autoconfig_topic_usv[128];
  char autoconfig_payload_usv[1024];

  snprintf(
    autoconfig_topic_cpm,
    127,
    "%s/sensor/%s/%s_cpm/config",
    HA_DISCOVERY_PREFIX,
    hostname,
    hostname
  );

  snprintf(
    autoconfig_topic_usv,
    127,
    "%s/sensor/%s/%s_usv/config",
    HA_DISCOVERY_PREFIX,
    hostname,
    hostname
  );

  snprintf(
    autoconfig_payload_cpm,
    1023,

    AUTOCONFIG_PAYLOAD_TPL_CPM,

    FIRMWARE_PREFIX,
    hostname,
    hostname,
    FIRMWARE_PREFIX,
    hostname,
    hostname,
    hostname,
    hostname,
    hostname
  );

  snprintf(
    autoconfig_payload_usv,
    1023,

    AUTOCONFIG_PAYLOAD_TPL_USV,

    FIRMWARE_PREFIX,
    hostname,
    hostname,
    FIRMWARE_PREFIX,
    hostname,
    hostname,
    hostname,
    hostname,
    hostname
  );

  if(
    mqttClient.publish(autoconfig_topic_cpm, autoconfig_payload_cpm, true) &&
    mqttClient.publish(autoconfig_topic_usv, autoconfig_payload_usv, true)
  ){
    Serial.println("Autoconf publish successful");
  } else {
    Serial.println("Autoconf publish failed. Is MQTT_MAX_PACKET_SIZE large enough?");
  }
}
#endif

void loop() {
  timer.run();
  mqttConnect();
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

void updateRadiationValues() {
  char tmp[8];

  if (currentCPM != lastCPM) {
    String(currentCPM).toCharArray(tmp, 8);
    Serial.print("Sending CPM: ");
    Serial.println(tmp);
    mqttClient.publish(MQTT_TOPIC_CPM_MEASUREMENT, tmp, true);
  }

  if (currentuSv != lastuSv) {
    String(currentuSv).toCharArray(tmp, 8);
    Serial.print("Sending uSv: ");
    Serial.println(tmp);
    mqttClient.publish(MQTT_TOPIC_USV_MEASUREMENT, tmp, true);
  }

  lastCPM = currentCPM;
  lastuSv = currentuSv;
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