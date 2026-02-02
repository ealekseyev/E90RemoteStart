#ifndef CUSTOMKEYS_H
#define CUSTOMKEYS_H

#include "CarControl.hpp"

class CustomKeys {
public:
    static CustomKeys* getInstance();

    void init(CarControl* carControl);
    void update();  // Call every loop cycle

private:
    CustomKeys();
    CustomKeys(const CustomKeys&) = delete;
    CustomKeys& operator=(const CustomKeys&) = delete;

    static CustomKeys* instance;
    CarControl* carControl;

    // Button state machine
    enum ButtonState {
        IDLE,
        FIRST_PRESS_DOWN,
        WAITING_FOR_SECOND_PRESS,
        SECOND_PRESS_DOWN,
        LONG_PRESS_ACTIVE
    };

    ButtonState state;
    unsigned long pressStartTime;
    unsigned long firstReleaseTime;
    bool lastButtonState;

    // Timing constants (milliseconds)
    static const unsigned long LONG_PRESS_THRESHOLD = 800;
    static const unsigned long DOUBLE_PRESS_WINDOW = 400;

    // Event handlers
    void onSinglePress();
    void onDoublePress();
    void onLongPress();
};

#endif
