#include <FS.h>           //this needs to be first, or it all crashes and burns...
#include <WiFiManager.h>  //https://github.com/tzapu/WiFiManager

#ifdef ESP32
#include <SPIFFS.h>
#endif

#include <ArduinoJson.h>  //https://github.com/bblanchon/ArduinoJson
#include <HTTPClient.h>
#include <WiFi.h>

//define your default values here, if there are different values in config.json, they are overwritten.
char login[10] = "admin";
char pass[16] = "";
char gateway[20] = "";
char command[40] = "";
const int buttonPin = 35;
const int ledPin = 15;
int lastState = LOW;
int currentState;
String httpPrefix = "http://";
String httpPostfix = "/rest/system/";
char ssid[40] = "wifi-red-button";
char password[40] = "P@ssw0rd";

//flag for saving data
bool shouldSaveConfig = false;

//WiFiManager
WiFiManager wifiManager;
WiFiManagerParameter gatewayIp("gateway", "gateway IP", gateway, 20);
WiFiManagerParameter apiLogin("login", "api login", login, 10);
WiFiManagerParameter apiPass("pass", "api password", pass, 16);
WiFiManagerParameter apiCommand("command", "command (http://{gateway}/rest/system/{command})", command, 40);

//callback notifying us of the need to save config
void saveConfigCallback() {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void resetEspSettings() {
  digitalWrite(ledPin, HIGH);
  delay(2000);
  digitalWrite(ledPin, LOW);
  SPIFFS.format();
  wifiManager.resetSettings();
  ESP.restart();
}

void readConfigFromJson() {
  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);
        configFile.readBytes(buf.get(), size);
#if defined(ARDUINOJSON_VERSION_MAJOR) && ARDUINOJSON_VERSION_MAJOR >= 6
        DynamicJsonDocument json(1024);
        auto deserializeError = deserializeJson(json, buf.get());
        serializeJson(json, Serial);
        if (!deserializeError) {
#else
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
#endif
          Serial.println("\nparsed json");
          strcpy(gateway, json["gateway"]);
          strcpy(login, json["login"]);
          strcpy(pass, json["pass"]);
          strcpy(command, json["command"]);
        } else {
          Serial.println("failed to load json config");
        }
        configFile.close();
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
}

void saveConfigToJson() {
  Serial.println("saving config");
#if defined(ARDUINOJSON_VERSION_MAJOR) && ARDUINOJSON_VERSION_MAJOR >= 6
  DynamicJsonDocument json(1024);
#else
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
#endif
  json["login"] = login;
  json["pass"] = pass;
  json["gateway"] = gateway;
  json["command"] = command;

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("failed to open config file for writing");
  }

#if defined(ARDUINOJSON_VERSION_MAJOR) && ARDUINOJSON_VERSION_MAJOR >= 6
  serializeJson(json, Serial);
  serializeJson(json, configFile);
#else
  json.printTo(Serial);
  json.printTo(configFile);
#endif
  configFile.close();
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);
  currentState = digitalRead(buttonPin);
  readConfigFromJson();
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.addParameter(&gatewayIp);
  wifiManager.addParameter(&apiLogin);
  wifiManager.addParameter(&apiPass);
  wifiManager.addParameter(&apiCommand);
  if (currentState == HIGH) {
    resetEspSettings();
  }
  if (!wifiManager.autoConnect(ssid, password)) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.restart();
    delay(5000);
  }
  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");

  strcpy(login, apiLogin.getValue());
  strcpy(pass, apiPass.getValue());
  strcpy(gateway, gatewayIp.getValue());
  strcpy(command, apiCommand.getValue());
  saveConfigToJson();
  Serial.println("local ip");
  Serial.println(WiFi.localIP());
}

void loop() {
  currentState = digitalRead(buttonPin);
  if (currentState == HIGH) {
    //Check WiFi connection status
    if (WiFi.status() == WL_CONNECTED) {
      WiFiClient client;
      HTTPClient http;
      // setup URL
      String url = httpPrefix + gateway + httpPostfix + command;
      Serial.println(url);
      // Your Domain name with URL path or IP address with path
      http.begin(client, url);
      // If you need Node-RED/server authentication, insert user and password below
      http.setAuthorization(login, pass);
      http.setAuthorizationType("Basic");
      // If you need an HTTP request with a content type: application/json, use the following:
      http.addHeader("Content-Type", "application/json");
      int httpResponseCode = http.POST("{}");
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      Serial.println(url);
      // Free resources
      http.end();
    } else {
      Serial.println("WiFi Disconnected");
    }
  }
}