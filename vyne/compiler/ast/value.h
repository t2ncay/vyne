#pragma once
#include <iostream>
#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <variant>
#include <sstream>
#include <functional>
#include <cstdint>
#include <charconv>

class ASTNode;
struct Value;
struct FunctionData;
struct ModuleData;

struct ModuleData { 
    uint32_t moduleId;
    std::string name; 
};

struct FunctionData {
    std::vector<uint32_t> params;
    std::vector<std::shared_ptr<ASTNode>> body; 

    std::function<Value(std::vector<Value>&)> nativeFn;
    bool isNative = false;
};

using ValueData = std::variant<
        std::monostate,
        double,
        std::shared_ptr<std::string>,
        std::shared_ptr<std::vector<Value>>,
        std::shared_ptr<FunctionData>,
        ModuleData
>;

struct Value {
    enum TypeIndex { 
        NONE = 0, 
        NUMBER = 1, 
        STRING = 2, 
        ARRAY = 3, 
        FUNCTION = 4, 
        MODULE = 5 
    };

    ValueData data;
    bool isReadOnly = false;

    // constructors
    Value() : data(std::monostate{}) {}
    Value(double n) : data(n) {}
    Value(std::string s) 
    : data(std::make_shared<std::string>(std::move(s))) {}
    Value(std::vector<Value> l) 
    : data(std::make_shared<std::vector<Value>>(std::move(l))) {}
    Value(std::shared_ptr<FunctionData> f) : data(std::move(f)) {}
    Value(std::vector<uint32_t> p, std::vector<std::shared_ptr<ASTNode>> b) {
        auto func = std::make_shared<FunctionData>();
        func->params = std::move(p);
        func->body = std::move(b);
        
        data = std::move(func); 
    }
    Value(uint32_t mId, std::string moduleName, bool isModule) 
        : data(ModuleData{mId, std::move(moduleName)}) {}
    Value(std::function<Value(std::vector<Value>&)> native) {
        auto func = std::make_shared<FunctionData>();
        func->nativeFn = std::move(native);
        func->isNative = true;
        data = std::move(func);
    }
    Value(const Value&) = default;

    // safe getters
    int getType() const;
    std::string getTypeName() const;

    double asNumber() const;

    const std::string& asString() const;

    std::vector<Value>& asList();

    const std::vector<Value>& asList() const;

    const std::shared_ptr<FunctionData>& asFunction() const;

    const std::string& asModule() const;

    // core value functions
    Value& setReadOnly();
    bool isTruthy()                 const;
    void print(std::ostream& os)    const;
    size_t getDeepBytes()           const;
    size_t getShallowBytes()        const;
    bool equals(const Value& other) const;
    std::string toString()          const;
    int toNumber()                  const;

    bool operator==(const Value& other) const;
    bool operator!=(const Value& other) const;
    bool operator<(const Value& other) const;
};

// TODO ADD POOL CLEARING FEATURE WHEN THE DISMISS IS TRIGGERED

class StringPool {
    std::vector<std::string> idToStr;
    std::unordered_map<std::string, uint32_t> strToId;

    StringPool() {
        idToStr.reserve(4096); 
        strToId.reserve(4096); 
        
        // OPTIONAL: Intern an empty string at ID 0 as a 'null' state
        // This is a common compiler trick for default values.
        // idToStr.emplace_back("");
        // strToId[""] = 0;
    }

public:
    static StringPool& instance() {
        static StringPool pool; 
        return pool;
    }
    
    static uint32_t intern(const std::string& s);

    const std::string& get(uint32_t id) const { 
        return idToStr.at(id); 
    }
};