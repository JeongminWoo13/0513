#include <WiFi.h>
#include <ESPmDNS.h>
#include <ConfigPortal32.h>
#include <TFT_eSPI.h>
#include <QRcode_eSPI.h>
#include <WiFiClient.h>
#include <HTTPClient.h>

TFT_eSPI tft = TFT_eSPI();
QRcode_eSPI qrcode(&tft);

char* ssid_pfix = (char*)"ccc";
String user_config_html = "<p><input type='text' name='meta.relayIP' placeholder='RelayWeb Address'>";
char relayIP[50];

#define PUSH_BUTTON 43
volatile bool buttonPressed = false;
volatile long lastPressed = 0;

WiFiClient client;
char toggleURL[200];

IRAM_ATTR void pressed() {
  if (millis() > lastPressed + 200) {
    lastPressed = millis();
    buttonPressed = true;
  }
}

void drawQRCode(const String& ssid_str) {
  tft.fillScreen(TFT_WHITE);
  tft.setTextColor(TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("Scan to Setup");

  delay(300);
  String qrData = "WIFI:S:" + ssid_str + ";T:nopass;;";
  qrcode.init();
  qrcode.create(qrData.c_str());
}

void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(1);

  String macHex = String((uint32_t)ESP.getEfuseMac(), HEX);
  macHex.toUpperCase();
  String ssid_str = String(ssid_pfix) + "_" + macHex;

  WiFi.disconnect(true, true);
  delay(200);
  WiFi.mode(WIFI_OFF);
  delay(200);
  WiFi.mode(WIFI_AP);

  IPAddress apIP(192, 168, 4, 1);
  IPAddress gw(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(apIP, gw, subnet);
  WiFi.softAP(ssid_str.c_str());

  Serial.println("SoftAP started");
  Serial.println(ssid_str);
  Serial.println(WiFi.softAPIP());

  delay(300);
  drawQRCode(ssid_str);

  loadConfig();
  if (!cfg.containsKey("config") || strcmp((const char*)cfg["config"], "done")) {
    configDevice();
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin((const char*)cfg["ssid"], (const char*)cfg["w_pw"]);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("WiFi connected");
  Serial.println(WiFi.localIP());

  if (cfg.containsKey("meta") && cfg["meta"].containsKey("relayIP")) {
    sprintf(relayIP, (const char*)cfg["meta"]["relayIP"]);
    sprintf(toggleURL, "http://%s/toggle", relayIP);
    Serial.println(relayIP);
  }

  pinMode(PUSH_BUTTON, INPUT_PULLUP);
  attachInterrupt(PUSH_BUTTON, pressed, FALLING);

  MDNS.begin("ccc");
}

void loop() {
  if (buttonPressed) {
    buttonPressed = false;

    HTTPClient http;
    if (http.begin(client, toggleURL)) {
      int httpCode = http.GET();
      if (httpCode > 0) {
        String payload = http.getString();
        Serial.println(payload);
      } else {
        Serial.printf("HTTP GET failed, code: %d\n", httpCode);
      }
      http.end();
    }
  }
}
