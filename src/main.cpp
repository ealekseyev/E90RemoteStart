#include <Arduino.h>
#include "canbus.h"
#include "config.h"
#include "hardwarePins.h"
#include "CarControl.hpp"

CANBus can(MCP2515_CS_PIN);
CarControl* carControl = CarControl::getInstance();

#define DEBUG_MODE

String serialBuffer = "";
bool custom_pressed = false;
bool last_custom_pressed = false;

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
    Serial.println("CAN Ready");
}

void loop() {
    // CRITICAL: Call update() every loop cycle for non-blocking timing
    carControl->update();

    // Test: Toggle dome light every 2 seconds
    // static unsigned long lastToggle = 0;
    // static bool domeState = false;
    // if (millis() - lastToggle >= 1000) {
    //     lastToggle = millis();
    //     domeState = !domeState;
    //     carControl->setDomeLight(domeState);
    //     //Serial.print("Dome light: ");
    //     //Serial.println(domeState ? "ON" : "OFF");
    // }

    CANFrame frame;
    if (can.read(frame)) {
        // Pass to CarControl for state tracking
        carControl->onCanFrameReceived(frame);

        // Debounce custom button
        custom_pressed = carControl->isSteeringButtonPressed(STEERING_BTN_CUSTOM);
        if (custom_pressed && !last_custom_pressed) {
            // Check window positions and toggle based on state
            uint8_t posPassengerRear = carControl->getWindowPosition(PASSENGER_REAR);
            uint8_t posDriverRear = carControl->getWindowPosition(DRIVER_REAR);

            WindowPosition targetPos = (posPassengerRear < 128) ? WINDOW_ROLL_DOWN : WINDOW_ROLL_UP;
            carControl->setWindow(PASSENGER_REAR | DRIVER_REAR, targetPos);
        }
        last_custom_pressed = custom_pressed;
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
        Serial.printf(carControl->isEngineRunning()?"ON":"OFF");
        Serial.printf(", Battery: %0.2fV", carControl->getBatteryVoltage());
        Serial.printf(", RPM: %u", carControl->getEngineRPM());

        uint8_t throttle = carControl->getThrottlePosition();
        if (throttle == 255) {
            Serial.printf(", Throttle: KICKDOWN");
        } else {
            uint8_t percent = (throttle * 100) / 254;
            Serial.printf(", Throttle: %u%%", percent);
        }
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
