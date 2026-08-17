#pragma once
#include "ast.h"
#include "../../runtime/diagnostics.h"
#include <string>
#include <vector>
#include <memory>

// ============================================================================
// Symbol Lookup Helpers
// ============================================================================

inline uint32_t getGlobalId() {
    static uint32_t globalId = StringPool::instance().intern("global");
    return globalId;
}

inline Value* lookupSymbol(SymbolContainer& env, uint32_t groupId, uint32_t id) {
    auto gIt = env.find(groupId);
    if (gIt != env.end()) {
        auto vIt = gIt->second.find(id);
        if (vIt != gIt->second.end()) return &vIt->second;
    }
    return nullptr;
}

inline Value* lookupSymbolWithFallback(SymbolContainer& env, 
                                       uint32_t targetGroupId, 
                                       uint32_t id,
                                       uint32_t fallbackGroupId) {
    Value* valPtr = lookupSymbol(env, targetGroupId, id);
    if (!valPtr && targetGroupId != fallbackGroupId) {
        valPtr = lookupSymbol(env, fallbackGroupId, id);
    }
    return valPtr;
}

inline uint32_t getGroupId(const std::string& groupName) {
    return StringPool::instance().intern(groupName);
}
// ============================================================================
// Type Checking Helpers
// ============================================================================

inline void checkTypeMatch(int expected, int got, const std::string& context, int line) {
    if (expected != got) {
        throw std::runtime_error("Type Error: " + context + " [ line " + std::to_string(line) + " ]");
    }
}

inline Value convertIfNeeded(const Value& val, int targetType, int line) {
    if (targetType == Value::FLOAT64 && val.getType() == Value::INT64) {
        return Value(val.asFloat());
    }
    return val;
}

inline void checkReadOnly(const Value& val, const std::string& name, int line) {
    if (val.isReadOnly) {
        throw std::runtime_error("Runtime Error: Cannot modify read-only '" + name + 
                                "' [ line " + std::to_string(line) + " ]");
    }
}

// ============================================================================
// Array/Index Helpers
// ============================================================================

inline size_t validateIndex(const Value& idxVal, size_t size, int line) {
    int64_t rawIdx = idxVal.asInt();
    if (rawIdx < 0) {
        throw std::runtime_error("Index Error: Negative index (" + std::to_string(rawIdx) + 
                                ") [ line " + std::to_string(line) + " ]");
    }
    size_t idx = static_cast<size_t>(rawIdx);
    if (idx >= size) {
        throw std::runtime_error("Index Error: Out of bounds (" + std::to_string(idx) + 
                                ") [ line " + std::to_string(line) + " ]");
    }
    return idx;
}

inline std::vector<Value>& getArrayRef(Value& val, const std::string& name, int line) {
    if (val.getType() != Value::ARRAY) {
        throw std::runtime_error("Type Error: '" + name + "' is not an array [ line " + 
                                std::to_string(line) + " ]");
    }
    return val.asList();
}

// ============================================================================
// Function/Method Call Helpers
// ============================================================================

struct CallContext {
    SymbolContainer& env;
    const std::string& currentGroup;
    int lineNumber;
    std::string callableName;
};

inline void checkArgumentCount(size_t expected, size_t got, const CallContext& ctx) {
    if (expected != got) {
        throw std::runtime_error("Argument Error: " + ctx.callableName + " expects " + 
                                std::to_string(expected) + " arguments, but got " + 
                                std::to_string(got) + " [ line " + std::to_string(ctx.lineNumber) + " ]");
    }
}

inline void checkCallableType(const Value& val, const CallContext& ctx) {
    if (val.getType() != Value::FUNCTION) {
        throw std::runtime_error("Type Error: '" + ctx.callableName + 
                                "' is not callable [ line " + std::to_string(ctx.lineNumber) + " ]");
    }
}

inline std::string createLocalScope(const std::string& prefix, const std::string& name) {
    static uint64_t callCount = 0;
    return prefix + "_" + name + "_" + std::to_string(callCount++);
}

inline Value* findVariableInScope(SymbolContainer& env, 
                                 uint32_t scopeId, // string yox, ID
                                 uint32_t varId) {
    auto scopeIt = env.find(scopeId);
    if (scopeIt != env.end()) {
        auto varIt = scopeIt->second.find(varId);
        if (varIt != scopeIt->second.end()) return &varIt->second;
    }
    return nullptr;
}

// ============================================================================
// Scope Management Helpers
// ============================================================================

class ScopedEnvironment {
    SymbolContainer& env;
    uint32_t scopeId;    // string yox, ID saxlayırıq
    uint32_t parentId;   
    std::string scopeNameStr; // debug üçün lazım ola bilər

public:
    ScopedEnvironment(SymbolContainer& e, std::string name, uint32_t parent = 0) 
        : env(e), scopeNameStr(name), parentId(parent) {
        scopeId = StringPool::instance().intern(scopeNameStr);
        env[scopeId] = SymbolTable();
    }
    
    ~ScopedEnvironment() {
        env.erase(scopeId);
    }
    
    void bind(uint32_t id, Value val) {
        env[scopeId][id] = std::move(val);
    }

    uint32_t getScopeId() const { return scopeId; }
};

// ============================================================================
// Method Call Helpers
// ============================================================================

inline Value executeFunction(std::shared_ptr<FunctionData> funcData,
                            const std::vector<Value>& args,
                            SymbolContainer& env,
                            uint32_t scopeId, // Dəyişdirildi
                            int line) {
    if (funcData->isNative) {
        std::vector<Value> mutableArgs = args;
        return funcData->nativeFn(mutableArgs);
    }
    
    Value result;
    try {
        for (const auto& stmt : funcData->body) {
            if (stmt) result = stmt->evaluate(env, scopeId); // ID ötürülür
        }
    } catch (const ReturnException& e) {
        result = e.value;
    }
    
    if (funcData->expectedReturnType != "null" && funcData->expectedReturnType != "Unknown") {
        if (result.getTypeName() != funcData->expectedReturnType) {
            throw std::runtime_error("Type Mismatch: Expected " + funcData->expectedReturnType + 
                                    ", but returned " + result.getTypeName() + 
                                    " [ line " + std::to_string(line) + " ]");
        }
    }
    
    return result;
}

// ============================================================================
// Deep Copy Helpers
// ============================================================================

inline Value deepCopyValue(const Value& val) {
    switch (val.getType()) {
        case Value::ARRAY: {
            auto& originalVec = val.asList();
            std::vector<Value> copiedVec;
            copiedVec.reserve(originalVec.size());
            for (const auto& element : originalVec) {
                copiedVec.push_back(deepCopyValue(element));
            }
            return Value(std::move(copiedVec));
        }
        case Value::STRUCT: {
            auto originalStruct = val.asStruct();
            auto newStruct = std::make_shared<VyneStruct>(originalStruct->typeName);
            
            for (const auto& [id, fieldVal] : originalStruct->fields) {
                newStruct->fields[id] = deepCopyValue(fieldVal);
            }
            
            newStruct->methods = originalStruct->methods;
            newStruct->methodNodes = originalStruct->methodNodes;
            
            return Value(newStruct);
        }
        case Value::MAP: { // ADDED
            auto& originalMap = val.asMap();
            std::unordered_map<uint32_t, Value> copiedMap;
            copiedMap.reserve(originalMap.size());
            for (const auto& [id, fieldVal] : originalMap) {
                copiedMap[id] = deepCopyValue(fieldVal);
            }
            return Value(std::move(copiedMap));
        }
        default:
            return val;
    }
}

// ============================================================================
// Interface Method Validators
// ============================================================================

inline void validateMethodCall(const FunctionNode* funcNode,
                               const std::vector<std::unique_ptr<ASTNode>>& arguments,
                               const std::string& methodName,
                               SymbolContainer& env,
                               uint32_t currentGroupId, // Dəyişdirildi: string -> uint32_t
                               int lineNumber) {
    if (arguments.size() != funcNode->getParameters().size()) {
        throw std::runtime_error(
            "Type Error: Method '" + methodName + 
            "' expects " + std::to_string(funcNode->getParameters().size()) + 
            " argument(s), but got " + std::to_string(arguments.size()) + 
            " [ line " + std::to_string(lineNumber) + " ]"
        );
    }
    
    for (size_t i = 0; i < arguments.size(); ++i) {
        Value argVal = arguments[i]->evaluate(env, currentGroupId);
        VType expectedType = funcNode->getParameters()[i].type;
        
        if (expectedType != VType::Unknown) {
            int argType = argVal.getType();
            bool typeMatch = false;
            
            // Implicit conversion logic (int -> float)
            if (expectedType == VType::Float64 && argType == Value::INT64) typeMatch = true;
            else if (static_cast<int>(expectedType) == argType) typeMatch = true;
            
            if (!typeMatch) {
                std::string expectedStr = VTypeToString(expectedType);
                throw std::runtime_error(
                    "Type Error: Argument " + std::to_string(i+1) + 
                    " of method '" + methodName + "' expects " + expectedStr + 
                    ", but got " + argVal.getTypeName() + 
                    " [ line " + std::to_string(lineNumber) + " ]"
                );
            }
        }
    }
}

inline void validateReturnType(const FunctionNode* funcNode,
                              const Value& result,
                              const std::string& methodName,
                              int lineNumber) {
    if (funcNode->getReturnType() != VType::Unknown) {
        std::string expectedReturn = VTypeToString(funcNode->getReturnType());
        if (result.getTypeName() != expectedReturn) {
            if (!(funcNode->getReturnType() == VType::Float64 && result.getType() == Value::INT64)) {
                throw std::runtime_error(
                    "Type Error: Method '" + methodName + 
                    "' should return " + expectedReturn + 
                    ", but returned " + result.getTypeName() + 
                    " [ line " + std::to_string(lineNumber) + " ]"
                );
            }
        }
    }
}