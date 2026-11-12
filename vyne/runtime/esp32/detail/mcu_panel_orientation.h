#pragma once
#include <stdint.h>

// ILI9341/ST7796 rotations share this MADCTL layout. Mirrors are expressed
// in logical screen coordinates, so MX/MY exchange roles in landscape.
static inline uint8_t vyne_panel_madctl(uint8_t rotation, uint8_t color_order,
                                      bool mirror_x, bool mirror_y) {
    const uint8_t orientation[] = {0x40, 0x20, 0x80, 0xE0};
    uint8_t value = orientation[rotation & 3] | (color_order & 8);
    if (mirror_x) value ^= (rotation & 1) ? 0x80 : 0x40;
    if (mirror_y) value ^= (rotation & 1) ? 0x40 : 0x80;
    return value;
}
