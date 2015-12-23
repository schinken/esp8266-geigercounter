#define IDX_CPS_KEY 0
#define IDX_CPM_KEY 2
#define IDX_uSv_KEY 4

#define IDX_CPM 3
#define IDX_uSv 5
#define IDX_MODE 6

#define RECV_LINE_SIZE 37

#define PIN_UART_RX 0 // 4
#define PIN_UART_TX 13 // UNUSED

#define UPDATE_INTERVAL_SECONDS 300L

#define BAUD_GEIGERCOUNTER 9600

//const char* ssid = "yourSSID";
//const char* password = "----";

#define MQTT_TOPIC_CPM "sensor/radiation/cpm"
#define MQTT_TOPIC_USV "sensor/radiation/uSv"
#define MQTT_TOPIC_LAST_WILL "state/sensor/geigercounter"

const char* mqttHost = "mqtt.core.bckspc.de";
const char* delimiter = ", ";
