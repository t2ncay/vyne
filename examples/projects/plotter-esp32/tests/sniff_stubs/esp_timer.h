#pragma once
#include <stdint.h>
namespace sniff_mock { inline int64_t clock_us = 1000; }
inline int64_t esp_timer_get_time() { return sniff_mock::clock_us; }
