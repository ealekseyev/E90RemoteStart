#include "WebServer.hpp"
#include "config.h"
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

VehicleWebServer* VehicleWebServer::instance = nullptr;
ESP8266WebServer server(80);

// Static pointers for handlers
static CarControl* g_carControl = nullptr;
static ClimateControl* g_climateControl = nullptr;

VehicleWebServer::VehicleWebServer() : carControl(nullptr), climateControl(nullptr) {
}

VehicleWebServer* VehicleWebServer::getInstance() {
    if (instance == nullptr) {
        instance = new VehicleWebServer();
    }
    return instance;
}

void VehicleWebServer::init(CarControl* car, ClimateControl* climate) {
    carControl = car;
    climateControl = climate;

    g_carControl = car;
    g_climateControl = climate;

    setupAP();
    setupRoutes();

    server.begin();
    Serial.println("Web server started");
}

void VehicleWebServer::setupAP() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD, WIFI_AP_CHANNEL);

    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);
}

void VehicleWebServer::setupRoutes() {
    server.on("/", handleRoot);
    server.on("/data", handleData);
}

void VehicleWebServer::update() {
    server.handleClient();
}

void VehicleWebServer::handleRoot() {
    String html = F(
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<title>CAN Debugger</title>"
        "<style>"
        "body { font-family: Arial, sans-serif; margin: 20px; background: #1a1a1a; color: #eee; }"
        "h1 { color: #4CAF50; }"
        "h2 { color: #2196F3; border-bottom: 2px solid #2196F3; padding-bottom: 5px; }"
        ".section { background: #2a2a2a; padding: 15px; margin: 10px 0; border-radius: 8px; }"
        ".data-row { display: flex; justify-content: space-between; padding: 8px; border-bottom: 1px solid #444; }"
        ".data-row:last-child { border-bottom: none; }"
        ".label { font-weight: bold; color: #aaa; }"
        ".value { color: #4CAF50; font-family: monospace; }"
        ".status-on { color: #4CAF50; }"
        ".status-off { color: #f44336; }"
        "</style>"
        "<script>"
        "function updateData() {"
        "  fetch('/data').then(r => r.json()).then(data => {"
        "    document.getElementById('engineRPM').textContent = data.engineRPM + ' RPM';"
        "    document.getElementById('throttle').textContent = data.throttle;"
        "    document.getElementById('steering').textContent = data.steering + '°';"
        "    document.getElementById('battery').textContent = data.battery + 'V';"
        "    document.getElementById('engineRunning').textContent = data.engineRunning;"
        "    document.getElementById('engineRunning').className = 'value status-' + (data.engineRunning === 'RUNNING' ? 'on' : 'off');"
        "    document.getElementById('braking').textContent = data.braking;"
        "    document.getElementById('braking').className = 'value status-' + (parseInt(data.braking) > 0 ? 'on' : 'off');"
        "    document.getElementById('parkingBrake').textContent = data.parkingBrake;"
        "    document.getElementById('parkingBrake').className = 'value status-' + (data.parkingBrake === 'ON' ? 'on' : 'off');"
        "    document.getElementById('doorLocked').textContent = data.doorLocked;"
        "    document.getElementById('doorOpen').textContent = data.doorOpen;"
        "    document.getElementById('doorOpen').className = 'value status-' + (data.doorOpen === 'All doors closed' ? 'off' : 'on');"
        "    document.getElementById('fanSpeed').textContent = data.fanSpeed;"
        "    document.getElementById('blowerState').textContent = data.blowerState;"
        "    document.getElementById('driverTemp').textContent = data.driverTemp + '°C';"
        "    document.getElementById('passengerTemp').textContent = data.passengerTemp + '°C';"
        "    document.getElementById('acActive').textContent = data.acActive;"
        "    document.getElementById('acActive').className = 'value status-' + (data.acActive === 'ON' ? 'on' : 'off');"
        "    document.getElementById('windowDF').textContent = data.windowDF + '%';"
        "    document.getElementById('windowPF').textContent = data.windowPF + '%';"
        "    document.getElementById('windowDR').textContent = data.windowDR + '%';"
        "    document.getElementById('windowPR').textContent = data.windowPR + '%';"
        "  });"
        "}"
        "setInterval(updateData, 500);"
        "window.onload = updateData;"
        "</script>"
        "</head>"
        "<body>"
        "<h1>BMW CAN Debugger</h1>"

        "<div class='section'>"
        "<h2>Engine & Powertrain</h2>"
        "<div class='data-row'><span class='label'>Engine Status:</span><span id='engineRunning' class='value'>-</span></div>"
        "<div class='data-row'><span class='label'>RPM:</span><span id='engineRPM' class='value'>-</span></div>"
        "<div class='data-row'><span class='label'>Throttle:</span><span id='throttle' class='value'>-</span></div>"
        "<div class='data-row'><span class='label'>Battery:</span><span id='battery' class='value'>-</span></div>"
        "</div>"

        "<div class='section'>"
        "<h2>Vehicle Status</h2>"
        "<div class='data-row'><span class='label'>Braking:</span><span id='braking' class='value'>-</span></div>"
        "<div class='data-row'><span class='label'>Parking Brake:</span><span id='parkingBrake' class='value'>-</span></div>"
        "<div class='data-row'><span class='label'>Steering Angle:</span><span id='steering' class='value'>-</span></div>"
        "<div class='data-row'><span class='label'>Doors Locked:</span><span id='doorLocked' class='value'>-</span></div>"
        "<div class='data-row'><span class='label'>Doors Open:</span><span id='doorOpen' class='value'>-</span></div>"
        "</div>"

        "<div class='section'>"
        "<h2>Climate Control</h2>"
        "<div class='data-row'><span class='label'>Fan Speed:</span><span id='fanSpeed' class='value'>-</span></div>"
        "<div class='data-row'><span class='label'>Blower:</span><span id='blowerState' class='value'>-</span></div>"
        "<div class='data-row'><span class='label'>Driver Temp:</span><span id='driverTemp' class='value'>-</span></div>"
        "<div class='data-row'><span class='label'>Passenger Temp:</span><span id='passengerTemp' class='value'>-</span></div>"
        "<div class='data-row'><span class='label'>AC Active:</span><span id='acActive' class='value'>-</span></div>"
        "</div>"

        "<div class='section'>"
        "<h2>Windows</h2>"
        "<div class='data-row'><span class='label'>Driver Front:</span><span id='windowDF' class='value'>-</span></div>"
        "<div class='data-row'><span class='label'>Passenger Front:</span><span id='windowPF' class='value'>-</span></div>"
        "<div class='data-row'><span class='label'>Driver Rear:</span><span id='windowDR' class='value'>-</span></div>"
        "<div class='data-row'><span class='label'>Passenger Rear:</span><span id='windowPR' class='value'>-</span></div>"
        "</div>"

        "</body>"
        "</html>"
    );

    server.send(200, "text/html", html);
}

void VehicleWebServer::handleData() {
    if (!g_carControl || !g_climateControl) {
        server.send(500, "application/json", "{\"error\":\"Not initialized\"}");
        return;
    }

    // Build JSON response
    String json = "{";

    // Engine & Powertrain
    json += "\"engineRPM\":" + String(g_carControl->getEngineRPM()) + ",";

    uint8_t throttle = g_carControl->getThrottlePosition();
    if (throttle == 255) {
        json += "\"throttle\":\"KICKDOWN\",";
    } else {
        uint8_t percent = (throttle * 100) / 254;
        json += "\"throttle\":\"" + String(percent) + "%\",";
    }

    json += "\"steering\":" + String(g_carControl->getSteeringWheelAngle(), 1) + ",";
    json += "\"battery\":" + String(g_carControl->getBatteryVoltage(), 2) + ",";

    IgnitionStatus ignition = g_carControl->getIgnitionStatus();
    const char* ignitionStr = (ignition == IGNITION_OFF) ? "OFF" :
                             (ignition == IGNITION_SECOND) ? "SECOND" : "RUNNING";
    json += "\"engineRunning\":\"" + String(ignitionStr) + "\",";


    // Vehicle Status
    uint8_t brakePercent = (g_carControl->getBrakeStatus() * 100) / 255;
    json += "\"braking\":\"" + String(brakePercent) + "%\",";
    json += "\"parkingBrake\":\"" + String(g_carControl->isParkingBrakeOn() ? "ON" : "OFF") + "\",";
    json += "\"doorLocked\":\"" + String(g_carControl->isDoorLocked() ? "YES" : "NO") + "\",";

    // Build list of open doors
    String openDoors = "";
    if (g_carControl->isDoorOpen(DRIVER_FRONT)) openDoors += "Driver Front, ";
    if (g_carControl->isDoorOpen(PASSENGER_FRONT)) openDoors += "Passenger Front, ";
    if (g_carControl->isDoorOpen(DRIVER_REAR)) openDoors += "Driver Rear, ";
    if (g_carControl->isDoorOpen(PASSENGER_REAR)) openDoors += "Passenger Rear, ";

    if (openDoors.length() > 0) {
        openDoors = openDoors.substring(0, openDoors.length() - 2);  // Remove trailing ", "
    } else {
        openDoors = "All doors closed";
    }
    json += "\"doorOpen\":\"" + openDoors + "\",";

    // Climate
    json += "\"fanSpeed\":\"" + String(g_climateControl->getFanSpeed()) + "\",";
    json += "\"driverTemp\":" + String(g_climateControl->getDriverTemp()) + ",";
    json += "\"passengerTemp\":" + String(g_climateControl->getPassengerTemp()) + ",";
    json += "\"acActive\":\"" + String(g_climateControl->isACActive() ? "ON" : "OFF") + "\",";

    // Blower distribution
    uint8_t blower = g_climateControl->getBlowerState();
    String blowerStr = "";
    if (blower == BLOWER_AUTO) {
        blowerStr = "AUTO";
    } else {
        if (blower & BLOWER_WINDSHIELD) blowerStr += "Windshield, ";
        if (blower & BLOWER_CENTER) blowerStr += "Center, ";
        if (blower & BLOWER_FOOTWELL) blowerStr += "Footwell, ";
        if (blowerStr.length() > 0) {
            blowerStr = blowerStr.substring(0, blowerStr.length() - 2);  // Remove trailing ", "
        }
    }
    json += "\"blowerState\":\"" + blowerStr + "\",";

    // Windows (as percentage)
    uint8_t wdf = (g_carControl->getWindowPosition(DRIVER_FRONT) * 100) / 255;
    uint8_t wpf = (g_carControl->getWindowPosition(PASSENGER_FRONT) * 100) / 255;
    uint8_t wdr = (g_carControl->getWindowPosition(DRIVER_REAR) * 100) / 255;
    uint8_t wpr = (g_carControl->getWindowPosition(PASSENGER_REAR) * 100) / 255;

    json += "\"windowDF\":" + String(wdf) + ",";
    json += "\"windowPF\":" + String(wpf) + ",";
    json += "\"windowDR\":" + String(wdr) + ",";
    json += "\"windowPR\":" + String(wpr);

    json += "}";

    server.send(200, "application/json", json);
}
