/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#pragma once
#include "sdkconfig.h"
#include <Arduino.h>
#if CONFIG_IDF_TARGET_ESP32S3

#include "esp_afe_sr_models.h"
#include "esp_afe_sr_iface.h"
#include "driver/i2s_types.h"
#include "esp32-hal-sr.h"
#include "esp_err.h"

#define SR_CMD_STR_LEN_MAX     64
#define SR_CMD_PHONEME_LEN_MAX 64
#define WAKEWORD_COMMAND 			 ""

srmodel_list_t* getModels();
afe_config_t* getAfeConfig();
esp_afe_sr_data_t* getAfeData();
const esp_afe_sr_iface_t *getAfeHandle();
void feedAfe(int16_t *audio_buffer);
afe_fetch_result_t* fetchAfe();
vad_state_t getAfeState();
unsigned long getLastSpeech();

namespace SR {

typedef struct csr_cmd_t {
  int command_id;
  char str[SR_CMD_STR_LEN_MAX];
  char phoneme[SR_CMD_PHONEME_LEN_MAX];
} csr_cmd_t;

esp_err_t setup(
	sr_fill_cb fill_cb, void *fill_cb_arg, sr_mode_t mode, const SR::csr_cmd_t *sr_commands, size_t cmd_number, sr_event_cb cb, void *cb_arg
);

esp_err_t start(BaseType_t feedCore = 1, BaseType_t detectCore = 0);
esp_err_t stop(void);
esp_err_t pause(void);
esp_err_t resume(void);
esp_err_t set_mode(sr_mode_t mode);

}

#endif  // CONFIG_IDF_TARGET_ESP32S3
