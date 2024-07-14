#include <WiFi.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>
#include <vector>

#define LED_PIN    D10       // Pin where the LED strip is connected
#define NUM_LEDS   31        // Number of LEDs in your strip
#define HEARTBEAT_INTERVAL 60000 // Heartbeat interval in milliseconds

const char* mqtt_server = "mqtt server goes here";
const int MQTT_PORT = 1883;
const char* MQTT_TOPIC_TRIGGER = "trigger/ledstrip1";
const char* ALIVE_TOPIC = "alive/ledstrip1";

WiFiClient espClient;
PubSubClient client(espClient);
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

unsigned long lastHeartbeat = 0;

// Structure to track timing for specific LED ranges
struct LedTimer {
  unsigned long endTime;
  int startLED;
  int endLED;
};

std::vector<LedTimer> ledTimers;

void setup() {
  Serial.begin(115200);
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'

  WiFiManager wifiManager;
  wifiManager.autoConnect("LEDStrip1");

  client.setServer(mqtt_server, MQTT_PORT);
  client.setCallback(mqttCallback);

  reconnect();
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

  checkTimers();
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("LEDStripController")) {
      Serial.println("connected");
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

  if (String(topic) == MQTT_TOPIC_TRIGGER) {
    processTriggerMessage(message);
  }
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

  // Set LEDs to the new color
  setLEDs(startLED, endLED + 1, colorCode); // endLED is inclusive

  if (duration > 0) {
    unsigned long newEndTime = millis() + duration * 1000; // Convert duration to milliseconds
    bool updated = false;

    // Check if the timer for this range already exists and update it
    for (auto& timer : ledTimers) {
      if (timer.startLED == startLED && timer.endLED == endLED + 1) {
        timer.endTime = newEndTime;
        updated = true;
        break;
      }
    }

    // If no existing timer was updated, create a new one
    if (!updated) {
      LedTimer newTimer = { newEndTime, startLED, endLED + 1 };
      ledTimers.push_back(newTimer);
    }
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

void checkTimers() {
  unsigned long currentTime = millis();
  for (auto it = ledTimers.begin(); it != ledTimers.end(); ) {
    if (currentTime > it->endTime) {
      turnOffLEDs(it->startLED, it->endLED);
      it = ledTimers.erase(it); // Remove expired timer
    } else {
      ++it;
    }
  }
}

void turnOffLEDs(int start, int end) {
  for (int i = start; i < end; i++) {
    strip.setPixelColor(i, strip.Color(0, 0, 0));
  }
  strip.show();
}
