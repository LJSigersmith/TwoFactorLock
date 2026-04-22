#pragma once

#include <Arduino.h>
#include <SdFat.h>

void logging_init(SdFat& sd);
void log_event_to_file(String stage, String result, String detail);
String getTimestamp(bool pretty);
