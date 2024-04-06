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

#include <Arduino.h>
#include "fauxhue.h"

// -----------------------------------------------------------------------------
// UDP
// -----------------------------------------------------------------------------

void Fauxhue::_sendUDPResponse() {

	DEBUG_MSG_FAUXHUE("[FAUXHUE] Responding to M-SEARCH request\r\n");

	IPAddress ip = WiFi.localIP();
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    mac.toLowerCase();

	char response[strlen(FAUXHUE_UDP_RESPONSE_TEMPLATE) + 128];
    snprintf_P(
        response, sizeof(response),
        FAUXHUE_UDP_RESPONSE_TEMPLATE,
        ip[0], ip[1], ip[2], ip[3],
		_tcp_port,
        mac.c_str(), mac.c_str()
    );

	#if DEBUG_FAUXHUE_VERBOSE_UDP
    	DEBUG_MSG_FAUXHUE("[FAUXHUE] UDP response sent to %s:%d\r\n%s", _udp.remoteIP().toString().c_str(), _udp.remotePort(), response);
	#endif

    _udp.beginPacket(_udp.remoteIP(), _udp.remotePort());
	#if defined(ESP32)
	    _udp.printf(response);
	#else
	    _udp.write(response);
	#endif
    _udp.endPacket();

}

void Fauxhue::_handleUDP() {

	int len = _udp.parsePacket();
    if (len > 0) {

		unsigned char data[len+1];
        _udp.read(data, len);
        data[len] = 0;

		#if DEBUG_FAUXHUE_VERBOSE_UDP
			DEBUG_MSG_FAUXHUE("[FAUXHUE] UDP packet received\r\n%s", (const char *) data);
		#endif

        String request = (const char *) data;
        if (request.indexOf("M-SEARCH") >= 0) {
            if ((request.indexOf("ssdp:discover") > 0) || (request.indexOf("upnp:rootdevice") > 0) || (request.indexOf("device:basic:1") > 0)) {
                _sendUDPResponse();
            }
        }
    }

}


// -----------------------------------------------------------------------------
// TCP
// -----------------------------------------------------------------------------

void Fauxhue::_sendTCPResponse(AsyncClient *client, const char * code, char * body, const char * mime) {

	char headers[strlen_P(FAUXHUE_TCP_HEADERS) + 32];
	snprintf_P(
		headers, sizeof(headers),
		FAUXHUE_TCP_HEADERS,
		code, mime, strlen(body)
	);

	#if DEBUG_FAUXHUE_VERBOSE_TCP
		DEBUG_MSG_FAUXHUE("[FAUXHUE] Response:\r\n%s%s\r\n", headers, body);
	#endif

	client->write(headers);
	client->write(body);

}

String Fauxhue::_deviceJson(uint8_t id, bool all = true) {

	if (id >= _devices.size()) return "{}";

	fauxhue_device_t device = _devices[id];

	DEBUG_MSG_FAUXHUE("[FAUXHUE] Sending device info for \"%s\", uniqueID = \"%s\"\r\n", device.name, device.uniqueid);
	char buffer[strlen_P(FAUXHUE_DEVICE_JSON_TEMPLATE) + 64];

	if (all)
	{
		snprintf_P(
			buffer, sizeof(buffer),
			FAUXHUE_DEVICE_JSON_TEMPLATE,
			device.name, device.uniqueid,
			device.state.on ? "true": "false",
			device.state.bri,
			device.state.colormode,
			device.state.hue,
			device.state.sat,
			device.state.ct
		);
	}
	else
	{
		snprintf_P(
			buffer, sizeof(buffer),
			FAUXHUE_DEVICE_JSON_TEMPLATE_SHORT,
			device.name, device.uniqueid
		);
	}

	return String(buffer);
}

String Fauxhue::_byte2hex(uint8_t zahl)
{
  String hstring = String(zahl, HEX);
  if (zahl < 16)
  {
    hstring = "0" + hstring;
  }

  return hstring;
}

String Fauxhue::_makeMD5(String text)
{
  unsigned char bbuf[16];
  String hash = "";
  MD5Builder md5;
  md5.begin();
  md5.add(text);
  md5.calculate();
  
  md5.getBytes(bbuf);
  for (uint8_t i = 0; i < 16; i++)
  {
    hash += _byte2hex(bbuf[i]);
  }

  return hash;
}

bool Fauxhue::_onTCPDescription(AsyncClient *client, String url, String body) {

	(void) url;
	(void) body;

	DEBUG_MSG_FAUXHUE("[FAUXHUE] Handling /description.xml request\r\n");

	IPAddress ip = WiFi.localIP();
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    mac.toLowerCase();

	char response[strlen_P(FAUXHUE_DESCRIPTION_TEMPLATE) + 64];
    snprintf_P(
        response, sizeof(response),
        FAUXHUE_DESCRIPTION_TEMPLATE,
        ip[0], ip[1], ip[2], ip[3], _tcp_port,
        ip[0], ip[1], ip[2], ip[3], _tcp_port,
        mac.c_str(), mac.c_str()
    );

	_sendTCPResponse(client, "200 OK", response, "text/xml");

	return true;

}

bool Fauxhue::_onTCPList(AsyncClient *client, String url, String body) {

	DEBUG_MSG_FAUXHUE("[FAUXHUE] Handling list request\r\n");

	// Get the index
	int pos = url.indexOf("lights");
	if (-1 == pos) return false;

	// Get the id
	uint8_t id = url.substring(pos+7).toInt();

	// This will hold the response string	
	String response;

	// Client is requesting all devices
	if (0 == id) {

		response += "{";
		for (unsigned char i=0; i< _devices.size(); i++) {
			if (i>0) response += ",";
			response += "\"" + String(i+1) + "\":" + _deviceJson(i, false);	// send short template
		}
		response += "}";

	// Client is requesting a single device
	} else {
		response = _deviceJson(id-1);
	}

	_sendTCPResponse(client, "200 OK", (char *) response.c_str(), "application/json");
	
	return true;

}

bool Fauxhue::_onTCPControl(AsyncClient *client, String url, String body) {

	// "devicetype" request
	if (body.indexOf("devicetype") > 0) {
		DEBUG_MSG_FAUXHUE("[FAUXHUE] Handling devicetype request\r\n");
		_sendTCPResponse(client, "200 OK", (char *) "[{\"success\":{\"username\": \"2WLEDHardQrI3WHYTHoMcXHgEspsM8ZZRpSKtBQr\"}}]", "application/json");
		return true;
	}

	// "state" request
	if ((url.indexOf("state") > 0) && (body.length() > 0)) {

		// Get the index
		int pos = url.indexOf("lights");
		if (-1 == pos) return false;

		DEBUG_MSG_FAUXHUE("[FAUXHUE] Handling state request\r\n");

		// Get the index
		uint8_t id = url.substring(pos+7).toInt();
		if (id > 0) {

			--id;

			// Brightness
			pos = body.indexOf("bri");
			if (pos > 0) {
				unsigned char value = body.substring(pos+5).toInt();
				_devices[id].state.bri = value;
				_devices[id].state.on = (value > 0);
				_adjustRGBFromBri(id);
			} else if (body.indexOf("false") > 0) {
				_devices[id].state.on = false;
			} else {
				_devices[id].state.on = true;
				if (0 == _devices[id].state.bri) {
					_devices[id].state.bri = 254; //Fixme: 254 or 255????
					_setRGBFromHSB(id);
				}
			}

			// Hue
			pos = body.indexOf("hue");
			if (pos > 0) {
				uint16_t hue = body.substring(pos+5).toInt();
				_devices[id].state.hue = hue;
				strcpy(_devices[id].state.colormode, "hs");
			}

			// Saturation
			pos = body.indexOf("sat");
			if (pos > 0) {
				unsigned char sat = body.substring(pos+5).toInt();
				_devices[id].state.sat = sat;
				strcpy(_devices[id].state.colormode, "hs");
				_setRGBFromHSB(id);
			}

			// color temperature (ct)
			pos = body.indexOf("ct");
			if (pos > 0) {
				uint16_t ct = body.substring(pos+4).toInt();
				_devices[id].state.ct = ct;
				strcpy(_devices[id].state.colormode, "ct");
				_setRGBFromCT(id);
			}

			// TODO: Only respond with states that are changed. Needed??
			char response[strlen_P(FAUXHUE_TCP_STATE_RESPONSE)+20];
			snprintf_P(
				response, sizeof(response),
				FAUXHUE_TCP_STATE_RESPONSE,
				id+1, _devices[id].state.on ? "true" : "false", 
				id+1, _devices[id].state.bri, 
				id+1, _devices[id].state.hue, 
				id+1, _devices[id].state.sat,
				id+1, _devices[id].state.ct
			);
			_sendTCPResponse(client, "200 OK", response, "text/xml");

			if (_setCallback) {
				_setCallback(id, _devices[id].name, _devices[id].state);
			}

			return true;

		}

	}

	return false;
	
}

bool Fauxhue::_onTCPRequest(AsyncClient *client, bool isGet, String url, String body) {

    if (!_enabled) return false;

	#if DEBUG_FAUXHUE_VERBOSE_TCP
		DEBUG_MSG_FAUXHUE("[FAUXHUE] isGet: %s\r\n", isGet ? "true" : "false");
		DEBUG_MSG_FAUXHUE("[FAUXHUE] URL: %s\r\n", url.c_str());
		if (!isGet) DEBUG_MSG_FAUXHUE("[FAUXHUE] Body:\r\n%s\r\n", body.c_str());
	#endif

	if (url.equals("/description.xml")) {
        return _onTCPDescription(client, url, body);
    }

	// Read to undestand the API: https://developers.meethue.com/develop/get-started-2/
	// Also this readme: https://github.com/tigoe/hue-control?tab=readme-ov-file
	if (url.startsWith("/api")) {
		if (isGet) {
			return _onTCPList(client, url, body);
		} else {
       		return _onTCPControl(client, url, body);
		}
	}

	return false;

}

bool Fauxhue::_onTCPData(AsyncClient *client, void *data, size_t len) {

    if (!_enabled) return false;

	char * p = (char *) data;
	p[len] = 0;

	#if DEBUG_FAUXHUE_VERBOSE_TCP
		DEBUG_MSG_FAUXHUE("[FAUXHUE] TCP request\r\n%s\r\n", p);
	#endif

	// Method is the first word of the request
	char * method = p;

	while (*p != ' ') p++;
	*p = 0;
	p++;
	
	// Split word and flag start of url
	char * url = p;

	// Find next space
	while (*p != ' ') p++;
	*p = 0;
	p++;

	// Find double line feed
	unsigned char c = 0;
	while ((*p != 0) && (c < 2)) {
		if (*p != '\r') {
			c = (*p == '\n') ? c + 1 : 0;
		}
		p++;
	}
	char * body = p;

	bool isGet = (strncmp(method, "GET", 3) == 0);

	return _onTCPRequest(client, isGet, url, body);

}

void Fauxhue::_onTCPClient(AsyncClient *client) {

	if (_enabled) {

	    for (unsigned char i = 0; i < FAUXHUE_TCP_MAX_CLIENTS; i++) {

	        if (!_tcpClients[i] || !_tcpClients[i]->connected()) {

	            _tcpClients[i] = client;

	            client->onAck([i](void *s, AsyncClient *c, size_t len, uint32_t time) {
	            }, 0);

	            client->onData([this, i](void *s, AsyncClient *c, void *data, size_t len) {
	                _onTCPData(c, data, len);
	            }, 0);
	            client->onDisconnect([this, i](void *s, AsyncClient *c) {
					if(_tcpClients[i] != NULL) {
						_tcpClients[i]->free();
						_tcpClients[i] = NULL;
					} 
					else {
						DEBUG_MSG_FAUXHUE("[FAUXHUE] Client %d already disconnected\r\n", i);
					}
					delete c;
					DEBUG_MSG_FAUXHUE("[FAUXHUE] Client #%d disconnected\r\n", i);
	            }, 0);

	            client->onError([i](void *s, AsyncClient *c, int8_t error) {
	                DEBUG_MSG_FAUXHUE("[FAUXHUE] Error %s (%d) on client #%d\r\n", c->errorToString(error), error, i);
	            }, 0);

	            client->onTimeout([i](void *s, AsyncClient *c, uint32_t time) {
	                DEBUG_MSG_FAUXHUE("[FAUXHUE] Timeout on client #%d at %i\r\n", i, time);
	                c->close();
	            }, 0);

                    client->setRxTimeout(FAUXHUE_RX_TIMEOUT);

	            DEBUG_MSG_FAUXHUE("[FAUXHUE] Client #%d connected\r\n", i);
	            return;

	        }

	    }

		DEBUG_MSG_FAUXHUE("[FAUXHUE] Rejecting - Too many connections\r\n");

	} else {
		DEBUG_MSG_FAUXHUE("[FAUXHUE] Rejecting - Disabled\r\n");
	}

    client->onDisconnect([](void *s, AsyncClient *c) {
        c->free();
        delete c;
    });
    client->close(true);

}

void Fauxhue::_adjustRGBFromBri(uint8_t id) 
{
	if (id < 0) 
		return;

	// Get the greatest of the RGB values
	uint8_t largest = (_devices[id].color.red > _devices[id].color.green) ? _devices[id].color.red : _devices[id].color.green;
	largest = (_devices[id].color.blue > largest) ? _devices[id].color.blue : largest;

	if (largest > 0)
	{
		float factor = (float) _devices[id].state.bri / (float) largest;
		_devices[id].color.red *= factor;
		_devices[id].color.green *= factor;
		_devices[id].color.blue *= factor;
	}
	else
	{
		_devices[id].color.red = 0;
		_devices[id].color.green = 0;
		_devices[id].color.blue = 0;
	}
}

void Fauxhue::_setRGBFromHSB(uint8_t id) 
{
	if (id < 0) 
		return;

	float dh, ds, db;
	dh = _devices[id].state.hue;
	ds = _devices[id].state.sat;
	db = _devices[id].state.bri / 256.0;

	// lifted from https://github.com/Aircoookie/Espalexa/blob/master/src/EspalexaDevice.cpp    
	float h = ((float)dh)/65536.0;
	float s = ((float)ds)/255.0;
	byte i = floor(h*6);
	float f = h * 6-i;
	float p = 255 * (1-s);
	float q = 255 * (1-f*s);
	float t = 255 * (1-(1-f)*s);
	switch (i%6) {
		case 0: 
			_devices[id].color.red = 255;
			_devices[id].color.green = t;
			_devices[id].color.blue = p;
			break;
		case 1: 
			_devices[id].color.red = q;
			_devices[id].color.green = 255;
			_devices[id].color.blue = p;
			break;
		case 2: 
			_devices[id].color.red = p;
			_devices[id].color.green = 255;
			_devices[id].color.blue = t;
			break;
		case 3: 
			_devices[id].color.red = p;
			_devices[id].color.green = q;
			_devices[id].color.blue = 255;
			break;
		case 4: 
			_devices[id].color.red = t;
			_devices[id].color.green = p;
			_devices[id].color.blue = 255;
			break;
		case 5: 
			_devices[id].color.red = 255;
			_devices[id].color.green = p;
			_devices[id].color.blue = q;
			break;
	}

	_devices[id].color.red = _devices[id].color.red * db;
	_devices[id].color.green = _devices[id].color.green * db;
	_devices[id].color.blue = _devices[id].color.blue * db;

 }

void Fauxhue::_setRGBFromCT(uint8_t id) 
{
	if (id < 0) 
		return;

	float temp = 10000.0 / _devices[id].state.ct;
	float r, g, b;

	if (temp <= 66)
	{
		r = 255;
		g = 99.470802 * log(temp) - 161.119568;

		if (temp <= 19)
		{
			b = 0;
		}
		else
		{
			b = 138.517731 * log(temp - 10) - 305.044793;
		}
	}
	else
	{
		r = 329.698727 * pow(temp - 60, -0.13320476);
		g = 288.12217 * pow(temp - 60, -0.07551485 );
		b = 255;
	}

	r = constrain(r, 0, 255);
	g = constrain(g, 0, 255);
	b = constrain(b, 0, 255);

	_devices[id].color.red = r;
	_devices[id].color.green = g;
	_devices[id].color.blue = b;

	//printf("RGB %f %f %f\n", r, g, b);    

}

// -----------------------------------------------------------------------------
// Devices
// -----------------------------------------------------------------------------

Fauxhue::~Fauxhue() {
  	
	// Free the name for each device
	for (auto& device : _devices) {
		free(device.name);
  	}
  	
	// Delete devices  
	_devices.clear();

}

void Fauxhue::setDeviceUniqueId(uint8_t id, const char *uniqueid)
{
    strncpy(_devices[id].uniqueid, uniqueid, FAUXHUE_DEVICE_UNIQUE_ID_LENGTH);
}

unsigned char Fauxhue::addDevice(const char * device_name) {

    fauxhue_device_t device;
    unsigned int device_id = _devices.size();

    // init properties
    device.name = strdup(device_name);
  	device.state.on = false;
	device.state.bri = 0;
	device.state.hue = 0;
	device.state.sat = 0;

    // create the uniqueid
    String mac = WiFi.macAddress();

    snprintf(device.uniqueid, FAUXHUE_DEVICE_UNIQUE_ID_LENGTH, "%s:%s-%02X", mac.c_str(), "00:00", device_id);

    // Attach
    _devices.push_back(device);

    DEBUG_MSG_FAUXHUE("[FAUXHUE] Device '%s' added as #%d\r\n", device_name, device_id);

    return device_id;

}

int Fauxhue::getDeviceId(const char * device_name) {
    for (unsigned int id=0; id < _devices.size(); id++) {
        if (strcmp(_devices[id].name, device_name) == 0) {
            return id;
        }
    }
    return -1;
}

bool Fauxhue::renameDevice(uint8_t id, const char * device_name) {
    if (id < _devices.size()) {
        free(_devices[id].name);
        _devices[id].name = strdup(device_name);
        DEBUG_MSG_FAUXHUE("[FAUXHUE] Device #%d renamed to '%s'\r\n", id, device_name);
        return true;
    }
    return false;
}

bool Fauxhue::renameDevice(const char * old_device_name, const char * new_device_name) {
	int id = getDeviceId(old_device_name);
	if (id < 0) return false;
	return renameDevice(id, new_device_name);
}

bool Fauxhue::removeDevice(uint8_t id) {
    if (id < _devices.size()) {
        free(_devices[id].name);
		_devices.erase(_devices.begin()+id);
        DEBUG_MSG_FAUXHUE("[FAUXHUE] Device #%d removed\r\n", id);
        return true;
    }
    return false;
}

bool Fauxhue::removeDevice(const char * device_name) {
	int id = getDeviceId(device_name);
	if (id < 0) return false;
	return removeDevice(id);
}

char * Fauxhue::getDeviceName(uint8_t id, char * device_name, size_t len) {
    if ((id < _devices.size()) && (device_name != NULL)) {
        strncpy(device_name, _devices[id].name, len);
    }
    return device_name;
}

fauxhue_rgb_t Fauxhue::getColor(uint8_t id)
{
	if (id < _devices.size())
		return _devices[id].color;
}
char * Fauxhue::getColormode(uint8_t id, char * buffer, size_t len)
{

}

bool Fauxhue::setState(uint8_t id, fauxhue_state_t state) {
    if (id < _devices.size()) {
		_devices[id].state.on = state.on;
		_devices[id].state.bri = state.bri;
		_devices[id].state.hue = state.hue;
		_devices[id].state.sat = state.sat;
		_devices[id].state.ct = state.ct;
		strncpy(_devices[id].state.colormode, state.colormode, 3);

		if (strncmp(state.colormode, "ct", 2) == 0)
			_setRGBFromCT(id);
		else if (strncmp(state.colormode, "hs", 2) == 0)
			_setRGBFromHSB(id);

		// _adjustRGBFromBri(id); // Fixme: Needed for "ct" colormode??

		return true;
	}
	return false;
}


bool Fauxhue::setState(const char * device_name, fauxhue_state_t state) {
	int id = getDeviceId(device_name);
	if (id < 0) return false;
	return setState(id, state);
}

bool Fauxhue::setStateBri(uint8_t id, bool on, uint8_t bri) {
    if (id < _devices.size()) {
		_devices[id].state.on = on;
		_devices[id].state.bri = bri;

		_adjustRGBFromBri(id);
		return true;
	}
	return false;
}

bool Fauxhue::setStateBri(const char * device_name, bool on, uint8_t bri) {
	int id = getDeviceId(device_name);
	if (id < 0) return false;
	return setStateBri(id, on, bri);
}

bool Fauxhue::setStateHueSat(uint8_t id, uint16_t hue, uint8_t sat) {
    if (id < _devices.size()) {
		_devices[id].state.hue = hue;
		_devices[id].state.sat = sat;
		strncpy(_devices[id].state.colormode, "hs", 3);

		_setRGBFromHSB(id);
		return true;
	}
	return false;
}

bool Fauxhue::setStateHueSat(const char * device_name, uint16_t hue, uint8_t sat) {
	int id = getDeviceId(device_name);
	if (id < 0) return false;
	return setStateHueSat(id, hue, sat);
}

bool Fauxhue::setStateColTemp(uint8_t id, uint16_t ct) {
    if (id < _devices.size()) {
		_devices[id].state.ct = ct;
		strncpy(_devices[id].state.colormode, "ct", 3);

		_setRGBFromCT(id);
		return true;
	}
	return false;
}

bool Fauxhue::setStateColTemp(const char * device_name, uint16_t ct) {
	int id = getDeviceId(device_name);
	if (id < 0) return false;
	return setStateColTemp(id, ct);
}


// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------

bool Fauxhue::process(AsyncClient *client, bool isGet, String url, String body) {
	return _onTCPRequest(client, isGet, url, body);
}

void Fauxhue::handle() {
    if (_enabled) _handleUDP();
}

void Fauxhue::enable(bool enable) {

	if (enable == _enabled) return;
    _enabled = enable;
	if (_enabled) {
		DEBUG_MSG_FAUXHUE("[FAUXHUE] Enabled\r\n");
	} else {
		DEBUG_MSG_FAUXHUE("[FAUXHUE] Disabled\r\n");
	}

    if (_enabled) {

		// Start TCP server if internal
		if (_internal) {
			if (NULL == _server) {
				_server = new AsyncServer(_tcp_port);
				_server->onClient([this](void *s, AsyncClient* c) {
					_onTCPClient(c);
				}, 0);
			}
			_server->begin();
		}

		// UDP setup
		#ifdef ESP32
            _udp.beginMulticast(FAUXHUE_UDP_MULTICAST_IP, FAUXHUE_UDP_MULTICAST_PORT);
        #else
            _udp.beginMulticast(WiFi.localIP(), FAUXHUE_UDP_MULTICAST_IP, FAUXHUE_UDP_MULTICAST_PORT);
        #endif
        DEBUG_MSG_FAUXHUE("[FAUXHUE] UDP server started\r\n");

	}

}
