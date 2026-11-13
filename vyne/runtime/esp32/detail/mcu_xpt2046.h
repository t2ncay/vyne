#pragma once
#include <Arduino.h>
#include <SPI.h>

#if !defined(VYNE_TOUCH_CS) || !defined(VYNE_TOUCH_SCLK) || \
    !defined(VYNE_TOUCH_MOSI) || !defined(VYNE_TOUCH_MISO)
#error "Separate-SPI XPT2046 requires VYNE_TOUCH_CS/SCLK/MOSI/MISO"
#endif
#ifndef VYNE_TOUCH_SPI_PORT
#define VYNE_TOUCH_SPI_PORT VSPI
#endif
#ifndef VYNE_TOUCH_SPI_FREQUENCY
#define VYNE_TOUCH_SPI_FREQUENCY 2000000
#endif

// XPT2046's command byte selects a 12-bit differential ADC channel. Keep
// the ADC on while sampling; finish with PD=00 so PENIRQ works again.
// Independent SPIClass leaves the panel's HSPI pin routing untouched.
static SPIClass g_vyne_touch_spi(VYNE_TOUCH_SPI_PORT);
static bool g_vyne_touch_spi_inited = false;

struct VyneTouchRaw { uint16_t x, y, pressure; };

static inline void _vyne_xpt_begin(void) {
    if (g_vyne_touch_spi_inited)
        return;
    pinMode(VYNE_TOUCH_CS, OUTPUT);
    digitalWrite(VYNE_TOUCH_CS, HIGH);
#if defined(VYNE_TOUCH_IRQ) && VYNE_TOUCH_IRQ >= 0
    pinMode(VYNE_TOUCH_IRQ, INPUT);
#endif
    g_vyne_touch_spi.begin(VYNE_TOUCH_SCLK, VYNE_TOUCH_MISO, VYNE_TOUCH_MOSI, VYNE_TOUCH_CS);
    g_vyne_touch_spi_inited = true;
}

static inline uint16_t _vyne_xpt_adc(uint8_t channel) {
    g_vyne_touch_spi.transfer(channel);
    delayMicroseconds(5);
    return (g_vyne_touch_spi.transfer16(0) >> 3) & 0x0FFF;
}

static inline uint16_t _vyne_xpt_median(uint8_t channel) {
    uint16_t samples[5];
    (void)_vyne_xpt_adc(channel); // Discard the first conversion after changing channel.
    for (int i = 0; i < 5; ++i) {
        samples[i] = _vyne_xpt_adc(channel);
        for (int j = i; j > 0 && samples[j] < samples[j - 1]; --j) {
            uint16_t temp = samples[j];
            samples[j] = samples[j - 1];
            samples[j - 1] = temp;
        }
    }
    return samples[2];
}

static inline bool _vyne_xpt_read_raw(VyneTouchRaw& out, int threshold = 600) {
    _vyne_xpt_begin();
    g_vyne_touch_spi.beginTransaction(SPISettings(VYNE_TOUCH_SPI_FREQUENCY, MSBFIRST, SPI_MODE0));
    digitalWrite(VYNE_TOUCH_CS, LOW);
    uint16_t z1 = _vyne_xpt_adc(0xB1);
    uint16_t z2 = _vyne_xpt_adc(0xC1);
    uint16_t x = _vyne_xpt_median(0xD1);
    uint16_t y = _vyne_xpt_median(0x91);
    (void)_vyne_xpt_adc(0xD0); // Power down and re-enable PENIRQ.
    digitalWrite(VYNE_TOUCH_CS, HIGH);
    g_vyne_touch_spi.endTransaction();
    int pressure = (int)z1 + 4095 - z2;
    out = {x, y, (uint16_t)pressure};
    // Reject disconnected all-zero/all-high ADC responses as well as no contact.
    return z1 > 0 && z1 < 4095 && x > 0 && x < 4095 && y > 0 && y < 4095 && pressure >= threshold;
}
