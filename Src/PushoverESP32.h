
#pragma once

#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

struct PushoverMessage
{
  public:
    const char *message = "";
    const char *title = "";
    const char *url = "";
    const char *url_title = "";
    const char *sound = "";
    bool html = false;
    uint8_t priority = 0;
    uint32_t timestamp;
};

class Pushover
{
  private:
    uint16_t _timeout = 5000;
    const char *_token;
    const char *_user;

  public:
    Pushover(const char *, const char *);
    Pushover();
    void setUser(const char *);
    void setToken(const char *);
    int send(PushoverMessage message);
};
