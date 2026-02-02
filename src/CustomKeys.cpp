#include "CustomKeys.h"
#include "config.h"
#include "canbus.h"

extern CANBus can;
extern CarControl* carControl;

CustomKeys* CustomKeys::instance = nullptr;

CustomKeys::CustomKeys()
    : carControl(nullptr),
      state(IDLE),
      pressStartTime(0),
      firstReleaseTime(0),
      lastButtonState(false) {
}

CustomKeys* CustomKeys::getInstance() {
    if (instance == nullptr) {
        instance = new CustomKeys();
    }
    return instance;
}

void CustomKeys::init(CarControl* car) {
    carControl = car;
    state = IDLE;
}

void CustomKeys::update() {
    if (!carControl) return;

    bool buttonPressed = carControl->isSteeringButtonPressed(STEERING_BTN_CUSTOM);
    unsigned long currentTime = millis();

    switch (state) {
        case IDLE:
            if (buttonPressed && !lastButtonState) {
                // Button pressed (rising edge)
                state = FIRST_PRESS_DOWN;
                pressStartTime = currentTime;
            }
            break;

        case FIRST_PRESS_DOWN:
            if (!buttonPressed) {
                // Button released (short press)
                state = WAITING_FOR_SECOND_PRESS;
                firstReleaseTime = currentTime;
            } else if (currentTime - pressStartTime >= LONG_PRESS_THRESHOLD) {
                // Held long enough for long press
                state = LONG_PRESS_ACTIVE;
                onLongPress();
            }
            break;

        case WAITING_FOR_SECOND_PRESS:
            if (buttonPressed && !lastButtonState) {
                // Second press detected
                state = SECOND_PRESS_DOWN;
            } else if (currentTime - firstReleaseTime >= DOUBLE_PRESS_WINDOW) {
                // Timeout - was a single press
                onSinglePress();
                state = IDLE;
            }
            break;

        case SECOND_PRESS_DOWN:
            if (!buttonPressed) {
                // Second press released - double press confirmed
                onDoublePress();
                state = IDLE;
            }
            break;

        case LONG_PRESS_ACTIVE:
            if (!buttonPressed) {
                // Long press ended
                state = IDLE;
            }
            break;
    }

    lastButtonState = buttonPressed;
}

void CustomKeys::onSinglePress() {
    carControl->playGong();
}

void CustomKeys::onDoublePress() {
    static WindowPosition lastAction = WINDOW_ROLL_UP; // false is roll down, true is roll up
    // Current window toggle implementation
    if (!carControl) return;

    uint8_t posPassengerRear = carControl->getWindowPosition(PASSENGER_REAR);

    WindowPosition targetPos;
    // if the window is rolled mostly up but last action was to roll down, roll back up
    if(posPassengerRear > 230) {
        targetPos = WINDOW_ROLL_UP;
    // if the window is rolled mostly down but last action was to roll up, roll back down
    } else if(posPassengerRear < 25) {
        targetPos = WINDOW_ROLL_DOWN;
    } else {
        targetPos = (lastAction == WINDOW_ROLL_UP) ? WINDOW_ROLL_DOWN:WINDOW_ROLL_UP;
    }

    carControl->setWindow(PASSENGER_REAR | DRIVER_REAR, targetPos);

    lastAction = targetPos;
}

void CustomKeys::onLongPress() {
    // Empty for now - placeholder for future functionality
}
