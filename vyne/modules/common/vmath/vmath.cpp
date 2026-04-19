#include "vmath.h"
#include <cmath>
#include <random>
#include <algorithm>
#include <stdexcept>

/**
 * VMathNative Native Method Implementations
 */

namespace VMathNative {

    bool isNumeric(const Value& v) {
        int type = v.getType();
        return type == Value::FLOAT64 || type == Value::INT64;
    }

    Value clamp(std::vector<Value>& args) {
        if (args.size() != 3) {
            throw std::runtime_error("Argument Error: vmath.clamp() expects 3 arguments (val, min, max), but got " + std::to_string(args.size()) + ".");
        }

        double val = args[0].asFloat();
        double min = args[1].asFloat();
        double max = args[2].asFloat();

        if (min > max) std::swap(min, max);

        if (val < min) return Value(min);
        if (val > max) return Value(max);
        
        return Value(val);
    }
    
    Value random(std::vector<Value>& args) {
        if (args.size() < 2) throw std::runtime_error("Argument Error : vcore.random() expects 2 arguments (min, max)");
        
        static std::random_device rd;
        static std::mt19937 gen(rd());
        
        std::uniform_int_distribution<long long> dist(
            args[0].asInt(), 
            args[1].asInt()
        );
        return Value(static_cast<int64_t>(dist(gen)));
    }

    Value sqrt(std::vector<Value>& args) {
        if (args.size() != 1) {
            throw std::runtime_error("Argument Error: vmath.sqrt() expects 1 argument.");
        }

        if (!isNumeric(args[0])) {
            throw std::runtime_error("Argument Error: vmath.sqrt() expects a Number, but got " + args[0].getTypeName());
        }

        double val = args[0].asFloat();
        if (val < 0) {
            throw std::runtime_error("Runtime Error: Square root of negative number is not supported.");
        }

        return Value(std::sqrt(val));
    }

    Value abs(std::vector<Value>& args) {
        if (args.size() != 1) throw std::runtime_error("vmath.abs() expects 1 argument.");
        if (!isNumeric(args[0])) throw std::runtime_error("vmath.abs() expects a Number.");

        double val = args[0].asFloat();
        return Value(std::abs(val));
    }

    Value sinh(std::vector<Value>& args) {
        if (args.size() != 1 || !isNumeric(args[0])) throw std::runtime_error("vmath.sinh() invalid arguments.");
        return Value(std::sinh(args[0].asFloat()));
    }

    Value cosh(std::vector<Value>& args) {
        if (args.size() != 1 || !isNumeric(args[0])) throw std::runtime_error("vmath.cosh() invalid arguments.");
        return Value(std::cosh(args[0].asFloat()));
    }

    Value tanh(std::vector<Value>& args) {
        if (args.size() != 1 || !isNumeric(args[0])) throw std::runtime_error("vmath.tanh() invalid arguments.");
        return Value(std::tanh(args[0].asFloat()));
    }

    Value degrees(std::vector<Value>& args) {
        if (args.size() != 1 || !isNumeric(args[0])) throw std::runtime_error("vmath.degrees() invalid arguments.");
        return Value(args[0].asFloat() * 180.0 / 3.141592653589793);
    }

    Value radians(std::vector<Value>& args) {
        if (args.size() != 1 || !isNumeric(args[0])) throw std::runtime_error("vmath.radians() invalid arguments.");
        return Value(args[0].asFloat() * 3.141592653589793 / 180.0);
    }

    Value fmod(std::vector<Value>& args) {
        if (args.size() != 2 || !isNumeric(args[0]) || !isNumeric(args[1])) 
            throw std::runtime_error("vmath.fmod() expects 2 Numbers.");

        double a = args[0].asFloat();
        double b = args[1].asFloat();
        if (b == 0) throw std::runtime_error("Runtime Error: Division by zero in fmod.");
        return Value(std::fmod(a, b));
    }

    Value hypot(std::vector<Value>& args) {
        if (args.size() != 2 || !isNumeric(args[0]) || !isNumeric(args[1])) 
            throw std::runtime_error("vmath.hypot() expects 2 Numbers.");
        return Value(std::hypot(args[0].asFloat(), args[1].asFloat()));
    }

    Value sin(std::vector<Value>& args) {
        if (args.size() != 1 || !isNumeric(args[0])) throw std::runtime_error("vmath.sin() invalid arguments.");
        return Value(std::sin(args[0].asFloat()));
    }

    Value cos(std::vector<Value>& args) {
        if (args.size() != 1 || !isNumeric(args[0])) throw std::runtime_error("vmath.cos() invalid arguments.");
        return Value(std::cos(args[0].asFloat()));
    }

    Value tan(std::vector<Value>& args) {
        if (args.size() != 1 || !isNumeric(args[0])) throw std::runtime_error("vmath.tan() invalid arguments.");
        return Value(std::tan(args[0].asFloat()));
    }

    Value asin(std::vector<Value>& args) {
        if (args.size() != 1 || !isNumeric(args[0])) throw std::runtime_error("vmath.asin() invalid arguments.");
        double val = args[0].asFloat();
        if (val < -1.0 || val > 1.0) throw std::runtime_error("asin range error.");
        return Value(std::asin(val));
    }

    Value acos(std::vector<Value>& args) {
        if (args.size() != 1 || !isNumeric(args[0])) throw std::runtime_error("vmath.acos() invalid arguments.");
        double val = args[0].asFloat();
        if (val < -1.0 || val > 1.0) throw std::runtime_error("acos range error.");
        return Value(std::acos(val));
    }

    Value atan(std::vector<Value>& args) {
        if (args.size() != 1 || !isNumeric(args[0])) throw std::runtime_error("vmath.atan() invalid arguments.");
        return Value(std::atan(args[0].asFloat()));
    }

    Value atan2(std::vector<Value>& args) {
        if (args.size() != 2 || !isNumeric(args[0]) || !isNumeric(args[1])) 
            throw std::runtime_error("vmath.atan2() invalid arguments.");
        return Value(std::atan2(args[0].asFloat(), args[1].asFloat()));
    }

    Value log(std::vector<Value>& args) {
        if (args.size() != 1 || !isNumeric(args[0])) throw std::runtime_error("vmath.log() invalid arguments.");
        double val = args[0].asFloat();
        if (val <= 0) throw std::runtime_error("log must be positive.");
        return Value(std::log(val));
    }

    Value log10(std::vector<Value>& args) {
        if (args.size() != 1 || !isNumeric(args[0])) throw std::runtime_error("vmath.log10() invalid arguments.");
        double val = args[0].asFloat();
        if (val <= 0) throw std::runtime_error("log10 must be positive.");
        return Value(std::log10(val));
    }

    Value exp(std::vector<Value>& args) {
        if (args.size() != 1 || !isNumeric(args[0])) throw std::runtime_error("vmath.exp() invalid arguments.");
        return Value(std::exp(args[0].asFloat()));
    }

    Value pow(std::vector<Value>& args) {
        if (args.size() != 2 || !isNumeric(args[0]) || !isNumeric(args[1])) 
            throw std::runtime_error("vmath.pow() expects 2 Numbers.");
        return Value(std::pow(args[0].asFloat(), args[1].asFloat()));
    }

    Value floor(std::vector<Value>& args) {
        if (args.size() != 1 || !isNumeric(args[0])) throw std::runtime_error("vmath.floor() invalid arguments.");
        return Value(std::floor(args[0].asFloat()));
    }

    Value ceil(std::vector<Value>& args) {
        if (args.size() != 1 || !isNumeric(args[0])) throw std::runtime_error("vmath.ceil() invalid arguments.");
        return Value(std::ceil(args[0].asFloat()));
    }

    Value round(std::vector<Value>& args) {
        if (args.size() != 1 || !isNumeric(args[0])) throw std::runtime_error("vmath.round() invalid arguments.");
        return Value(std::round(args[0].asFloat()));
    }

    Value min(std::vector<Value>& args) {
        if (args.size() != 2 || !isNumeric(args[0]) || !isNumeric(args[1])) 
            throw std::runtime_error("vmath.min() invalid arguments.");
        double a = args[0].asFloat();
        double b = args[1].asFloat();
        return Value(a < b ? a : b);
    }

    Value max(std::vector<Value>& args) {
        if (args.size() != 2 || !isNumeric(args[0]) || !isNumeric(args[1])) 
            throw std::runtime_error("vmath.max() invalid arguments.");
        double a = args[0].asFloat();
        double b = args[1].asFloat();
        return Value(a > b ? a : b);
    }

    Value erf(std::vector<Value>& args) {
        if (args.size() != 1 || !isNumeric(args[0])) throw std::runtime_error("vmath.erf() invalid arguments.");
        return Value(std::erf(args[0].asFloat()));
    }

    Value erfc(std::vector<Value>& args) {
        if (args.size() != 1 || !isNumeric(args[0])) throw std::runtime_error("vmath.erfc() invalid arguments.");
        return Value(std::erfc(args[0].asFloat()));
    }

    Value tgamma(std::vector<Value>& args) {
        if (args.size() != 1 || !isNumeric(args[0])) throw std::runtime_error("vmath.tgamma() invalid arguments.");
        return Value(std::tgamma(args[0].asFloat()));
    }

    Value lgamma(std::vector<Value>& args) {
        if (args.size() != 1 || !isNumeric(args[0])) throw std::runtime_error("vmath.lgamma() invalid arguments.");
        return Value(std::lgamma(args[0].asFloat()));
    }

    Value sigmoid(std::vector<Value>& args) {
        if (args.size() != 1 || !isNumeric(args[0])) {
            throw std::runtime_error("Argument Error: vmath.sigmoid() expects a Number.");
        }
        
        double x = args[0].asFloat();
        return Value(1.0 / (1.0 + std::exp(-x)));
    }

    Value relu(std::vector<Value>& args) {
        if (args.size() != 1 || !isNumeric(args[0])) {
            throw std::runtime_error("Argument Error: vmath.sigmoid() expects a Number.");
        }
        double x = args[0].asFloat();
        return Value(x > 0 ? x : 0.0);
    }
}

void setupVMath(SymbolContainer& env, StringPool& pool) {
    const std::string& path = "vmath";
    
    if (env.find(path) == env.end()) {
        env[path] = SymbolTable();
    }

    auto& vmath = env[path];

    // VMath methods
    vmath[pool.intern("clamp")]    = Value(VMathNative::clamp);
    vmath[pool.intern("random")]   = Value(VMathNative::random);
    vmath[pool.intern("sqrt")]     = Value(VMathNative::sqrt);
    vmath[pool.intern("abs")]      = Value(VMathNative::abs);
    vmath[pool.intern("sinh")]     = Value(VMathNative::sinh);
    vmath[pool.intern("cosh")]     = Value(VMathNative::cosh);
    vmath[pool.intern("tanh")]     = Value(VMathNative::tanh);
    vmath[pool.intern("degrees")]  = Value(VMathNative::degrees);
    vmath[pool.intern("radians")]  = Value(VMathNative::radians);
    vmath[pool.intern("fmod")]     = Value(VMathNative::fmod);
    vmath[pool.intern("hypot")]    = Value(VMathNative::hypot);
    vmath[pool.intern("sin")]      = Value(VMathNative::sin);
    vmath[pool.intern("cos")]      = Value(VMathNative::cos);
    vmath[pool.intern("tan")]      = Value(VMathNative::tan);
    vmath[pool.intern("asin")]     = Value(VMathNative::asin);
    vmath[pool.intern("acos")]     = Value(VMathNative::acos);
    vmath[pool.intern("atan")]     = Value(VMathNative::atan);
    vmath[pool.intern("atan2")]    = Value(VMathNative::atan2);
    vmath[pool.intern("log")]      = Value(VMathNative::log);
    vmath[pool.intern("log10")]    = Value(VMathNative::log10);
    vmath[pool.intern("exp")]      = Value(VMathNative::exp);
    vmath[pool.intern("pow")]      = Value(VMathNative::pow);
    vmath[pool.intern("floor")]    = Value(VMathNative::floor);
    vmath[pool.intern("ceil")]     = Value(VMathNative::ceil);
    vmath[pool.intern("round")]    = Value(VMathNative::round);
    vmath[pool.intern("min")]      = Value(VMathNative::min);
    vmath[pool.intern("max")]      = Value(VMathNative::max);
    vmath[pool.intern("erf")]      = Value(VMathNative::erf);
    vmath[pool.intern("erfc")]     = Value(VMathNative::erfc);
    vmath[pool.intern("tgamma")]   = Value(VMathNative::tgamma);
    vmath[pool.intern("lgamma")]   = Value(VMathNative::lgamma);
    vmath[pool.intern("sigmoid")] = Value(VMathNative::sigmoid);

    // VMath constants
    vmath[pool.intern("pi")]         = Value(3.141592653589793).setReadOnly();
    vmath[pool.intern("e")]          = Value(2.718281828459045).setReadOnly();
    vmath[pool.intern("tau")]        = Value(6.283185307179586).setReadOnly();
    vmath[pool.intern("phi")]        = Value(1.618033988749895).setReadOnly();
    vmath[pool.intern("sqrt2")]      = Value(1.414213562373095).setReadOnly();
    vmath[pool.intern("inf")]        = Value(INFINITY).setReadOnly();
    vmath[pool.intern("nan")]        = Value(NAN).setReadOnly();

    vmath[pool.intern("version")]  = Value("v0.0.1-alpha").setReadOnly();
}