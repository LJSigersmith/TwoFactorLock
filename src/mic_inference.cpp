#define EIDSP_QUANTIZE_FILTERBANK 0
#include <Final-Project_inferencing.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_pdm.h"

#include "mic_inference.h"

// XIAO ESP32S3 Sense PDM mic pins
#define MIC_CLK_PIN  42
#define MIC_DATA_PIN 41

typedef struct {
    signed short *buffers[2];
    unsigned char buf_select;
    unsigned char buf_ready;
    unsigned int  buf_count;
    unsigned int  n_samples;
} inference_t;

static inference_t       s_inference;
static signed short      s_sample_buf[2048];
static bool              s_record_status = true;
static int               s_slice_count   = -(EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW);
static i2s_chan_handle_t s_rx_chan        = NULL;

/* ── Internal helpers ──────────────────────────────────────────────────────── */

static void audio_inference_callback(uint32_t n_bytes) {
    for (int i = 0; i < n_bytes >> 1; i++) {
        s_inference.buffers[s_inference.buf_select][s_inference.buf_count++] = s_sample_buf[i];
        if (s_inference.buf_count >= s_inference.n_samples) {
            s_inference.buf_select ^= 1;
            s_inference.buf_count   = 0;
            s_inference.buf_ready   = 1;
        }
    }
}

static void capture_samples(void* arg) {
    const int32_t bytes_to_read = (uint32_t)arg;
    size_t bytes_read = 0;
    Serial.println("DBG: capture task started");
    while (s_record_status) {
        esp_err_t ret = i2s_channel_read(s_rx_chan, (void*)s_sample_buf, bytes_to_read, &bytes_read, portMAX_DELAY);
        Serial.println("DBG: i2s_read ret=" + String(ret) + " bytes=" + String(bytes_read));
        if (ret == ESP_OK && bytes_read > 0) {
            for (int x = 0; x < (int)(bytes_read / 2); x++)
                s_sample_buf[x] = (int16_t)(s_sample_buf[x]) * 8;
            if (s_record_status) audio_inference_callback(bytes_read);
        }
    }
    vTaskDelete(NULL);
}

static bool mic_pdm_init(uint32_t sampling_rate) {
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    if (i2s_new_channel(&chan_cfg, NULL, &s_rx_chan) != ESP_OK) return false;

    i2s_pdm_rx_config_t pdm_cfg = {
        .clk_cfg  = I2S_PDM_RX_CLK_DEFAULT_CONFIG(sampling_rate),
        .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .clk = (gpio_num_t)MIC_CLK_PIN,
            .din = (gpio_num_t)MIC_DATA_PIN,
            .invert_flags = { .clk_inv = false },
        },
    };
    if (i2s_channel_init_pdm_rx_mode(s_rx_chan, &pdm_cfg) != ESP_OK) return false;
    if (i2s_channel_enable(s_rx_chan) != ESP_OK) return false;
    return true;
}

static int audio_signal_get_data(size_t offset, size_t length, float *out_ptr) {
    numpy::int16_to_float(
        &s_inference.buffers[s_inference.buf_select ^ 1][offset], out_ptr, length);
    return 0;
}

/* ── Public API ────────────────────────────────────────────────────────────── */

void mic_inference_init() {
    uint32_t n_samples = EI_CLASSIFIER_SLICE_SIZE;

    s_inference.buffers[0] = (signed short*)malloc(n_samples * sizeof(signed short));
    s_inference.buffers[1] = (signed short*)malloc(n_samples * sizeof(signed short));
    if (!s_inference.buffers[0] || !s_inference.buffers[1]) {
        Serial.println("ERR: mic buffer alloc failed");
        return;
    }
    s_inference.buf_select = 0;
    s_inference.buf_count  = 0;
    s_inference.n_samples  = n_samples;
    s_inference.buf_ready  = 0;

    if (!mic_pdm_init(EI_CLASSIFIER_FREQUENCY)) {
        Serial.println("ERR: PDM init failed");
        return;
    }

    run_classifier_init();
    ei_sleep(100);

    s_record_status = true;
    xTaskCreate(capture_samples, "CaptureSamples", 1024 * 32, (void*)2048, 10, NULL);
}

void mic_inference_reset() {
    s_slice_count = -(EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW);
}

bool mic_inference_run(String &out_label) {
    Serial.println("DBG: waiting for slice...");
    while (s_inference.buf_ready == 0) delay(1);
    s_inference.buf_ready = 0;
    Serial.println("DBG: slice ready");

    signal_t signal;
    signal.total_length = EI_CLASSIFIER_SLICE_SIZE;
    signal.get_data     = &audio_signal_get_data;
    ei_impulse_result_t result = {0};

    EI_IMPULSE_ERROR err = run_classifier_continuous(&signal, &result, false);
    if (err != EI_IMPULSE_OK) {
        Serial.println("DBG: classifier error " + String(err));
        return false;
    }

    if (++s_slice_count < EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW) {
        Serial.println("DBG: slice " + String(s_slice_count) + "/" + String(EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW));
        return false;
    }
    s_slice_count = 0;

    Serial.println("DBG: --- window results ---");
    for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        Serial.println("  " + String(result.classification[i].label)
                       + ": " + String(result.classification[i].value, 3));
    }

    int   best_idx = -1;
    float best_val = EI_CLASSIFIER_THRESHOLD;
    for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        if (result.classification[i].value > best_val) {
            best_val = result.classification[i].value;
            best_idx = i;
        }
    }
    if (best_idx < 0) {
        Serial.println("DBG: no confident detection");
        return false;
    }

    String label = String(result.classification[best_idx].label);
    label.toLowerCase();
    Serial.println("DBG: best = " + label + " (" + String(best_val, 3) + ")");

    if (label == "noise" || label == "noise2" || label == "unknown" || label == "off") {
        Serial.println("DBG: filtered as noise");
        return false;
    }

    out_label = label;
    return true;
}
