#pragma once
#include <Arduino.h>

// Call once in setup()
void mic_inference_init();

// Call at the start of each WAITING_FOR_KEYWORD entry to reset the slice counter
void mic_inference_reset();

// Call every loop tick while in WAITING_FOR_KEYWORD.
// Returns true and sets out_label when a confident color word is detected.
// out_label will be lowercase (e.g. "blue", "red"). Noise/unknown returns false.
bool mic_inference_run(String &out_label);
