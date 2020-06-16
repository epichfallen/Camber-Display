/**
    Author: Cem Aydın & Dünya Cengiz
   (c) Copyright by Mosa Technologies.
**/

#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <EEPROM.h>
#include <AsyncMqttClient.h>
#include <time.h>
#include <GD23Z.h>
#include "default_assets.h"


//Variables
int i = 0;
int statusCode;
const char* ssid = "text";
const char* passphrase = "text";
String st;
String content;


//Function Decalration
bool testWifi(void);
void launchWeb(void);
void setupAP(void);

//Establishing Local server at port 80 whenever required
WebServer server(80);


//#define WIFI_SSID "Camber"
//#define WIFI_PASSWORD "12345678"
#define MQTT_HOST IPAddress(10, 3, 141, 1)
#define MQTT_PORT 1883

#define INPUTCOUNT 7 //COUNT OF SENSOR INPUTS

const char* ntpServer = "10.3.141.1";    //ntp server address
const long  gmtOffset_sec = 3 * 60 * 60; //gmt setting
const int   daylightOffset_sec = 0;      //daylight savings?

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
}

char sensordata[INPUTCOUNT][10];  //2d Char array to temporarily store sensor data
char* config_data[5];  //Char array to store config payload
int display_mode, i1, i2, i3, i4; //Configuration elements
char* tags[INPUTCOUNT] = {"TWS", "TWA", "AWS", "AWA", "BS", "HEEL", "DPTH"};
bool tws, twa, aws, awa, bs, heel, dpth = 0; // Bool for configuration; if requested by config => 1 else =>0
bool conf_s, tws_s, twa_s, aws_s, awa_s, bs_s, heel_s, dpth_s; //Subscription state for mqtt channel; subscribed => 1 not-subscribed=>0

char starttime_str[15];
int starttime;
char timenow[50]; //time
TaskHandle_t TaskHandle_1;

AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;

String chipId, ownconfig, ownstate, ownreset;


//================ Functions Begin ==================


//void connectToWifi() {
//  Serial.println("Connecting to Wi-Fi...");
//  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
//}

void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}

void WiFiEvent(WiFiEvent_t event) {
  Serial.printf("[WiFi-event] event: %d\n", event);
  switch (event) {
    case SYSTEM_EVENT_STA_GOT_IP:
      Serial.println("WiFi connected");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());
      connectToMqtt();
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      Serial.println("WiFi lost connection");
      xTimerStop(mqttReconnectTimer, 0); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
      xTimerStart(wifiReconnectTimer, 0);
      break;
  }
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);
  mqttClient.publish(ownstate.c_str(), 0, 1, "Online", strlen("Online"));

  uint16_t config_packet = mqttClient.subscribe(ownconfig.c_str(), 0);
  uint16_t reset_packet = mqttClient.subscribe(ownreset.c_str(), 0);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("Disconnected from MQTT.");

  if (WiFi.isConnected()) {
    xTimerStart(mqttReconnectTimer, 0);
  }
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos) {
  Serial.println("Subscribed successfully.");
  Serial.print("  qos: ");
  Serial.println(qos);
}

void onMqttUnsubscribe(uint16_t packetId) {
  Serial.println("Unsubscribed successfully.");
}

void onMqttPublish(uint16_t packetId) {
  Serial.println("Published successfully.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {

  if (strcmp(topic, ownconfig.c_str()) == 0) {
    Serial.println("Received Config Payload");
    sep_by_comma(payload, config_data);
    i1 = *config_data[1] - '0';
    i2 = *config_data[2] - '0';
    i3 = *config_data[3] - '0';
    i4 = *config_data[4] - '0';
    display_mode = (int) * config_data[0] - '0';

    // ======= TWS =======
    if (i1 == 0 || i2 == 0 || i3 == 0 || i4 == 0) { // check if tws was requested in configuration
      if (tws_s == 0) {
        if (uint16_t tws_packet = mqttClient.subscribe("wind/tws", 0) != 0) {
          Serial.println("Subscribing to TWS");
          tws_s = 1;
        }
      }
    }
    else if (tws_s == 1) {
      if (mqttClient.unsubscribe("wind/tws") != 0) {
        Serial.println("Unsubscribing from TWS");
        tws_s = 0;
      }
    }
    // ======= TWA =======
    if (i1 == 1 || i2 == 1 || i3 == 1 || i4 == 1) {
      if (twa_s == 0) {
        if (uint16_t twa_packet = mqttClient.subscribe("wind/twa", 0) != 0) {
          Serial.println("Subscribing to TWA");
          twa_s = 1;
        }
      }
    }
    else if (twa_s == 1) {
      if (mqttClient.unsubscribe("wind/twa") != 0) {
        Serial.println("Unsubscribing from TWA");
        twa_s = 0;
      }
    }
    // ======= AWS =======
    if (i1 == 2 || i2 == 2 || i3 == 2 || i4 == 2) {
      if (aws_s == 0) {
        if (uint16_t aws_packet = mqttClient.subscribe("wind/aws", 0) != 0) {
          Serial.println("Subscribing to AWS");
          aws_s = 1;
        }
      }
    }
    else if (aws_s == 1) {
      if (mqttClient.unsubscribe("wind/aws") != 0) {
        Serial.println("Unsubscribing from AWS");
        aws_s = 0;
      }
    }
    // ======= AWA =======
    if (i1 == 3 || i2 == 3 || i3 == 3 || i4 == 3) {
      if (awa_s == 0) {
        if (uint16_t awa_packet = mqttClient.subscribe("wind/awa", 0) != 0) {
          Serial.println("Subscribing to AWA");
          awa_s = 1;
        }
      }
    }
    else if (awa_s == 1) {
      if (mqttClient.unsubscribe("wind/awa") != 0 ) {
        Serial.println("Unsubscribing from AWA");
        awa_s = 0;
      }
    }
    // ======= BS =======
    if (i1 == 4 || i2 == 4 || i3 == 4 || i4 == 4) {
      if (bs_s == 0) {
        if (uint16_t bs_packet = mqttClient.subscribe("boat/speed", 0) != 0) {
          Serial.println("Subscribing to BS");
          bs_s = 1;
        }
      }
    }
    else if (bs_s == 1) {
      if (mqttClient.unsubscribe("boat/speed") != 0) {
        Serial.println("Unsubscribing from BS");
        bs_s = 0;
      }
    }
    // ======= HEEL =======
    if (i1 == 5 || i2 == 5 || i3 == 5 || i4 == 5) {
      if (heel_s == 0) {
        if (uint16_t heel_packet = mqttClient.subscribe("boat/heel", 0) != 0) {
          Serial.println("Subscribing to HEEL");
          heel_s = 1;
        }
      }
    }
    else if (heel_s == 1) {
      if (mqttClient.unsubscribe("boat/heel") != 0) {
        Serial.println("Unsubscribing from HEEL");
        heel_s = 0;
      }
    }
    // ======= DPTH =======
    if (i1 == 6 || i2 == 6 || i3 == 6 || i4 == 6) {
      if (dpth_s == 0) {
        if (uint16_t dpth_packet = mqttClient.subscribe("boat/depth", 0) != 0) {
          Serial.println("Subscribing to DPTH");
          dpth_s = 1;
        }
      }
    }
    else if (dpth_s == 1) {
      if (mqttClient.unsubscribe("boat/depth") != 0) {
        Serial.println("Unsubscribing from DPTH");
        dpth_s = 0;
      }
    }
  }


  //  if (strcmp(topic, "race/starttime") == 0) {
  //    for (int i = 0; i < length; i++) {
  //      starttime_str[i] = (char)payload[i];
  //    }
  //    starttime_str[length] = '\0';
  //    starttime = atoi(starttime_str);
  //    vTaskResume(TaskHandle_1);
  //  }



  if (strcmp(topic, ownreset.c_str()) == 0) { //reset wifi credentials
    for (int i = 0; i < 96; ++i) {
      EEPROM.write(i, 0);
    }
    EEPROM.commit();
    ESP.restart();
  }

  if (strcmp(topic, "wind/tws") == 0) { //get tws data
    for (int i = 0; i < len; i++) {
      sensordata[0][i] = (char)payload[i];
    }
    sensordata[0][len] = '\0';
  }
  if (strcmp(topic, "wind/twa") == 0) { //get twa data
    for (int i = 0; i < len; i++) {
      sensordata[1][i] = (char)payload[i];
    }
    sensordata[1][len] = '\0';
  }
  if (strcmp(topic, "wind/aws") == 0) { //get aws data
    for (int i = 0; i < len; i++) {
      sensordata[2][i] = (char)payload[i];
    }
    sensordata[2][len] = '\0';
  }
  if (strcmp(topic, "wind/awa") == 0) { //get awa data
    for (int i = 0; i < len; i++) {
      sensordata[3][i] = (char)payload[i];
    }
    sensordata[3][len] = '\0';
  }
  if (strcmp(topic, "boat/speed") == 0) {//get boat speed
    for (int i = 0; i < len; i++) {
      sensordata[4][i] = (char)payload[i];
    }
    sensordata[4][len] = '\0';
  }
  if (strcmp(topic, "boat/heel") == 0) {//get heel
    for (int i = 0; i < len; i++) {
      sensordata[5][i] = (char)payload[i];
    }
    sensordata[5][len] = '\0';
  }
  if (strcmp(topic, "boat/depth") == 0) {//get depth
    for (int i = 0; i < len; i++) {
      sensordata[6][i] = (char)payload[i];
    }
    sensordata[6][len] = '\0';
  }

  GD.ClearColorRGB(0x000000);
  GD.Clear();

  GD.Begin(LINES);
  GD.Vertex2f(16 * 400, 0);
  GD.Vertex2f(16 * 400, 16 * 480);
  GD.End();

  GD.Begin(LINES);
  GD.Vertex2f(0, 16 * 240);
  GD.Vertex2f(16 * 800, 16 * 240);
  GD.End();

  GD.Begin(RECTS);
  GD.ColorRGB(0xFF0000);
  GD.Vertex2f(0, 0);
  GD.Vertex2f(250, 3801);
  GD.End();

  GD.Begin(RECTS);
  GD.ColorRGB(0x0000FF);
  GD.Vertex2f(0, 3878);
  GD.Vertex2f(250, 12800);
  GD.End();

  GD.Begin(RECTS);
  GD.ColorRGB(0x00FF00);
  GD.Vertex2f(12510, 3878);
  GD.Vertex2f(12800, 12800);
  GD.End();

  GD.Begin(RECTS);
  GD.ColorRGB(0xFF00FF);
  GD.Vertex2f(12510, 0);
  GD.Vertex2f(12800, 3801);
  GD.End();

  GD.ColorRGB(0xFFFFFF);

  switch (display_mode) {
    case 1:// 1 data displayed on screen
      GD.cmd_text(GD.w / 2, 5, 23, OPT_CENTER | OPT_RIGHTX, timenow);
      display_data(i1, 0);
      GD.cmd_text(380, 36, BEBAS2_HANDLE, OPT_CENTERY | OPT_RIGHTX, tags[i1]);//top left
      GD.swap();
      break;

    case 2:// 2 data displayed on screen
      GD.cmd_text(GD.w / 2, 5, 23, OPT_CENTER | OPT_RIGHTX, timenow);

      display_data(i1, 0);
      display_data(i2, 1);
      GD.cmd_text(380, 36, BEBAS2_HANDLE, OPT_CENTERY | OPT_RIGHTX, tags[i1]);//top left
      GD.cmd_text(770, 36, BEBAS2_HANDLE, OPT_CENTERY | OPT_RIGHTX, tags[i2]);//top right
      GD.swap();

      break;
    case 4:// 4 data displayed on screen

      GD.cmd_text(GD.w / 2, 5, 23, OPT_CENTER | OPT_RIGHTX, timenow);

      display_data(i1, 0);
      display_data(i2, 1);
      display_data(i3, 2);
      display_data(i4, 3);

      GD.cmd_text(380, 36, BEBAS2_HANDLE, OPT_CENTERY | OPT_RIGHTX, tags[i1]);//top left
      GD.cmd_text(770, 36, BEBAS2_HANDLE, OPT_CENTERY | OPT_RIGHTX, tags[i2]);//top right
      GD.cmd_text(380, 270, BEBAS2_HANDLE, OPT_CENTERY | OPT_RIGHTX, tags[i3]);//bottom left
      GD.cmd_text(770, 270, BEBAS2_HANDLE, OPT_CENTERY | OPT_RIGHTX, tags[i4]);//bottom right
      GD.swap();

      break;
    case 5:// 4 data displayed on screen

      break;
  }
  //    Serial.println(ESP.getFreeHeap());
}

void display_data(int id, int loc) {

  if (loc == 0) {
    GD.cmd_text(50, 140, BEBAS_HANDLE, OPT_CENTERY, sensordata[id]); //upper left
  }
  if (loc == 1) {
    GD.cmd_text(450, 140, BEBAS_HANDLE, OPT_CENTERY, sensordata[id]); // upper right
  }
  if (loc == 2) {
    GD.cmd_text(50, 380, BEBAS_HANDLE, OPT_CENTERY, sensordata[id]); //lower left
  }
  if (loc == 3) {
    GD.cmd_text(450, 380, BEBAS_HANDLE, OPT_CENTERY, sensordata[id]); //lower right
  }
  if (loc == 4) {
    GD.cmd_text(GD.w / 2, GD.h / 2, BEBAS_HANDLE, OPT_CENTERY, sensordata[id]); //center
  }
}


//c type string manipulation with fixed construct type
void sep_by_comma(char main_string[], char* array_of_pointers[7]) {
  const char seperator[] = ",";
  char* to_array = array_of_pointers[0];
  char* token = strtok(main_string, seperator);
  int i = 0;
  while (token != NULL) {
    array_of_pointers[i] = token;
    token = strtok(NULL, seperator);
    i += 1;
  }
}


void printLocalTime() {
  struct tm timeinfo;
  strftime(timenow, 50, "%x - %X", &timeinfo);
  Serial.println(timenow);
}


void setup() {
  GD.begin();
  LOAD_ASSETS();
  logo_debug("Loading Resources");
  
  Serial.begin(115200);
  WiFi.disconnect();
  EEPROM.begin(512); //Initialasing EEPROM
  delay(10);
  pinMode(LED_BUILTIN, OUTPUT);

  chipId = String((uint32_t)ESP.getEfuseMac(), HEX);
  ownconfig = String("display/" + chipId + "/config");
  ownstate = String("display/" + chipId + "/state");
  ownreset = String("display/" + chipId + "/reset");

  //---------------------------------------- Read eeprom for ssid and pass
  Serial.println("Reading EEPROM ssid");

  String esid;
  for (int i = 0; i < 32; ++i)
  {
    esid += char(EEPROM.read(i));
  }
  Serial.println();
  Serial.print("SSID: ");
  Serial.println(esid);
  Serial.println("Reading EEPROM pass");

  String epass = "";
  for (int i = 32; i < 96; ++i)
  {
    epass += char(EEPROM.read(i));
  }

  Serial.print("PASS: ");
  Serial.println(epass);


  WiFi.begin(esid.c_str(), epass.c_str());
  if (testWifi())
  {
    Serial.println("Succesfully Connected!!!");
    //  WiFi.onEvent(WiFiEvent);

    mqttClient.onConnect(onMqttConnect);
    mqttClient.onDisconnect(onMqttDisconnect);
    mqttClient.onSubscribe(onMqttSubscribe);
    mqttClient.onUnsubscribe(onMqttUnsubscribe);
    mqttClient.onMessage(onMqttMessage);
    mqttClient.onPublish(onMqttPublish);
    delay(2000);
    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    mqttClient.setWill(ownstate.c_str(), 0, 1, "Offline", strlen("Offline"));

    //  connectToWifi();

    connectToMqtt();

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    xTaskCreate(timeEngine, "TimeEngine", 10000, NULL, 1, &TaskHandle_1);
    return;
  }
  else
  {
    Serial.println("Turning the HotSpot On");
    launchWeb();
    setupAP();// Setup HotSpot
  }

  Serial.println();
  Serial.println("Waiting.");

  while ((WiFi.status() != WL_CONNECTED))
  {
    Serial.print(".");
    delay(100);
    server.handleClient();
  }
}


void loop() {
}

void logo_debug (char* texttoprint) {
  GD.ClearColorRGB(0x000015);
  GD.Clear();
  GD.cmd_text(GD.w / 2, 228, 31, OPT_CENTER, "CAMBER LOGO");
  GD.cmd_text(GD.w / 2, 290, 27, OPT_CENTER, texttoprint);
  GD.swap();
}

static void timeEngine(void* pvParameters)
{
  for (;;)
  {
    //    int now;
    //    int timeLeft;
    //    char minsecprint[6];
    //    memset(minsecprint, 0, sizeof minsecprint);
    //    char mintemp[3];
    //    char sectemp[3];
    //
    //    now = time(NULL); // get total seconds from Jan 1, 1970
    //    timeLeft = starttime - now; //get total seconds of the start from Jan 1, 1970
    //
    //    printLocalTime();
    //
    //    int minute = timeLeft / 60; //get minutes only
    //    int second = timeLeft % 60; //get seconds only
    //
    //    itoa(minute, mintemp, 10); // convert int's to str
    //    itoa(second, sectemp, 10);
    //
    //    if (minute >= 0 && minute <= 9) { //add zero infront if minute is only one digit
    //      char xhamster[3] = "0";
    //      strcat(xhamster, mintemp);
    //      mintemp[0] = xhamster[0];
    //      mintemp[1] = xhamster[1];
    //      mintemp[2] = xhamster[2];
    //    }
    //    if (second >= 0 && second <= 9) { //add zero infront if second is only one digit
    //      char xnxx[3] = "0";
    //      strcat(xnxx, sectemp);
    //      sectemp[0] = xnxx[0];
    //      sectemp[1] = xnxx[1];
    //      sectemp[2] = xnxx[2];
    //    }
    //
    //    strcat(minsecprint, mintemp);
    //    strcat(minsecprint, ":");
    //    strcat(minsecprint, sectemp);
    //
    //    Serial.println(now);
    //    Serial.println(starttime);
    //    Serial.println(minsecprint);
    //    Serial.println(timeLeft);

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    //    if (timeLeft == 0) {
    //      display_mode = 4;
    //      break;
    //    }
  }
  vTaskSuspend( NULL );
}


//----------------------------------------------- Fuctions used for WiFi credentials saving and connecting to it which you do not need to change
bool testWifi(void)
{
  int c = 0;
  Serial.println("Waiting for Wifi to connect");
  while ( c < 20 ) {
    if (WiFi.status() == WL_CONNECTED)
    {
      return true;
    }
    delay(500);
    Serial.print("*");
    c++;
  }
  Serial.println("");
  Serial.println("Connect timed out, opening AP");
  return false;
}

void launchWeb()
{
  Serial.println("");
  if (WiFi.status() == WL_CONNECTED)
    Serial.println("WiFi connected");
  Serial.print("Local IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("SoftAP IP: ");
  Serial.println(WiFi.softAPIP());
  createWebServer();
  // Start the server
  server.begin();
  Serial.println("Server started");
}

void setupAP(void)
{
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0)
    Serial.println("no networks found");
  else
  {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i)
    {
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == 7) ? " " : "*");
      delay(10);
    }
  }
  Serial.println("");
  st = "<ol>";
  for (int i = 0; i < n; ++i)
  {
    // Print SSID and RSSI for each network found
    st += "<li>";
    st += WiFi.SSID(i);
    st += " (";
    st += WiFi.RSSI(i);

    st += ")";
    st += (WiFi.encryptionType(i) == 7) ? " " : "*";
    st += "</li>";
  }
  st += "</ol>";
  delay(100);
  WiFi.softAP("Camber Display", "12345678");
  Serial.println("softap");
  launchWeb();
  Serial.println("over");
}

void createWebServer()
{
  {
    server.on("/", []() {

      IPAddress ip = WiFi.softAPIP();
      String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
      content = "<!DOCTYPE HTML>\r\n<html>Hello from ESP32 at ";
      content += "<form action=\"/scan\" method=\"POST\"><input type=\"submit\" value=\"scan\"></form>";
      content += ipStr;
      content += "<p>";
      content += st;
      content += "</p><form action='/setting' method='get'><label>SSID: </label><input name='ssid' length=32><input name='pass' length=64><input type='submit'></form>";
      content += "</html>";
      server.send(200, "text/html", content);
    });
    server.on("/scan", []() {
      //setupAP();
      IPAddress ip = WiFi.softAPIP();
      String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);

      content = "<!DOCTYPE HTML>\r\n<html>go back";
      server.send(200, "text/html", content);
    });

    server.on("/setting", []() {
      String qsid = server.arg("ssid");
      String qpass = server.arg("pass");
      if (qsid.length() > 0 && qpass.length() > 0) {
        Serial.println("clearing eeprom");
        for (int i = 0; i < 96; ++i) {
          EEPROM.write(i, 0);
        }
        Serial.println(qsid);
        Serial.println("");
        Serial.println(qpass);
        Serial.println("");

        Serial.println("writing eeprom ssid:");
        for (int i = 0; i < qsid.length(); ++i)
        {
          EEPROM.write(i, qsid[i]);
          Serial.print("Wrote: ");
          Serial.println(qsid[i]);
        }
        Serial.println("writing eeprom pass:");
        for (int i = 0; i < qpass.length(); ++i)
        {
          EEPROM.write(32 + i, qpass[i]);
          Serial.print("Wrote: ");
          Serial.println(qpass[i]);
        }
        EEPROM.commit();

        content = "{\"Success\":\"saved to eeprom... reset to boot into new wifi\"}";
        statusCode = 200;
        ESP.restart();
      } else {
        content = "{\"Error\":\"404 not found\"}";
        statusCode = 404;
        Serial.println("Sending 404");
      }
      server.sendHeader("Access-Control-Allow-Origin", "*");
      server.send(statusCode, "application/json", content);

    });
  }
}
