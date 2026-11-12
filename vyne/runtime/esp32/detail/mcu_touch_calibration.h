#pragma once
#include <stdint.h>
#include <stdlib.h>

// Separate-SPI touch calibration has its own versioned NVS blob. The
// existing TFT_eSPI five-word calibration remains under its original key.
struct VyneTouchCalibration {
    uint32_t version;
    uint16_t width, height;
    uint8_t rotation, swap_axes;
    uint16_t reserved;
    int32_t x0, x1, y0, y1;
};

static inline bool vyne_touch_calibration_valid(const VyneTouchCalibration& c) {
    return c.version == 1 && c.width > 1 && c.width <= 4096 &&
           c.height > 1 && c.height <= 4096 && c.rotation < 4 && c.swap_axes < 2 && (c.reserved & ~3) == 0 &&
           c.x0 >= -4096 && c.x0 <= 8192 && c.x1 >= -4096 && c.x1 <= 8192 &&
           c.y0 >= -4096 && c.y0 <= 8192 && c.y1 >= -4096 && c.y1 <= 8192 &&
           abs(c.x1 - c.x0) >= 100 && abs(c.y1 - c.y0) >= 100;
}

// Samples are TL, TR, BR, BL at a fixed inset. Signed endpoint spans
// represent axis inversion without unsigned subtraction or division bugs.
static inline bool vyne_touch_calibration_fit(
    const int32_t raw[4][2], int width, int height, int inset, int rotation,
    VyneTouchCalibration& out, bool mirror_x = false, bool mirror_y = false
) {
    if (width < 2 || width > 4096 || height < 2 || height > 4096 ||
        inset < 0 || inset > (width - 2) / 2 || inset > (height - 2) / 2 || rotation < 0 || rotation > 3)
        return false;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 2; ++j)
            if (raw[i][j] < 0 || raw[i][j] > 4095)
                return false;
    bool swap = abs(raw[1][1] - raw[0][1]) > abs(raw[1][0] - raw[0][0]);
    int xaxis = swap ? 1 : 0;
    int yaxis = swap ? 0 : 1;
    int32_t left = (raw[0][xaxis] + raw[3][xaxis]) / 2;
    int32_t right = (raw[1][xaxis] + raw[2][xaxis]) / 2;
    int32_t top = (raw[0][yaxis] + raw[1][yaxis]) / 2;
    int32_t bottom = (raw[2][yaxis] + raw[3][yaxis]) / 2;
    // Opposite corners of the same edge should agree on that axis.
    // Reject a missed target or mixed readings rather than saving a warp.
    int32_t tolerance_x = abs(right - left) / 4;
    int32_t tolerance_y = abs(bottom - top) / 4;
    if (abs(raw[0][xaxis] - raw[3][xaxis]) > tolerance_x ||
        abs(raw[1][xaxis] - raw[2][xaxis]) > tolerance_x ||
        abs(raw[0][yaxis] - raw[1][yaxis]) > tolerance_y ||
        abs(raw[2][yaxis] - raw[3][yaxis]) > tolerance_y)
        return false;
    int32_t dx = (right - left) * inset / (width - 1 - 2 * inset);
    int32_t dy = (bottom - top) * inset / (height - 1 - 2 * inset);
    VyneTouchCalibration c = {1, (uint16_t)width, (uint16_t)height,
        (uint8_t)rotation, (uint8_t)swap, (uint16_t)(mirror_x | (mirror_y << 1)),
        left - dx, right + dx, top - dy, bottom + dy};
    if (!vyne_touch_calibration_valid(c))
        return false;
    out = c;
    return true;
}

static inline bool vyne_touch_calibration_matches(const VyneTouchCalibration& c,
                                                 int width, int height, int rotation) {
    if (!vyne_touch_calibration_valid(c) || rotation < 0 || rotation > 3)
        return false;
    bool swapped = ((rotation - c.rotation + 4) % 4) & 1;
    return width == (swapped ? c.height : c.width) && height == (swapped ? c.width : c.height);
}

static inline int32_t _vyne_touch_scale(int32_t raw, int32_t lo, int32_t hi, int extent) {
    int64_t value = ((int64_t)raw - lo) * (extent - 1) / ((int64_t)hi - lo);
    if (value < 0) value = 0;
    if (value >= extent) value = extent - 1;
    return (int32_t)value;
}

static inline bool vyne_touch_calibration_map(
    const VyneTouchCalibration& c, int raw_x, int raw_y, int rotation,
    uint16_t& x, uint16_t& y, bool mirror_x = false, bool mirror_y = false
) {
    if (!vyne_touch_calibration_valid(c) || raw_x < 0 || raw_x > 4095 ||
        raw_y < 0 || raw_y > 4095 || rotation < 0 || rotation > 3)
        return false;
    int32_t xx = _vyne_touch_scale(c.swap_axes ? raw_y : raw_x, c.x0, c.x1, c.width);
    int32_t yy = _vyne_touch_scale(c.swap_axes ? raw_x : raw_y, c.y0, c.y1, c.height);
    if (c.reserved & 1) xx = c.width - 1 - xx;
    if (c.reserved & 2) yy = c.height - 1 - yy;
    int delta = (rotation - c.rotation + 4) % 4;
    switch (delta) {
    case 0: x = xx; y = yy; break;
    case 1: x = yy; y = c.width - 1 - xx; break;
    case 2: x = c.width - 1 - xx; y = c.height - 1 - yy; break;
    case 3: x = c.height - 1 - yy; y = xx; break;
    }
    if (mirror_x) x = ((delta & 1) ? c.height : c.width) - 1 - x;
    if (mirror_y) y = ((delta & 1) ? c.width : c.height) - 1 - y;
    return true;
}
