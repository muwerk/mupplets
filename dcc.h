// dcc.h

// After:
// NMRA National Model Railroad Association
// Standard S-9.1, Electrical standards for digital command control
// Version 2006
// https://www.nmra.org/index-nmra-standards-and-recommended-practices
// https://www.nmra.org/sites/default/files/standards/sandrp/pdf/s-9.1_electrical_standards_2006.pdf
// https://www.nmra.org/sites/default/files/s-92-2004-07.pdf

#pragma once

#include "scheduler.h"
//#include "home_assistant.h"

namespace ustd {

#ifdef __ESP32__
#define G_INT_ATTR IRAM_ATTR
#else
#ifdef __ESP__
#error ESP8266 not yet supported!
#define G_INT_ATTR ICACHE_RAM_ATTR
#else
#define G_INT_ATTR
#endif
#endif

#define USTD_MAX_DCC_TIMERS (4)
#define DCC_BITSTREAM_LEN (10)
#define DCC_MAX_CMD_QUEUE (16)

typedef struct t_dcc_cmd {
    uint8_t bitLen;
    uint8_t bitPos;
    uint8_t bitstream[DCC_BITSTREAM_LEN];
} T_DCC_CMD;

hw_timer_t *dccTimer[USTD_MAX_DCC_TIMERS] = {NULL, NULL, NULL, NULL};
portMUX_TYPE dccTimerMux[USTD_MAX_DCC_TIMERS] = {
    portMUX_INITIALIZER_UNLOCKED, portMUX_INITIALIZER_UNLOCKED, portMUX_INITIALIZER_UNLOCKED,
    portMUX_INITIALIZER_UNLOCKED};
volatile bool waveState[USTD_MAX_DCC_TIMERS] = {false, false, false, false};
volatile uint8_t wavePin[USTD_MAX_DCC_TIMERS] = {0xff, 0xff, 0xff, 0xff};

typedef ustd::queue<T_DCC_CMD> T_DQ;
T_DQ dcc_cmd_que[USTD_MAX_DCC_TIMERS] = {T_DQ(16), T_DQ(16), T_DQ(16), T_DQ(16)};
T_DCC_CMD dcc_curData[USTD_MAX_DCC_TIMERS];

volatile uint8_t dcc_currentBit[USTD_MAX_DCC_TIMERS] = {1, 1, 1, 1};
volatile uint8_t dcc_zeroSkip[USTD_MAX_DCC_TIMERS] = {1, 1, 1, 1};
volatile uint8_t dcc_inData[USTD_MAX_DCC_TIMERS] = {0, 0, 0, 0};

volatile unsigned long cmdsSent = 0;
volatile unsigned long qS = 0;

volatile bool irqAct = false;

void G_INT_ATTR ustd_dcc_timer_irq_master(uint8_t timerNo) {
    portENTER_CRITICAL(&(dccTimerMux[timerNo]));
    if (dcc_currentBit[timerNo] == 0 && dcc_zeroSkip[timerNo] == 1) {
        dcc_zeroSkip[timerNo] = 0;
        portEXIT_CRITICAL(&(dccTimerMux[timerNo]));
        return;
    }
    dcc_zeroSkip[timerNo] = 1;
    if (waveState[timerNo]) {
        waveState[timerNo] = false;
        if (irqAct)
            digitalWrite(wavePin[timerNo], false);
    } else {
        waveState[timerNo] = true;
        if (irqAct)
            digitalWrite(wavePin[timerNo], true);
        if (!dcc_inData[timerNo]) {
            if (dcc_cmd_que[timerNo].length() > 0) {
                dcc_curData[timerNo] = dcc_cmd_que[timerNo].pop();
                dcc_inData[timerNo] = 1;
                ++qS;
                irqAct = true;
            }
        }
        if (dcc_inData[timerNo]) {
            uint8_t ind = (dcc_curData[timerNo].bitPos) / 8;
            uint8_t pos = 1 << ((dcc_curData[timerNo].bitPos) % 8);
            if (dcc_curData[timerNo].bitstream[ind] & pos)
                dcc_currentBit[timerNo] = 1;
            else
                dcc_currentBit[timerNo] = 0;
            ++dcc_curData[timerNo].bitPos;
            if (dcc_curData[timerNo].bitPos == dcc_curData[timerNo].bitLen) {
                dcc_inData[timerNo] = 0;
                memset(&dcc_curData[timerNo], 0, sizeof(dcc_curData[timerNo]));
                ++cmdsSent;
            }
            if (dcc_currentBit[timerNo] == 0)
                dcc_zeroSkip[timerNo] = 1;
        } else {
            dcc_currentBit[timerNo] = 1;
            dcc_zeroSkip[timerNo] = 0;

            irqAct = false;
        }
    }
    portEXIT_CRITICAL(&(dccTimerMux[timerNo]));
}

void G_INT_ATTR ustd_dcc_timer_irq0() {
    ustd_dcc_timer_irq_master(0);
}
void G_INT_ATTR ustd_dcc_timer_irq1() {
    ustd_dcc_timer_irq_master(1);
}
void G_INT_ATTR ustd_dcc_timer_irq2() {
    ustd_dcc_timer_irq_master(2);
}
void G_INT_ATTR ustd_dcc_timer_irq3() {
    ustd_dcc_timer_irq_master(3);
}

void (*ustd_dcc_timer_irq_table[USTD_MAX_DCC_TIMERS])() = {
    ustd_dcc_timer_irq0, ustd_dcc_timer_irq1, ustd_dcc_timer_irq2, ustd_dcc_timer_irq3};

class Dcc {
  public:
    enum Mode { DCC, HBRIDGE, DC };
    String DCC_VERSION = "0.1.0";
    Scheduler *pSched;
    int tID;
    int pwmrange;
    int pwmfreq;
    int pwmtimerbits = 10;

    String name;
    Mode mode;
    uint8_t pin_pwm;
    uint8_t channel;
    uint8_t pin_in1, pin_in2;
    bool timerStarted = false;

    int trainSpeed;
    /*
    #ifdef __ESP__
    HomeAssistant *pHA;
    #endif
    */

    Dcc(String name, Mode mode, uint8_t pin_pwm, int8_t channel, uint8_t pin_in1 = 0xff,
        uint8_t pin_in2 = 0xff)
        : name(name), mode(mode), pin_pwm(pin_pwm), channel(channel), pin_in1(pin_in1),
          pin_in2(pin_in2) {
    }

    ~Dcc() {
        if (timerStarted) {
            timerEnd(dccTimer[channel]);
            timerStarted = false;
        }
    }

    void begin(Scheduler *_pSched) {
        pSched = _pSched;

        digitalWrite(pin_pwm, false);
        pinMode(pin_pwm, OUTPUT);
        if (mode != Mode::DC) {
            digitalWrite(pin_in1, true);
            digitalWrite(pin_in2, false);
            pinMode(pin_in1, OUTPUT);
            pinMode(pin_in2, OUTPUT);
        }

        switch (mode) {
        case Mode::DCC:
            if (channel >= 0 && channel < USTD_MAX_DCC_TIMERS) {
                // Use channel=[0..3] timer.
                // Set 80 divider for prescaler, ->us. (see ESP32 Technical
                // Reference Manual for more info).
                dccTimer[channel] = timerBegin(channel, 80, true);
                // Attach onTimer function to our timer.
                timerAttachInterrupt(dccTimer[channel], ustd_dcc_timer_irq_table[channel], true);
                // Set alarm to call onTimer function every second (value in
                // microseconds). Repeat the alarm (third parameter)
                timerAlarmWrite(dccTimer[channel], 58, true);  // every 58 uSec
                // Start repeating alarm
                timerStarted = true;
                wavePin[channel] = pin_pwm;
                timerAlarmEnable(dccTimer[channel]);
            }
            break;
        case Mode::DC:
#if defined(__ESP32__)
            pwmfreq = 6;
            ledcSetup(channel, pwmfreq, pwmtimerbits);
            ledcAttachPin(pin_pwm, channel);
#else
            pinMode(pin_pwm, OUTPUT);
#endif
#ifdef __ESP__
            pwmrange = 1023;
#else
            pwmrange = 255;
#endif
        default:
            break;
        }

        auto ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, 1000000);

        auto fnall = [=](String topic, String msg, String originator) {
            this->subsMsg(topic, msg, originator);
        };
        pSched->subscribe(tID, name + "/dcc/#", fnall);
    }

    void encodeAddBit(T_DCC_CMD *dccCmd, uint8_t bit) {
        uint8_t ind = (dccCmd->bitPos) / 8;
        uint8_t pos = 1 << ((dccCmd->bitPos) % 8);
        if (bit)
            dccCmd->bitstream[ind] |= pos;
        ++dccCmd->bitPos;
    }

    void encodeAddByte(T_DCC_CMD *dccCmd, uint8_t byte) {
        for (uint8_t b = 0; b < 8; b++) {
            if (byte & (1 << (7 - b)))
                encodeAddBit(dccCmd, 1);
            else
                encodeAddBit(dccCmd, 0);
        }
    }

    bool encode(int len, uint8_t *buf, T_DCC_CMD *dccCmd) {
        if (DCC_BITSTREAM_LEN * 8 < (len + 1) * 9 + 13) {
#ifdef USE_SERIAL_DBG
            Serial.println("Cmd len exeeded for DCC buf");
#endif
            return false;
        }
        dccCmd->bitPos = 0;
        uint8_t crc = 0;
        uint8_t preambleBits = 16;
        for (uint8_t i = 0; i < preambleBits; i++)
            encodeAddBit(dccCmd, 1);
        encodeAddBit(dccCmd, 0);
        for (uint8_t i = 0; i < len; i++) {
            crc ^= buf[i];
            encodeAddByte(dccCmd, buf[i]);
            encodeAddBit(dccCmd, 0);
        }
        encodeAddByte(dccCmd, crc);
        encodeAddBit(dccCmd, 1);
        dccCmd->bitLen = dccCmd->bitPos;
        dccCmd->bitPos = 0;
        return true;
    }

    bool sendCmd(int len, uint8_t *buf) {
        T_DCC_CMD tcc;
        if (encode(len, buf, &tcc)) {
            portENTER_CRITICAL(&(dccTimerMux[channel]));
            if (!dcc_cmd_que[channel].push(tcc)) {
                portEXIT_CRITICAL(&(dccTimerMux[channel]));
#ifdef USE_SERIAL_DBG
                Serial.println("DCC Queue full");
#endif
                return false;
            } else {
                portEXIT_CRITICAL(&(dccTimerMux[channel]));
                return true;
            }
        }
        return false;
    }

    bool setTrainSpeed(uint8_t _trainSpeed, bool direction = true, uint8_t trainDccAddress = 0x00) {
        trainSpeed = _trainSpeed;
        switch (mode) {
        case Mode::DCC:
            uint8_t buf[2];
            buf[0] = trainDccAddress & 127;
            buf[1] = trainSpeed & 0x1f;
            if (direction)
                buf[1] |= 0x20;
            buf[1] |= 0x40;
            return sendCmd(2, buf);
            break;
        case Mode::DC:
#if defined(__ESP32__)
            ledcWrite(channel, trainSpeed);
#else
            analogWrite(pin_pwm, trainSpeed);
#endif

            return true;
            break;
        default:
            return false;
            break;
        }
    }
    /*
        #ifdef __ESP__
        void registerHomeAssistant(String homeAssistantFriendlyName, String
       projectName="", String homeAssistantDiscoveryPrefix="homeassistant") {
            pHA=new HomeAssistant(name, tID, homeAssistantFriendlyName,
       projectName, DCC_VERSION, homeAssistantDiscoveryPrefix); pHA->addDcc();
            pHA->begin(pSched);
        }
        #endif
    */

    // uint8_t speed=0;
    int n = 0;
    void loop() {
        if (pwmfreq < 30) {
            ++n;
            pwmfreq += 1;
            setTrainSpeed(trainSpeed + 2);
            if (n == 12 || n == 14 || n == 16)
                ledcWriteTone(channel, 220);
            else
                ledcWriteTone(channel, pwmfreq);
            ledcWrite(channel, trainSpeed);
            // ledcSetup(channel, pwmfreq, pwmtimerbits);
            // ledcAttachPin(pin_pwm, channel);
        }
        if (timerStarted) {
            //#ifdef USE_SERIAL_DBG
            // char buf[128];
            // sprintf(buf,"Cmds: %ld sent: %ld", qS, cmdsSent);
            // Serial.println(buf);
            //#endif

            // setTrainSpeed(3,13);
            //++speed;
            // if (speed>31) speed=0;
        }
    }

    void subsMsg(String topic, String msg, String originator) {
        if (topic == name + "/dcc/state/get") {
            char buf[32];
            sprintf(buf, "bad");
            pSched->publish(name + "/dcc/state", buf);
        }
    }
};  // Dcc

}  // namespace ustd
