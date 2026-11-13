#include "../../vyne/runtime/esp32/detail/mcu_touch_calibration.h"
#include "../../vyne/runtime/esp32/detail/mcu_panel_orientation.h"
#include <assert.h>
#include <stdio.h>

int main() {
    assert(vyne_panel_madctl(1, 0, false, false) == 0x20);
    assert(vyne_panel_madctl(1, 0, false, true) == 0x60);
    assert(vyne_panel_madctl(1, 8, false, true) == 0x68);
    assert(vyne_panel_madctl(0, 0, true, false) == 0);
    assert(vyne_panel_madctl(3, 0, true, true) == 0x20);
    VyneTouchCalibration c = {1, 320, 240, 1, 0, 0, 400, 3600, 500, 3500};
    uint16_t x = 0, y = 0;
    assert(vyne_touch_calibration_map(c, 400, 500, 1, x, y) && x == 0 && y == 0);
    assert(vyne_touch_calibration_map(c, 3600, 3500, 1, x, y) && x == 319 && y == 239);
    assert(vyne_touch_calibration_map(c, 0, 4095, 1, x, y) && x == 0 && y == 239);
    assert(vyne_touch_calibration_map(c, 400, 500, 2, x, y) && x == 0 && y == 319);
    assert(vyne_touch_calibration_map(c, 400, 500, 3, x, y) && x == 319 && y == 239);
    assert(vyne_touch_calibration_map(c, 400, 500, 0, x, y) && x == 239 && y == 0);
    assert(vyne_touch_calibration_matches(c, 320, 240, 1));
    assert(vyne_touch_calibration_matches(c, 240, 320, 2));
    assert(!vyne_touch_calibration_matches(c, 480, 320, 1));
    c.swap_axes = 1;
    c.x0 = 3600; c.x1 = 400;
    assert(vyne_touch_calibration_map(c, 500, 3600, 1, x, y) && x == 0 && y == 0);
    assert(vyne_touch_calibration_map(c, 3500, 400, 1, x, y) && x == 319 && y == 239);

    const int32_t corners[4][2] = {{500, 3600}, {500, 400}, {3500, 400}, {3500, 3600}};
    assert(vyne_touch_calibration_fit(corners, 320, 240, 20, 1, c));
    assert(c.swap_axes == 1 && c.x1 < c.x0);
    assert(vyne_touch_calibration_map(c, 500, 3600, 1, x, y));
    assert(x >= 19 && x <= 20 && y >= 19 && y <= 20);
    assert(vyne_touch_calibration_map(c, 3500, 400, 1, x, y));
    assert(x >= 299 && x <= 300 && y >= 219 && y <= 220);

    VyneTouchCalibration before = c;
    const int32_t flat[4][2] = {{1, 1}, {1, 1}, {1, 1}, {1, 1}};
    const int32_t missed_target[4][2] = {{576, 3445}, {548, 451}, {3374, 1199}, {915, 3425}};
    assert(!vyne_touch_calibration_fit(missed_target, 480, 320, 20, 1, c));
    assert(!vyne_touch_calibration_fit(flat, 320, 240, 20, 1, c));
    assert(c.x0 == before.x0 && c.x1 == before.x1);
    assert(!vyne_touch_calibration_fit(corners, 320, 240, INT32_MAX, 1, c));
    assert(!vyne_touch_calibration_fit(corners, 65536 + 320, 240, 20, 1, c));
    assert(!vyne_touch_calibration_fit(corners, 320, 240, 20, 257, c));
    c.version = 2;
    assert(!vyne_touch_calibration_map(c, 500, 3600, 1, x, y));
    c = before; c.x1 = c.x0;
    assert(!vyne_touch_calibration_map(c, 500, 3600, 1, x, y));
    c = before; c.rotation = 4;
    assert(!vyne_touch_calibration_valid(c));
    c = before; c.x0 = INT32_MIN;
    assert(!vyne_touch_calibration_valid(c));
    c = before; c.reserved = 2;
    assert(vyne_touch_calibration_map(c, 500, 3600, 1, x, y, false, true));
    assert(x >= 19 && x <= 20 && y >= 19 && y <= 20);
    c.reserved = 4;
    assert(!vyne_touch_calibration_valid(c));
    assert(!vyne_touch_calibration_map(before, -1, 0, 1, x, y));
    puts("touch calibration geometry, rotation, invalid-data checks: PASS");
}
