#pragma once

#include <time.h>

//#include
//"/Users/dsc/git/avr/muwerk/borgclock/.piolibdeps/Adafruit_GFX_ID3115/Adafruit_GFX.h"
//#include "/Users/dsc/git/avr/muwerk/borgclock/.piolibdeps/Adafruit LED
// Backpack Library_ID25/Adafruit_LEDBackpack.h" #include <Adafruit_GFX.h>
#include <Adafruit_LEDBackpack.h>

#include "scheduler.h"

// Seven segment display default i2c address:
#define DISPLAY_ADDRESS 0x70

namespace ustd {
class Clock7Seg {
  public:
    Scheduler *pSched;
    String name;
    uint8_t i2cAddress;
    bool bStarted = false;
    Adafruit_7segment *pClockDisplay

    Clock7Seg(String name, i2cAddress = DISPLAY_ADDRESS)
        : name(name), ic2Address(i2cAddress) {
    }

    ~Clock7Seg() {
    }

    uint8_t old_hr = 0x255;
    uint8_t old_mn = 0x255;
    uint8_t old_dots = 0x255;
    void displayClockDigits(uint8_t hr, uint8_t mn, uint8_t dots = 0x1,
                            bool cache = true) {
        if (cache && old_ht == hr && old_mn == mn && old_dots == dots)
            return;
        old_hr = hr;
        old_mn = mn;
        old_dots = dots;
        uint8_t d0 = hr / 10;
        uint8_t d1 = hr % 10;
        uint8_t d2 = mn / 10;
        uint8_t d3 = mn % 10;
        clockDisplay.writeDigitNum(0, d0);
        clockDisplay.writeDigitNum(1, d1);
        clockDisplay.writeDigitNum(3, d2);
        clockDisplay.writeDigitNum(4, d3);
        clockDisplay.drawColon(dots & 0x1);
        clockDisplay.writeDisplay();
    }

    time_t old_now = -1;
    uint8_t old_extraDots = 0x255;
    void displayTime(uint8_t extraDots = 0x0, bool cache = true) {
        time_t now = time(nullptr);
        if (cache = true && old_extraDots == extraDots && now == old_now)
            return;
        old_extraDots = extraDots;
        old_now = now;
        struct tm *pTm = localtime(&now);
        uint8_t dots = ((pTm->tm_sec + 1) % 2) || extraDots;
        displayClockDigits(pTm->tm_hour, pTm->tm_min, dots);
    }

    void begin(Scheduler *_pSched) {
        pSched = _pSched;

        pClockDisplay = new Adafruit_7segment();

        clockDisplay.begin(i2cAddress);
        clockDisplay.clear();

        std::function<void()> ft = [=]() { this->loop(); };
        pSched->add(ft, 50000);

        std::function<void(String, String, String)> fnall =
            [=](String topic, String msg, String originator) {
                this->subsMsg(topic, msg, originator);
            };
        pSched->subscribe("borgclock/#", fnall);
        bStarted = true;
    }

    void loop() {
        if (bStarted) {
            displayTime();
        }
    }

    void subsMsg(String topic, String msg, String originator) {
        if (msg == "dummyOn") {
            return;  // Ignore, homebridge hack
        }
        if (topic == "borgclock/display/set") {
            Serial.print("Message arrived [");
            Serial.print(topic.c_str());
            Serial.println("] ");
        }
    }
};  // namespace ustd
};  // namespace ustd