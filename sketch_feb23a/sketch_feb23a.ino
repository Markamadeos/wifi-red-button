#include <FS.h>
#include <LittleFS.h>
#include <WiFiManager.h>  //https://github.com/tzapu/WiFiManager
#include <HTTPClient.h>
#include <WiFi.h>

#define BUTTON_PIN 35
#define LED_PIN 15
#define FORMAT_FS_NEEDED true
#define AP_SSID "wifi-red-button"
#define AP_PASSWORD "P@ssw0rd"
#define HTTP_PREFIX "http://"
#define HTTP_POSTFIX "/rest/system/"
#define GATEWAY_WM_PARAM_ID "gateway"
#define LOGIN_WM_PARAM_ID "login"
#define PASSWORD_WM_PARAM_ID "pass"
#define COMMAND_WM_PARAM_ID "command"
#define COMMAND_OPTION_WM_PARAM_ID "commandOption"
#define CONFIG_FILE_PATH "/config.txt"

typedef struct {
  char routerLogin[20] = "";
  char routerPass[20] = "";
  char routerGateway[20] = "";
  char apiCommand[20] = "";
  char apiCommandOption[20] = "";
} parameters;

parameters params;
int lastState = LOW;
int currentState;
bool shouldSaveConfig = false;

WiFiManagerParameter gatewayIp(GATEWAY_WM_PARAM_ID, "gateway IP", params.routerGateway, 20);
WiFiManagerParameter apiLogin(LOGIN_WM_PARAM_ID, "api login", params.routerLogin, 20);
WiFiManagerParameter apiPass(PASSWORD_WM_PARAM_ID, "api password", params.routerPass, 20);
WiFiManagerParameter apiCommand(COMMAND_WM_PARAM_ID, "command (http://{gateway}/rest/system/{command})", params.apiCommand, 20);
WiFiManagerParameter apiCommandOption(COMMAND_OPTION_WM_PARAM_ID, "ex. force = yes", params.apiCommandOption, 20);

void saveConfigCallback() {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void saveConfig(fs::FS &fs) {
  Serial.printf("Writing file: %s\r\n", CONFIG_FILE_PATH);

  File file = fs.open(CONFIG_FILE_PATH, FILE_WRITE);
  if (!file) {
    Serial.println("- failed to open file for writing");
    return;
  }
  if (file.println(params.routerLogin)
      || file.println(params.routerPass)
      || file.println(params.routerGateway)
      || file.println(params.apiCommand)
      || file.println(params.apiCommandOption)) {
    Serial.println("- file written");
  } else {
    Serial.println("- write failed");
  }
  file.close();
}

void readConfig(fs::FS &fs) {
  Serial.printf("Reading file: %s\r\n", CONFIG_FILE_PATH);

  File file = fs.open(CONFIG_FILE_PATH);
  if (!file || file.isDirectory()) {
    Serial.println("- failed to open file for reading");
    return;
  }

  Serial.println("- read from file:");
  while (file.available()) {
    Serial.println(file.readStringUntil('\n'));
  }
  file.close();
}

void resetEspSettings() {
  digitalWrite(LED_PIN, HIGH);
  delay(2000);
  digitalWrite(LED_PIN, LOW);
  if (!LittleFS.begin(FORMAT_FS_NEEDED)) {
    Serial.println("LittleFS Mount Failed");
  }
}

void sendCommand() {
  WiFiClient client;
  HTTPClient http;
  // setup URL
  String url = HTTP_PREFIX + String(params.routerGateway) + HTTP_POSTFIX + String(params.apiCommand);
  Serial.println(url);
  // Your Domain name with URL path or IP address with path
  http.begin(client, url);
  // If you need Node-RED/server authentication, insert user and password below
  http.setAuthorization(params.routerLogin, params.routerPass);
  http.setAuthorizationType("Basic");
  // If you need an HTTP request with a content type: application/json, use the following:
  http.addHeader("Content-Type", "application/json");
  int httpResponseCode = http.POST("{}");
  Serial.print("HTTP Response code: ");
  Serial.println(httpResponseCode);
  Serial.println(url);
  // Free resources
  http.end();
}

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  readConfig(LittleFS);
  WiFiManager wm;
  currentState = digitalRead(BUTTON_PIN);
  if (currentState == HIGH) {
    resetEspSettings();
    wm.resetSettings();
    ESP.restart();
    delay(5000);
  }
  Serial.begin(115200);
  while (!Serial) {
    ;  // wait for serial port to connect
  }

  // WiFi.mode(WIFI_STA);
  //wm.setSaveConfigCallback(saveConfigCallback);
  wm.addParameter(&gatewayIp);
  wm.addParameter(&apiLogin);
  wm.addParameter(&apiPass);
  wm.addParameter(&apiCommand);
  wm.addParameter(&apiCommandOption);
  wm.setSaveConfigCallback(saveConfigCallback);

  if (!wm.autoConnect(AP_SSID, AP_PASSWORD)) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    ESP.restart();
    delay(5000);
  }

  Serial.println("connected...yeey :)");
  strcpy(params.routerLogin, apiLogin.getValue());
  strcpy(params.routerPass, apiPass.getValue());
  strcpy(params.routerGateway, gatewayIp.getValue());
  strcpy(params.apiCommand, apiCommand.getValue());
  strcpy(params.apiCommandOption, apiCommandOption.getValue());
  Serial.println(params.apiCommand);
  saveConfig(LittleFS);
  Serial.println("local ip");
  Serial.println(WiFi.localIP());
}

void loop() {
  currentState = digitalRead(BUTTON_PIN);
  if (currentState == HIGH) {
    if (WiFi.status() == WL_CONNECTED) {
      sendCommand();
      delay(3000);
    }
  }
}