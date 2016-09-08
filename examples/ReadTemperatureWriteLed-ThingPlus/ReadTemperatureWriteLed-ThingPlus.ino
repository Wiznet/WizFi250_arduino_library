#include <Time.h>
#include <TimeLib.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Arduino.h>

#include <SPI.h>
#include "WizFi250.h"

#include <Timer.h>
#include <Thingplus.h>

//////////////////////////////////////////////////////////////////
const char *ssid = "SSID";                           //FIXME
const char *password = "PASSWORD";                             //FIXME
const char *apikey = "APIKEY";           //FIXME APIKEY
const char *ledId = "LEDID";                      //FIXME LED ID
const char *temperatureId = "TEMPERATUREID";      //FIXME TEMPERATURE ID
//////////////////////////////////////////////////////////////////

Timer t;

int ledOffTimer = 0;
int ledBlinkTimer = 0;

int LED_GPIO = 8;
int TEMP_GPIO = A0;
int reportIntervalSec = 60;

static WiFiClient wifiClient;

static void _serialInit(void)
{
  Serial.begin(115200);
  while (!Serial);// wait for serial port to connect.


  Serial.println(F("Start"));
  Serial.println();
}

static void _wifiInit(void)
{
#define WIFI_MAX_RETRY 150

  WiFi.mode(WIFI_STA);
  WiFi.begin((char*)ssid, password);

  Serial.print(F("[INFO] WiFi connecting to "));
  Serial.println(ssid);

  int retry = 0;
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(retry++);
    Serial.print(F("."));
    if (!(retry % 50))
      Serial.println();
    if (retry == WIFI_MAX_RETRY) {
      Serial.println();
      Serial.print(F("[ERR] WiFi connection failed. SSID:"));
      Serial.print(ssid);
      Serial.print(F(" PASSWD:"));
      Serial.println(password);
      while (1) {
        yield();
      }
    }
    delay(100);
  }

  Serial.println();
  Serial.print(F("[INFO] WiFi connected"));
  Serial.print(F(" IP:"));
  Serial.println(WiFi.localIP());
}

static void _gpioInit(void) {
  pinMode(LED_GPIO, OUTPUT);
}

static void _ledOff() {
  t.stop(ledBlinkTimer);
  digitalWrite(LED_GPIO, LOW);
}

char* actuatingCallback(const char *id, const char *cmd, JsonObject& options) {
  if (strcmp(id, ledId) == 0) {
    t.stop(ledBlinkTimer);
    t.stop(ledOffTimer);

    if (strcmp(cmd, "on") == 0) {
      int duration = options.containsKey("duration") ? options["duration"] : 0;

      digitalWrite(LED_GPIO, HIGH);

      if (duration)
        ledOffTimer = t.after(duration, _ledOff);

      return "success";
    }
    else  if (strcmp(cmd, "off") == 0) {
      _ledOff();
      return "success";
    }
    else  if (strcmp(cmd, "blink") == 0) {
      if (!options.containsKey("interval")) {
        Serial.println(F("[ERR] No blink interval"));
        return NULL;
      }

      ledBlinkTimer = t.oscillate(LED_GPIO, options["interval"], HIGH);

      if (options.containsKey("duration"))
        ledOffTimer = t.after(options["duration"], _ledOff);

      return "success";
    }
    else {
      return NULL;
    }
  }

  return NULL;
}

void setup() {
  _serialInit();

  WiFi.init();

  //uint8_t mac[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  uint8_t mac[6];
  WiFi.macAddress(mac);

  Serial.print(F("[INFO] Gateway Id:"));
  Serial.println(WiFi.macAddress());

  _wifiInit();
  _gpioInit();

  Thingplus.begin(wifiClient, mac, apikey);
  Thingplus.actuatorCallbackSet(actuatingCallback);
  Thingplus.connect();
}

time_t current;
time_t nextReportInterval = now();

float temperatureGet() {
  float temperature;
  float val = analogRead(TEMP_GPIO);
  float voltage = (val * 5000) / 1024;
  voltage = voltage - 500;
  temperature = voltage / 10;
  return temperature;
}

void loop() {
  current = now();
  if (current > nextReportInterval) {
    Thingplus.gatewayStatusPublish(true, reportIntervalSec * 3);
    Thingplus.sensorStatusPublish(temperatureId, true, reportIntervalSec * 3);
    Thingplus.valuePublish(temperatureId, temperatureGet());
    nextReportInterval = current + reportIntervalSec;
  }
  t.update();
  Thingplus.loop();

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network
    WiFi.begin((char*)ssid, password);
  }


}

