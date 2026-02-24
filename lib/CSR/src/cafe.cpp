#include "csr.h"

srmodel_list_t *models = nullptr;
afe_config_t *afe_config;
const esp_afe_sr_iface_t *afe_handle;
esp_afe_sr_data_t *afe_data;
int16_t *afe_in_buffer;
vad_state_t afe_state = VAD_SILENCE;
unsigned long afe_last_speech = 0;

srmodel_list_t* getModels() {
	if (models)
		return models;

  models = esp_srmodel_init("model");
	return models;
}

afe_config_t* getAfeConfig() {
	if (afe_config) return afe_config;

	// Load WakeWord Detection
  // https://docs.espressif.com/projects/esp-sr/en/latest/esp32/audio_front_end/migration_guide.html
  afe_config = afe_config_init("M", models, AFE_TYPE_SR, AFE_MODE_HIGH_PERF);
  afe_config->wakenet_model_name = esp_srmodel_filter(models, ESP_WN_PREFIX, WAKEWORD_COMMAND);
  afe_config->aec_init = true;
  afe_config->aec_mode = AEC_MODE_SR_HIGH_PERF;
  afe_config->se_init = true;
  afe_config->vad_mode = VAD_MODE_1;
	afe_config->fixed_first_channel = true;
  afe_config->memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;
  afe_config = afe_config_check(afe_config);
  afe_config_print(afe_config);

	return afe_config;
}

esp_afe_sr_data_t* getAfeData() {
	if (afe_data)
		return afe_data;

	afe_data = getAfeHandle()->create_from_config(getAfeConfig());
	return afe_data;
}

const esp_afe_sr_iface_t* getAfeHandle() {
	if (afe_handle)
		return afe_handle;

	auto models = getModels();
  afe_handle = (esp_afe_sr_iface_t*)esp_afe_handle_from_config(getAfeConfig());
	return afe_handle;
}

int getAfeChunkSize(esp_afe_sr_data_t *data) {
	return getAfeHandle()->get_feed_chunksize(data) * sizeof(int16_t);
}

void feedAfe(int16_t *audio_buffer) {
	getAfeHandle()->feed(getAfeData(), audio_buffer);
}

afe_fetch_result_t* fetchAfe() {
	afe_fetch_result_t *result = getAfeHandle()->fetch(getAfeData());
	if (result && result->ret_value != ESP_FAIL){
		afe_state = result->vad_state;
		if (afe_state == VAD_SPEECH) {
			afe_last_speech = millis();
		};
	}
	return result;
}

unsigned long getLastSpeech() {
	return afe_last_speech;
}

vad_state_t getAfeState() {
	return afe_state;
}