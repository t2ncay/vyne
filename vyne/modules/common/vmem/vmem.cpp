#include "vmem.h"

/**
 * vmem Native Method Implementations
 */

namespace VMemNative {
    static SymbolContainer* g_env = nullptr;

    void setEnv(SymbolContainer& env) {
        g_env = &env;
    }

    Value peek(std::vector<Value>& args){
        if (args.empty()) return Value(0.0);

        uint32_t stringId = std::get<uint32_t>(args[0].data);
        const std::string& addrStr = StringPool::instance().get(stringId);

        uintptr_t addr = std::stoull(addrStr, nullptr, 16);
        double* ptr = reinterpret_cast<double*>(addr);
        return Value((double)*ptr);
    }

    Value poke(std::vector<Value>& args){
        if (args.empty()) return Value(0.0);
        double newVal = std::get<double>(args[1].data);

        uint32_t stringId = std::get<uint32_t>(args[0].data);
        const std::string& addrStr = StringPool::instance().get(stringId);

        uintptr_t addr = std::stoull(addrStr, nullptr, 16);
        double* ptr = reinterpret_cast<double*>(addr);

        *ptr = newVal;;

        return Value(true);
    }

    Value usage(std::vector<Value>& args){
        size_t totalBytes = 0;

        if(args.size() > 1) throw std::runtime_error("Argument Error : vmem.usage() takes 1 or non-argument, but got " + std::to_string(args.size()));

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
    const std::string& path = "global.vmem";
    
    if (env.find(path) == env.end()) {
        env[path] = SymbolTable();
    }

    auto& vmem = env[path];

    // vmem methods
    vmem[pool.intern("usage")]   = Value(VMemNative::usage);
    vmem[pool.intern("peek")]    = Value(VMemNative::peek);
    vmem[pool.intern("poke")]    = Value(VMemNative::poke);

    // vmem properties
}
