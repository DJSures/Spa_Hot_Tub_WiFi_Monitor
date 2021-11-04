/*
   Hot Tub Logger by DJ Sures (c)2021

   Updated: 2021/10/31

  1) Choose the wifi mode by uncomment AP_MODE or not
  2) Enter the SSID/PWD for either AP or Client mode
  3) Connect GND from the Spa (Pin #4) to the Heltec GND (they need to have common gnd)
  4) Connect Spa Data (Pin #5) to Heltech pin #12
  5) Connect Spa Clock (Pin #6) to Heltech pin #14
  6) Get USER Key and API Token from https://pushover.net/api
  7) Edit the #defines below

*/

// ----------------------------------------------------------------
// Network settings
// ----------------------------------------------------------------

// The WiFi mode for the ESP32 module
// Uncomment: Access Point Mode (configure AP mode settings below)
// Commented: Client Mode (configure client credentials below)
//#define AP_MODE

// AP Mode Settings
// This requires #define AP_MODE to be uncommented
// AP mode means you can connect to the ESP32 as if it's a router or server
// Default AP Mode IP Address of this device will be 192.168.50.1
// For an open system with no password, leave the password field empty
#define WIFI_AP_SSID "GetOffMyLawn"
#define WIFI_AP_PWD  ""

// Client Mode Credentials
// This requires #define AP_MODE to be commented
// Client mode means the ESP32 will connect to your router and you will have to know its IP Address
// Or, you can check the USB serial debug output
#define WIFI_CLIENT_SSID "macods"
#define WIFI_CLIENT_PWD  "robdule"

// PushOver credentials.
#define PUSH_OVER_API_TOKEN "a5wv8d8u6v484p69971d"
#define PUSH_OVER_USER_KEY "urn2q3sewrnz5cy7kkg52d"

// Push Notification Title
#define PUSH_TITLE "Camp Hot Tub"
#define PUSH_TITLE_ALARM "Camp Hot Tub ALERT"

// the pin used for the clock interrupt
#define CLOCK_PIN 14

// the pin used for reading the data
#define DATA_PIN 12

// -----------------------------------------------------------------------------------------------------------------------------------------------------------

#include "heltec.h"
#include <WiFi.h>
#include "esp_system.h"
#include <esp_wifi.h>
#include <WiFiAP.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include "ESP32Tone.h"
#include "PushoverESP32.h"

WebServer      _server(80);

// used for the display scrolling feature
String         _scrollBuffer[6];
byte           _scrollPos      = 0;

// gets the external IP so it can be accessed through router on internet
String         _externalIP     = "n/a";

Pushover       _pushoverClient(PUSH_OVER_API_TOKEN, PUSH_OVER_USER_KEY);

volatile byte  _temp                    = 0;     // the current temperature of the hot tub
volatile bool  _heaterStatus            = false; // the status if the heater is on or off
bool           _lastHeaterStatus        = false; // the status of the last heater status in the loop for sending notification only when status has changed
unsigned long  _heaterStartTime         = 0;     // the last time the heater turning ON
unsigned long  _heaterEndTime           = 0;     // the last time the heater turned OFF
unsigned long  _lastHeaterRunLengthTime = 0;     // the last run length of the heater

unsigned long  _lastScreenUpdateTime   = 0;      // used to only allow screen to update every every X seconds
unsigned long  _lastPushNotifyTime     = 0;      // used to prevent notify alarm to update only ever X seconds
unsigned long  _lastLocalAlarmTime     = 0;      // used to prevent the local speaker alarm to only X seconds
unsigned long  _lastHeartBeatAlarmTime = 0;      // used to manage the heart beat notification to go out every hour
unsigned long  _lastRealDataNotifyTime = 0;      // used to prevent the notification of no data alert to every X seconds

// used by the clock pin interrupt
byte _bitPosition = 0;
byte _tmpFirst = 0;
byte _tmpSecond = 0;
byte _tmpThird = 0;

unsigned long _lastRealDataTime   = 0; // the last time we got real data
unsigned long _packetCnt          = 0; // the number of packets processed
unsigned long _recoveredPacketCnt = 0; // the number of packets that we had to recover due to time between packets
unsigned long _brokenPacketCnt    = 0; // the number of packets that were not numbers to be used for temperature
unsigned int  _heaterChangeCnt    = 0; // used to keep track of the heater status change becuase packets can be corrupt
byte          _tempChangeCnt      = 0; // used to keep track of the temp chnage because packets can be corrupt


// -----------------------------------------------------------------------------------------------------------------------------------------------------------

void clearScroll() {

  _scrollPos = 0;

  for (byte i = 0; i < 5; i++)
    _scrollBuffer[i] = "";
}

void writeScroll(String s) {

  Heltec.display->clear();

  if (_scrollPos < 6) {

    _scrollBuffer[_scrollPos] = s;

    _scrollPos++;
  } else {

    for (byte i = 0; i < 5; i++)
      _scrollBuffer[i] = _scrollBuffer[i + 1];

    _scrollBuffer[5] = s;
  }

  for (byte i = 0; i < 6; i++)
    Heltec.display->drawString(0, i * 9, _scrollBuffer[i]);

  Heltec.display->display();

  Serial.println(s);
}

void getExternalIP() {

  HTTPClient http;

  http.begin("https://admin.synthiam.com/WS/WhereAmIV2.aspx");

  int httpResponseCode = http.GET();

  if (httpResponseCode > 0) {

    _externalIP = http.getString();
  } else {

    _externalIP = "Error";
  }

  http.end();
}

void notify(String msg) {

  if (WiFi.status() != WL_CONNECTED)
    return;

  PushoverMessage myMessage;

  myMessage.title = PUSH_TITLE;
  myMessage.message = msg.c_str();

  String url = "http://" + WiFi.localIP().toString();
  myMessage.url = url.c_str();
  myMessage.url_title = "Live Data";

  _pushoverClient.send(myMessage);
}

void notifyHTML(String msg) {

  if (WiFi.status() != WL_CONNECTED)
    return;

  PushoverMessage myMessage;

  myMessage.title = PUSH_TITLE;
  myMessage.message = msg.c_str();
  myMessage.html = true;

  String url = "http://" + WiFi.localIP().toString();
  myMessage.url = url.c_str();
  myMessage.url_title = "Live Data";

  _pushoverClient.send(myMessage);
}

void notifyHTMLPing() {

  if (WiFi.status() != WL_CONNECTED)
    return;

  String s = "Heater is ";

  if (_heaterStatus) {

    s += "<span style='color: green; font-weight: bold;'>ON</span>";
    s += "(running " + getTimeFormatted(millis() - _heaterStartTime) + ")";
  } else {

    s += "<span style='color: red; font-weight: bold;'>OFF</span>";
  }

  s += "<br />";

  // ----------------------------------------

  if (_heaterEndTime > 0) {

    s += "Last heater ran " + getTimeFormatted(millis() - _heaterEndTime) + " ago for " + getTimeFormatted(_lastHeaterRunLengthTime);

    s += "<br />";
  }
  // ----------------------------------------

  s += "Temp is " + String(_temp) + "F";

  s += "<br />";

  // ----------------------------------------

  s += "Received " + String(_packetCnt) + " packets";

  s += "<br />";

  // ----------------------------------------

  s += "Recovered " + String(_recoveredPacketCnt) + " packets";

  s += "<br />";

  // ----------------------------------------

  s += "Uptime: " + getTimeFormatted(millis());

  s += "<br />";

  // ----------------------------------------

  s += "Broken packets: " + String(_brokenPacketCnt);

  s += "<br />";

  // ----------------------------------------

  s += "Last packet received: " + getTimeFormatted(millis() - _lastRealDataTime);

  PushoverMessage myMessage;

  myMessage.title = PUSH_TITLE;
  myMessage.message = s.c_str();
  myMessage.html = true;

  String url = "http://" + WiFi.localIP().toString();
  myMessage.url = url.c_str();
  myMessage.url_title = "Live Data";

  _pushoverClient.send(myMessage);
}

void notifyAlarm(String msg) {

  if (WiFi.status() != WL_CONNECTED)
    return;

  PushoverMessage myMessage;

  myMessage.title = PUSH_TITLE_ALARM;
  myMessage.message = msg.c_str();
  myMessage.sound = "spacealarm";

  String url = "http://" + WiFi.localIP().toString();
  myMessage.url = url.c_str();
  myMessage.url_title = "Live Data";

  _pushoverClient.send(myMessage);
}

String wl_status_to_string() {

  switch (WiFi.status()) {
    case WL_NO_SHIELD:
      return "No Wifi Shield";
    case WL_IDLE_STATUS:
      return "Idle";
    case WL_NO_SSID_AVAIL:
      return "No SSID Avail";
    case WL_SCAN_COMPLETED:
      return "Scan Completed";
    case WL_CONNECTED:
      return "Connected";
    case WL_CONNECT_FAILED:
      return "Connect Failed";
    case WL_CONNECTION_LOST:
      return "Connect Lost";
    case WL_DISCONNECTED:
      return "Disconnected";
    default:
      return "Unknown";
  }
}

String getTimeFormatted(unsigned long ms) {

  unsigned long secs = ms / 1000; // set the seconds remaining
  unsigned long mins = secs / 60; //convert seconds to minutes
  unsigned long hours = mins / 60; //convert minutes to hours
  unsigned long days = hours / 24; //convert hours to days

  secs = secs - (mins * 60); //subtract the coverted seconds to minutes in order to display 59 secs max
  mins = mins - (hours * 60); //subtract the coverted minutes to hours in order to display 59 minutes max
  hours = hours - (days * 24); //subtract the coverted hours to days in order to display 23 hours max

  if (days > 0)
    return String(days) + " days " + String(hours) + ":" + String(mins) + ":" + String(secs);

  return String(hours) + ":" + String(mins) + ":" + String(secs);
}

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// Interrupt routine
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

byte IRAM_ATTR convertToRealValue(byte in) {

  switch (in) {
    case 0x00:
      return 0;
    case 0x7e:
      return 0;
    case 0x30:
      return 1;
    case 0x6d:
      return 2;
    case 0x79:
      return 3;
    case 0x33:
      return 4;
    case 0x5b:
      return 5;
    case 0x5f:
      return 6;
    case 0x70:
      return 7;
    case 0x7f:
      return 8;
    case 0x7b:
      return 9;
    case 0x77:
      return 'A';
    case 0x1f:
      return 'b';
    case 0x0d:
      return 'c';
    case 0x4e:
      return 'C';
    case 0x3d:
      return 'd';
    case 0x4f:
      return 'E';
    case 0x47:
      return 'F';
    case 0x37:
      return 'H';
    case 0x0e:
      return 'L';
    case 0x15:
      return 'n';
    case 0x67:
      return 'P';
    case 0x05:
      return 'r';
    case 0x0f:
      return 't';
    case 0x3e:
      return 'U';
    case 0x3b:
      return 'Y';
    case 0x01:
      return '-';
  }

  return '?'; // 63 0x3f
}

void IRAM_ATTR readDataBit() {

  bool val = digitalRead(DATA_PIN);

  if (millis() >= _lastRealDataTime + 2) {

    // reset position to 0 because a packet length is 0.52ms
    // If we have had > 2 milliseconds since last interrupt,
    // must be start of new packet.
    // Also, there's 16ms between end of a prevoius packet
    // and start of the next.
    _bitPosition = 0;

    _lastRealDataTime = millis();

    _recoveredPacketCnt++;
  }

  if (_bitPosition >= 0 && _bitPosition <= 6) {

    if (_bitPosition == 4) {

      if (_heaterStatus != val)
        _heaterChangeCnt++;
      else
        _heaterChangeCnt = 0;

      // 200 samples before agreeing the heater has indeed changed
      // this is because packets can be garbled from the controller
      if (_heaterChangeCnt > 200)
        _heaterStatus = val;
    } else {

      bitWrite(_tmpFirst, 6 - _bitPosition, val);
    }
  } else if (_bitPosition >= 7 && _bitPosition <= 13) {

    bitWrite(_tmpSecond, 6 - (_bitPosition - 7), val);
  } else if (_bitPosition >= 14 && _bitPosition <= 20) {

    bitWrite(_tmpThird, 6 - (_bitPosition - 14), val);
  }

  if (_bitPosition == 20) {

    byte t1 = convertToRealValue(_tmpFirst);
    byte t2 = convertToRealValue(_tmpSecond);
    byte t3 = convertToRealValue(_tmpThird);

    // Temp can only be digits
    if (t1 >= 0 && t1 <= 9 &&
        t2 >= 0 && t2 <= 9 &&
        t3 >= 0 && t3 <= 9) {

      byte tempCache = (t1 * 100) + (t2 * 10) + t3;

      if (tempCache != _temp)
        _tempChangeCnt++;
      else
        _tempChangeCnt = 0;

      // 200 samples before agreeing the temp has indeed changed
      // this is because packets can be garbled from the controller
      if (_tempChangeCnt > 200)
        _temp = tempCache;
    } else {

      _brokenPacketCnt++;

      Serial.print(F("NO: 0x"));
      Serial.print(_tmpFirst, HEX);
      Serial.print(F(" 0x"));
      Serial.print(_tmpSecond, HEX);
      Serial.print(F(" 0x"));
      Serial.println(_tmpThird, HEX);
    }

    _bitPosition = 0;

    _packetCnt++;
  } else {

    _bitPosition++;
  }
}

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

void setup() {

  Heltec.begin(
    true,  // DisplayEnable Enable
    false, // LoRa Enable
    true); // Serial Enable

  Heltec.display->clear();

  Serial.begin(115200);

  esp_wifi_set_ps(WIFI_PS_NONE);
  esp_wifi_set_max_tx_power(WIFI_POWER_19_5dBm);

#ifdef AP_MODE

  writeScroll("AP WiFi mode");

  writeScroll("SSID:" + WIFI_AP_SSID);
  writeScroll("PWD:" + WIFI_AP_PWD);

  WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PWD);

  // Wait for WiFi to start
  delay(500);

  IPAddress Ip(192, 168, 50, 1);
  IPAddress NMask(255, 255, 255, 0);
  WiFi.softAPConfig(Ip, Ip, NMask);

  IPAddress myIP = WiFi.softAPIP();
  writeScroll("IP: " + myIP);

  notify("AP Mode IP: " + myIP);

#else

  writeScroll("Client WiFi mode");

  WiFi.begin(WIFI_CLIENT_SSID, WIFI_CLIENT_PWD);
  writeScroll("SSID: " + String(WIFI_CLIENT_SSID));

  while (WiFi.status() != WL_CONNECTED)
    delay(1000);

  writeScroll("Connected!");
  writeScroll("IP: " + WiFi.localIP().toString());

  notify("Client mode IP: " + WiFi.localIP().toString());

#endif

  _server.on("/", handle_OnIndex);
  _server.onNotFound(handle_404);
  _server.begin();

  writeScroll("HTTP Server Started");

  writeScroll("Getting external IP");

  getExternalIP();

  writeScroll("Ext IP: " + _externalIP);

  notify("Got external IP: " + _externalIP);

  writeScroll("Assign Data Pin");
  pinMode(DATA_PIN, INPUT_PULLUP);

  writeScroll("Assign Clock Pin");
  pinMode(CLOCK_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(CLOCK_PIN), readDataBit, RISING);

  writeScroll("Notifier Started");

  notify("Notifier started");
}

// -----------------------------------------------------------------------------------------------------------------------------------------------------------

// Reset function at address 0
void(* resetFunc) (void) = 0;


// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// Loop
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

void loop() {

  Heltec.display->clear();

  // heater status changed, send notification
  if (_lastHeaterStatus != _heaterStatus) {

    _lastHeaterStatus = _heaterStatus;

    if (_heaterStatus) {

      _heaterStartTime = millis();

      notifyHTML("Heater is <span style='color: green; font-weight: bold;'>ON</span> ("
                 + String(_temp)
                 + "F. Last ran "
                 + ( _heaterEndTime == 0 ? "n/a" : String(getTimeFormatted(millis() - _heaterEndTime)) )
                 + " ago)");
    } else {

      _heaterEndTime = millis();

      _lastHeaterRunLengthTime = _heaterEndTime - _heaterStartTime;

      notifyHTML("Heater is <span style='color: red; font-weight: bold;'>OFF</span> ("
                 + String(_temp)
                 + "F. Ran for "
                 + String(getTimeFormatted(_lastHeaterRunLengthTime))
                 + ")");
    }
  }

  // send heart beat notification every hour
  if (millis() > _lastHeartBeatAlarmTime + 3600000) {

    notifyHTMLPing();

    _lastHeartBeatAlarmTime = millis();
  }

  // force a reboot every 4 weeks
  if (millis() > 604800000) {

    writeScroll("4 weeks is up!");
    writeScroll("Rebooting...");

    delay(2000);

    resetFunc();
  }

  // alarm notify if temp is out of range
  if (_temp < 80 || _temp > 110) {

    // limit speaker alarm to every 2 seconds
    if (millis() > _lastLocalAlarmTime + 2000) {

      // Playsound_pilot_light_alarm();

      _lastLocalAlarmTime = millis();
    }

    // limit push alarm to every 5 minutes
    if (millis() > _lastPushNotifyTime + 300000) {

      notifyAlarm("ALERT Temp is " + String(_temp) + "F!!!");

      _lastPushNotifyTime = millis();
    }
  }

  // alarm notify if packet hasn't been received in 5 seconds
  // only notify every 5 minutes
  if (millis() > _lastRealDataTime + 5000 && millis() > _lastRealDataNotifyTime + 300000) {

    notifyAlarm("ALERT Have not received packet in " + getTimeFormatted(millis() - _lastRealDataTime));

    _lastRealDataNotifyTime = millis();
  }

  // update display every second
  if (millis() > _lastScreenUpdateTime + 1000) {

    _lastScreenUpdateTime = millis();

    Heltec.display->drawString(0, 0, "Local: http://" + WiFi.localIP().toString());
    Heltec.display->drawString(0, 9, "Ext: http://" + _externalIP);

    if (_heaterStatus)
      Heltec.display->drawString(0, 18, "Heater: ON (run " + getTimeFormatted(millis() - _heaterStartTime) + ")");
    else
      Heltec.display->drawString(0, 18, "Heater: OFF");

    Heltec.display->drawString(0, 27, "Last len " + getTimeFormatted(_lastHeaterRunLengthTime) + " ago " + getTimeFormatted(_heaterEndTime));

    Heltec.display->drawString(0, 36, "Temp: " + String(_temp) + "F");

    Heltec.display->drawString(0, 45, "WiFi: " + wl_status_to_string());

    Heltec.display->drawString(0, 54, getTimeFormatted(millis()) + " Broke: " + String(_brokenPacketCnt));

    Heltec.display->display();
  }

  _server.handleClient();
}

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// HTTP Handles
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

void handle_OnIndex() {

  _server.send(200, "text/html", getIndexPage());
}

void handle_404() {

  _server.send(200, "text/html", "404 error");
}

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// HTML Pages
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

String getIndexPage() {

  String s = "<html>";

  s += "<head>";
  s += "<meta http-equiv='refresh' content='2'>";
  s += "</head>";

  // ----------------------------------------

  s += "<body>";

  s += "<h1>";
  s += PUSH_TITLE;
  s += "</h1>";
  s += "<hr />";

  // ----------------------------------------

  s += "Heater is ";

  if (_heaterStatus) {

    s += "<span style='color: green; font-weight: bold;'>ON</span> ";
    s += "(running " + getTimeFormatted(millis() - _heaterStartTime) + ")";
  } else {

    s += "<span style='color: red; font-weight: bold;'>OFF</span>";
  }

  s += "<br />";

  // ----------------------------------------

  if (_heaterEndTime > 0) {

    s += "Last heater ran " + getTimeFormatted(millis() - _heaterEndTime) + " ago for " + getTimeFormatted(_lastHeaterRunLengthTime);

    s += "<br />";
  }

  // ----------------------------------------

  s += "Temp is " + String(_temp) + "F";

  s += "<br />";

  // ----------------------------------------

  s += "Received " + String(_packetCnt) + " packets";

  s += "<br />";

  // ----------------------------------------

  s += "Recovered " + String(_recoveredPacketCnt) + " packets";

  s += "<br />";

  // ----------------------------------------

  s += "Uptime: " + getTimeFormatted(millis());

  s += "<br />";

  // ----------------------------------------

  s += "Last packet received: " + getTimeFormatted(millis() - _lastRealDataTime);

  s += "<br />";

  // ----------------------------------------

  s += "Broken packets: " + String(_brokenPacketCnt);

  s += "<br />";

  // ----------------------------------------

  s += "</body>";
  s += "</html>";

  return s;
}
