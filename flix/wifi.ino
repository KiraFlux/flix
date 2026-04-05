// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

#define USE_ESPNOW // uncomment to replace from WIFI to ESPNOW

const int W_DISABLED = 0, W_AP = 1, W_STA = 2;
int wifiMode = W_AP;
int udpLocalPort = 14550;
int udpRemotePort = 14550;

#ifdef USE_ESPNOW
// EspNow communication

#include <freertos/queue.h>
#include <esp_now.h>
#include <WiFi.h>

const int maxChunk = ESP_NOW_MAX_DATA_LEN;
const int recvQueueSize = 1024;
const int wifiChannel = 1;

esp_now_peer_info_t peer{
	.peer_addr={0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.channel=0,
	.encrypt=false,
};

QueueHandle_t recvQueue = nullptr;
volatile uint32_t lostPackets = 0;
bool espnowInitialized = false;

void IRAM_ATTR onReceive(const esp_now_recv_info_t *, const uint8_t *data, int len) {
	if (!espnowInitialized) return;

	for (int i = 0; i < len; i += 1) {
        if (xQueueSendFromISR(recvQueue, &data[i], nullptr) != pdTRUE) {
			lostPackets += 1;
		}
	}
}

// bulk init
esp_err_t initEspNow() {
	if (!WiFi.mode(WIFI_STA)) return ESP_FAIL;
	WiFi.setChannel(wifiChannel);

	recvQueue = xQueueCreate(recvQueueSize, sizeof(uint8_t));
    if (recvQueue == nullptr) return ESP_ERR_NO_MEM;
    
	esp_err_t e;
	
	e = esp_now_init();
	if (e != ESP_OK) return e;

	e = esp_now_register_recv_cb(onReceive);
	if (e != ESP_OK) return e;
    
    return esp_now_add_peer(&peer);
}

void setupWiFi() {
	esp_err_t init_result = initEspNow();
	espnowInitialized = (init_result == ESP_OK);
	
	if (espnowInitialized) {
		print("EspNow: init OK\n");
	} else {
		print("EspNow: init failed: %s\n", esp_err_to_name(init_result));
	}
}

void sendWiFi(const uint8_t *buf, int len) {
    if (!espnowInitialized) return;

	int remaining = len;

	while (remaining > 0) {
        int chunkLen = (remaining < maxChunk) ? remaining : maxChunk;
        (void) esp_now_send(peer.peer_addr, buf, chunkLen);
        
		buf += chunkLen;
        remaining -= chunkLen;
    }
}

int receiveWiFi(uint8_t *buf, int len) {
	if (!espnowInitialized) return 0;

	int i;
	for (i = 0; i < len; i += 1) {
        if (xQueueReceive(recvQueue, &buf[i], 0) != pdTRUE) break;
    }

    return i;
}

void printWiFiInfo() {
	if (!espnowInitialized) return;

	print("ESPNOW mode (no Wi-Fi AP/STA)\n");
    print("MAC: %s\n", WiFi.macAddress().c_str());
    print("Channel: %d\n", WiFi.channel());
    UBaseType_t itemsWaiting = uxQueueMessagesWaiting(recvQueue);
    print("Queue pending bytes: %d\n", itemsWaiting);
    print("Lost packets: %lu\n", lostPackets);
}

// ESPNOW does not use SSID/password. This function is kept for compatibility.
void configWiFi(bool ap, const char *ssid, const char *password) {
    print("ESPNOW: configWiFi called but not applicable\n");
}

#else
// Wi-Fi communication

#include <WiFi.h>
#include <WiFiAP.h>
#include <WiFiUdp.h>
#include "Preferences.h"

extern Preferences storage; // use the main preferences storage

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