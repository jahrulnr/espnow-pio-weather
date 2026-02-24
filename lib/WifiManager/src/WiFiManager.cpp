#include "WiFiManager.h"
#include <Preferences.h>
#include <LittleFS.h>

WifiManager::WifiManager() : apMode(false), lastReconnectAttempt(0) {}

WifiManager::~WifiManager() {
    stopHotspot();
}

bool WifiManager::init() {
    ESP_LOGI("WIFI", "Initializing WiFi manager");
    WiFi.onEvent(onWiFiEvent);
    WiFi.persistent(true);

    delay(1);
    return true;
}

void WifiManager::begin() {
    ESP_LOGI("WIFI", "Starting WiFi connection process");

    // Try to connect to any available saved network
    if (connectToAvailableNetwork()) {
        ESP_LOGI("WIFI", "Connected to WiFi successfully");
        return;
    }

    ESP_LOGW("WIFI", "No saved networks available or connection failed, starting hotspot");
    startHotspot();
}

bool WifiManager::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

String WifiManager::getIPAddress() {
    if (apMode) {
        return WiFi.softAPIP().toString();
    }
    return WiFi.localIP().toString();
}

std::vector<String> WifiManager::scanNetworks() {
    ESP_LOGI("WIFI", "Starting WiFi scan...");
    // Ensure we're in STA mode and disconnected for clean scan
    WiFi.mode(WIFI_STA);
    delay(100);
    std::vector<String> networks;
    int numNetworks = WiFi.scanNetworks();
    ESP_LOGI("WIFI", "Scan completed, found %d networks", numNetworks);
    for (int i = 0; i < numNetworks; i++) {
        String ssid = WiFi.SSID(i);
        ESP_LOGI("WIFI", "Network %d: %s", i, ssid.c_str());
        networks.push_back(ssid);
    }
    return networks;
}

bool WifiManager::connect(const String& ssid, const String& password) {
    WiFi.softAPdisconnect();
    WiFi.begin(ssid.c_str(), password.c_str());

    int attempts = 0;
    wl_status_t event;
    do{
        event = WiFi.status();
        delay(1000);
        ESP_LOGI("WIFI", "Connecting to: %s; attempt: %d", ssid.c_str(), ++attempts);
    }
    while ((event != WL_CONNECTED && event != ARDUINO_EVENT_WIFI_STA_CONNECTED) && attempts < 20);
    
    return WiFi.status() == WL_CONNECTED;
}

bool WifiManager::addNetwork(const String& ssid, const String& password) noexcept  {
    if (ssid.length() == 0) return false;
    
    std::vector<String> savedNetworks = getSavedNetworks();
    Preferences preferences;
    delay(100);
    if (!preferences.begin("wifi", false)) {
        ESP_LOGE("WIFI", "Failed to open wifi preferences");
        return false;
    }
    
    // Check if network already exists
    for (const auto& savedSsid : savedNetworks) {
        if (savedSsid == ssid) {
            // Update password for existing network
            String key = "pwd_" + ssid;
            preferences.putString(key.c_str(), password);
            preferences.end();
            ESP_LOGI("WIFI", "Updated password for network: %s", ssid.c_str());
            return true;
        }
    }
    
    // Check if we have space for new network
    if (savedNetworks.size() >= MAX_SAVED_NETWORKS) {
        preferences.end();
        ESP_LOGW("WIFI", "Maximum number of saved networks reached (%d)", MAX_SAVED_NETWORKS);
        return false;
    }
    
    // Add new network
    String networksKey = "networks";
    String currentNetworks = preferences.getString(networksKey.c_str(), "");
    
    if (currentNetworks.length() > 0) {
        currentNetworks += ",";
    }
    currentNetworks += ssid;
    
    preferences.putString(networksKey.c_str(), currentNetworks);
    
    String pwdKey = "pwd_" + ssid;
    preferences.putString(pwdKey.c_str(), password);
    
    preferences.end();
    ESP_LOGI("WIFI", "Added new network: %s", ssid.c_str());
    return true;
}

bool WifiManager::removeNetwork(const String& ssid) {
    Preferences preferences;
    if (!preferences.begin("wifi", false)) {
        return false;
    }
    
    String networksKey = "networks";
    String currentNetworks = preferences.getString(networksKey.c_str(), "");
    
    if (currentNetworks.length() == 0) {
        preferences.end();
        return false;
    }
    
    // Remove from networks list
    std::vector<String> networks;
    int start = 0;
    int end = currentNetworks.indexOf(',');
    while (end != -1) {
        String network = currentNetworks.substring(start, end);
        if (network != ssid) {
            networks.push_back(network);
        }
        start = end + 1;
        end = currentNetworks.indexOf(',', start);
    }
    // Handle last network
    String lastNetwork = currentNetworks.substring(start);
    if (lastNetwork != ssid && lastNetwork.length() > 0) {
        networks.push_back(lastNetwork);
    }
    
    // Rebuild networks string
    String newNetworks = "";
    for (size_t i = 0; i < networks.size(); i++) {
        if (i > 0) newNetworks += ",";
        newNetworks += networks[i];
    }
    
    preferences.putString(networksKey.c_str(), newNetworks);
    
    // Remove password
    String pwdKey = "pwd_" + ssid;
    preferences.remove(pwdKey.c_str());
    
    preferences.end();
    ESP_LOGI("WIFI", "Removed network: %s", ssid.c_str());
    return true;
}

std::vector<String> WifiManager::getSavedNetworks() {
    ESP_LOGI("WIFI", "Getting saved networks from NVS");
    std::vector<String> networks;
    Preferences preferences;
    if (!preferences.begin("wifi", true)) {
        return networks;
    }
    
    String networksStr = preferences.getString("networks", "");
    preferences.end();
    
    if (networksStr.length() == 0) {
        ESP_LOGI("WIFI", "No saved networks found");
        return networks;
    }
    
    ESP_LOGI("WIFI", "Parsing saved networks string: %s", networksStr.c_str());
    int start = 0;
    int end = networksStr.indexOf(',');
    while (end != -1) {
        networks.push_back(networksStr.substring(start, end));
        start = end + 1;
        end = networksStr.indexOf(',', start);
    }
    // Handle last network
    String lastNetwork = networksStr.substring(start);
    if (lastNetwork.length() > 0) {
        networks.push_back(lastNetwork);
    }
    
    ESP_LOGI("WIFI", "Found %d saved networks", networks.size());
    return networks;
}

bool WifiManager::connectToAvailableNetwork() {
    if (WiFi.status() == WL_CONNECTED) return true;

    ESP_LOGI("WIFI", "Scanning for available networks");
    // Get available networks
    std::vector<String> availableNetworks = scanNetworks();
    ESP_LOGI("WIFI", "Found %d available networks", availableNetworks.size());
    
    ESP_LOGI("WIFI", "Retrieving saved networks");
    std::vector<String> savedNetworks = getSavedNetworks();
    
    if (availableNetworks.empty() || savedNetworks.empty()) {
        ESP_LOGW("WIFI", "No available or saved networks");
        return false;
    }
    
    // Try to connect to any saved network that's available
    for (const auto& savedNetwork : savedNetworks) {
        for (const auto& availableNetwork : availableNetworks) {
            if (savedNetwork == availableNetwork) {
                ESP_LOGI("WIFI", "Found saved network: %s", savedNetwork.c_str());
                
                // Get password for this network
                Preferences preferences;
                if (!preferences.begin("wifi", true)){
                    continue;
                }
                String pwdKey = "pwd_" + savedNetwork;
                String password = preferences.getString(pwdKey.c_str(), "");
                preferences.end();
                
                if (password.length() > 0 && connect(savedNetwork, password)) {
                    ESP_LOGI("WIFI", "Successfully connected to: %s", savedNetwork.c_str());
                    return true;
                }
                break;
            }
        }
    }
    
    return false;
}

void WifiManager::startHotspot() {
    if (apMode) return;

    ESP_LOGI("WIFI", "Starting AP hotspot");
    WiFi.softAP("PioAssistant", "tes12345");
    apMode = true;

    ESP_LOGI("WIFI", "Hotspot started. Connect to ESP32-Config and visit %s", WiFi.softAPIP().toString().c_str());
}

void WifiManager::stopHotspot() {
    if (!apMode) return;

    ESP_LOGI("WIFI", "Stopping hotspot");
    WiFi.softAPdisconnect(true);
    apMode = false;
}

void WifiManager::handle() {
    if (!apMode && !isConnected()) {
        unsigned long now = millis();
        if (now - lastReconnectAttempt > 10000) {
            ESP_LOGI("WIFI", "Attempting to reconnect...");
            connectToAvailableNetwork();
            lastReconnectAttempt = now;
        }
    }
}


void WifiManager::onWiFiEvent(WiFiEvent_t event) {
    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            ESP_LOGI("WIFI", "Connected to AP");
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            ESP_LOGW("WIFI", "Disconnected from AP");
            break;
        case ARDUINO_EVENT_WIFI_AP_START:
            ESP_LOGI("WIFI", "AP started");
            break;
        case ARDUINO_EVENT_WIFI_AP_STOP:
            ESP_LOGI("WIFI", "AP stopped");
            break;
        default:
            break;
    }
}