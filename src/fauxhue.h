/*

FAUXHUE

Copyright (C) 2024 by Avra Mitra

The MIT License (MIT)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

*/

#pragma once

#define FAUXHUE_UDP_MULTICAST_IP     IPAddress(239,255,255,250)
#define FAUXHUE_UDP_MULTICAST_PORT   1900
#define FAUXHUE_TCP_MAX_CLIENTS      10
#define FAUXHUE_TCP_PORT             1901
#define FAUXHUE_RX_TIMEOUT           3
#define FAUXHUE_DEVICE_UNIQUE_ID_LENGTH  27

#define DEBUG_FAUXHUE                Serial
#ifdef DEBUG_FAUXHUE
    #if defined(ARDUINO_ARCH_ESP32)
        #define DEBUG_MSG_FAUXHUE(fmt, ...) { DEBUG_FAUXHUE.printf_P((PGM_P) PSTR(fmt), ## __VA_ARGS__); }
    #else
        #define DEBUG_MSG_FAUXHUE(fmt, ...) { DEBUG_FAUXHUE.printf(fmt, ## __VA_ARGS__); }
    #endif
#else
    #define DEBUG_MSG_FAUXHUE(...)
#endif

#ifndef DEBUG_FAUXHUE_VERBOSE_TCP
#define DEBUG_FAUXHUE_VERBOSE_TCP    false
#endif

#ifndef DEBUG_FAUXHUE_VERBOSE_UDP
#define DEBUG_FAUXHUE_VERBOSE_UDP    false
#endif

#include <Arduino.h>

#if defined(ESP8266)
    #include <ESP8266WiFi.h>
    #include <ESPAsyncTCP.h>
#elif defined(ESP32)
    #include <WiFi.h>
    #include <AsyncTCP.h>
#elif defined(ARDUINO_RASPBERRY_PI_PICO_W)
    #include <AsyncTCP_RP2040W.h>
#else
	#error Platform not supported
#endif

#include <WiFiUdp.h>
#include <functional>
#include <vector>
#include <MD5Builder.h>
#include "templates.h"


typedef struct {
    bool on;
    uint8_t bri;
    uint16_t hue;
    uint8_t sat;
    uint16_t ct;
    char colormode[3];
} fauxhue_state_t;

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} fauxhue_rgb_t;

typedef struct {
    char * name;
    fauxhue_state_t state;
    fauxhue_rgb_t color;
    char uniqueid[FAUXHUE_DEVICE_UNIQUE_ID_LENGTH];
} fauxhue_device_t;

typedef std::function<void(uint8_t, const char *, fauxhue_state_t)> TSetStateCallback;

class Fauxhue {

    public:

        ~Fauxhue();

        unsigned char addDevice(const char * device_name);
        bool renameDevice(uint8_t id, const char * device_name);
        bool renameDevice(const char * old_device_name, const char * new_device_name);
        bool removeDevice(uint8_t id);
        bool removeDevice(const char * device_name);
        char * getDeviceName(uint8_t id, char * buffer, size_t len);
        int getDeviceId(const char * device_name);
        void setDeviceUniqueId(uint8_t id, const char *uniqueid);
        void setStateCbHandler(TSetStateCallback fn) { _setCallback = fn; }

        fauxhue_rgb_t getColor(uint8_t id);
        char * getColormode(uint8_t id, char colormode[3]);

        bool setState(uint8_t id, fauxhue_state_t state);
        bool setState(const char * device_name, fauxhue_state_t state);
        bool setStateBri(uint8_t id, bool on, uint8_t bri);
        bool setStateBri(const char * device_name, bool on, uint8_t bri);
        bool setStateHueSat(uint8_t id, uint16_t hue, uint8_t sat);
        bool setStateHueSat(const char * device_name, uint16_t hue, uint8_t sat);
        bool setStateColTemp(uint8_t id, uint16_t ct);
        bool setStateColTemp(const char * device_name, uint16_t ct);

        bool process(AsyncClient *client, bool isGet, String url, String body);
        void enable(bool enable);
        void createServer(bool internal) { _internal = internal; }
        void setPort(unsigned long tcp_port) { _tcp_port = tcp_port; }
        void handle();

    private:

        AsyncServer * _server;
        bool _enabled = false;
        bool _internal = true;
        unsigned int _tcp_port = FAUXHUE_TCP_PORT;
        std::vector<fauxhue_device_t> _devices;
		#ifdef ESP8266
        WiFiEventHandler _handler;
		#endif
        WiFiUDP _udp;
        AsyncClient * _tcpClients[FAUXHUE_TCP_MAX_CLIENTS];
        TSetStateCallback _setCallback = NULL;

        String _deviceJson(uint8_t id, bool all); 	// all = true means we are listing all devices so use full description template

        void _setRGBFromHSB(uint8_t id);
        void _adjustRGBFromBri(uint8_t id);
        void _setRGBFromCT(uint8_t id);

        void _handleUDP();
        void _onUDPData(const IPAddress remoteIP, unsigned int remotePort, void *data, size_t len);
        void _sendUDPResponse();

        void _onTCPClient(AsyncClient *client);
        bool _onTCPData(AsyncClient *client, void *data, size_t len);
        bool _onTCPRequest(AsyncClient *client, bool isGet, String url, String body);
        bool _onTCPDescription(AsyncClient *client, String url, String body);
        bool _onTCPList(AsyncClient *client, String url, String body);
        bool _onTCPControl(AsyncClient *client, String url, String body);
        void _sendTCPResponse(AsyncClient *client, const char * code, char * body, const char * mime);

        String _byte2hex(uint8_t zahl);
        String _makeMD5(String text);
};
