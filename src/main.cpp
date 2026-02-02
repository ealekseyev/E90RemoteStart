#include <Arduino.h>
#include "canbus.h"
#include "config.h"
#include "hardwarePins.h"
#include "CarControl.hpp"
#include "ClimateControl.hpp"
#include "CustomKeys.h"
#ifdef ENABLE_WEBSERVER
#include "WebServer.hpp"
#endif

CANBus can(MCP2515_CS_PIN);
CarControl* carControl = CarControl::getInstance();
ClimateControl* climateControl = ClimateControl::getInstance();
CustomKeys* customKeys = CustomKeys::getInstance();
#ifdef ENABLE_WEBSERVER
VehicleWebServer* webServer = VehicleWebServer::getInstance();
#endif

String serialBuffer = "";

uint8_t hexCharToNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

uint8_t hexByteToByte(char high, char low) {
    return (hexCharToNibble(high) << 4) | hexCharToNibble(low);
}

void processSerialCommand(const String& cmd) {
    // Expected format: xxx:yyyy...
    // xxx = 3 hex nibbles for CAN ID
    // yyyy... = pairs of hex nibbles for data bytes

    int colonPos = cmd.indexOf(':');
    if (colonPos != 3) {
        return;
    }

    CANFrame frame;
    frame.id = 0;
    for (int i = 0; i < 3; i++) {
        frame.id = (frame.id << 4) | hexCharToNibble(cmd.charAt(i));
    }

    String dataStr = cmd.substring(colonPos + 1);
    frame.dlc = dataStr.length() / 2;
    if (frame.dlc > 8) frame.dlc = 8;

    for (uint8_t i = 0; i < frame.dlc; i++) {
        frame.data[i] = hexByteToByte(dataStr.charAt(i * 2), dataStr.charAt(i * 2 + 1));
    }

    can.write(frame);
}

void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    while (!Serial);

    can.init(CAN_BITRATE);
    carControl->init(&can);
    climateControl->init(&can);
    customKeys->init(carControl);
    Serial.println("CAN Ready");

#ifdef ENABLE_WEBSERVER
    webServer->init(carControl, climateControl);
#endif
}

void loop() {
    // CRITICAL: Call update() every loop cycle for non-blocking timing
    carControl->update();
    climateControl->update();
    customKeys->update();
#ifdef ENABLE_WEBSERVER
    webServer->update();
#endif

    CANFrame frame;
    if (can.read(frame)) {
        // Pass to CarControl for state tracking
        carControl->onCanFrameReceived(frame);
        climateControl->onCanFrameReceived(frame);

        // Print frame for debugging
#ifdef DEBUG_MODE
        Serial.print("RX: 0x");
        if (frame.id < 0x100) Serial.print("0");
        if (frame.id < 0x10) Serial.print("0");
        Serial.print(frame.id, HEX);
        Serial.print(" Data:");

        for (uint8_t i = 0; i < frame.dlc; i++) {
            Serial.print(" ");
            if (frame.data[i] < 0x10) Serial.print("0");
            Serial.print(frame.data[i], HEX);
        }
        Serial.println();
#else
        Serial.printf("Engine ");
        IgnitionStatus ignition = carControl->getIgnitionStatus();
        switch (ignition) {
            case IGNITION_OFF:
                Serial.printf("OFF");
                break;
            case IGNITION_SECOND:
                Serial.printf("SECOND");
                break;
            case IGNITION_RUNNING:
                Serial.printf("RUNNING");
                break;
        }
        Serial.printf(", Battery: %0.2fV", carControl->getBatteryVoltage());
        Serial.printf(", RPM: %u", carControl->getEngineRPM());

        uint8_t throttle = carControl->getThrottlePosition();
        if (throttle == 255) {
            Serial.printf(", Throttle: KICKDOWN");
        } else {
            uint8_t percent = (throttle * 100) / 254;
            Serial.printf(", Throttle: %u%%", percent);
        }

        Serial.printf(", Steering: %.1f°", carControl->getSteeringWheelAngle());

        Serial.printf(", Climate - Fan: %u", climateControl->getFanSpeed());
        Serial.printf(" | Driver: %dC", climateControl->getDriverTemp());
        Serial.printf(" | Passenger: %dC", climateControl->getPassengerTemp());
        Serial.printf(" | AC: %s", climateControl->isACActive() ? "ON" : "OFF");
        Serial.println();
#endif

    }

    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (serialBuffer.length() > 0) {
                processSerialCommand(serialBuffer);
                serialBuffer = "";
            }
        } else {
            serialBuffer += c;
        }
    }
}
