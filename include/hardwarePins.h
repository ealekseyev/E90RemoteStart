#ifndef HARDWARE_PINS_H
#define HARDWARE_PINS_H

// ESP8266 standard SPI pins:
// MOSI: GPIO13 (D7)
// MISO: GPIO12 (D6)
// SCK:  GPIO14 (D5)
// CS:   GPIO15 (D8)

// MCP2515 CAN Controller
#define MCP2515_CS_PIN 15  // GPIO15 (D8)
#define MCP2515_INT_PIN 4  // GPIO4 (D2) - Interrupt pin

#endif
