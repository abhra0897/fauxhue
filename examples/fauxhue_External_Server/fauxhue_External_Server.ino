#include <Arduino.h>
#if defined(ESP8266)
    #include <ESP8266WiFi.h>
#elif defined(ESP32)
    #include <WiFi.h>
#endif
#include <ESPAsyncWebServer.h>
#include "fauxhue.h"

// Rename the credentials.sample.h file to credentials.h and 
// edit it according to your router configuration
#include "credentials.h"

Fauxhue fauxhue;
AsyncWebServer server(80);

// -----------------------------------------------------------------------------

#define SERIAL_BAUDRATE                 115200
#define LED                             2

// -----------------------------------------------------------------------------
// Wifi
// -----------------------------------------------------------------------------

void wifiSetup() {

    // Set WIFI module to STA mode
    WiFi.mode(WIFI_STA);

    // Connect
    Serial.printf("[WIFI] Connecting to %s ", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    // Wait
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(100);
    }
    Serial.println();

    // Connected!
    Serial.printf("[WIFI] STATION Mode, SSID: %s, IP address: %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());

}

void serverSetup() {

    // Custom entry point (not required by the library, here just as an example)
    server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", "Hello, world");
    });

    // These two callbacks are required for gen1 and gen3 compatibility
    server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        if (fauxhue.process(request->client(), request->method() == HTTP_GET, request->url(), String((char *)data))) return;
        // Handle any other body request here...
    });
    server.onNotFound([](AsyncWebServerRequest *request) {
        String body = (request->hasParam("body", true)) ? request->getParam("body", true)->value() : String();
        if (fauxhue.process(request->client(), request->method() == HTTP_GET, request->url(), body)) return;
        // Handle not found request here...
    });

    // Start the server
    server.begin();

}

void setup() {

    // Init serial port and clean garbage
    Serial.begin(SERIAL_BAUDRATE);
    Serial.println();
    Serial.println();

    // LED
    pinMode(LED, OUTPUT);
    digitalWrite(LED, HIGH); // Our LED has inverse logic (high for OFF, low for ON)

    // Wifi
    wifiSetup();

    // Web server
    serverSetup();

    // Set Fauxhue to not create an internal TCP server and redirect requests to the server on the defined port
    // The TCP port must be 80 for gen3 devices (default is 1901)
    // This has to be done before the call to enable()
    fauxhue.createServer(false);
    fauxhue.setPort(80); // This is required for gen3 devices

    // You have to call enable(true) once you have a WiFi connection
    // You can enable or disable the library at any moment
    // Disabling it will prevent the devices from being discovered and switched
    fauxhue.enable(true);

    // You can use different ways to invoke alexa to modify the devices state:
    // "Alexa, turn kitchen on" ("kitchen" is the name of the first device below)
    // "Alexa, turn on kitchen"
    // "Alexa, set kitchen to fifty" (50 means 50% of brightness)

    // Add virtual devices
    fauxhue.addDevice("kitchen");
	fauxhue.addDevice("livingroom");

    // You can add more devices
	//fauxhue.addDevice("light 3");
    //fauxhue.addDevice("light 4");
    //fauxhue.addDevice("light 5");
    //fauxhue.addDevice("light 6");
    //fauxhue.addDevice("light 7");
    //fauxhue.addDevice("light 8");

    fauxhue.setStateCbHandler([](unsigned char device_id, const char * device_name, fauxhue_state_t state) {
        
        // Callback when a command from Alexa is received. 
        // Suported states so far --->
        //     on: true/false
        //     bri (brightness): 0-254
        //     hue: 0-65535, through the colorwheel from red to red
        //     sat (saturation): 0-254
        //     ct (color temperature): in mired, 153 - 500
        //     colormode: "hs"=HSV, "ct"=Color Temperature
        
        // if (0 == device_id) digitalWrite(RELAY1_PIN, state.on);
        // if (1 == device_id) digitalWrite(RELAY2_PIN, state.on);
        // if (2 == device_id) analogWrite(LED1_PIN, state.bri);
        
        fauxhue_rgb_t color = fauxhue.getColor(device_id);
        Serial.printf("[MAIN] Device #%d (%s)" 
                        "\r\n\t on: %s "
                        "\r\n\t bri: %d "
                        "\r\n\t hue: %d "
                        "\r\n\t sat: %d "
                        "\r\n\t ct: %d "
                        "\r\n\t colormode: %s \r\n"
                        "\r\n\t RGB Color: Red %d | Green %d | Blue %d \r\n", 
                        device_id, device_name, state.on ? "ON" : "OFF", state.bri, state.hue, state.sat, state.ct, state.colormode,  
                        color.red, color.green, color.blue);

        // For the example we are turning the same LED on and off regardless fo the device triggered or the value
        // Also, color values are not utilized here. Use analogWrite() on a RGB LED to use color values. 
        digitalWrite(LED, !(state.on)); // we are nor-ing the state because our LED has inverse logic.

    });

}

void loop() {

    // Fauxhue uses an async TCP server but a sync UDP server
    // Therefore, we have to manually poll for UDP packets
    fauxhue.handle();

    // This is a sample code to output free heap every 5 seconds
    // This is a cheap way to detect memory leaks
    static unsigned long last = millis();
    if (millis() - last > 5000) {
        last = millis();
        Serial.printf("[MAIN] Free heap: %d bytes\n", ESP.getFreeHeap());
    }

}
