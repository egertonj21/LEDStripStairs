#include <WiFi.h>      
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>

#define LED_PIN    D10       // Pin where the LED strip is connected
#define NUM_LEDS   31        // Number of LEDs in your strip
#define HEARTBEAT_INTERVAL 60000 // Heartbeat interval in milliseconds

const char* mqtt_server = "serverip";
const int MQTT_PORT = 1883;
const char* MQTT_TOPIC_SET_COLOR = "set_colour/ledstrip2";
const char* MQTT_TOPIC_TRIGGER = "trigger/ledstrip2";
const char* ALIVE_TOPIC = "alive/ledstrip2";

WiFiClient espClient;
PubSubClient client(espClient);
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

unsigned long lastHeartbeat = 0;
unsigned long lightOffTime = 0;
uint32_t prevColor[NUM_LEDS];
bool isOn = false;
uint32_t defaultColor = strip.Color(0, 0, 0);

void setup() {
  Serial.begin(115200);
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'

  WiFiManager wifiManager;
  wifiManager.autoConnect("LEDStrip2");

  client.setServer(mqtt_server, MQTT_PORT);
  client.setCallback(mqttCallback);

  reconnect();
  // Initialize the previous color state to off
  for (int i = 0; i < NUM_LEDS; i++) {
    prevColor[i] = strip.Color(0, 0, 0);
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  if (millis() - lastHeartbeat > HEARTBEAT_INTERVAL) {
    publishHeartbeat();
    lastHeartbeat = millis();
  }

  if (lightOffTime != 0 && millis() > lightOffTime) {
    revertLEDs(); // Revert LEDs to previous state
    lightOffTime = 0;
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("LEDStripController")) {
      Serial.println("connected");
      client.subscribe(MQTT_TOPIC_SET_COLOR);
      client.subscribe(MQTT_TOPIC_TRIGGER);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void publishHeartbeat() {
  String heartbeatMsg = "alive";
  client.publish(ALIVE_TOPIC, heartbeatMsg.c_str());
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  char msg[length + 1];
  memcpy(msg, payload, length);
  msg[length] = '\0';

  String message = String(msg);
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(message);

  if (String(topic) == MQTT_TOPIC_SET_COLOR) {
    processSetColorMessage(message);
  } else if (String(topic) == MQTT_TOPIC_TRIGGER) {
    processTriggerMessage(message);
  }
}

void processSetColorMessage(String message) {
  uint32_t colorCode = getColorFromRGBString(message);
  for (int i = 0; i < NUM_LEDS; i++) {
    prevColor[i] = colorCode;
    strip.setPixelColor(i, colorCode);
  }
  strip.show();
  isOn = (colorCode != strip.Color(0, 0, 0));
}

void processTriggerMessage(String message) {
  int firstSepIndex = message.indexOf('&');
  int secondSepIndex = message.indexOf('&', firstSepIndex + 1);

  String range = message.substring(0, firstSepIndex);
  String color = message.substring(firstSepIndex + 1, secondSepIndex);
  String durationStr = message.substring(secondSepIndex + 1);

  int startLED = range.substring(0, range.indexOf('-')).toInt();
  int endLED = range.substring(range.indexOf('-') + 1).toInt();
  uint32_t colorCode = getColorFromRGBString(color);
  int duration = durationStr.toInt();

  // Save the current state of the LEDs in the specified range
  saveLEDState(startLED, endLED + 1);

  // Set LEDs to the new color
  setLEDs(startLED, endLED + 1, colorCode); // endLED is inclusive

  if (duration > 0) {
    lightOffTime = millis() + duration * 1000; // Convert duration to milliseconds
  } else {
    lightOffTime = 0; // If duration is 0 or less, light up indefinitely
  }
}

uint32_t getColorFromRGBString(String rgb) {
  int firstCommaIndex = rgb.indexOf(',');
  int secondCommaIndex = rgb.indexOf(',', firstCommaIndex + 1);

  int red = rgb.substring(0, firstCommaIndex).toInt();
  int green = rgb.substring(firstCommaIndex + 1, secondCommaIndex).toInt();
  int blue = rgb.substring(secondCommaIndex + 1).toInt();

  return strip.Color(red, green, blue);
}

void setLEDs(int start, int end, uint32_t color) {
  for (int i = start; i < end; i++) {
    strip.setPixelColor(i, color);
  }
  strip.show();
}

void saveLEDState(int start, int end) {
  for (int i = start; i < end; i++) {
    prevColor[i] = strip.getPixelColor(i);
  }
}

void revertLEDs() {
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, prevColor[i]);
  }
  strip.show();
}
