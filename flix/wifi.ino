// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

#define USE_ESPNOW // uncomment to replace from WIFI to ESPNOW
#define USE_ESPNOW // uncomment to replace from WIFI to ESPNOW

#include "Preferences.h"

extern Preferences storage; // use the main preferences storage

// these variables are here for a compilation errors reason (references via extern)
// these variables are here for a compilation errors reason (references via extern)
const int W_DISABLED = 0, W_AP = 1, W_STA = 2;
int wifiMode = W_AP;
int udpLocalPort = 14550;
int udpRemotePort = 14550;

#ifdef USE_ESPNOW

#include <queue> // todo replace with FreeRTOS ring buffer
#include <esp_now.h>
#include <WiFi.h>

// EspNow communication 

struct esp_now_recv_info; // forward declaration for a reason

esp_now_peer_info_t peer{
	// broadcast address (can receive from anyone)
	.peer_addr={0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.channel=0,
	.encrypt=false,
};

// First dirty implemenration. (WILL be replaced with FreeRTOS queue)
std::queue<uint8_t> recv_queue{};

// WARNING: onReceive called on interrupt (or other Task) context, so STL is kinda forbdden.. 
void onReceive(const struct esp_now_recv_info *, const uint8_t *data, int len) {	
	for (int i = 0; i < len; i += 1) {
		recv_queue.push(data[i]);
	}
}

// bulk espnow init
esp_err_t initEspNow() {
	if (!WiFi.mode(WIFI_STA)) return ESP_FAIL;

	esp_err_t e;
	
	e = esp_now_init();
	if (e != ESP_OK) return e;

	e = esp_now_register_recv_cb(onReceive);
	if (e != ESP_OK) return e;
        
    return esp_now_add_peer(&peer);
}

// setup ESPNOW, peer, WIFI mode STA
void setupWiFi() {
	esp_err_t init_result = initEspNow();
	if (init_result != ESP_OK) {
		print("EspNow: init failed: %s\n", esp_err_to_name(init_result));
	} else {
		print("EspNow: init OK\n");
	}
}

// send to espnow peer
void sendWiFi(const uint8_t *buf, int len) {
	esp_err_t send_result = esp_now_send(peer.peer_addr, buf, len);
	if (send_result != ESP_OK) {
		print("EspNow: send failed: %s\n", esp_err_to_name(send_result));
	}
}

// read available from queue
// WARNING: this is not safe, but still works..
// TODO: 	replace with ring buffer via queue from RTOS
int receiveWiFi(uint8_t *buf, int len) {
	int readed = 0;

	while ((readed < len) && !recv_queue.empty()) {
		buf[readed] = recv_queue.front();
		recv_queue.pop();

		readed += 1;
	}

	return readed;
}

void printWiFiInfo() {
	// todo implement

	// ESPNOW info?
	// maybe show some stats (need to collect stats)
}

void configWiFi(bool ap, const char *ssid, const char *password) {
	// todo implement
	
	// maybe interpret ssid as human-readable peer MAC address string, parse it and set into preferences? 
}

#else

// Wi-Fi communication

#include <WiFi.h>
#include <WiFiAP.h>
#include <WiFiUdp.h>

IPAddress udpRemoteIP = "255.255.255.255";

WiFiUDP udp;

void setupWiFi() {
	print("Setup Wi-Fi\n");
	if (wifiMode == W_AP) {
		WiFi.softAP(storage.getString("WIFI_AP_SSID", "flix").c_str(), storage.getString("WIFI_AP_PASS", "flixwifi").c_str());
	} else if (wifiMode == W_STA) {
		WiFi.begin(storage.getString("WIFI_STA_SSID", "").c_str(), storage.getString("WIFI_STA_PASS", "").c_str());
	}
	udp.begin(udpLocalPort);
}

void sendWiFi(const uint8_t *buf, int len) {
	if (WiFi.softAPgetStationNum() == 0 && !WiFi.isConnected()) return;
	udp.beginPacket(udpRemoteIP, udpRemotePort);
	udp.write(buf, len);
	udp.endPacket();
}

int receiveWiFi(uint8_t *buf, int len) {
	udp.parsePacket();
	if (udp.remoteIP()) udpRemoteIP = udp.remoteIP();
	return udp.read(buf, len);
}

void printWiFiInfo() {
	if (WiFi.getMode() == WIFI_MODE_AP) {
		print("Mode: Access Point (AP)\n");
		print("MAC: %s\n", WiFi.softAPmacAddress().c_str());
		print("SSID: %s\n", WiFi.softAPSSID().c_str());
		print("Password: ***\n");
		print("Clients: %d\n", WiFi.softAPgetStationNum());
		print("IP: %s\n", WiFi.softAPIP().toString().c_str());
	} else if (WiFi.getMode() == WIFI_MODE_STA) {
		print("Mode: Client (STA)\n");
		print("Connected: %d\n", WiFi.isConnected());
		print("MAC: %s\n", WiFi.macAddress().c_str());
		print("SSID: %s\n", WiFi.SSID().c_str());
		print("Password: ***\n");
		print("IP: %s\n", WiFi.localIP().toString().c_str());
	} else {
		print("Mode: Disabled\n");
		return;
	}
	print("Remote IP: %s\n", udpRemoteIP.toString().c_str());
	print("MAVLink connected: %d\n", mavlinkConnected);
}

void configWiFi(bool ap, const char *ssid, const char *password) {
	if (ap) {
		storage.putString("WIFI_AP_SSID", ssid);
		storage.putString("WIFI_AP_PASS", password);
	} else {
		storage.putString("WIFI_STA_SSID", ssid);
		storage.putString("WIFI_STA_PASS", password);
	}
	print("✓ Reboot to apply new settings\n");
}

#endif
#endif