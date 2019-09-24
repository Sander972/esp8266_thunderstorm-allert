//#define MQTT_MAX_PACKET_SIZE 512
// https://github.com/koenieee/PushBullet-ESP8266

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <NTPClient.h>
#include <PubSubClient.h>
#include <Ticker.h>
#include <WiFiClientSecureBearSSL.h>
#include <WiFiUdp.h>

#include "secret.h"

/*
#########content of "secret.h"##########
#define MQTT "broker.com"
#define DEVICE "device-num" // name of the device
#define TELEMETRY "topic/topic"
const char*  ssid_palazzetti = "************";
const char*  pws_palazzetti = "************";;
const char*  ssid_casa = "************";
const char*  pws_casa = "************";
const char*  ssid_hotspot = "************";
const char*  pws_hotspot = "************";
const char*  location_palazzetti = "************";
const char*  location_casa = "************";
String apiUrl = "************";
String state = ",us";
String apiKey = "&appid=************";
String bulletUrl = "api.pushbullet.com";
String token = "************";
const char* fingerprint = "************";
const char* awsenroll_root_ca = "************";
*/

ESP8266WiFiMulti wifiMulti;
WiFiClient espClient;
PubSubClient client(espClient);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
Ticker checker;

int unit = 60; // 1 = 1s        time of update
int count = 0;

bool flag = 0;
String location = "unknown";
String weather = "unknown";
int weatherId = 800;

int first = 0;
String line;
bool dismissed = true;

void flags() { flag = !flag; }

int scanWifi() {
  Serial.println("Scanning WiFis");

  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0) {
    Serial.println("no networks found");
  } else {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i) {
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*");
      delay(10);
    }
  }
  Serial.println("");
  if (n == -1) {
    Serial.println("problem with wifi interfaces, rebooting...");
    delay(100);
    ESP.restart();
  }
  return n;
}

void setup_wifi() {
  // WiFi.mode(WIFI_OFF);
  WiFi.mode(WIFI_STA);
  WiFi.setOutputPower(0);
  wifiMulti.addAP(ssid_hotspot, pws_hotspot);
  wifiMulti.addAP(ssid_casa, pws_casa);
  wifiMulti.addAP(ssid_palazzetti, pws_palazzetti);
}

void checkIp(String ssid, String ip) {
  if (ssid.equals(ssid_palazzetti)) {
    String ok = "192.168.20";
    String subnet = ip.substring(0, 10);
    if (!ok.equals(subnet)) {
      Serial.println("Wrong subnet, retrying to connect");
      delay(500);
      connect_wifi();
    } else {
      Serial.println("subnet ok!");
    }
  }
}

void connect_wifi() {
  if (scanWifi() > 0) {
    delay(100);
    Serial.println("Connecting Wifi...");

    if (wifiMulti.run() == WL_CONNECTED) {
      Serial.println("");
      Serial.print("Connected to: ");
      Serial.println(WiFi.SSID());
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      checkIp(WiFi.SSID(), WiFi.localIP().toString());
      String msg = "Mi sono connesso al wifi: " + WiFi.SSID();
      notify("New conn", msg);
    }
  }
  delay(100);
}

String hello() {
  String hello = "connected to: " + WiFi.SSID() + "\n" +
                 "ip: " + WiFi.localIP().toString() + "\n" +
                 "succesfully connected!";
  return hello;
}

void connectMQTT() {
  Serial.println("estabilishing mqtt connection");
  if (client.connect(DEVICE)) {
    Serial.println("mqtt connected");
    client.subscribe(TELEMETRY);
  } else {
    Serial.print(". ");
    delay(100);
  }
}

void publishMQTT(String msg) {
  String time = timeClient.getFormattedTime();
  String message = "current time: " + time + "\n" + msg;
  if (client.publish(TELEMETRY, String(message).c_str()) == 1) {
    Serial.println("############mqtt message sent#########\n" + message);
    Serial.println("######################################");
  }
}

void updateMQTT() {
  String str =
      "current location: " + location + "\n" + "current weather: " + weather;
  publishMQTT(str);
  Serial.println("MQTT update sent!");
}

void updateLocation() {

  String ssid = WiFi.SSID().c_str();
  if (ssid == ssid_palazzetti) {
    location = location_palazzetti;
  } else if (ssid == ssid_casa) {
    location = location_casa;
  }
}

void checkNotifyWeather() {
  if (dismissed) {
    Serial.println("i can notify");
    String title;
    String message;

    if (weatherId >= 200 && weatherId <= 622) {
      title = "RUN!";
      message = "Corri a chiudere i finestrini che sta piovento!";
      notify(title, message);

    } else if (weatherId >= 801 && weatherId <= 804) {
      title = "OCCHIO";
      message = "Guarda che sta diventando nuvoloso, tra poco piove!";
      notify(title, message);

    } else if (weatherId >= 701 && weatherId <= 800) {
      title = "TUTTO TRANQUILLO";
      message = "Nessun problema in vista, il cielo è pulito";
      Serial.print("the weather is: ");
      Serial.print(weather);
      Serial.println(" , so no need to notify the user");
      // notify(title, message);      //no need to notify
    }

    // notify(title, message);
  }
}

void checkNotify() {

  std::unique_ptr<BearSSL::WiFiClientSecure> HttpClient(
      new BearSSL::WiFiClientSecure);
  HttpClient->setFingerprint(fingerprint);
  HTTPClient* http = new HTTPClient();

  http->begin(*HttpClient, bulletUrl); // Specify the URL and certificate
  http->addHeader("Content-Type",
                  "application/json"); // Specify content-type header
  http->addHeader("Access-Token", token);
  http->addHeader("User-Agent",
                  "ESP8266_sander");      // Specify content-type header
  http->addHeader("Connection", "close"); // Specify content-type header

  int httpCode = http->GET();
  if (httpCode == 200) {
    // Serial.println("Notify = ok");
    String payload = http->getString();
    // Serial.println(payload);
    DynamicJsonDocument doc(4096);
    deserializeJson(doc, payload);
    bool status = doc["pushes"][0]["dismissed"].as<bool>();
    String oldPush = doc["pushes"][0]["title"].as<String>();

    Serial.print("Pushes status: ");
    Serial.println(status);
    if (status) {
      dismissed = true;
    } else {
      dismissed = false;
    }
    if (oldPush == "New conn") {
      dismissed = true;
    }

  } else {
    Serial.printf("Error checking Notify, code: %d\n", httpCode);
  }
  http->end();
  delete http;
}

void notify(String title, String message) {

  std::unique_ptr<BearSSL::WiFiClientSecure> HttpClient(
      new BearSSL::WiFiClientSecure);
  HttpClient->setFingerprint(fingerprint);
  HTTPClient* http = new HTTPClient();

  http->begin(*HttpClient, bulletUrl); // Specify the URL and certificate
  http->addHeader("Content-Type",
                  "application/json"); // Specify content-type header
  http->addHeader("Access-Token", token);
  http->addHeader("User-Agent",
                  "ESP8266_sander");      // Specify content-type header
  http->addHeader("Connection", "close"); // Specify content-type header

  String req = "{\"body\":\"" + message + "\",\"title\":\"" + title +
               "\",\"type\":\"note\"}";
  // Serial.println(req);

  int httpCode = http->POST(req);
  if (httpCode == 200) {
    Serial.println("Notify = ok");
    dismissed = 0;
  } else {
    Serial.printf("Error posting Notify, code: %d\n", httpCode);
  }
  http->end();
  delete http;
}

void requestWeather() {

  HTTPClient* http = new HTTPClient();
  // Serial.println("beghin http req");
  String url = apiUrl + location + state + apiKey;
  // Serial.println(url);

  http->begin(url);

  int httpCode = http->GET();

  if (httpCode == 200) {

    String payload = http->getString();
    Serial.println("weather get ok");
    DynamicJsonDocument doc(4096);
    deserializeJson(doc, payload);
    weather = doc["weather"][0]["main"].as<String>();
    weatherId = doc["weather"][0]["id"].as<int>();

  } else {
    Serial.print("erro getting weather, err code: ");
    Serial.println(httpCode);
  }

  http->end(); // Close connection
  delete http;
  checkNotifyWeather();
}

void setup() {

  Serial.begin(115200);
  Serial.println("\n");
  delay(100);
  setup_wifi();
  connect_wifi();

  timeClient.begin();
  timeClient.setTimeOffset(7200); // set timezone = GMT+2
  timeClient.update();

  client.setServer(MQTT, 1883);
  // connectMQTT();

  checker.attach(unit, flags);
}

void loop() {

  if (!(wifiMulti.run() == WL_CONNECTED)) {
    flag = 0;
    connect_wifi();
    delay(500);
  } else if (!client.connected()) {
    first = 0;
    connectMQTT();
  }

  client.loop();

  timeClient.update();

  if (flag && wifiMulti.run() == WL_CONNECTED) {
    if (first == 0) {
      publishMQTT(hello());
      requestWeather();
      // notify("New conn", hello());
      first = 1;
    } else {
      updateLocation(); // based on wifi ssid  -   every 1 min
      updateMQTT();     // send variables to mqtt  -   every 1min
      checkNotify();    // every 1 min

      if (count >= 5) {
        requestWeather(); // no more than 60 per min   -   every 5min
        count = 0;
      }
      count++;
      flag = 0;
    }
  }

  delay(100);
}
