#ifndef WEBSERVER_HPP
#define WEBSERVER_HPP

#include <Arduino.h>
#include "CarControl.hpp"
#include "ClimateControl.hpp"

class VehicleWebServer {
public:
    static VehicleWebServer* getInstance();

    void init(CarControl* car, ClimateControl* climate);
    void update();  // Call in loop

private:
    VehicleWebServer();
    static VehicleWebServer* instance;

    CarControl* carControl;
    ClimateControl* climateControl;

    void setupAP();
    void setupRoutes();

    // HTTP handlers
    static void handleRoot();
    static void handleData();
};

#endif
