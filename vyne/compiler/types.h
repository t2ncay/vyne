#pragma once
#include <string_view>
#include <string>

enum class VType { 
    Unknown = 0, 
    Float64 = 1,
    Int64 = 2,
    String = 3,
    Array = 4,
    Function = 5, 
    Module = 6
};

inline VType stringToVType(std::string_view name) {
    if (name == "Int64")    return VType::Int64;
    if (name == "Float64")  return VType::Float64;
    if (name == "Array")  return VType::Array;
    if (name == "String") return VType::String;
    return VType::Unknown;
}

inline std::string VTypeToString(VType type) {
    switch (type) {
        case VType::Int64:    return "Int64";
        case VType::Float64:  return "Float64";
        case VType::Array:    return "Array";
        case VType::String:   return "String";
        case VType::Function: return "Function";
        case VType::Module:   return "Module";
        default:              return "Unknown";
    }
}