#include "defines.h"

#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <AsyncMqtt_Generic.h>
#include <ArduinoOTA.h>
#include "DHTesp.h"
DHTesp dht;

//#define MQTT_HOST         IPAddress(192, 168, 2, 110)
#define MQTT_HOST         "192.168.1.87"        // Broker address
#define MQTT_PORT         1883

const char *PubTopic  = "ESP8266DHT22/Kok/Temperature";               // Topic to publish
const char *PubTopic2  = "ESP8266DHT22/Kok/Humidity"; 

AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;

Ticker wifiReconnectTimer;

void connectToWifi()
{
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void connectToMqtt()
{
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}

void onWifiConnect(const WiFiEventStationModeGotIP& event)
{
  (void) event;

  Serial.print("Connected to Wi-Fi. IP address: ");
  Serial.println(WiFi.localIP());
  connectToMqtt();
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected& event)
{
  (void) event;

  Serial.println("Disconnected from Wi-Fi.");
  mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
  wifiReconnectTimer.once(2, connectToWifi);
}

void printSeparationLine()
{
  Serial.println("************************************************");
}

void onMqttConnect(bool sessionPresent)
{
  Serial.print("Connected to MQTT broker: ");
  Serial.print(MQTT_HOST);
  Serial.print(", port: ");
  Serial.println(MQTT_PORT);
  Serial.print("PubTopic: ");
  Serial.println(PubTopic);

  printSeparationLine();
  Serial.print("Session present: ");
  Serial.println(sessionPresent);

  uint16_t packetIdSub = mqttClient.subscribe(PubTopic, 2);
  Serial.print("Subscribing at QoS 2, packetId: ");
  Serial.println(packetIdSub);

  mqttClient.publish(PubTopic, 0, true, "ESP8266 Test1");
  Serial.println("Publishing at QoS 0");

  uint16_t packetIdPub1 = mqttClient.publish(PubTopic, 1, true, "ESP8266 Test2");
 /* Serial.print("Publishing at QoS 1, packetId: ");
  Serial.println(packetIdPub1); */

  uint16_t packetIdPub2 = mqttClient.publish(PubTopic, 2, true, "ESP8266 Test3");
 /* Serial.print("Publishing at QoS 2, packetId: ");
  Serial.println(packetIdPub2); */

  printSeparationLine();
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
  (void) reason;

  Serial.println("Disconnected from MQTT.");

  if (WiFi.isConnected())
  {
    mqttReconnectTimer.once(2, connectToMqtt);
  }
}

void onMqttSubscribe(const uint16_t& packetId, const uint8_t& qos)
{
  /*Serial.println("Subscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
  Serial.print("  qos: ");
  Serial.println(qos);*/
}

void onMqttUnsubscribe(const uint16_t& packetId)
{
  /*Serial.println("Unsubscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId); */
}

void onMqttMessage(char* topic, char* payload, const AsyncMqttClientMessageProperties& properties,
                   const size_t& len, const size_t& index, const size_t& total)
{
  /*(void) payload;
  Serial.println("Payload:");
  Serial.print(payload);
  Serial.println("Publish received.");
  Serial.print("  topic: ");
  Serial.println(topic);
  Serial.print("  qos: ");
  Serial.println(properties.qos);
  Serial.print("  dup: ");
  Serial.println(properties.dup);
  Serial.print("  retain: ");
  Serial.println(properties.retain);
  Serial.print("  len: ");
  Serial.println(len);
  Serial.print("  index: ");
  Serial.println(index);
  Serial.print("  total: ");
  Serial.println(total); */
}

void onMqttPublish(const uint16_t& packetId)
{
 /* Serial.println("Publish acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId); */
}

void setup()
{
  Serial.begin(115200);
  dht.setup(D4, DHTesp::DHT22);

  while (!Serial && millis() < 5000);

  delay(300);

  Serial.print("\nStarting FullyFeature_ESP8266 on ");
  Serial.println(ARDUINO_BOARD);
  Serial.println(ASYNC_MQTT_GENERIC_VERSION);

  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onSubscribe(onMqttSubscribe);
  mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);

  connectToWifi();

  // Take care of OTA
  Serial.print("Checking OTA stuff...      ");
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("\n\nOTA: Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA: End");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA: Progress: %u%%\n", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    } else {
      Serial.println("Unknown error!");
    }
  });
  ArduinoOTA.begin();
  Serial.println("OTA OK");
}

void loop()
{
  ArduinoOTA.handle();

  float h = dht.getHumidity();
  float t = dht.getTemperature();
 
   /* Serial.print("{\"humidity\": ");
    Serial.print(h);
    Serial.print(", \"temp\": ");
    Serial.print(t);
    Serial.print("}\n");*/
   
  char array[6];
  
  sprintf(array, "%0.2f", t);
  uint16_t packetIdPub1 = mqttClient.publish(PubTopic, 1, true, array);
  /*Serial.print("Publishing at QoS 1, packetId: ");
  Serial.println(packetIdPub1);*/

  sprintf(array, "%0.3f", h);
  uint16_t packetIdPub2 = mqttClient.publish(PubTopic2, 1, true, array); 
 /* Serial.print("Publishing at QoS 1, packetId: ");
  Serial.println(packetIdPub2);*/
 
    delay(8000);
}
