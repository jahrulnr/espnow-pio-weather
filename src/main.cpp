#include <Arduino.h>
#include <core/wdt.h>
#include "core/nvs.h"
#include <LittleFS.h>
#include <nvs_flash.h>
#include "app/espnow/slave.h"
#include "app/espnow/payload_codec.h"
#include "app/espnow/state_binary.h"
#include "app/sensor/dht_sensor.h"
#include "app/weather/open_meteo_locations.h"
#include <app_config.h>

using app::espnow::espnowSlave;
using app::sensor::dhtSensor;
using app::weather::Area;

static uint32_t lastDhtReadMs = 0;
static uint32_t lastWeatherRefreshMs = 0;
static uint32_t lastWeatherRequestMs = 0;
static String cachedProxyRequest;
static bool wasMasterLinked = false;
static String cachedWeatherUrl;

static void sendIdentityState() {
	app::espnow::state_binary::IdentityState state = {};
	app::espnow::state_binary::initHeader(state.header, app::espnow::state_binary::Type::Identity);
	strncpy(state.id, DEVICE_NAME, sizeof(state.id) - 1);
	espnowSlave.sendStateBinary(&state, sizeof(state));
	ESP_LOGI("SLAVE", "Sent identity state: id=%s", DEVICE_NAME);
}

static void sendFeaturesState() {
	app::espnow::state_binary::FeaturesState state = {};
	app::espnow::state_binary::initHeader(state.header, app::espnow::state_binary::Type::Features);
	state.contractVersion = 1;
	state.featureBits = static_cast<uint32_t>(app::espnow::state_binary::FeatureIdentity)
		| static_cast<uint32_t>(app::espnow::state_binary::FeatureSensor)
		| static_cast<uint32_t>(app::espnow::state_binary::FeatureWeather)
		| static_cast<uint32_t>(app::espnow::state_binary::FeatureProxyClient);
	espnowSlave.sendStateBinary(&state, sizeof(state));
	ESP_LOGI("SLAVE", "Sent features state: bits=0x%08lX", static_cast<unsigned long>(state.featureBits));
}

static void refreshWeatherCache() {
	const Area area = static_cast<Area>(WEATHER_AREA_INDEX);
	cachedWeatherUrl = app::weather::buildCurrentWeatherUrl(area);
	cachedProxyRequest = app::espnow::codec::buildPayload({
		{"state", "proxy_req"},
		{"method", "GET"},
		{"url", cachedWeatherUrl},
		{"payload", "{}"},
	});
	app::weather::saveLastReport(cachedProxyRequest);
	ESP_LOGI("WEATHER", "Proxy request refreshed: %s", cachedProxyRequest.c_str());
}

void init(){
	esp_panic_handler_disable_timg_wdts();
	nvs_init();
}

void setup() {
	LittleFS.begin(true);

	#if BOARD_HAS_PSRAM
	heap_caps_malloc_extmem_enable(0);
	#endif

	espnowSlave.begin(app::espnow::DEFAULT_CHANNEL);

	#if DHT_SENSOR_ENABLED
	dhtSensor.begin(DHT_SENSOR_PIN, DHT_SENSOR_IS_DHT22 == 1);
	lastDhtReadMs = millis();
	#endif

	#if WEATHER_REPORT_ENABLED
	if (app::weather::loadLastReport(cachedProxyRequest)) {
		if (!cachedProxyRequest.startsWith("state=proxy_req")) {
			ESP_LOGW("WEATHER", "Persisted request format outdated, rebuilding");
			refreshWeatherCache();
		} else {
			if (!app::espnow::codec::getField(cachedProxyRequest, "url", cachedWeatherUrl) || cachedWeatherUrl.isEmpty()) {
				ESP_LOGW("WEATHER", "Persisted request missing URL, rebuilding");
				refreshWeatherCache();
			} else {
				ESP_LOGI("WEATHER", "Loaded persisted proxy request: %s", cachedProxyRequest.c_str());
			}
		}
	} else {
		refreshWeatherCache();
	}

	if (!cachedProxyRequest.isEmpty()) {
		app::espnow::state_binary::ProxyReqState request = {};
		app::espnow::state_binary::initHeader(request.header, app::espnow::state_binary::Type::ProxyReq);
		request.method = static_cast<uint8_t>(app::espnow::state_binary::HttpMethod::Get);
		strncpy(request.url, cachedWeatherUrl.c_str(), sizeof(request.url) - 1);
		espnowSlave.sendStateBinary(&request, sizeof(request));
	}

	lastWeatherRefreshMs = millis();
	lastWeatherRequestMs = millis();
	#endif

}

void loop() {
	espnowSlave.loop();

	#if WEATHER_REPORT_ENABLED
	const bool isMasterLinked = espnowSlave.isMasterLinked();
	if (isMasterLinked && !wasMasterLinked) {
		sendIdentityState();
		sendFeaturesState();

		if (cachedProxyRequest.isEmpty()) {
			refreshWeatherCache();
		}

		app::espnow::state_binary::ProxyReqState request = {};
		app::espnow::state_binary::initHeader(request.header, app::espnow::state_binary::Type::ProxyReq);
		request.method = static_cast<uint8_t>(app::espnow::state_binary::HttpMethod::Get);
		strncpy(request.url, cachedWeatherUrl.c_str(), sizeof(request.url) - 1);
		espnowSlave.sendStateBinary(&request, sizeof(request));
		ESP_LOGI("WEATHER", "Master linked, sent bootstrap proxy request");
		lastWeatherRequestMs = millis();
	}
	wasMasterLinked = isMasterLinked;
	#endif

	#if DHT_SENSOR_ENABLED
	const uint32_t now = millis();
	if (now - lastDhtReadMs >= DHT_READ_INTERVAL_MS) {
		app::sensor::DhtReading reading;
		if (dhtSensor.read(reading) && reading.valid) {
			app::espnow::state_binary::SensorState state = {};
			app::espnow::state_binary::initHeader(state.header, app::espnow::state_binary::Type::Sensor);
			state.temperature10 = static_cast<int16_t>(reading.temperatureC * 10.0f);
			state.humidity10 = static_cast<uint16_t>(reading.humidityPercent * 10.0f);
			ESP_LOGI("DHT", "sensor temp=%.1fC hum=%.1f%%", reading.temperatureC, reading.humidityPercent);
			espnowSlave.sendStateBinary(&state, sizeof(state));
		} else {
			ESP_LOGW("DHT", "Failed to read sensor");
		}

		lastDhtReadMs = now;
	}
	#endif

	#if WEATHER_REPORT_ENABLED
	const uint32_t weatherNow = millis();
	if (weatherNow - lastWeatherRefreshMs >= WEATHER_REPORT_INTERVAL_MS) {
		refreshWeatherCache();
		lastWeatherRefreshMs = weatherNow;
	}

	if (!cachedProxyRequest.isEmpty() && (weatherNow - lastWeatherRequestMs >= WEATHER_PROXY_REQUEST_INTERVAL_MS)) {
		app::espnow::state_binary::ProxyReqState request = {};
		app::espnow::state_binary::initHeader(request.header, app::espnow::state_binary::Type::ProxyReq);
		request.method = static_cast<uint8_t>(app::espnow::state_binary::HttpMethod::Get);
		strncpy(request.url, cachedWeatherUrl.c_str(), sizeof(request.url) - 1);
		espnowSlave.sendStateBinary(&request, sizeof(request));
		ESP_LOGI("WEATHER", "Sent proxy request to master");
		lastWeatherRequestMs = weatherNow;
	}
	#endif

	delay(10);
}