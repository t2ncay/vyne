// Project-local compatibility for missing ESP32 scalar Float32 constructors.
// Boxed storage remains the runtime's double union; values round through float.
// This does not replace generated inference arithmetic or fix general array ABI.
#pragma once
static inline VyneValue vyne_float32(float value) {
    VyneValue result = vyne_float((double)value);
    result.type = static_cast<VyneType>(V_FLOAT32);
    return result;
}
static inline VyneValue vyne_to_float32(VyneValue value) {
    return vyne_float32((float)_vyne_as_f64(value));
}
