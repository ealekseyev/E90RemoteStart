#ifndef CONFIG_H
#define CONFIG_H

// Feature toggles
#define DEBUG_MODE              // Comment out to disable debug mode (shows formatted status instead of raw CAN)
#define ENABLE_WEBSERVER        // Comment out to disable web server

// Serial configuration
#define SERIAL_BAUD_RATE 115200

// CAN configuration
#define CAN_BITRATE 100000  // 100kbps

// MCP2515 crystal frequency
#define MCP2515_CRYSTAL_8MHZ   // Options: MCP2515_CRYSTAL_8MHZ, MCP2515_CRYSTAL_16MHZ

// WiFi AP configuration (for web server)
#define WIFI_AP_SSID "CANDebugger"
#define WIFI_AP_PASSWORD "candebugger123"  // Min 8 characters
#define WIFI_AP_CHANNEL 6

// Non-blocking CAN operation timings
#define BUTTON_PRESS_DURATION_MS 200   // Button press/release delay
#define WINDOW_FRAME_INTERVAL_MS 200   // Window frame repeat interval
#define WINDOW_DURATION_MS 3000        // Window operation total duration

#define STEERING_BTN_VOLUME_UP 0b10000000
#define STEERING_BTN_VOLUME_DOWN 0b01000000
#define STEERING_BTN_SIRI 0b00100000
#define STEERING_BTN_PHONE 0b00010000
#define STEERING_BTN_CUSTOM 0b00001000
#define STEERING_BTN_CHANNEL 0b00000100
#define STEERING_BTN_PREV 0b00000010
#define STEERING_BTN_NEXT 0b00000001


#endif
