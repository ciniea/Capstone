#include <Adafruit_BME280.h>
#include <ArduinoJson.h> //TODO: Update to v6 and refactor affected code
#include <ESP8266WiFi.h>
#include <FastLED.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <Wire.h>

#define LED_ON LOW
#define LED_OFF HIGH

#define LED_GND D5
#define LED_PIN D6
#define NUM_LEDS 12

#define LARGE_LED D8

#define BME_POWER D4

const char *ssid = "Netgear-52G";
const char *password = "RaspberryPiWiFi8";
const char *mqtt_server = "192.168.8.1";

void callback(char *topic, byte *payload, unsigned int length);
WiFiClient espClient;
PubSubClient client(mqtt_server, 1883, callback, espClient);
long lastMsg = 0;
float temp = 0;
char msg[32];

CRGB leds[NUM_LEDS];
int brightness = 75;

Adafruit_BME280 bme;

void setup() {
  Serial.begin(115200);
  pinMode(LARGE_LED, OUTPUT); // Initialize the LED pin as an output
  pinMode(BME_POWER, OUTPUT); // Enable BME sensor
  pinMode(LED_GND, OUTPUT);   // Enable LED strip ground
  digitalWrite(LED_GND, LOW);
  digitalWrite(BME_POWER, HIGH); // Turn on BME
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(brightness);
  if (!bme.begin()) {
    // Could not initialize bme280
    blink_led(10, 90); // angry LED
  }
  blink_led(1, 200);
  setup_wifi();
  // client.setServer(mqtt_server, 1883);
  // client.setCallback(callback);
}

void loop() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastMsg > 30000) {
    lastMsg = now;
    temp = bme.readTemperature();
    snprintf(msg, 32, "Current temperature: %3.2f°C", temp);
    client.publish("outIoT", msg);

    StaticJsonBuffer<200> jsonBuffer;
    JsonObject &JSONencoder = jsonBuffer.createObject();

    JSONencoder["SensorID"] = "ESP8266_29C5E3";
    JSONencoder["Temperature"] = temp;
    JSONencoder["Pressure"] = bme.readPressure();
    JSONencoder["Humidity"] = bme.readHumidity();

    char JSONmessageBuffer[100];
    JSONencoder.printTo(JSONmessageBuffer, sizeof(JSONmessageBuffer));
    client.publish("outIoT/full", JSONmessageBuffer);
  }
}

void setup_wifi() {
  delay(10);
  WiFi.mode(WIFI_STA);
  // We start by connecting to a WiFi network
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    // Serial.print(".");
  }
  /*
    Serial.println("");
      Serial.println("WiFi connected");
        Serial.println("IP address: ");
          Serial.println(WiFi.localIP());
            */
  blink_led(3, 200);
  led_strip_init_seq();
}

void callback(char *topic, byte *payload, unsigned int length) {
  Serial.println();
  Serial.print("Message arrived (");
  Serial.print(length);
  Serial.print(") [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }

  if (strcmp(topic, "inIoT/12hr") == 0) {
    pinMode(LARGE_LED, LED_ON);
    const size_t bufferSize =
        12 * JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(12) + 360;
    StaticJsonBuffer<bufferSize> jsonBuffer;
    JsonObject &root = jsonBuffer.parseObject((char *)payload);

    // Show the precipitation forecast
    int i = 0;
    leds[i] = CRGB::Black;
    FastLED.show();
    delay(150);
    for (auto kv : root) {
      // atoi(kv.key) --> hour "hh"
      leds[i] = CRGB(0, 0, (2.55 * kv.value["rain"].as<unsigned int>()));
      if (i < NUM_LEDS - 1) {
        leds[i + 1] = CRGB::Black;
      }
      FastLED.show();
      FastLED.delay(150);
      i++;
    }
    /*
        // Show the temperature forecast

                for(int i = 0; i < 12; i++)
                    {
                          leds[i] = HeatColor(root[i]["temp"]);
                                FastLED.show();
                                      FastLED.delay(150);
                                          }
                                              */
    pinMode(LARGE_LED, LED_OFF);
  }

  else if (strcmp(topic, "inIoT/leds/mode") == 0) {
    // const size_t bufferSize = JSON_OBJECT_SIZE(1) + 80;
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject &root = jsonBuffer.parseObject((char *)payload);
    // const char * mode_str = root["mode"];
    if (root["mode"] == "off") {
      fill_solid(leds, NUM_LEDS, CRGB::Black);
    } else if (root["mode"] ==
               "rainbow") // strcmp doesn't work, even with
                          // casting. this does. for some reason.
    {
      fill_rainbow(leds, NUM_LEDS, 0, 21);
    }
    FastLED.show();
  }

  else if (strcmp(topic, "inIoT/leds") == 0) {
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject &root = jsonBuffer.parseObject((char *)payload);

    if (root.containsKey("led_num") && root.containsKey("color")) {
      int red = root["color"][0];
      int green = root["color"][1];
      int blue = root["color"][2];
      int led_num = root["led_num"];
      leds[led_num] = CRGB(red, green, blue);
    }

    FastLED.show();
  }

  else if (strcmp(topic, "inIoT") == 0) {
    // Switch on the LED if an 1 was received as first character
    if ((char)payload[0] == '1') {
      digitalWrite(LARGE_LED, LED_ON); // Turn the large LED on
    } else {
      digitalWrite(LARGE_LED, LED_OFF); // Turn the large LED off
    }
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    // Attempt to connect
    if (client.connect("ESP8266_29C5E3", "user", "password")) {
      // Once connected, publish an announcement...
      client.publish("outIoT", "Device 29C5E3 online");
      // ... and resubscribe
      client.subscribe("inIoT/#");
    } else {
      // Serial.print("failed, rc=");
      // Serial.print(client.state());
      // Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void blink_led(int count, int duration) {
  for (int i = 0; i < count; i++) {
    digitalWrite(LARGE_LED, LED_ON);
    delay(duration);
    digitalWrite(LARGE_LED, LED_OFF);
    delay(duration);
  }
}

void led_strip_init_seq() {
  for (int n = 0; n < NUM_LEDS; n++) {
    leds[n] = CRGB::Blue;
    FastLED.show();
    delay(50);
  }
  for (int n = NUM_LEDS - 1; n >= 0; n--) {
    leds[n] = CRGB::Black;
    FastLED.show();
    delay(30);
  }
  delay(40);
  fill_rainbow(leds, NUM_LEDS, 0, 21);
  FastLED.show();
}
