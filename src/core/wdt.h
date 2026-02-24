#pragma once

#include "hal/timer_hal.h"
#include "hal/wdt_types.h"
#include "hal/wdt_hal.h"
#include <esp_task_wdt.h>

inline void esp_panic_handler_disable_timg_wdts(void)
{

  // Configure Task Watchdog Timer to prevent system hangs
  esp_task_wdt_config_t config = {
    .timeout_ms = 3 * 1000,
    .trigger_panic = false,
  };

  if (ESP_OK != esp_task_wdt_init(&config)) {
    esp_task_wdt_reconfigure(&config);
  }

  wdt_hal_context_t wdt0_context = {.inst = WDT_MWDT0, .mwdt_dev = &TIMERG0};
  wdt_hal_write_protect_disable(&wdt0_context);
  wdt_hal_disable(&wdt0_context);
  wdt_hal_write_protect_enable(&wdt0_context);

  wdt_hal_context_t wdt1_context = {.inst = WDT_MWDT1, .mwdt_dev = &TIMERG1};
  wdt_hal_write_protect_disable(&wdt1_context);
  wdt_hal_disable(&wdt1_context);
  wdt_hal_write_protect_enable(&wdt1_context);
}
