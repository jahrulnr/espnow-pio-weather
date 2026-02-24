#pragma once
#include <esp_partition.h>
#include <nvs_flash.h>
#include <esp_log.h>
#include <app_config.h>

const char *NVS_PARTITION_NAME = NULL;
const char *TAG = "nvs_core";

inline void nvs_init(void)
{
  const esp_partition_t *partitionNvs = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, NVS_PARTITION_NAME);
  if (partitionNvs == NULL)
  {
    ESP_LOGI(TAG, "NVS Partition with name \"%s\" not found", NVS_PARTITION_NAME);
  }
  else
  {
    ESP_LOGI(TAG, "NVS Partition labeled \"%s\" found", partitionNvs->label);
  }

  // Initialize NVS - options
  // esp_err_t err = nvs_flash_init(); // default - only for use with partition 'nvs'
  // esp_err_t err = nvs_flash_init_partition(NVS_PARTITION_NAME); // given a partition name char*
  // esp_err_t err = nvs_flash_init_partition_ptr(partitionNvs); // given a pointer to an esp_partition_t

  esp_err_t err = nvs_flash_init_partition_ptr(partitionNvs);
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    // NVS partition was truncated and needs to be erased
    // Retry nvs_flash_init
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);

  if (err != ESP_OK)
  {
    goto close;
  }
  
  ESP_LOGI(TAG, "Flash initialized success");
  nvs_handle_t my_handle;
  err = nvs_open("wifi", NVS_READWRITE, &my_handle);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
    goto close;
  }

  close:
  nvs_close(my_handle);
}