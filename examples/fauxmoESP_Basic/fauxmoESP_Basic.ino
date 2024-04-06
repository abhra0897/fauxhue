#include <Arduino.h>
#ifdef ESP32
    #include <WiFi.h>
#else
    #include <ESP8266WiFi.h>
#endif
#include "fauxhue.h"

// Rename the credentials.sample.h file to credentials.h and 
// edit it according to your router configuration
#include "credentials.h"

Fauxhue fauxhue;

// -----------------------------------------------------------------------------

#define SERIAL_BAUDRATE     115200

#define LED_YELLOW          4
#define LED_GREEN           5
#define LED_BLUE            0
#define LED_PINK            2
#define LED_WHITE           15

#define ID_YELLOW           "yellow lamp"
#define ID_GREEN            "green lamp"
#define ID_BLUE             "blue lamp"
#define ID_PINK             "pink lamp"
#define ID_WHITE            "white lamp"

// -----------------------------------------------------------------------------

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

void setup() {

    // Init serial port and clean garbage
    Serial.begin(SERIAL_BAUDRATE);
    Serial.println();
    Serial.println();

    // LEDs
    pinMode(LED_YELLOW, OUTPUT);
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_BLUE, OUTPUT);
    pinMode(LED_PINK, OUTPUT);
    pinMode(LED_WHITE, OUTPUT);
    digitalWrite(LED_YELLOW, LOW);
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_BLUE, LOW);
    digitalWrite(LED_PINK, LOW);
    digitalWrite(LED_WHITE, LOW);

    // Wifi
    wifiSetup();

    // By default, Fauxhue creates it's own webserver on the defined port
    // The TCP port must be 80 for gen3 devices (default is 1901)
    // This has to be done before the call to enable()
    fauxhue.createServer(true); // not needed, this is the default value
    fauxhue.setPort(80); // This is required for gen3 devices

    // You have to call enable(true) once you have a WiFi connection
    // You can enable or disable the library at any moment
    // Disabling it will prevent the devices from being discovered and switched
    fauxhue.enable(true);

    // You can use different ways to invoke alexa to modify the devices state:
    // "Alexa, turn yellow lamp on"
    // "Alexa, turn on yellow lamp
    // "Alexa, set yellow lamp to fifty" (50 means 50% of brightness, note, this example does not use this functionality)

    // Add virtual devices
    fauxhue.addDevice(ID_YELLOW);
    fauxhue.addDevice(ID_GREEN);
    fauxhue.addDevice(ID_BLUE);
    fauxhue.addDevice(ID_PINK);
    fauxhue.addDevice(ID_WHITE);

    fauxhue.setStateCbHandler([](unsigned char device_id, const char * device_name, fauxhue_state_t state) {
        
        // Callback when a command from Alexa is received. 
        // Suported states so far --->
        //     on: true/false
        //     bri (brightness): 0-254
        //     hue: 0-65535, through the colorwheel from red to red
        //     sat (saturation): 0-254
        //     ct (color temperature): in mired, 153 - 500
        //     colormode: "hs"=HSV, "ct"=Color Temperature
        
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

        // Checking for device_id is simpler if you are certain about the order they are loaded and it does not change.
        // Otherwise comparing the device_name is safer.

        // NOTE: Color values are not utilized here. 
        // Get RGB LEDs and use analogWrite() to change color of the LED

        if (strcmp(device_name, ID_YELLOW)==0) {
            digitalWrite(LED_YELLOW, state.on ? HIGH : LOW);
        } else if (strcmp(device_name, ID_GREEN)==0) {
            digitalWrite(LED_GREEN, state.on ? HIGH : LOW);
        } else if (strcmp(device_name, ID_BLUE)==0) {
            digitalWrite(LED_BLUE, state.on ? HIGH : LOW);
        } else if (strcmp(device_name, ID_PINK)==0) {
            digitalWrite(LED_PINK, state.on ? HIGH : LOW);
        } else if (strcmp(device_name, ID_WHITE)==0) {
            digitalWrite(LED_WHITE, state.on ? HIGH : LOW);
        }

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

    // If your device state is changed by any other means (MQTT, physical button,...)
    // you can instruct the library to report the new state to Alexa on next request:
    // setState()         <-- Set all states 
    // setStateBri()      <-- Set brightness state
    // setStateHueSat()   <-- Set hue and saturation state
    // setStateColTemp()  <-- Set color temperature state

}
