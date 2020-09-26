#include "wifi.h"

#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <SoftwareSerial.h>

#include "settings.h"

uint8_t mqttRetryCounter = 0;


WiFiManager wifiManager;
WiFiClient wifiClient;
PubSubClient mqttClient;

WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, sizeof(mqtt_server));
WiFiManagerParameter custom_mqtt_user("user", "MQTT username", username, sizeof(username));
WiFiManagerParameter custom_mqtt_pass("pass", "MQTT password", password, sizeof(password));

unsigned long lastMqttConnectionAttempt = millis();
const long mqttConnectionInterval = 60000;

unsigned long statusPublishPreviousMillis = millis();
const long statusPublishInterval = 30000;

char identifier[24];
#define FIRMWARE_PREFIX "esp8266-geigercounter"
#define AVAILABILITY_ONLINE "online"
#define AVAILABILITY_OFFLINE "offline"
char MQTT_TOPIC_AVAILABILITY[128];
char MQTT_TOPIC_CPM[128];
char MQTT_TOPIC_USV[128];

char MQTT_TOPIC_AUTOCONF_CPM[128];
char MQTT_TOPIC_AUTOCONF_USV[128];

SoftwareSerial geigerCounterSerial(PIN_UART_RX, PIN_UART_TX);


bool shouldSaveConfig = false;

void saveConfigCallback () {
  shouldSaveConfig = true;
}

void setup() {
  delay(3000);
  Serial.begin(9600);
  delay(2000);
  Serial.println("\n");
  Serial.println("Hello from esp8266-geigercounter");
  Serial.printf("Core Version: %s\n", ESP.getCoreVersion().c_str());
  Serial.printf("Boot Version: %u\n", ESP.getBootVersion());
  Serial.printf("Boot Mode: %u\n", ESP.getBootMode());
  Serial.printf("CPU Frequency: %u MHz\n", ESP.getCpuFreqMHz());
  Serial.printf("Reset reason: %s\n", ESP.getResetReason().c_str());

  geigerCounterSerial.begin(9600);

  snprintf(identifier, sizeof(identifier), "GEIGERCTR-%X", ESP.getChipId());
  snprintf(MQTT_TOPIC_AVAILABILITY, 127, "%s/%s/status", FIRMWARE_PREFIX, identifier);
  snprintf(MQTT_TOPIC_CPM, 127, "%s/%s_cpm/state", FIRMWARE_PREFIX, identifier);
  snprintf(MQTT_TOPIC_USV, 127, "%s/%s_usv/state", FIRMWARE_PREFIX, identifier);

  snprintf(MQTT_TOPIC_AUTOCONF_CPM, 127, "homeassistant/sensor/%s/%s_cpm/config", FIRMWARE_PREFIX, identifier);
  snprintf(MQTT_TOPIC_AUTOCONF_USV, 127, "homeassistant/sensor/%s/%s_usv/config", FIRMWARE_PREFIX, identifier);



  WiFi.hostname(identifier);

  loadConfig();
  setupWifi();
  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setKeepAlive(10);
  mqttClient.setBufferSize(2048);

  Serial.print("Hostname: ");
  Serial.println(identifier);
  Serial.print("\nIP: ");
  Serial.println(WiFi.localIP());

  Serial.println("-- Current GPIO Configuration --");
  Serial.print("PIN_UART_RX: ");
  Serial.println(PIN_UART_RX);

  //Disable blue LED
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  mqttReconnect();
}


void loop() {
  mqttClient.loop();

  handleUart();
  
  if (statusPublishInterval <= (millis() - statusPublishPreviousMillis)) {
    statusPublishPreviousMillis = millis();
    updateRadiationValues();
  }

  if (!mqttClient.connected() && (mqttConnectionInterval <= (millis() - lastMqttConnectionAttempt)) )  {
    lastMqttConnectionAttempt = millis();
    mqttReconnect();
  }
}

void setupWifi() {
  wifiManager.setDebugOutput(false);
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_user);
  wifiManager.addParameter(&custom_mqtt_pass);

  WiFi.hostname(identifier);
  wifiManager.autoConnect(identifier);
  mqttClient.setClient(wifiClient);

  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(username, custom_mqtt_user.getValue());
  strcpy(password, custom_mqtt_pass.getValue());

  if (shouldSaveConfig) {
    saveConfig();
  } else {
    //For some reason, the read values get overwritten in this function
    //To combat this, we just reload the config
    //This is most likely a logic error which could be fixed otherwise
    loadConfig();
  }
}

void resetWifiSettingsAndReboot() {
  wifiManager.resetSettings();
  delay(3000);
  ESP.restart();
}

void mqttReconnect() {
  for (int attempt = 0; attempt < 3; ++attempt) {
    if (mqttClient.connect(identifier, username, password, MQTT_TOPIC_AVAILABILITY, 1, true, AVAILABILITY_OFFLINE)) {
      mqttClient.publish(MQTT_TOPIC_AVAILABILITY, AVAILABILITY_ONLINE, true);

      Serial.println("Connected to MQTT Server");

      publishAutoConfig();
      break;
    } else {
      Serial.println("Failed to connect to MQTT Server :(");
      delay(5000);
    }
  }
}

boolean isMqttConnected() {
  return mqttClient.connected();
}

void publishAutoConfig() {
  char mqttPayload[2048];
  DynamicJsonDocument device(256);
  StaticJsonDocument<64> identifiersDoc;
  JsonArray identifiers = identifiersDoc.to<JsonArray>();

  identifiers.add(identifier);

  device["identifiers"] = identifiers;
  device["manufacturer"] = "MightyOhm LLC";
  device["model"] = "Geiger Counter";
  device["name"] = identifier;
  device["sw_version"] = "0.0.1";


  DynamicJsonDocument cpmSensorPayload(512);

  cpmSensorPayload["device"] = device.as<JsonObject>();
  cpmSensorPayload["availability_topic"] = MQTT_TOPIC_AVAILABILITY;
  cpmSensorPayload["state_topic"] = MQTT_TOPIC_CPM;
  cpmSensorPayload["name"] = identifier + String(" CPM");
  cpmSensorPayload["unit_of_measurement"] = "CPM";
  cpmSensorPayload["unique_id"] = identifier + String("_cpm");

  serializeJson(cpmSensorPayload, mqttPayload);
  mqttClient.publish(MQTT_TOPIC_AUTOCONF_CPM, mqttPayload, true);

  DynamicJsonDocument usvSensorPayload(512);

  usvSensorPayload["device"] = device.as<JsonObject>();
  usvSensorPayload["availability_topic"] = MQTT_TOPIC_AVAILABILITY;
  usvSensorPayload["state_topic"] = MQTT_TOPIC_USV;
  usvSensorPayload["name"] = identifier + String(" uSv");
  usvSensorPayload["unit_of_measurement"] = "µSv/h";
  usvSensorPayload["unique_id"] = identifier + String("_uSv");

  serializeJson(usvSensorPayload, mqttPayload);
  mqttClient.publish(MQTT_TOPIC_AUTOCONF_USV, mqttPayload, true);

  Serial.println("Published MQTT Autoconf");
}
