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
#include "app/tasks/inputTask.h"
#include "app/tasks/networkTask.h"
#include <app_config.h>

using app::espnow::espnowSlave;
using app::sensor::dhtSensor;
using app::weather::Area;

void init(){
	esp_panic_handler_disable_timg_wdts();
	nvs_init();
}

void setup() {
	LittleFS.begin(true);

	#if BOARD_HAS_PSRAM
	heap_caps_malloc_extmem_enable(0);
	#endif

	// network task will initialize ESP-NOW and manage proxy requests
	app::tasks::startNetworkTask();

	// start input task (battery + DHT reads)
	if (!app::tasks::startInputTask()){
		ESP_LOGE("MAIN", "Input task failed to start");
	}

}

void loop() {
	  // network + input tasks run the work; keep loop idle
	  vTaskDelete(NULL);
}