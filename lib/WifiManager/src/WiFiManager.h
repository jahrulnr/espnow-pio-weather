#pragma once

#include <WiFi.h>
#include <Preferences.h>

/**
 * WiFi Manager class for ESP32
 * Handles WiFi connection, auto-reconnect, and AP hotspot for configuration
 * Supports multiple saved networks with auto-roaming
 */
class WifiManager {
public:
    WifiManager();
    ~WifiManager();

    /**
     * Initialize WiFi manager
     * @return true if successful
     */
    bool init();

    /**
     * Start WiFi connection process
     * Attempts to connect to any saved network, starts AP if none available
     */
    void begin();

    /**
     * Check if connected to WiFi
     * @return true if connected
     */
    bool isConnected();
    inline wl_status_t getState() const { return WiFi.status(); }

    /**
     * Get current IP address
     * @return IP address as string
     */
    String getIPAddress();

    /**
     * Scan for available WiFi networks
     * @return vector of network SSIDs
     */
    std::vector<String> scanNetworks();

    /**
     * Connect to specific network
     * @param ssid Network SSID
     * @param password Network password
     * @return true if connection successful
     */
    bool connect(const String& ssid, const String& password);

    /**
     * Add a WiFi network to saved networks
     * @param ssid Network SSID
     * @param password Network password
     * @return true if added successfully
     */
    bool addNetwork(const String& ssid, const String& password) noexcept;

    /**
     * Remove a saved network
     * @param ssid Network SSID to remove
     * @return true if removed successfully
     */
    bool removeNetwork(const String& ssid);

    /**
     * Get list of saved networks
     * @return vector of saved SSIDs
     */
    std::vector<String> getSavedNetworks();

    /**
     * Try to connect to any available saved network
     * @return true if connected to any network
     */
    bool connectToAvailableNetwork();

    /**
     * Start AP hotspot for configuration
     */
    void startHotspot();

    /**
     * Stop AP hotspot
     */
    void stopHotspot();

    /**
     * Handle periodic tasks (reconnect)
     * Call this in main loop
     */
    void handle();

private:
    bool apMode;
    unsigned long lastReconnectAttempt;
    
    static const int MAX_SAVED_NETWORKS = 5; // Maximum number of saved networks

    // WiFi event handlers
    static void onWiFiEvent(WiFiEvent_t event);
};

extern WifiManager wifiManager;