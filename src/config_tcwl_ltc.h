#pragma once
#include "config.h"
#define TCWL_LTC 1
#undef MAX7219_ENABLE
#define MAX7219_ENABLE 0

// XIAO ESP32-C3: A0 = GPIO 0 (ADC1_CH0), 200k:200k divider
#undef BAT_ADC_PIN
#define BAT_ADC_PIN           0
