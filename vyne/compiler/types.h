#pragma once
#include <string_view>
#include <string>

enum class VType { 
    Unknown, 
    Int64, 
    Float64, 
    String, 
    Array, 
    Function, 
    Module 
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