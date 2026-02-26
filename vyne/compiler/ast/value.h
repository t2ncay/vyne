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

struct StructInstance {
    std::string typeName;
    std::unordered_map<std::string, Value> fields;
};

struct VyneObject {
    enum class ObjType { Array, Function, Module, Struct };
    ObjType objType;
    virtual ~VyneObject() = default;
    VyneObject(ObjType t) : objType(t) {}
};

struct VyneArray : public VyneObject {
    std::vector<Value> elements;
    VyneArray(std::vector<Value> e) : VyneObject(ObjType::Array), elements(std::move(e)) {}
};

struct FunctionData : public VyneObject {
    int arity = 0;
    std::vector<uint32_t> params;
    std::vector<std::shared_ptr<ASTNode>> body; 
    std::function<Value(std::vector<Value>&)> nativeFn;
    bool isNative = false;
    std::string expectedReturnType = "null";
    FunctionData() : VyneObject(ObjType::Function) {}
};

// TODO PERFORMANCE ALIGNMENTS

struct ModuleData : public VyneObject { 
    uint32_t moduleId;
    std::string name;
    ModuleData(uint32_t id, std::string nId) : VyneObject(ObjType::Module), moduleId(id), name(std::move(nId)) {}
};

struct VyneStruct : public VyneObject {
    std::string typeName;
    std::unordered_map<uint32_t, Value> fields;
    
    VyneStruct(std::string name) 
        : VyneObject(ObjType::Struct), typeName(std::move(name)) {}
};

using ValueData = std::variant<
    std::monostate,
    double,
    int64_t,
    uint32_t,
    std::shared_ptr<VyneObject>
>;

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

struct Value {
    enum TypeIndex { 
        NONE = 0, 
        FLOAT64 = 1, 
        INT64 = 2,
        STRING = 3, 
        ARRAY = 4, 
        FUNCTION = 5, 
        MODULE = 6,
        STRUCT = 7
    };

    ValueData data;
    bool isReadOnly = false;

    // constructors
    Value() : data(std::monostate{}) {}
    Value(double n) : data(n) {}
    Value(int64_t n) : data(n) {}
    Value(int n) : data(static_cast<int64_t>(n)) {}
    Value(unsigned int n) : data(static_cast<int64_t>(n)) {}
    Value(size_t n) : data(static_cast<int64_t>(n)) {}
    Value(std::string s) {
        data = StringPool::intern(s);
    }
    Value(std::vector<Value> l) {
        data = std::make_shared<VyneArray>(std::move(l));
    }
    Value(std::shared_ptr<FunctionData> f) : data(std::move(f)) {}
    Value(std::vector<uint32_t> p, std::vector<std::shared_ptr<ASTNode>> b, std::string rt) {
        auto func = std::make_shared<FunctionData>();
        func->params = std::move(p);
        func->body = std::move(b);
        func->expectedReturnType = std::move(rt); 
        
        this->data = std::move(func);
    }
    Value(uint32_t mId, std::string moduleName, bool isModule) {
        data = std::make_shared<ModuleData>(mId, std::move(moduleName));
    }
    Value(std::function<Value(std::vector<Value>&)> native) {
        auto func = std::make_shared<FunctionData>();
        func->nativeFn = std::move(native);
        func->isNative = true;
        data = std::move(func);
    }
    Value(std::shared_ptr<VyneStruct> s) : data(std::move(s)) {}
    Value(const Value&) = default;

    // safe getters
    int getType() const;
    std::string getTypeName() const;

    double  asFloat() const;
    int64_t asInt()   const;

    const std::string& asString() const;

    std::vector<Value>& asList();

    const std::vector<Value>& asList() const;

    std::shared_ptr<FunctionData> asFunction() const;

    const std::string& asModule() const;

    // core value functions
    Value& setReadOnly();
    long getRefCount() const;
    bool isTruthy()                 const;
    void print(std::ostream& os)    const;
    size_t getDeepBytes()           const;
    size_t getShallowBytes()        const;
    bool equals(const Value& other) const;
    std::string toString()          const;
    int toNumber()                  const;

    bool operator==(const Value& other) const;
    bool operator!=(const Value& other) const;
    bool operator<(const Value& other)  const;
};

// TODO ADD POOL CLEARING FEATURE WHEN THE DISMISS IS TRIGGERED