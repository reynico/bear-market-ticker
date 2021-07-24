#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>

Adafruit_SSD1306 display(-1);

const char* ssid     = "your-wifi-ssid";
const char* password = "your-wifi-password";
const int screen_width = 128;

const char *bitstamp_host = "www.bitstamp.net";
const String bitstamp_uri = "/api/v2/ticker/btcusd/";
//const String bitstamp_uri = "/api/v2/ticker/ethusd/";

const char bitstamp_fingerprint[] PROGMEM = "6E 1B 38 B4 E4 E1 55 56 2D 16 28 85 6D A1 9D CC 0F 89 0A 8A";

int16_t x, y;
uint16_t textWidth, textHeight;

void setup() {
  Wire.begin(2, 0);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  Serial.begin(115200);
  delay(10);

  Serial.print("Connecting to ");
  Serial.println(ssid);

  display.getTextBounds("Connecting...", 0, 0, &x, &y, &textWidth, &textHeight);
  display.setCursor(display.width() / 2 - textWidth / 2, 5);
  display.println("Connecting...");

  display.getTextBounds(ssid, 0, 0, &x, &y, &textWidth, &textHeight);
  display.setCursor(display.width() / 2 - textWidth / 2, 20);
  display.println(ssid);
  display.display();

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  display.clearDisplay();
  display.getTextBounds("Connected!", 0, 0, &x, &y, &textWidth, &textHeight);
  display.setCursor(display.width() / 2 - textWidth / 2, 5);
  display.println("Connected!");

  display.getTextBounds(WiFi.localIP().toString(), 0, 0, &x, &y, &textWidth, &textHeight);
  display.setCursor(display.width() / 2 - textWidth / 2, 20);
  display.println(WiFi.localIP());
  display.display();
}

WiFiClientSecure client;

void loop() {
  client.setFingerprint(bitstamp_fingerprint);
  client.connect(bitstamp_host, 443);

  client.println("GET " + bitstamp_uri + " HTTP/1.0");
  client.print("Host: ");
  client.println(bitstamp_host);
  client.println("User-Agent: ArduinoTicker");
  client.println("Connection: close");
  client.println();

  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      break;
    }
  }

  String content = client.readStringUntil('\n');
  if (!content.startsWith("{\"high\"")) {
    Serial.println("Failed to fetch bitstamp data");
  }

  DynamicJsonDocument doc(1024);

  DeserializationError error = deserializeJson(doc, content);
  if (error) {
    display.clearDisplay();
    display.setTextSize(3);
    display.getTextBounds("ERROR", 0, 0, &x, &y, &textWidth, &textHeight);
    display.setCursor(display.width() / 2 - textWidth / 2, display.height() / 2 - textHeight / 2);
    display.println("ERROR");
    display.display();
    Serial.print("Couldn't deserialize json: ");
    Serial.println(error.f_str());
    client.stop();
    return;
  }

  int rate_int = doc["last"]; // This is float but I want int
  Serial.println(rate_int);

  display.clearDisplay();
  display.setTextSize(3);
  display.getTextBounds(String(rate_int), 0, 0, &x, &y, &textWidth, &textHeight);
  display.setCursor(display.width() / 2 - textWidth / 2, display.height() / 2 - textHeight / 2);
  display.println(rate_int);
  display.display();

  delay(10000);
}
