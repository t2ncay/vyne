#include "vmath.h"

/**
 * VMathNative Native Method Implementations
 */

namespace VMathNative {
    Value clamp(std::vector<Value>& args) {
        if (args.size() != 3) {
            throw std::runtime_error("Argument Error: vmath.clamp() expects 3 arguments (val, min, max), but got " + std::to_string(args.size()) + ".");
        }

        double val = args[0].asNumber();
        double min = args[1].asNumber();
        double max = args[2].asNumber();

        if (min > max) std::swap(min, max);

        if (val < min) return Value(min);
        if (val > max) return Value(max);
        
        return Value(val);
    }

    Value sqrt(std::vector<Value>& args) {
        if (args.size() != 1) {
            throw std::runtime_error("Argument Error: vmath.sqrt() expects 1 argument, but got " + std::to_string(args.size()) + ".");
        }

        if (args[0].getType() != Value::NUMBER) {
            throw std::runtime_error("Argument Error: vmath.sqrt() excepts only Number type as argument, but got " + args[0].getTypeName());
        }

        double val = args[0].asNumber();

        if (val < 0) {
            throw std::runtime_error("Runtime Error: Square root of negative number is not supported in vmath.sqrt().");
        }

        double returnVal = std::sqrt(val);

        return Value(returnVal);
    }

    Value abs(std::vector<Value>& args) {
        if (args.size() != 1) {
            throw std::runtime_error("Argument Error: vmath.abs() expects 1 argument, but got " + std::to_string(args.size()) + ".");
        }

        if (args[0].getType() != Value::NUMBER) {
            throw std::runtime_error("Argument Error: vmath.abs() expects only Number type as argument, but got " + args[0].getTypeName());
        }

        double val = args[0].asNumber();
        
        double returnVal = std::abs(val);

        return Value(returnVal);
    }

    Value sinh(std::vector<Value>& args) {
        if (args.size() != 1) {
            throw std::runtime_error("Argument Error: vmath.sinh() expects 1 argument, but got " + std::to_string(args.size()) + ".");
        }

        if (args[0].getType() != Value::NUMBER) {
            throw std::runtime_error("Argument Error: vmath.sinh() expects only Number type as argument, but got " + args[0].getTypeName());
        }

        double val = args[0].asNumber();
        return Value(std::sinh(val));
    }

    Value cosh(std::vector<Value>& args) {
        if (args.size() != 1) {
            throw std::runtime_error("Argument Error: vmath.cosh() expects 1 argument, but got " + std::to_string(args.size()) + ".");
        }

        if (args[0].getType() != Value::NUMBER) {
            throw std::runtime_error("Argument Error: vmath.cosh() expects only Number type as argument, but got " + args[0].getTypeName());
        }

        double val = args[0].asNumber();
        return Value(std::cosh(val));
    }

    Value tanh(std::vector<Value>& args) {
        if (args.size() != 1) {
            throw std::runtime_error("Argument Error: vmath.tanh() expects 1 argument, but got " + std::to_string(args.size()) + ".");
        }

        if (args[0].getType() != Value::NUMBER) {
            throw std::runtime_error("Argument Error: vmath.tanh() expects only Number type as argument, but got " + args[0].getTypeName());
        }

        double val = args[0].asNumber();
        return Value(std::tanh(val));
    }

    Value degrees(std::vector<Value>& args) {
        if (args.size() != 1) {
            throw std::runtime_error("Argument Error: vmath.degrees() expects 1 argument, but got " + std::to_string(args.size()) + ".");
        }

        if (args[0].getType() != Value::NUMBER) {
            throw std::runtime_error("Argument Error: vmath.degrees() expects only Number type as argument, but got " + args[0].getTypeName());
        }

        double val = args[0].asNumber();
        return Value(val * 180.0 / 3.141592653589793);
    }

    Value radians(std::vector<Value>& args) {
        if (args.size() != 1) {
            throw std::runtime_error("Argument Error: vmath.radians() expects 1 argument, but got " + std::to_string(args.size()) + ".");
        }

        if (args[0].getType() != Value::NUMBER) {
            throw std::runtime_error("Argument Error: vmath.radians() expects only Number type as argument, but got " + args[0].getTypeName());
        }

        double val = args[0].asNumber();
        return Value(val * 3.141592653589793 / 180.0);
    }

    Value fmod(std::vector<Value>& args) {
        if (args.size() != 2) {
            throw std::runtime_error("Argument Error: vmath.fmod() expects 2 arguments, but got " + std::to_string(args.size()) + ".");
        }

        if (args[0].getType() != Value::NUMBER || args[1].getType() != Value::NUMBER) {
            throw std::runtime_error("Argument Error: vmath.fmod() expects only Number types as arguments.");
        }

        double a = args[0].asNumber();
        double b = args[1].asNumber();
        if (b == 0) {
            throw std::runtime_error("Runtime Error: Division by zero in fmod.");
        }
        return Value(std::fmod(a, b));
    }

    Value hypot(std::vector<Value>& args) {
        if (args.size() != 2) {
            throw std::runtime_error("Argument Error: vmath.hypot() expects 2 arguments, but got " + std::to_string(args.size()) + ".");
        }

        if (args[0].getType() != Value::NUMBER || args[1].getType() != Value::NUMBER) {
            throw std::runtime_error("Argument Error: vmath.hypot() expects only Number types as arguments.");
        }

        double a = args[0].asNumber();
        double b = args[1].asNumber();
        return Value(std::hypot(a, b));
    }

    Value sin(std::vector<Value>& args) {
        if (args.size() != 1) {
            throw std::runtime_error("Argument Error: vmath.sin() expects 1 argument, but got " + std::to_string(args.size()) + ".");
        } // My trigonometry is so ahh lmao

        if (args[0].getType() != Value::NUMBER) {
            throw std::runtime_error("Argument Error: vmath.sin() expects only Number type as argument, but got " + args[0].getTypeName());
        }

        double val = args[0].asNumber();
        return Value(std::sin(val));
    }

    Value cos(std::vector<Value>& args) {
        if (args.size() != 1) {
            throw std::runtime_error("Argument Error: vmath.cos() expects 1 argument, but got " + std::to_string(args.size()) + ".");
        }

        if (args[0].getType() != Value::NUMBER) {
            throw std::runtime_error("Argument Error: vmath.cos() expects only Number type as argument, but got " + args[0].getTypeName());
        }

        double val = args[0].asNumber();
        return Value(std::cos(val));
    }

    Value tan(std::vector<Value>& args) {
        if (args.size() != 1) {
            throw std::runtime_error("Argument Error: vmath.tan() expects 1 argument, but got " + std::to_string(args.size()) + ".");
        }

        if (args[0].getType() != Value::NUMBER) {
            throw std::runtime_error("Argument Error: vmath.tan() expects only Number type as argument, but got " + args[0].getTypeName());
        }

        double val = args[0].asNumber();
        return Value(std::tan(val));
    }

    Value asin(std::vector<Value>& args) {
        if (args.size() != 1) {
            throw std::runtime_error("Argument Error: vmath.asin() expects 1 argument, but got " + std::to_string(args.size()) + ".");
        }

        if (args[0].getType() != Value::NUMBER) {
            throw std::runtime_error("Argument Error: vmath.asin() expects only Number type as argument, but got " + args[0].getTypeName());
        }

        double val = args[0].asNumber();
        if (val < -1.0 || val > 1.0) {
            throw std::runtime_error("Runtime Error: asin argument must be between -1 and 1.");
        }
        return Value(std::asin(val));
    }

    Value acos(std::vector<Value>& args) {
        if (args.size() != 1) {
            throw std::runtime_error("Argument Error: vmath.acos() expects 1 argument, but got " + std::to_string(args.size()) + ".");
        }

        if (args[0].getType() != Value::NUMBER) {
            throw std::runtime_error("Argument Error: vmath.acos() expects only Number type as argument, but got " + args[0].getTypeName());
        }

        double val = args[0].asNumber();
        if (val < -1.0 || val > 1.0) { // I'm suprised that this even works
            throw std::runtime_error("Runtime Error: acos argument must be between -1 and 1.");
        }
        return Value(std::acos(val));
    }

    Value atan(std::vector<Value>& args) {
        if (args.size() != 1) {
            throw std::runtime_error("Argument Error: vmath.atan() expects 1 argument, but got " + std::to_string(args.size()) + ".");
        }

        if (args[0].getType() != Value::NUMBER) {
            throw std::runtime_error("Argument Error: vmath.atan() expects only Number type as argument, but got " + args[0].getTypeName());
        }

        double val = args[0].asNumber();
        return Value(std::atan(val));
    }

    Value atan2(std::vector<Value>& args) {
        if (args.size() != 2) {
            throw std::runtime_error("Argument Error: vmath.atan2() expects 2 arguments, but got " + std::to_string(args.size()) + ".");
        }

        if (args[0].getType() != Value::NUMBER || args[1].getType() != Value::NUMBER) {
            throw std::runtime_error("Argument Error: vmath.atan2() expects only Number types as arguments.");
        }

        double y = args[0].asNumber();
        double x = args[1].asNumber();
        return Value(std::atan2(y, x));
    }

    Value log(std::vector<Value>& args) {
        if (args.size() != 1) {
            throw std::runtime_error("Argument Error: vmath.log() expects 1 argument, but got " + std::to_string(args.size()) + ".");
        }

        if (args[0].getType() != Value::NUMBER) {
            throw std::runtime_error("Argument Error: vmath.log() expects only Number type as argument, but got " + args[0].getTypeName());
        }

        double val = args[0].asNumber();
        if (val <= 0) {
            throw std::runtime_error("Runtime Error: log argument must be positive.");
        }
        return Value(std::log(val)); // The GOAT
    }

    Value log10(std::vector<Value>& args) {
        if (args.size() != 1) {
            throw std::runtime_error("Argument Error: vmath.log10() expects 1 argument, but got " + std::to_string(args.size()) + ".");
        }

        if (args[0].getType() != Value::NUMBER) {
            throw std::runtime_error("Argument Error: vmath.log10() expects only Number type as argument, but got " + args[0].getTypeName());
        }

        double val = args[0].asNumber();
        if (val <= 0) {
            throw std::runtime_error("Runtime Error: log10 argument must be positive.");
        }
        return Value(std::log10(val));
    }

    Value exp(std::vector<Value>& args) {
        if (args.size() != 1) {
            throw std::runtime_error("Argument Error: vmath.exp() expects 1 argument, but got " + std::to_string(args.size()) + ".");
        }

        if (args[0].getType() != Value::NUMBER) {
            throw std::runtime_error("Argument Error: vmath.exp() expects only Number type as argument, but got " + args[0].getTypeName());
        }

        double val = args[0].asNumber();
        return Value(std::exp(val));
    }

    Value pow(std::vector<Value>& args) {
        if (args.size() != 2) {
            throw std::runtime_error("Argument Error: vmath.pow() expects 2 arguments, but got " + std::to_string(args.size()) + ".");
        }

        if (args[0].getType() != Value::NUMBER || args[1].getType() != Value::NUMBER) {
            throw std::runtime_error("Argument Error: vmath.pow() expects only Number types as arguments.");
        }

        double base = args[0].asNumber();
        double exponent = args[1].asNumber();
        return Value(std::pow(base, exponent));
    }

    Value floor(std::vector<Value>& args) {
        if (args.size() != 1) {
            throw std::runtime_error("Argument Error: vmath.floor() expects 1 argument, but got " + std::to_string(args.size()) + ".");
        }

        if (args[0].getType() != Value::NUMBER) {
            throw std::runtime_error("Argument Error: vmath.floor() expects only Number type as argument, but got " + args[0].getTypeName());
        }

        double val = args[0].asNumber();
        return Value(std::floor(val));
    }

    Value ceil(std::vector<Value>& args) {
        if (args.size() != 1) {
            throw std::runtime_error("Argument Error: vmath.ceil() expects 1 argument, but got " + std::to_string(args.size()) + ".");
        }

        if (args[0].getType() != Value::NUMBER) {
            throw std::runtime_error("Argument Error: vmath.ceil() expects only Number type as argument, but got " + args[0].getTypeName());
        }

        double val = args[0].asNumber();
        return Value(std::ceil(val));
    }

    Value round(std::vector<Value>& args) {
        if (args.size() != 1) {
            throw std::runtime_error("Argument Error: vmath.round() expects 1 argument, but got " + std::to_string(args.size()) + ".");
        }

        if (args[0].getType() != Value::NUMBER) {
            throw std::runtime_error("Argument Error: vmath.round() expects only Number type as argument, but got " + args[0].getTypeName());
        }

        double val = args[0].asNumber();
        return Value(std::round(val));
    }

    Value min(std::vector<Value>& args) {
        if (args.size() != 2) {
            throw std::runtime_error("Argument Error: vmath.min() expects 2 arguments, but got " + std::to_string(args.size()) + ".");
        }

        if (args[0].getType() != Value::NUMBER || args[1].getType() != Value::NUMBER) {
            throw std::runtime_error("Argument Error: vmath.min() expects only Number types as arguments.");
        }

        double a = args[0].asNumber();
        double b = args[1].asNumber();
        return Value(a < b ? a : b);
    }

    Value max(std::vector<Value>& args) {
        if (args.size() != 2) {
            throw std::runtime_error("Argument Error: vmath.max() expects 2 arguments, but got " + std::to_string(args.size()) + ".");
        }

        if (args[0].getType() != Value::NUMBER || args[1].getType() != Value::NUMBER) {
            throw std::runtime_error("Argument Error: vmath.max() expects only Number types as arguments.");
        }

        double a = args[0].asNumber();
        double b = args[1].asNumber();
        return Value(a > b ? a : b);
    } // Some boring stuff

    Value erf(std::vector<Value>& args) {
        if (args.size() != 1) {
            throw std::runtime_error("Argument Error: vmath.erf() expects 1 argument, but got " + std::to_string(args.size()) + ".");
        }

        if (args[0].getType() != Value::NUMBER) {
            throw std::runtime_error("Argument Error: vmath.erf() expects only Number type as argument, but got " + args[0].getTypeName());
        }

        double val = args[0].asNumber();
        return Value(std::erf(val));
    }

    Value erfc(std::vector<Value>& args) {
        if (args.size() != 1) {
            throw std::runtime_error("Argument Error: vmath.erfc() expects 1 argument, but got " + std::to_string(args.size()) + ".");
        }

        if (args[0].getType() != Value::NUMBER) {
            throw std::runtime_error("Argument Error: vmath.erfc() expects only Number type as argument, but got " + args[0].getTypeName());
        }

        double val = args[0].asNumber();
        return Value(std::erfc(val));
    }

    Value tgamma(std::vector<Value>& args) {
        if (args.size() != 1) {
            throw std::runtime_error("Argument Error: vmath.tgamma() expects 1 argument, but got " + std::to_string(args.size()) + ".");
        }

        if (args[0].getType() != Value::NUMBER) {
            throw std::runtime_error("Argument Error: vmath.tgamma() expects only Number type as argument, but got " + args[0].getTypeName());
        }

        double val = args[0].asNumber();
        return Value(std::tgamma(val));
    }

    Value lgamma(std::vector<Value>& args) {
        if (args.size() != 1) {
            throw std::runtime_error("Argument Error: vmath.lgamma() expects 1 argument, but got " + std::to_string(args.size()) + ".");
        }

        if (args[0].getType() != Value::NUMBER) {
            throw std::runtime_error("Argument Error: vmath.lgamma() expects only Number type as argument, but got " + args[0].getTypeName());
        }

        double val = args[0].asNumber();
        return Value(std::lgamma(val));
    }
}

void setupVMath(SymbolContainer& env, StringPool& pool) {
    std::string path = "global.vmath";
    
    if (env.find(path) == env.end()) {
        env[path] = SymbolTable();
    }

    auto& vmath = env[path];

    // VMath methods
    vmath[pool.intern("clamp")]    = Value(VMathNative::clamp);
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

    // VMath constants
    vmath[pool.intern("pi")]       = Value(3.141592653589793).setReadOnly();
    vmath[pool.intern("e")]        = Value(2.718281828459045).setReadOnly();
    vmath[pool.intern("tau")]      = Value(6.283185307179586).setReadOnly();
    vmath[pool.intern("phi")]      = Value(1.618033988749895).setReadOnly();
    vmath[pool.intern("sqrt2")]    = Value(1.414213562373095).setReadOnly();
    vmath[pool.intern("pi_half")]  = Value(1.5707963267948966).setReadOnly();
    vmath[pool.intern("pi_quarter")] = Value(0.7853981633974483).setReadOnly();
    vmath[pool.intern("sqrt3")]    = Value(1.732050807568877).setReadOnly();
    vmath[pool.intern("sqrt5")]    = Value(2.23606797749979).setReadOnly();
    vmath[pool.intern("ln2")]      = Value(0.6931471805599453).setReadOnly();
    vmath[pool.intern("ln10")]     = Value(2.302585092994046).setReadOnly();
    vmath[pool.intern("euler_gamma")] = Value(0.5772156649015329).setReadOnly();
    vmath[pool.intern("inf")]      = Value(INFINITY).setReadOnly();
    vmath[pool.intern("nan")]      = Value(NAN).setReadOnly();

    // VMath properties
    vmath[pool.intern("version")]  = Value("v0.0.1-alpha").setReadOnly();
}