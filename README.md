# washeewashee

I FIX WASHEE WASHEE MACHINE

## ESP8266

### Probable libz

* https://github.com/khoih-prog/ESP8266_ISR_Servo
* https://github.com/me-no-dev/ESPAsyncWebServer
* https://github.com/earlephilhower/arduino-esp8266littlefs-plugin
* https://github.com/ayushsharma82/ElegantOTA

### Pinout

* D4 - PWM out; also happens to be onboard LED
* GND
* VBUS - 5v

## ESP32-C3

Dis would gib me back a button and an RGB LED

### Probable libz

almost everything from ESP8266 plus watever that RGB LED library waz

### Pinout

* GPIO6 - PWM out
* GND
* VBUS - 5v

Mite use button if I can get it in the right place in the case

## Program stuff

### Misc ESC notes

"Braking" on ESC means double-tap to reverse.  Disable braking.  Also the way I wired it, "clockwise" is faster, but the basket will probably unlock during spin if you spin it that way.  So use counter-clockwise for the spin cycle.

Standard signal is 50hz (20ms) cycle, varying from 1000ms to 2000ms, center at 1500ms.  Might check if the ESC truly needs that much dead time.  CPPM dead time appears to be minimum 4ms or so.  (CITATION NEEDED)

Should allow for ramp-up either direction so it doesn't blow itself up.

### Wash

Oscillate back and forth, ~6 seconds each direction.  5 minutes total duration.

### Spin

Spins 1 direction only, standard option 3 minutes.

### Button

Original button was short press change mode, long press power off.

Maybe just go straight to wash after a delay upon powerup.


