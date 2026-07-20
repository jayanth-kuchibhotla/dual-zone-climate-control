// ─── Libraries ──────────────────────────────────────
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <DHT.h>
#include<ArduinoJson.h>
#include <secrets.h>

// ─── AWS IoT endpoint ───────────────────────────────
#define AWS_IOT_PORT     8883

// ─── MQTT topic ─────────────────────────────────────
#define MQTT_TOPIC       "home/dual-zone-climate/monitoring-device"
#define CLIENT_ID        "dual-zone-climate-control"

const char* SHADOW_UPDATE =
"$aws/things/dual-zone-climate-control/shadow/update";

const char* SHADOW_DELTA =
"$aws/things/dual-zone-climate-control/shadow/update/delta";

const char* SHADOW_GET =
"$aws/things/dual-zone-climate-control/shadow/get";

const char* SHADOW_GET_ACCEPTED =
"$aws/things/dual-zone-climate-control/shadow/get/accepted";

// ─── Sensor pins ────────────────────────────────────
#define DHTPIN_A 4
#define DHTPIN_B 5
#define DHTTYPE DHT11
#define RELAY1_PIN 21
#define RELAY2_PIN 22

// ─── Thresholds ─────────────────────────────────────
#define TEMP_THRESHOLD 30.0
#define HUM_THRESHOLD  70.0

// ─── Globals ────────────────────────────────────────
WiFiClientSecure net;
PubSubClient mqtt(net);
DHT dhtA(DHTPIN_A, DHTTYPE);
DHT dhtB(DHTPIN_B, DHTTYPE);

bool relay1on = false;
bool relay2on = false;
bool deltaOverride = false;

unsigned long lastPublish = 0;
const unsigned long PUBLISH_INTERVAL = 10000;

// ─── WiFi connect ───────────────────────────────────
void connectWiFi() {
  Serial.print("[WiFi] Connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[WiFi] Connected. IP: ");
  Serial.println(WiFi.localIP());
}

// ─── MQTT callback ─────────────────────────────────
void callback(char* topic, byte* payload, unsigned int length) {

  Serial.println("[MQTT] Message received on topic: " + String(topic));

  char buffer[length + 1];
  memcpy(buffer, payload, length);
  buffer[length] = '\0';

  String message = String(buffer);

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, message);
  if(error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }

  if(strcmp(topic, SHADOW_DELTA) == 0) {
    relay1on = String(doc["state"]["zoneA"]["relay1"].as<const char*>()) == "ON";
    relay2on = String(doc["state"]["zoneB"]["relay2"].as<const char*>()) == "ON";
    deltaOverride = true;

    digitalWrite(RELAY1_PIN, relay1on ? HIGH : LOW);
    digitalWrite(RELAY2_PIN, relay2on ? HIGH : LOW);
  }
}

// ─── MQTT connect ───────────────────────────────────
void connectMQTT() {
  net.setCACert(AWS_ROOT_CA);
  net.setCertificate(DEVICE_CERT);
  net.setPrivateKey(DEVICE_KEY);
  mqtt.setServer(AWS_IOT_ENDPOINT, AWS_IOT_PORT);
  mqtt.setBufferSize(2048);

  Serial.print("[MQTT] Connecting to AWS IoT...");
  mqtt.setCallback(callback);

  while (!mqtt.connected()) {
    if(mqtt.connect(CLIENT_ID)){
      Serial.println(" connected.");
    } else {
      Serial.print(" failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(". Attempting to reconnect in 5s...");
      delay(5000);
    }
  }

  mqtt.subscribe(SHADOW_DELTA);
  mqtt.subscribe(SHADOW_GET_ACCEPTED);

  bool getResult = mqtt.publish(SHADOW_GET, "{}");
  Serial.println(getResult ? "[MQTT] Shadow GET published" : "[MQTT] Shadow GET FAILED");
  delay(500);
  mqtt.loop();
}

// ─── Setup ──────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  dhtA.begin();
  dhtB.begin();
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  digitalWrite(RELAY1_PIN, LOW);
  digitalWrite(RELAY2_PIN, LOW);

  connectWiFi();
  connectMQTT();

}

// ─── Loop ───────────────────────────────────────────
void loop() {
  if(!mqtt.connected()) connectMQTT();
  mqtt.loop();

  // Publish every 10 seconds
  if(millis() - lastPublish >= PUBLISH_INTERVAL) {
    lastPublish = millis();
    float tempA = dhtA.readTemperature();
    float humA  = dhtA.readHumidity();
    float tempB = dhtB.readTemperature();
    float humB  = dhtB.readHumidity();

    if(!deltaOverride) {
      relay1on = (!isnan(tempA) && tempA > TEMP_THRESHOLD);
      relay2on = (!isnan(humB)  && humB  > HUM_THRESHOLD);
    }

    digitalWrite(RELAY1_PIN, relay1on ? HIGH : LOW);
    digitalWrite(RELAY2_PIN, relay2on ? HIGH : LOW);

    // Build JSON payload
    JsonDocument doc;
    doc["state"]["reported"]["zoneA"]["temperature"] = tempA;
    doc["state"]["reported"]["zoneA"]["humidity"]    = humA;
    doc["state"]["reported"]["zoneA"]["relay1"]      = relay1on ? "ON" : "OFF";
    doc["state"]["reported"]["zoneB"]["temperature"] = tempB;
    doc["state"]["reported"]["zoneB"]["humidity"]    = humB;
    doc["state"]["reported"]["zoneB"]["relay2"]      = relay2on ? "ON" : "OFF";

    char payload[512];
    serializeJson(doc, payload, sizeof(payload));

    if(!mqtt.publish(MQTT_TOPIC, payload)) {
      Serial.println("[MQTT] Telemetry publish failed");
    }

    if(!mqtt.publish(SHADOW_UPDATE, payload)) {
      Serial.println("[MQTT] Shadow update publish failed");
    }

    Serial.println("[MQTT] Published:");
    Serial.println(payload);
    
    Serial.println("------ Sensor Readings ------");
    Serial.print("[Zone A] Temp: "); Serial.print(tempA, 1);
    Serial.print(" C  |  Humidity: "); Serial.print(humA, 1); Serial.println(" %");
    Serial.print("[Zone B] Temp: "); Serial.print(tempB, 1);
    Serial.print(" C  |  Humidity: "); Serial.print(humB, 1); Serial.println(" %");
    Serial.print("[Relay 1 - Fan]        "); Serial.println(relay1on ? "ON" : "OFF");
    Serial.print("[Relay 2 - Humidifier] "); Serial.println(relay2on ? "ON" : "OFF");
    Serial.println("-----------------------------\n");
  }
}