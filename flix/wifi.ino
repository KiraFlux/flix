// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix
// Modified by KiraFlux 2025.10.18

// Wi-Fi support

#if WIFI_ENABLED

#if 0
#include <WiFi.h>
#include <WiFiAP.h>
#include <WiFiUdp.h>

#define WIFI_AP_MODE 1
#define WIFI_AP_SSID "klyax"
#define WIFI_AP_PASSWORD "flixwifi"
#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#define WIFI_UDP_PORT 14550
#define WIFI_UDP_REMOTE_PORT 14550
#define WIFI_UDP_REMOTE_ADDR "255.255.255.255"

static WiFiUDP udp;
#endif

#include <WiFi.h>
#include <esp_now.h>
#include <queue>

static std::queue<uint8_t> espnow_queue;

static esp_now_peer_info_t controller_peer{
	.peer_addr = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.channel = 0,
	.encrypt = false,
};

struct esp_now_recv_info;

static void onEspnowReceive(const struct esp_now_recv_info * esp_now_info, const uint8_t *data, int len) {
	const uint8_t *end = data + len;
	
	for (const uint8_t *it = data; it < end; it += 1) {
		espnow_queue.push(*it);
	}
}

static esp_err_t initEspnow() {
  	esp_err_t ret = ESP_FAIL;

	if (not WiFi.mode(WIFI_STA)) goto end;

  	ret = esp_now_init();
  	if (ret != ESP_OK) goto end;

  	ret = esp_now_register_recv_cb(onEspnowReceive);
  	if (ret != ESP_OK) goto end;
	
	ret = esp_now_add_peer(&controller_peer);
	
end:
	return ret;
}

void setupWiFi() {
#if 0
	print("Setup Wi-Fi\n");
	if (WIFI_AP_MODE) {
		WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD);
	} else {
		WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
	}
	udp.begin(WIFI_UDP_PORT);
#endif

  const auto result = initEspnow();
  print("wifi: espnow init: %s\n", esp_err_to_name(result));
}

void sendWiFi(const uint8_t *buf, int len) {
#if 0
	if (WiFi.softAPIP() == IPAddress(0, 0, 0, 0) && WiFi.status() != WL_CONNECTED) return;
	udp.beginPacket(udp.remoteIP() ? udp.remoteIP() : WIFI_UDP_REMOTE_ADDR, WIFI_UDP_REMOTE_PORT);
	udp.write(buf, len);
	udp.endPacket();
#endif
	const auto result = esp_now_send(controller_peer.peer_addr, buf, len);
	if (ESP_OK != result) { print(esp_err_to_name(result)); }
}

int receiveWiFi(uint8_t *buf, int len) {
#if 0
	udp.parsePacket();
	return udp.read(buf, len);
#endif
	for (int i = 0; i < len; i += 1) {
		if (espnow_queue.empty()) {
			return i;
		}

		buf[i] = espnow_queue.front();
		espnow_queue.pop();
	}
}

#endif
