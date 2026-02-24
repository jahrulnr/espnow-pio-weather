#pragma once

#include <WiFiClientSecure.h>
#include <Arduino.h>
#include <base64.h>

// Simple WebSocket client using SSL for ESP32
class WebSocketClientSSL {
public:
    WebSocketClientSSL() : connected(false), _auth(nullptr) {}

    ~WebSocketClientSSL() {
        disconnect();
    }

    void setAuthorization(const char* auth) {
        _auth = auth;
    }

    bool connect(const char* host, uint16_t port, const char* path = "/") {
        this->host = host;
        this->port = port;
        this->path = path;

        client.setInsecure(); // Skip certificate verification for demo

        if (!client.connect(host, port)) {
            ESP_LOGE("WSS", "Fail to connect to GPT");
            return false;
        }

        return performHandshake();
    }

    void disconnect() {
        if (connected) {
            client.stop();
            connected = false;
        }
    }

    bool sendMessage(const String& message) {
        if (!connected) return false;

        size_t len = message.length();
        uint8_t frame[message.length() + 14]; // Max header size with mask
        frame[0] = 0x81; // Text frame

        size_t headerSize = 6; // 2 + 4 mask
        if (len < 126) {
            frame[1] = len | 0x80; // Set MASK bit
        } else if (len <= 65535) {
            frame[1] = 126 | 0x80;
            frame[2] = (len >> 8) & 0xFF;
            frame[3] = len & 0xFF;
            headerSize = 8;
        } else {
            frame[1] = 127 | 0x80;
            frame[2] = 0; frame[3] = 0; frame[4] = 0; frame[5] = 0;
            frame[6] = (len >> 24) & 0xFF;
            frame[7] = (len >> 16) & 0xFF;
            frame[8] = (len >> 8) & 0xFF;
            frame[9] = len & 0xFF;
            headerSize = 14;
        }

        // Generate random mask key
        uint8_t maskKey[4];
        for (int i = 0; i < 4; i++) {
            maskKey[i] = random(0, 256);
        }
        memcpy(&frame[headerSize - 4], maskKey, 4);

        // Copy and mask payload
        uint8_t* payload = &frame[headerSize];
        memcpy(payload, message.c_str(), len);
        for (size_t i = 0; i < len; i++) {
            payload[i] ^= maskKey[i % 4];
        }

        client.write(frame, headerSize + len);
        return true;
    }

    uint8_t* receiveMessage() {
        if (!connected || !client.available()) return nullptr;

        uint8_t header[2];
        client.readBytes(header, 2);

        uint8_t opcode = header[0] & 0x0F;
        uint8_t lenByte = header[1] & 0x7F;
        bool masked = (header[1] & 0x80) != 0;
        size_t len = lenByte;

        if (lenByte == 126) {
            uint8_t ext[2];
            client.readBytes(ext, 2);
            len = (ext[0] << 8) | ext[1];
        } else if (lenByte == 127) {
            uint8_t ext[8];
            client.readBytes(ext, 8);
            len = 0;
            for (int i = 0; i < 8; i++) {
                len = (len << 8) | ext[i];
            }
        }

        uint8_t maskKey[4] = {0};
        if (masked) {
            client.readBytes(maskKey, 4);
        }

        if (opcode != 1) return nullptr; // Not text

        uint8_t* message = (uint8_t*) heap_caps_malloc(len+1, MALLOC_CAP_SPIRAM);
        size_t read = 0;
        size_t length = len-1;
        unsigned long startTime = millis();
        while(len > 0) {
            if (millis() - startTime > 5000) {
                break;
            }
            if (!client.available()) {
                delay(1);
                continue;
            }
            int c = client.read(message+read, len - read);
            if (c > 0) {
                read+=c;
                len-=c;
                startTime = millis();
            } else {
                break;
            }
            taskYIELD();
        }

        if (length != read){ 
            ESP_LOGE("WSS", "not match. content-length=%d read=%d", length, read);
            heap_caps_free(message);
            return nullptr;
        }

        if (masked) {
            for(int i=0; i<read; i++)
                message[i] ^= maskKey[i % 4];
        }

        message[len+1] = '\0';
        return message;
    }

    bool isConnected() {
        return connected && client.connected();
    }

private:
    WiFiClientSecure client;
    String host;
    uint16_t port;
    String path;
    const char* _auth;
    bool connected;

    struct headers {

    };

    bool performHandshake() {
        String key = generateKey();
        String request = "GET " + path + " HTTP/1.1\r\n";
        request += "Host: " + host + ":" + port + "\r\n";
        request += "Upgrade: websocket\r\n";
        request += "Connection: Upgrade\r\n";
        request += "Sec-WebSocket-Key: " + key + "\r\n";
        request += "Sec-WebSocket-Version: 13\r\n";
        if (_auth)
            request += String("Authorization: ")+_auth+"\r\n";
        request += "User-Agent: esp32\r\n";
        request += "\r\n";

        client.print(request);
        ESP_LOGI("WSS", "request sended with auth: %s", _auth ? "yes":"no");

        // Wait for response
        unsigned long timeout = millis() + 5000;
        while (!client.available() && millis() < timeout) {
            delay(1);
        }

        if (!client.available()) {
            ESP_LOGW("WSS", "Timeout to waiting GPT Response");
            return false;
        }

        String response = client.readStringUntil('\n');
        if (!response.startsWith("HTTP/1.1 101")) {
            ESP_LOGE("WSS", "Fail to receive HTTP header");
            ESP_LOGE("WSS", "%s", response.c_str());
            return false;
        }

        // Skip headers until empty line
        while (client.available()) {
            String line = client.readStringUntil('\n');
            if (line == "\r") break;
        }

        ESP_LOGI("WSS", "GPT Connected");
        connected = true;
        return true;
    }

    String generateKey() {
        String key = "";
        for (int i = 0; i < 16; i++) {
            key += (char)random(0, 256);
        }
        return base64::encode((uint8_t*)key.c_str(), key.length());
    }
};