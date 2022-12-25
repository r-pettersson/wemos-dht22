#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <ESP8266SSDP.h>
#include <ESP8266WebServer.h>

// To get support for esp8266, add the following to "Additional Boards Manager URL:s" in File - Settings
//https://arduino.esp8266.com/stable/package_esp8266com_index.json

// Instead of using the standard ADAFruit DHT library (DHT.h), it seems to work much more reliable with the
// "DHT Sensor library for ESPxxx"
// https://desire.giesecke.tk/index.php/2018/01/30/esp32-dht11/
// https://github.com/beegee-tokyo/DHTesp
// This library gives values every time, and no more "failed to read sensor".
// Currently using version 1.17.0
#include <DHTesp.h>;

#define MAX_NBR_SENSORS 2
#define MAX_NBR_ACTUATORS 2

struct SensorInfo {
  bool inuse;
  DHTesp::DHT_MODEL_t type;
  int pin;
  String temperatureUrl;
  String humidityUrl;
};

struct ActuatorInfo {A
  bool inuse;
  int pin;
  String actuatorUrl;
};

struct BoardInfo {
  String hostname;
  String deviceid;
  SensorInfo si[MAX_NBR_SENSORS];
  ActuatorInfo ai[MAX_NBR_ACTUATORS];
};

// Wemos D1 Mini markings to GPIO:
// D0: 16
// D1: 5
// D3: 0
// D4: 2
// D5: 14
// D6: 12
// D7: 13
// D8: 15

//const BoardInfo bi = {"esp-______", "SteffeDev-TempHum-Test",         {{true, DHTesp::DHT11, 2, "/test/temperature",        "/test/humidity"},        {false}},                                                                    {{false},                    {false}}};
//const BoardInfo bi = {"esp-b21869", "SteffeDev-TempHum-Inomhus",      {{true, DHTesp::DHT22, 2, "/inomhus/temperature",     "/inomhus/humidity"},     {false}},                                                                    {{false},                    {false}}};
//const BoardInfo bi = {"esp-3c5b33", "SteffeDev-TempHum-Uterum",       {{true, DHTesp::DHT22, 0, "/uterum/temperature",      "/uterum/humidity"},      {false}},                                                                    {{false},                    {false}}};
//const BoardInfo bi = {"esp-361771", "SteffeDev-TempHum-Vind",         {{true, DHTesp::DHT22, 2, "/vind/temperature",        "/vind/humidity"},        {false}},                                                                    {{false},                    {false}}};
//const BoardInfo bi = {"esp-361950", "SteffeDev-TempHum-Klk",          {{true, DHTesp::DHT22, 2, "/klk/temperature",         "/klk/humidity"},         {false}},                                                                    {{false},                    {false}}};
//const BoardInfo bi = {"esp-36164d", "SteffeDev-TempHum-Fixarrummet",  {{true, DHTesp::DHT22, 2, "/fixarrummet/temperature", "/fixarrummet/humidity"}, {false}},                                                                    {{false},                    {false}}};
//const BoardInfo bi = {"esp-3c58d5", "SteffeDev-TempHum-GolvRadiator", {{true, DHTesp::DHT11, 0, "/golv/temperature",        "/golv/humidity"},        {true, DHTesp::DHT11, 2, "/radiator/temperature",    "/radiator/humidity"}}, {{true, 16, "/radiator/fan"}, {false}}};
const BoardInfo bi = {"esp-3c5b46", "SteffeDev-TempHum-Matplats",       {{true, DHTesp::DHT22, 2, "/matplats/temperature",     "/matplats/humidity"},     {false}},                                                                    {{false},                    {false}}};

#define STEFFEVERSION  "1.3.0"

#ifndef STASSID
#define STASSID ""
#define STAPSK  ""
#endif

const char* ssid = STASSID;
const char* password = STAPSK;

ESP8266WebServer server(80);
DHTesp dhts[MAX_NBR_SENSORS];

struct UpTime {
  unsigned long overflow;
  unsigned long lastmillis;
};
UpTime uptime;
#define MILLISMAXINSECONDS 4294967

void setup() {
  float val;
  ushort retries;
  uptime.overflow = 0;
  uptime.lastmillis = millis();
  Serial.begin(115200);
  Serial.println("");
  Serial.println("");
  Serial.println("-= Steffe OTA =-");
  Serial.println("");
  Serial.println("Booting up " + bi.deviceid + " version " + STEFFEVERSION);

  // Connect to Wifi
  Serial.print("Connecting to Wifi...      ");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting in 60 seconds...");
    delay(60000);
    ESP.restart();
  }
  Serial.print("OK, IP: ");
  Serial.println(WiFi.localIP());

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
  Serial.println("OK");

  // SSDP
  Serial.print("Starting SSDP...           ");
  SSDP.setSchemaURL("description.xml");
  SSDP.setHTTPPort(80);
  SSDP.setName("SteffeName");
  SSDP.setSerialNumber("SteffeSerial");
  SSDP.setURL("/");
  SSDP.setModelName(bi.deviceid);
  SSDP.setModelNumber(STEFFEVERSION);
  SSDP.setModelURL("http://steffe.net");
  SSDP.setManufacturer("SteffeManufacturer");
  SSDP.setManufacturerURL("http://steffe.net");
  SSDP.begin();
  Serial.println("OK");
  
  // Register default web server URLs
  Serial.print("Starting web server...     ");
  server.on("/", handleRoot);
  server.on("/reboot", handleReboot);
  server.on("/description.xml", handleDefaultXml);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("OK");

  // Start DHT:s and register their URLs
  for(int i=0;i<MAX_NBR_SENSORS;i++) {
    SensorInfo sensor = bi.si[i];
    if (sensor.inuse) {
      Serial.print("Starting ");
      Serial.print(getSensorString(sensor.type));
      Serial.print(" on pin ");
      Serial.print(sensor.pin);
      Serial.print("...");
      dhts[i].setup(sensor.pin, sensor.type);
      server.on(sensor.temperatureUrl, handleSensor);
      server.on(sensor.humidityUrl, handleSensor);
      val = dhts[i].getTemperature();
      retries = 0;
      while (isnan(val)) {
        if (retries > 100)
          break;
        delay(100);
        Serial.print(".");
        retries++;
        val = dhts[i].getTemperature();
      }
      Serial.print(" OK, Temperature: ");
      Serial.print(dhts[i].getTemperature());
      Serial.print(", Humidity: ");
      Serial.println(dhts[i].getHumidity());
    }
  }

  // Start actuators register their URLs
  for(int i=0;i<MAX_NBR_ACTUATORS;i++) {
    ActuatorInfo actuator = bi.ai[i];
    if (actuator.inuse) {
      Serial.print("Setting up and turning off actuator on pin ");
      Serial.print(actuator.pin);
      Serial.print("...");
      pinMode(actuator.pin, OUTPUT);
      digitalWrite(actuator.pin, LOW);
      server.on(actuator.actuatorUrl, handleActuator);
      server.on(actuator.actuatorUrl + "/on", handleActuatorOn);
      server.on(actuator.actuatorUrl + "/off", handleActuatorOff);
      Serial.println("");
    }
  }
}

String getSensorString(DHTesp::DHT_MODEL_t model) {
  if (model == DHTesp::AUTO_DETECT)
    return String("AutoDetect");
  if (model == DHTesp::DHT11)
    return String("DHT11");
  if (model == DHTesp::DHT22)
    return String("DHT22");
  if (model == DHTesp::AM2302)
    return String("AM2302");
  if (model == DHTesp::RHT03)
    return String("RHT03");
  return String("Unknown");
}

void handleDefaultXml() {
  SSDP.schema(server.client());
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void handleRoot() {
  String ip = WiFi.localIP().toString();
  String mac = WiFi.macAddress();
  String hostname = WiFi.hostname();
  String message = "";
  message += "ID:       " + String(bi.deviceid) + "\n";
  message += "Version:  " + String(STEFFEVERSION) + "\n";
  message += "MAC:      " + mac + "\n";
  message += "IP:       " + ip + "\n";
  message += "Hostname: " + hostname + "\n";
  message += "Uptime:   " + getUptime() + "\n";
  message += "\n";
  message += String("ESP8266@") + ESP.getCpuFreqMHz() + "Mhz with " + ESP.getFreeHeap() + " bytes of free heap (" + ESP.getHeapFragmentation() + " % fragmented)\n";
  message += String("Chip version ") + ESP.getCoreVersion() + ", SDK version " + ESP.getSdkVersion() + ", Chip ID " + ESP.getChipId() + "\n";
  message += "\n";
  message += "Sensors:\n";
  for (int i=0;i<MAX_NBR_SENSORS;i++) {
    SensorInfo sensor = bi.si[i];
    if (sensor.inuse) {
      message += "   * " + getSensorString(sensor.type) + " on pin " + sensor.pin + ":\n";
      message += "      " + String("Temperature: ") + dhts[i].getTemperature() + "C " + sensor.temperatureUrl + "\n";
      message += "      " + String("Humidity:    ") + dhts[i].getHumidity() + "% " + sensor.humidityUrl + "\n";
    }
  }
  message += "\n";
  message += "Actuators:\n";
  for (int i=0;i<MAX_NBR_ACTUATORS;i++) {
    ActuatorInfo actuator = bi.ai[i];
    if (actuator.inuse) {
      message += "   * Actuator ";
      message += actuator.actuatorUrl;
      message += " on pin " + String(actuator.pin) + ": ";
      if (digitalRead(actuator.pin) == LOW)
        message += "OFF\n";
      else
        message += "ON\n";
    }
  }
  message += "\n";
  message += "Capabilities:\n";
  for (int i=0;i<MAX_NBR_SENSORS;i++) {
    SensorInfo sensor = bi.si[i];
    if (sensor.inuse) {
      message += "http://" + ip + sensor.temperatureUrl + "\n";
      message += "http://" + ip + sensor.humidityUrl + "\n";
    }
  }
  for (int i=0;i<MAX_NBR_ACTUATORS;i++) {
    ActuatorInfo actuator = bi.ai[i];
    if (actuator.inuse) {
      message += "http://" + ip + actuator.actuatorUrl + "\n";
      message += "http://" + ip + actuator.actuatorUrl + "/on\n";
      message += "http://" + ip + actuator.actuatorUrl + "/off\n";
    }
  }
  server.send(200, "text/plain", message);
}

void handleReboot() {
  server.send(200, "text/plain", "Will reboot in 3 seconds...");
  delay(3000);
  ESP.restart();
}

void sendSensorReponse(DHTesp dht, bool temp) {
  float val;
  int code;
  String message;

  if (temp) {
    val = dht.getTemperature();
    Serial.print("Temperature: ");
  } else {
    val = dht.getHumidity();
    Serial.print("Humidity: ");
  }

  if (isnan(val)) {
    message = String("Failed to read sensor: ") + dht.getStatusString();
    code = 500;
  } else {
    message = String(val);
    code = 200;
  }
  server.send(code, "text/plain", message);
  Serial.println(message);
}

void sendActuatorReponse(int pin) {
  String message;

  Serial.print("Actuator is ");
  if (digitalRead(pin) == LOW) {
    message = "OFF";
  } else {
    message = "ON";
  }
  server.send(200, "text/plain", message);
  Serial.println(message);
}

void handleSensor() {
  String url = server.uri();
  for(int i=0;i<MAX_NBR_SENSORS;i++) {
    SensorInfo sensor = bi.si[i];
    if (sensor.inuse) {
      if (url == sensor.temperatureUrl)
        return sendSensorReponse(dhts[i], true);
      if (url == sensor.humidityUrl)
        return sendSensorReponse(dhts[i], false);
    }
  }
  server.send(500, "text/plain", String("Unknown sensor: ") + url);
}

void handleActuator() {
  String url = server.uri();
  for(int i=0;i<MAX_NBR_ACTUATORS;i++) {
    ActuatorInfo actuator = bi.ai[i];
    if (actuator.inuse) {
      if (url == actuator.actuatorUrl)
        return sendActuatorReponse(actuator.pin);
    }
  }
  server.send(500, "text/plain", String("Unknown sensor: ") + url);
}

void handleActuatorOn() {
  String url = server.uri();
  for(int i=0;i<MAX_NBR_ACTUATORS;i++) {
    ActuatorInfo actuator = bi.ai[i];
    if (actuator.inuse) {
      if (url == actuator.actuatorUrl + "/on") {
        Serial.print("Turning on actuator ");
        Serial.print(actuator.actuatorUrl);
        Serial.print(" on pin ");
        Serial.println(actuator.pin);
        digitalWrite(actuator.pin, HIGH);
        return sendActuatorReponse(actuator.pin);
      }
    }
  }
  server.send(500, "text/plain", String("Unknown sensor: ") + url);
}

void handleActuatorOff() {
  String url = server.uri();
  for(int i=0;i<MAX_NBR_ACTUATORS;i++) {
    ActuatorInfo actuator = bi.ai[i];
    if (actuator.inuse) {
      if (url == actuator.actuatorUrl + "/off") {
        Serial.print("Turning off actuator ");
        Serial.print(actuator.actuatorUrl);
        Serial.print(" on pin ");
        digitalWrite(actuator.pin, LOW);
        return sendActuatorReponse(actuator.pin);
      }
    }
  }
  server.send(500, "text/plain", String("Unknown sensor: ") + url);
}

void handleUptime() {
  unsigned long now = millis();
  if (now < uptime.lastmillis)
    uptime.overflow++;
  uptime.lastmillis = now;
}

String getUptime() {
  unsigned long totseconds = millis() / 1000;
  totseconds += uptime.overflow * MILLISMAXINSECONDS;
  unsigned long seconds = (totseconds)               % 60;
  unsigned long minutes = (totseconds /  60)         % 60;
  unsigned long hours   = (totseconds / (60*60))     % 24;
  unsigned long days    = (totseconds / (60*60*24))  % 24;
  return String(String("[days:hours:minutes:seconds] [") + days + ":" + hours + ":" + minutes + ":" + seconds + "]");
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();
  handleUptime();
}
