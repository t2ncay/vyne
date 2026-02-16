#include "vmem.h"

/**
 * vmem Native Method Implementations
 */

namespace VMemNative {
    static SymbolContainer* g_env = nullptr;

    void setEnv(SymbolContainer& env) {
        g_env = &env;
    }

    Value address(std::vector<Value>& args) {
        if (args.empty()) return Value("0x0");

        const Value& val = args[0];
        const void* actualPtr = nullptr;

        switch (val.getType()) {
            case Value::NUMBER:
                actualPtr = &val.data; 
                break;
            case Value::STRING:
                actualPtr = std::get<std::shared_ptr<std::string>>(val.data).get();
                break;
            case Value::ARRAY:
                actualPtr = std::get<std::shared_ptr<std::vector<Value>>>(val.data).get();
                break;
            case Value::FUNCTION:
                actualPtr = val.asFunction().get();
                break;
            default:
                actualPtr = &val.data;
        }

        std::stringstream ss;
        ss << "0x" << std::hex << reinterpret_cast<uintptr_t>(actualPtr);
        return Value(ss.str());
    }

    Value usage(std::vector<Value>& args){
        size_t totalBytes = 0;

        if(args.size() > 1) throw std::runtime_error("Argument Error : vmem.usage() takes 1 or 0 arguments, but got " + std::to_string(args.size()));

        if (!g_env) return Value(0.0);

        if(args.empty()){
            for (auto const& [groupName, table] : *g_env) {
                totalBytes += groupName.capacity();

                for (auto const& [id, val] : table) {
                    totalBytes += sizeof(uint32_t); 
                    totalBytes += sizeof(Value);
                    totalBytes += val.getDeepBytes();
                }
            }

            return Value(static_cast<double>(totalBytes));
        } else {
            return Value(args[0].getDeepBytes());
        }
    }
}

void setupVMem(SymbolContainer& env, StringPool& pool) {
    VMemNative::setEnv(env);
    std::string path = "global.vmem";
    
    if (env.find(path) == env.end()) {
        env[path] = SymbolTable();
    }

    auto& vmem = env[path];

    // vmem methods
    vmem[pool.intern("address")] = Value(VMemNative::address);
    vmem[pool.intern("usage")]   = Value(VMemNative::usage);

    // vmem properties
}