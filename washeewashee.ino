#include <Arduino.h>
#include <LittleFS.h>

typedef enum { POWERUP = 0,
               IDLE = 1,
               OSCILLATING = 2,
               SPINNING = 3,
               LAST = 4 } eWasherState;

#include <RTCMemory.h>

// This is almost like a context struct across resets, could save all kinds of stuff.
// Max size is 508 bytes.
typedef struct {
  eWasherState WasherState;
} rtcData_struct;

RTCMemory<rtcData_struct> rtcMemory;
rtcData_struct *rtcData;

#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>

unsigned long ota_progress_millis = 0;

void onOTAStart() {
  Serial.println("OTA update started!");
}

void onOTAProgress(size_t current, size_t final) {
  // Log every 1 second
  if (millis() - ota_progress_millis > 1000) {
    ota_progress_millis = millis();
    Serial.printf("OTA Progress Current: %u bytes, Final: %u bytes\n", current, final);
  }
}

void onOTAEnd(bool success) {
  // Log when OTA has finished
  if (success) {
    Serial.println("OTA update finished successfully!");
  } else {
    Serial.println("There was an error during OTA update!");
  }
}

AsyncWebServer server(80);

#define ISR_SERVO_DEBUG 0
#include "ESP8266_ISR_Servo.h"

int servoHandle = -1;

//  probably a more elegant way to do this but whatevs
unsigned long ts_powerup;
unsigned long ts_oscillating;
unsigned long ts_spinning;

#define TIME_POWERDELAY (30 * 1000)
#define TIME_OSCILLATE (5 * 60 * 1000)
#define TIME_SPIN (3 * 60 * 1000)

//  this ramp is kind of...  rough.
int osc_pattern[] = { 135, 150, 180, 180, 180, 120, 90, 45, 30, 0, 0, 0, 45, 90 };
int osc_pattern_idx;  //  whoever calls for the state change should set these up.
unsigned long osc_last;

void setWasherState(rtcData_struct *data, eWasherState newstate) {
  Serial.println("DEBUG:  WasherState = " + String((int)newstate));
  data->WasherState = newstate;
  rtcMemory.save();
}

String processor(const String &var) {
  if (var == "WASHERSTATE")
    return (String((int)rtcData->WasherState));
  return String();
}


void setup() {
  ts_powerup = ts_oscillating = ts_spinning = millis();
  Serial.begin(115200);

  Serial.println("\n\n");

  if (rtcMemory.begin()) {
    Serial.println("INFO:  Initialization done! Previous data found.");
    rtcData = rtcMemory.getData();
    rtcData->WasherState = static_cast<eWasherState>(rtcData->WasherState + 1);  // mang fuck dis language
    if (rtcData->WasherState >= LAST) {
      rtcData->WasherState = IDLE;
    }
  } else {
    Serial.println("INFO:  Initialization done! No previous data found. The buffer is cleared.");
    //  dunno what "the buffer is cleared" actually means.
    rtcData = rtcMemory.getData();
    rtcData->WasherState = POWERUP;
  }

  rtcMemory.save();

  WiFi.softAP("washeewashee", "washeewashee");
  IPAddress IP = WiFi.softAPIP();

  Serial.print("AP IP address: ");
  Serial.println(IP);
  Serial.println("DEBUG:  WasherState = " + String((int)rtcData->WasherState));

  if (!LittleFS.begin()) {
    Serial.println("ERROR:  could not initialize LittleFS");
    //return;
  }

  servoHandle = ISR_Servo.setupServo(D4, 1000, 2000);
  if (servoHandle == -1) {
    Serial.println("ERROR:  could not initialize ISR servo");
  }                                        //  halt n catch a stray?
  ISR_Servo.setPosition(servoHandle, 90);  // weird there was no init/failsafe?
  //Serial.println(ISR_Servo.getPulseWidth(servoHandle));

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html", String(), false, processor);
  });

  server.on("/stop", HTTP_GET, [](AsyncWebServerRequest *request) {
    setWasherState(rtcData, IDLE);
    request->redirect("/");
  });

  server.on("/spin_start", HTTP_GET, [](AsyncWebServerRequest *request) {
    setWasherState(rtcData, SPINNING);
    ts_spinning = millis();
    request->redirect("/");
  });

  server.on("/wash_start", HTTP_GET, [](AsyncWebServerRequest *request) {
    setWasherState(rtcData, OSCILLATING);
    osc_pattern_idx = 0;
    ts_oscillating = osc_last = millis();
    request->redirect("/");
  });

  server.serveStatic("/", LittleFS, "/");

  ElegantOTA.begin(&server);  // Start ElegantOTA
  // ElegantOTA callbacks
  ElegantOTA.onStart(onOTAStart);
  ElegantOTA.onProgress(onOTAProgress);
  ElegantOTA.onEnd(onOTAEnd);

  server.begin();
}



void check_and_set(int handle, int pos) {
  if (ISR_Servo.getPosition(handle) != pos) {
    ISR_Servo.setPosition(handle, pos);
  }
}

void loop() {
  switch (rtcData->WasherState) {
    case POWERUP:
      if (millis() - ts_powerup > (TIME_POWERDELAY)) {
        setWasherState(rtcData, OSCILLATING);
        osc_pattern_idx = 0;
        ts_oscillating = osc_last = millis();
        //  alright I already don't like how this code is looking but whatever
      }
      break;
    case OSCILLATING:
      {
        if (millis() - ts_oscillating > (TIME_OSCILLATE)) {
          setWasherState(rtcData, IDLE);
        }
        if (millis() - osc_last > 1000) {
          osc_last = millis();
          osc_pattern_idx = osc_pattern_idx >= (sizeof(osc_pattern) / sizeof(osc_pattern[0])) - 1 ? 0 : osc_pattern_idx + 1;
          //Serial.println(osc_pattern_idx);
        }

        int pos = osc_pattern[osc_pattern_idx];
        check_and_set(servoHandle, pos);
        break;
      }
    case SPINNING:
      {
        if (millis() - ts_spinning > (TIME_SPIN)) {
          setWasherState(rtcData, IDLE);
        } else if (millis() - ts_spinning > (1000)) {
          check_and_set(servoHandle, 180);
        } else {
          check_and_set(servoHandle, 90 + 45);  //  would also like a ramp-up here.  todo.
        }
        break;
      }
    case IDLE:
      check_and_set(servoHandle, 90);
      break;
    default:
      Serial.println("ERROR:  wat is ur major malfunction boy");
  }
  ElegantOTA.loop();
  delay(100);
}
