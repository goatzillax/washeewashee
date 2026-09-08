#include "LittleFS.h"

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>

unsigned long ota_progress_millis = 0;

void onOTAStart() {
  // Log when OTA has started
  Serial.println("OTA update started!");
  // <Add your own code here>
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
  // <Add your own code here>
}


AsyncWebServer server(80);

#define ISR_SERVO_DEBUG 0
#include "ESP8266_ISR_Servo.h"

int servoHandle = -1;

enum eWasherState { POWERUP,
                    OSCILLATING,
                    SPINNING,
                    IDLE,
                    LAST };

eWasherState WasherState = POWERUP;

//  probably a more elegant way to do this but whatevs
unsigned long ts_powerup;
unsigned long ts_oscillating;
unsigned long ts_spinning;

//  this ramp is kind of...  rough.
int osc_pattern[] = { 135, 150, 180, 180, 180, 120, 90, 45, 30, 0, 0, 0, 45, 90 };
int osc_pattern_idx;  //  whoever calls for the state change should set these up.
unsigned long osc_last;

void setup() {
  ts_powerup = millis();
  Serial.begin(115200);

  WiFi.softAP("washeewashee", "washeewashee");
  IPAddress IP = WiFi.softAPIP();

  while (!Serial)
    ;
  Serial.println("\n\n");
  Serial.print("AP IP address: ");
  Serial.println(IP);

  if (!LittleFS.begin()) {
    Serial.println("ERROR:  could not initialize LittleFS");
    return;
  }

#if 0
        File file = LittleFS.open("/index.html", "r");
        if(!file){
                Serial.println("ERROR:  Failed to open index.html for reading");
                return;
        }
  
        Serial.println("File Content:");
                while(file.available()){
                Serial.write(file.read());
        }
        file.close();
#endif

  servoHandle = ISR_Servo.setupServo(D4, 1000, 2000);
  if (servoHandle == -1) {
    Serial.println("ERROR:  could not initialize ISR servo");
  }  //  halt n catch a stray?
  ISR_Servo.setPosition(servoHandle, 90);
  //Serial.println(ISR_Servo.getPulseWidth(servoHandle));

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html", "text/html");
  });

  server.on("/stop", HTTP_GET, [](AsyncWebServerRequest *request) {
    WasherState = IDLE;  //  probably need some sort of locking or dropbox since this is async...  ish.  heavy on the ish.
    request->redirect("/");
  });

  server.on("/spin_start", HTTP_GET, [](AsyncWebServerRequest *request) {
    WasherState = SPINNING;  //  probably need some sort of locking or dropbox since this is async...  ish.  heavy on the ish.
    ts_spinning = millis();
    request->redirect("/");
  });

  server.on("/wash_start", HTTP_GET, [](AsyncWebServerRequest *request) {
    WasherState = OSCILLATING;  //  probably need some sort of locking or dropbox since this is async...  ish.  heavy on the ish.
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

#define TIME_POWERDELAY (20 * 1000)
#define TIME_OSCILLATE (5 * 60 * 1000)
#define TIME_SPIN (3 * 60 * 1000)

void check_and_set(int handle, int pos) {
  if (ISR_Servo.getPosition(handle) != pos) {
    ISR_Servo.setPosition(handle, pos);
  }
}

void loop() {
  switch (WasherState) {
    case POWERUP:
      //  wait 5 seconds and then start oscillating
      if (millis() - ts_powerup > (TIME_POWERDELAY)) {
        WasherState = OSCILLATING;
        osc_pattern_idx = 0;
        ts_oscillating = osc_last = millis();
        //  alright I already don't like how this code is looking but whatever
      }
      break;
    case OSCILLATING:
      {
        if (millis() - ts_oscillating > (TIME_OSCILLATE)) {
          WasherState = IDLE;
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

          WasherState = IDLE;
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
