
const char* delimiter = ", ";
int lastCPM = 0, currentCPM = 0;
float lastuSv = 0, currentuSv = 0;

byte serialRxBuf[255];



void handleUart() {
  if (geigerCounterSerial.available()) {
    geigerCounterSerial.readBytesUntil('\n', serialRxBuf, 250);

    parseReceivedLine((char*)serialRxBuf);
  }
}

void parseReceivedLine(char* input) {
  Serial.println(input);
  
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
        //Serial.printf("\nCurrent CPM: %s\n", token);
        cpm = String(token).toInt();
        break;

      case IDX_uSv:
        //Serial.printf("Current uSv/hr: %s\n", token);
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

void updateRadiationValues() {
  char tmp[8];

  if (currentCPM != lastCPM) {
    String(currentCPM).toCharArray(tmp, 8);
    Serial.print("Sending CPM: ");
    Serial.println(tmp);
    mqttClient.publish(MQTT_TOPIC_CPM, tmp, true);
  }

  if (currentuSv != lastuSv) {
    String(currentuSv).toCharArray(tmp, 8);
    Serial.print("Sending uSv: ");
    Serial.println(tmp);
    mqttClient.publish(MQTT_TOPIC_USV, tmp, true);
  }

  lastCPM = currentCPM;
  lastuSv = currentuSv;
}
