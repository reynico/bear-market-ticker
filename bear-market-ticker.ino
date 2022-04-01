#include <EEPROM.h>

#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>

#define TRIGGER_PIN 2

Adafruit_SSD1306 display(-1);
const int screen_width = 128;
const int special_char = 37;

const char *bitstamp_host = "www.bitstamp.net";
const String bitstamp_uri = "/api/v2/ticker/";
String errors = "";

String initial_pair = "btcusd";


int16_t x, y;
uint16_t text_width, text_height;

ESP8266WebServer server(80);

unsigned long prev_millis = 0;

const long check_interval = 60000; // 1 minute in milliseconds

void setup() {
  pinMode(TRIGGER_PIN, INPUT_PULLUP);
  Serial.begin(115200);
  EEPROM.begin(512);
  Wire.begin(2, 0);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  Serial.println("Init!");
  display.getTextBounds("Hola!", 0, 0, &x, &y, &text_width, &text_height);
  display.setCursor(display.width() / 2 - text_width / 2, 5);
  display.println("Hola!");

  display.getTextBounds("Soy tu Bitcoin Ticker", 0, 0, &x, &y, &text_width, &text_height);
  display.setCursor(display.width() / 2 - text_width / 2, 20);
  display.println("Soy tu Bitcoin Ticker");
  display.display();

  for (int i = 0; i <= 100; i++) {
    reset_default();
    delay(100);
  }

  WiFiManager wifiManager;

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Not connected to WiFi, starting Config Portal");
    display.clearDisplay();
    display.getTextBounds("Wifi: ticker", 0, 0, &x, &y, &text_width, &text_height);
    display.setCursor(display.width() / 2 - text_width / 2, 5);
    display.println("Wifi: ticker");

    display.getTextBounds("pass: notsatoshi", 0, 0, &x, &y, &text_width, &text_height);
    display.setCursor(display.width() / 2 - text_width / 2, 20);
    display.println("pass: notsatoshi");
    display.display();
    wifiManager.autoConnect("ticker", "notsatoshi");
  }
  Serial.print("Connected to WiFi: ");
  Serial.print(wifiManager.getWiFiSSID());
  Serial.print("\t with IP: ");
  Serial.println(WiFi.localIP().toString());
  display.clearDisplay();
  display.getTextBounds(wifiManager.getWiFiSSID(), 0, 0, &x, &y, &text_width, &text_height);
  display.setCursor(display.width() / 2 - text_width / 2, 5);
  display.println(wifiManager.getWiFiSSID());

  display.getTextBounds(WiFi.localIP().toString(), 0, 0, &x, &y, &text_width, &text_height);
  display.setCursor(display.width() / 2 - text_width / 2, 20);
  display.println(WiFi.localIP());
  display.display();

  if (firstRun() == true) {
    write_word(initial_pair, 0);
  } else {
    initial_pair = read_word(0);
  }

  server.on("/", handle_root);
  server.on("/select_pair", handle_select_pair);
  server.begin();
  fetch_xrate();

}

void reset_default() {
  if (digitalRead(TRIGGER_PIN) == LOW ) {
    delay(50);
    if (digitalRead(TRIGGER_PIN) == LOW ) {
      WiFiManager wifiManager;
      delay(3000);
      if ( digitalRead(TRIGGER_PIN) == LOW ) {
        Serial.println("Reset!");
        reset_eeprom();
        wifiManager.resetSettings();
        ESP.restart();
      }
      wifiManager.setConfigPortalTimeout(120);
      if (!wifiManager.startConfigPortal("ticker", "notsatoshi")) {
        Serial.println("failed to connect or hit timeout");
        errors += "failed to connect or hit timeout</br>";
        delay(3000);
      } else {
        Serial.println("Connected (again)");
      }
    }
  }
}

void handle_root() {
  String s = "<!DOCTYPE html><html lang=\"es\"><head><title>Bitcoin Ticker</title><meta charset=\"utf-8\">";
  s += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  s += "<link rel=\"stylesheet\" href=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.4.1/css/bootstrap.min.css\">";
  s += "<script src=\"https://ajax.googleapis.com/ajax/libs/jquery/3.5.1/jquery.min.js\"></script>";
  s += "<script src=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.4.1/js/bootstrap.min.js\"></script>";
  s += "<style>.form-group {height:50px;display:flex;align-items:center;}";
  s += "button {min-height: 30px;padding: 8px;}";
  s += "button,input {border-radius: 0.3rem;cursor: pointer;border: 0;background-color: #1fa3ec;color: #fff;line-height: 2.4rem;font-size: 1.8rem;width: 100%;}";
  s += "label {text-align:right;clear:both;float:left;margin-right:15px;}</style>";
  s += "</head><body><div class=\"container\"><h2>Bitcoin Ticker</h2>";
  s += "<p>Desde acá podés configurar tu Bitcoin Ticker para que muestre el par que quieras, limitado a la disponibilidad de la API de Bitstamp.</p>";
  s += "<form action=\"/select_pair\"><div class=\"form-group\"><label for=\"par\">Par:</label>";
  s += "<input type=\"text\" class=\"form-control\" name=\"select_pair\" id=\"par\" placeholder=\"";
  s += initial_pair;
  s += "\">";
  s += "</div><span class=\"help-block\">";
  s += "<a href=\"https://www.bitstamp.net/api/#ticker\" target=\"_blank\">Ver pares disponibles en Bitstamp</span>";
  s += "<button type=\"submit\">Guardar</button>";
  s += "</form></div>";
  s += errors;
  s += "</body></html>";
  server.send(200, "text/html", s);
}

void handle_select_pair() {
  initial_pair = server.arg("select_pair");
  initial_pair.toLowerCase();
  write_word(initial_pair, 0);
  String s = "<!DOCTYPE html><html lang=\"es\"><head><title>Bitcoin Ticker</title><meta charset=\"utf-8\">";
  s += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  s += "<meta http-equiv=\"refresh\" content=\"3; URL=/\">";
  s += "<link rel=\"stylesheet\" href=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.4.1/css/bootstrap.min.css\">";
  s += "</head><body><div class=\"container\"><h2>Bitcoin Ticker</h2>";
  s += "<p>Listo!</p>";
  s += "<p><a href=\"/\">Volver</p>";
  s += "</body></html>";
  server.send(200, "text/html", s);
  fetch_xrate();
}

bool firstRun() {
  char eeprom_special_char = char(EEPROM.read(special_char));
  if (eeprom_special_char == special_char) {
    Serial.println("First run: False");
    return false;
  }
  Serial.print("First run: True");
  return true;
}

void reset_eeprom() {
  for (int i = 0; i < EEPROM.length(); ++i) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
}

void write_word(String word, int word_offset) {
  delay(10);

  for (int i = word_offset; i < word.length(); ++i) {
    EEPROM.write(i, word[i]);
  }

  EEPROM.write(word.length(), '\0');
  EEPROM.write(special_char, special_char);
  EEPROM.commit();
}

String read_word(int word_offset) {
  String word;
  char read_char;
  int i = word_offset;

  while (read_char != '\0') {
    read_char = char(EEPROM.read(i));
    delay(10);
    i++;

    if (read_char != '\0') {
      word += read_char;
    }
  }
  Serial.print("offset last position: ");
  Serial.println(i);
  return word;
}

void loop() {
  server.handleClient();
  unsigned long current_millis = millis();
  if (current_millis - prev_millis >= check_interval) {
    prev_millis = current_millis;
    fetch_xrate();
  }
}

WiFiClientSecure client;

void fetch_xrate() {
  client.setInsecure();
  client.connect(bitstamp_host, 443);
  client.println("GET " + bitstamp_uri + initial_pair + "/ HTTP/1.0");
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
    errors += "Failed to fetch bitstamp data</br>";
  }

  DynamicJsonDocument doc(1024);

  DeserializationError error = deserializeJson(doc, content);
  if (error) {
    display.clearDisplay();
    display.setTextSize(3);
    display.getTextBounds("ERROR", 0, 0, &x, &y, &text_width, &text_height);
    display.setCursor(display.width() / 2 - text_width / 2, display.height() / 2 - text_height / 2);
    display.println("ERROR");
    display.display();
    Serial.print("Couldn't deserialize json: ");
    Serial.println(error.f_str());
    errors += "Couldn't deserialize json: ";
    errors += error.f_str();
    errors += "</br>";
    client.stop();
    return;
  }

  int rate_int = doc["last"]; // This is float but I want int
  Serial.print("Exchange rate: ");
  Serial.println(rate_int);

  display.clearDisplay();
  display.setTextSize(3);
  display.getTextBounds(String(rate_int), 0, 0, &x, &y, &text_width, &text_height);
  display.setCursor(display.width() / 2 - text_width / 2, display.height() / 2 - text_height / 2);
  display.println(rate_int);
  display.display();
}
