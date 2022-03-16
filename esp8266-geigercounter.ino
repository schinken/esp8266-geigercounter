#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <SoftwareSerial.h>

#ifdef OTA_PASSWORD
  #include <ArduinoOTA.h>
#endif

#include "settings.h"

WiFiClient wifiClient;
PubSubClient mqttClient;
SoftwareSerial geigerCounterSerial(PIN_UART_RX, PIN_UART_TX);

uint8_t idx = 0;
char buffer[64];
char hostname[24];

int lastCPM = 0, currentCPM = 0;
float lastuSv = 0, currentuSv = 0;
unsigned long lastPublishMs = 0;

void setup() {
  // power up wait
  delay(3000);
  
  Serial.begin(115200);
  geigerCounterSerial.begin(9600);
  delay(10);
  
  #ifdef WIFI_HOSTNAME
    strncpy(hostname, WIFI_HOSTNAME, sizeof(hostname));
  #else
    snprintf(hostname, sizeof(hostname), "ESP-GEIGER-%X", ESP.getChipId());
  #endif

  WiFi.hostname(hostname);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
 
  mqttClient.setClient(wifiClient);
  mqttClient.setServer(MQTT_HOST, 1883);
  connectMqtt();

  #ifdef OTA_PASSWORD
    ArduinoOTA.setHostname(hostname);
    ArduinoOTA.setPassword(OTA_PASSWORD);
    ArduinoOTA.begin();
  #endif

  #ifdef PIN_PULSE
    pinMode(PIN_PULSE, INPUT);
    attachInterrupt(PIN_PULSE, onPulse, RISING);
  #endif
}

#ifdef PIN_PULSE
ICACHE_RAM_ATTR void onPulse() {
  mqttClient.publish(MQTT_TOPIC_PULSE, "true");
}
#endif

void publishValues() {
  char tmp[8];

  if (currentCPM != lastCPM) {
    sprintf(tmp, "%d", currentCPM); 
    mqttClient.publish(MQTT_TOPIC_CPM, tmp, true);
  }

  if (currentuSv != lastuSv) {
    sprintf(tmp, "%.2f", currentuSv); 
    mqttClient.publish(MQTT_TOPIC_USV, tmp, true);
  }

  lastCPM = currentCPM;
  lastuSv = currentuSv;
}

void connectMqtt() {

  while (!mqttClient.connected()) {
    if (mqttClient.connect(hostname, MQTT_TOPIC_LAST_WILL, 1, true, "disconnected")) {
      mqttClient.publish(MQTT_TOPIC_LAST_WILL, "connected", true);
      Serial.println("MQTT connected");
    } else {
      Serial.println("MQTT connect failed");
      delay(1000);
    }
  }

}

void parseReceivedLine(char* input) {
  char segment = 0;
  char *token;

  float uSv = 0;
  float cpm = 0;

  token = strtok(input, ", ");

  while (token != NULL) {

    switch (segment) {

      // This is just for validation
      case IDX_CPS_KEY: if (strcmp(token, "CPS") != 0) return;  break;
      case IDX_CPM_KEY: if (strcmp(token, "CPM") != 0) return;  break;
      case IDX_uSv_KEY: if (strcmp(token, "uSv/hr") != 0) return;  break;

      case IDX_CPM:
        Serial.printf("Current CPM: %s\n", token);
        cpm = atoi(token);
        break;

      case IDX_uSv:
        Serial.printf("Current uSv/hr: %s\n", token);
        uSv = atof(token);
        break;
    }

    if (segment > 7) {
      // Invalid! There should be no more than 7 segments
      return;
    }

    token = strtok(NULL, ", ");
    segment++;
  }

  currentuSv = uSv;
  currentCPM = cpm;
}

void loop() {

  connectMqtt();
  mqttClient.loop();

  if (millis() - lastPublishMs > MQTT_PUBLISH_INTERVAL_MS) {
    lastPublishMs = millis();
    publishValues();
  }

  if (geigerCounterSerial.available()) {
    char input = geigerCounterSerial.read();
    buffer[idx++] = input;

    // Echo Serial-Data from GeigerCounter to USB-Debug
    Serial.write(input);
    
    if (input == '\n') {
      parseReceivedLine(buffer);
      idx = 0;
    }

    // Just in case the buffer gets to big, start from scratch
    if (idx > 42) {
      idx = 0;
    }

  }
  
  #ifdef OTA_PASSWORD
    ArduinoOTA.handle();
  #endif
}
