#include "vmem.h"

/**
 * vmem Native Method Implementations
 */

namespace VMemNative {
    static SymbolContainer* g_env = nullptr;

    void setEnv(SymbolContainer& env) {
        g_env = &env;
    }

    Value peek(std::vector<Value>& args) {
        if (args.empty()) throw std::runtime_error("vmem.peek requires an address");

        uintptr_t addr = static_cast<uintptr_t>(std::get<int64_t>(args[0].data));
        
        Value* target = reinterpret_cast<Value*>(addr);

        return *target; 
    }

    Value poke(std::vector<Value>& args) {
        if (args.size() < 2) throw std::runtime_error("vmem.poke requires address and value");

        uintptr_t addr = static_cast<uintptr_t>(std::get<int64_t>(args[0].data));
        Value* target = reinterpret_cast<Value*>(addr);

        *target = args[1]; 

        return Value(true);
    }

    Value usage(std::vector<Value>& args){
        size_t totalBytes = 0;

        if(args.size() > 1) throw std::runtime_error("Argument Error : vmem.usage() takes 1 or non-argument, but got " + std::to_string(args.size()));

        if (!g_env) return Value(0.0);

        if(args.empty()){
            for (auto const& [groupId, table] : *g_env) {
                std::string groupName = StringPool::instance().get(groupId);
                totalBytes += groupName.capacity();
                
                totalBytes += sizeof(table);
                
                for (auto const& [id, val] : table) {
                    std::string symName = StringPool::instance().get(id);
                    totalBytes += symName.capacity();
                    
                    // Value storage
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
    const std::string& path = "vmem";
    
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