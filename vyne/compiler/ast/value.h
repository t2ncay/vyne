#pragma once
#include <iostream>
#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <sstream>
#include <functional>
#include <cstdint>
#include <charconv>
#include <deque>
#include <format>

#include "../types.h"

class FunctionNode;
class ASTNode;
struct Value;
struct Chunk;

struct Parameter {
    uint32_t id;
    std::string name;
    VType type;
    bool isReference;

    Parameter(uint32_t i, std::string n, VType t, bool ir) 
        : id(i), name(std::move(n)), type(t), isReference(ir) {}
};

struct VyneObject {
    enum class ObjType { Array, Function, Module, Struct, Map };
    ObjType objType;
    virtual ~VyneObject() = default;
    VyneObject(ObjType t) : objType(t) {}
};

struct VyneArray : public VyneObject {
    std::vector<Value> elements;
    VyneArray(std::vector<Value> e) : VyneObject(ObjType::Array), elements(std::move(e)) {}
};

struct VyneMap : public VyneObject {
    std::unordered_map<uint32_t, Value> elements;
    VyneMap() : VyneObject(ObjType::Map) {}
    VyneMap(std::unordered_map<uint32_t, Value> e) : VyneObject(ObjType::Map), elements(std::move(e)) {}
};

struct FunctionData : public VyneObject {
    int arity = 0;
    std::vector<Parameter> params;
    std::vector<std::shared_ptr<ASTNode>> body; 
    Chunk* bytecode = nullptr; 
    std::function<Value(std::vector<Value>&)> nativeFn;
    bool isNative = false;
    std::string expectedReturnType = "null";

    FunctionData();
    ~FunctionData();
};

struct ModuleData : public VyneObject { 
    uint32_t moduleId;
    std::string name;
    ModuleData(uint32_t id, std::string nId) : VyneObject(ObjType::Module), moduleId(id), name(std::move(nId)) {}
};

struct VyneStruct : public VyneObject {
    std::string typeName;
    std::unordered_map<uint32_t, Value> fields;
    std::unordered_map<uint32_t, Value> methods;
    std::unordered_map<uint32_t, FunctionNode*> methodNodes;
    
    VyneStruct(std::string name) 
        : VyneObject(ObjType::Struct), typeName(std::move(name)) {}
};

struct StringHash {
    using is_transparent = void;
    size_t operator()(std::string_view sv) const {
        return std::hash<std::string_view>{}(sv);
    }
};

class StringPool {
private:
    std::deque<std::string> idToStr;
    std::unordered_map<std::string_view, uint32_t, StringHash, std::equal_to<>> strToId;
    StringPool() { idToStr.emplace_back(""); strToId[""] = 0; }

public:
    static StringPool& instance() { static StringPool pool; return pool; }
    static uint32_t intern(std::string_view sv);
    static const std::string& get(uint32_t id);
};

struct Value {
    enum TypeIndex { 
        NONE = 0, FLOAT64 = 1, INT64 = 2, STRING = 3, 
        ARRAY = 4, FUNCTION = 5, MODULE = 6, STRUCT = 7, REFERENCE = 8, 
        MAP = 9
    };

    union Data {
        double f64;
        int64_t i64;
        std::shared_ptr<VyneObject> obj;
        Value* ref;

        Data() : i64(0) {}
        ~Data() {}
    } data;
    uint32_t stringId = 0;

    VType type;
    bool isReadOnly = false;

    Value() : type(VType::Null) { data.i64 = 0; }
    Value(double n) : type(VType::Float64) { data.f64 = n; }
    Value(int64_t n) : type(VType::Int64) { data.i64 = n; }
    Value(int n) : type(VType::Int64) { data.i64 = static_cast<int64_t>(n); }
    Value(unsigned int n) : type(VType::Int64) { data.i64 = static_cast<int64_t>(n); }
    Value(size_t n) : type(VType::Int64) { data.i64 = static_cast<int64_t>(n); }
    
    Value(std::string_view s) : type(VType::String), stringId(StringPool::intern(s)) {
        data.i64 = 0; 
    }

    Value(std::vector<Value> l);
    Value(std::shared_ptr<FunctionData> f);
    Value(std::vector<Parameter> p, std::vector<std::shared_ptr<ASTNode>> b, std::string rt);
    Value(uint32_t mId, std::string moduleName, bool isModule);
    Value(std::function<Value(std::vector<Value>&)> native);
    Value(std::shared_ptr<VyneStruct> s);
    Value(Value* refTarget) : type(VType::Reference) { data.ref = refTarget; }
    Value(std::unordered_map<uint32_t, Value> m);

    Value(const Value& other);
    Value(Value&& other) noexcept;
    Value& operator=(const Value& other);
    Value& operator=(Value&& other) noexcept;
    ~Value();

    inline bool isObject() const { return type >= VType::Array && type <= VType::Struct; }

    int getType() const;
    std::string getTypeName() const;
    double  asFloat() const;
    int64_t asInt()   const;
    const std::string& asString() const;
    std::vector<Value>& asList();
    const std::vector<Value>& asList() const;
    std::shared_ptr<FunctionData> asFunction() const;
    const std::string& asModule() const;
    std::shared_ptr<VyneStruct> asStruct() const;
    std::unordered_map<uint32_t, Value>& asMap();
    const std::unordered_map<uint32_t, Value>& asMap() const;
    bool isReference() const { return type == VType::Reference; }
    Value* getPointer() const;

    Value& setReadOnly() { isReadOnly = true; return *this; }
    long getRefCount() const;
    bool isTruthy() const;
    void print() const;
    size_t getDeepBytes() const;
    size_t getShallowBytes() const;
    bool equals(const Value& other) const;
    std::string toString() const;
    int toNumber() const;

    bool operator==(const Value& other) const;
    bool operator!=(const Value& other) const;
    bool operator<(const Value& other)  const;
};