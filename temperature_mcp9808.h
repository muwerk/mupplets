// temperature_mcp9808.h
// adapted from:
// https://github.com/adafruit/Adafruit-MLX90614-Library/blob/master/examples/mlxtest/mlxtest.ino
#pragma once

#include "scheduler.h"
#include "sensors.h"

//#include <Adafruit_Sensor.h>
#include <Adafruit_MCP9808.h>
#include <home_assistant.h>

namespace ustd {
class TemperatureMCP9808 {
  public:
    /*! High precision temperature measurement with MCP9808
     */
    String MCP9808_TEMP_VERSION = "0.1.0";
    Scheduler *pSched;
    int tID;
    String name;
    uint8_t i2cAddress;
    uint8_t resolution;
    double temperatureSensorVal;
    bool bActive = false;
    String errmsg;
    ustd::sensorprocessor temperatureSensor = ustd::sensorprocessor(6, 300, 0.01);
    Adafruit_MCP9808 *pTemp;
#ifdef __ESP__
    HomeAssistant *pHA;
#endif

    TemperatureMCP9808(String name, uint8_t i2cAddress = 0x18, uint8_t resolution = 3)
        : name(name), i2cAddress(i2cAddress), resolution(resolution) {
        /*! Instantiate an Interval Motor
         * @param name              The name of the entity
         * @param i2cAddress I2C-address of sensor. Default is 0x18, for Adafruit's sensor, address
         * can be configured:
         *  A2 |  A1 | A0  address
         * --- | --- --- -------
         *  0  |  0  |  0   0x18  [default]
         *  0  |  0  |  1   0x19
         *  0  |  1  |  0   0x1A
         *  0  |  1  |  1   0x1B
         *  1  |  0  |  0   0x1C
         *  1  |  0  |  1   0x1D
         *  1  |  1  |  0   0x1E
         *  1  |  1  |  1   0x1F
         * @param resolution Resolution of measurement:
         * Res | Precision | Delay
         * --- | --------- | -----
         *  0  | 0.5°C     | 30 ms
         *  1  | 0.25°C    | 65 ms
         *  2  | 0.125°C   | 130 ms
         *  3  | 0.0625°C  | 250 ms [default]
         */
        pTemp = new Adafruit_MCP9808();
    }

    ~TemperatureMCP9808() {
    }

    double getTemperature() {
        return temperatureSensorVal;
    }

    void setResulution(int _resolution) {
        resolution = _resolution;
    }

    void begin(Scheduler *_pSched) {
        pSched = _pSched;

        Wire.begin();
        if (pTemp->begin(i2cAddress)) {
            bActive = true;
            errmsg = "OK";
        } else {
            errmsg = "Can't find or initialize MCP9808 sensor";
        }

        auto ft = [=]() { this->loop(); };
        /* tID = */ pSched->add(ft, name, 2000000);

        auto fnall = [=](String topic, String msg, String originator) {
            this->subsMsg(topic, msg, originator);
        };
        pSched->subscribe(tID, name + "/sensor/temperature/get", fnall);
    }

#ifdef __ESP__
    void registerHomeAssistant(String homeAssistantFriendlyName, String projectName = "",
                               String homeAssistantDiscoveryPrefix = "homeassistant") {
        pHA = new HomeAssistant(name, tID, homeAssistantFriendlyName, projectName,
                                MCP9808_TEMP_VERSION, homeAssistantDiscoveryPrefix);
        pHA->addSensor("temperature", "Temperature", "\\u00B0C", "temperature", "mdi:thermometer");
        pHA->begin(pSched);
    }
#endif

    void publishTemperature() {
        char buf[32];
        pSched->publish(name + "/sensor/result", errmsg);
        sprintf(buf, "%5.2f", temperatureSensorVal);
        pSched->publish(name + "/sensor/temperature", buf);
    }

    void loop() {
        if (bActive) {
            pTemp->wake();
            double t = pTemp->readTempC();
            if (temperatureSensor.filter(&t)) {
                temperatureSensorVal = t;
                publishTemperature();
            }
            pTemp->shutdown_wake(1);
        } else {
            pSched->publish(name + "/sensor/result", errmsg);
        }
    }

    void subsMsg(String topic, String msg, String originator) {
        if (topic == name + "/sensor/temperature/get") {
            publishTemperature();
        }
    };
};  // MCP9808

}  // namespace ustd
