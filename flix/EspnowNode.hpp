#pragma once

#include <queue>

#include <esp_now.h>
#include <WiFi.h>

struct esp_now_recv_info;

struct EspnowNode {

    private:

    esp_now_peer_info_t peer{
        .peer_addr={0xff,0xff,0xff,0xff,0xff,0xff},
        .channel=0,
        .encrypt=false
    };
    std::queue<uint8_t> recv_queue{};

    public:

    esp_err_t init() {
        esp_err_t ret = ESP_FAIL;

        if (not WiFi.mode(WIFI_STA)) goto end;

        ret = esp_now_init();
        if (ret != ESP_OK) goto end;

        ret = esp_now_register_recv_cb(EspnowNode::onReceive);
        if (ret != ESP_OK) goto end;
        
        ret = esp_now_add_peer(&peer);
        
    end:
        return ret;
    }

    esp_err_t send(const uint8_t *data, int len) {
        return esp_now_send(peer.peer_addr, data, len);
    }

    int read(uint8_t *dest, int len) {
        int i = 0;
        
        while (i < len) {
            if (recv_queue.empty()) { break; }

            dest[i] = recv_queue.front();
            recv_queue.pop();

            i += 1;
        }

        return i;
    }

    static EspnowNode &instance() {
        static EspnowNode i{};
        return i;
    }

    EspnowNode(const EspnowNode&) = delete;

    private:

    static void onReceive(const struct esp_now_recv_info * esp_now_info, const uint8_t *data, int len) {	
        auto &self = EspnowNode::instance();
        for (int i = 0; i < len; i += 1) {
            self.recv_queue.push(data[i]);
        }
    }

    EspnowNode() = default;
};