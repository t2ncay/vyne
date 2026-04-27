#include "value.h"

FunctionData::FunctionData() : VyneObject(ObjType::Function), bytecode(nullptr) {}
FunctionData::~FunctionData() { if (bytecode) delete bytecode; }

Value::Value(std::vector<Value> l) : type(VType::Array) {
    new (&data.obj) std::shared_ptr<VyneObject>(std::make_shared<VyneArray>(std::move(l)));
}

Value::Value(std::shared_ptr<FunctionData> f) : type(VType::Function) {
    new (&data.obj) std::shared_ptr<VyneObject>(std::move(f));
}

Value::Value(std::vector<Parameter> p, std::vector<std::shared_ptr<ASTNode>> b, std::string rt) : type(VType::Function) {
    auto func = std::make_shared<FunctionData>();
    func->arity = static_cast<int>(p.size());
    func->params = std::move(p);
    func->body = std::move(b);
    func->expectedReturnType = std::move(rt);
    new (&data.obj) std::shared_ptr<VyneObject>(std::move(func));
}

Value::Value(uint32_t mId, std::string moduleName, bool isModule) : type(VType::Module) {
    new (&data.obj) std::shared_ptr<VyneObject>(std::make_shared<ModuleData>(mId, std::move(moduleName)));
}

Value::Value(std::function<Value(std::vector<Value>&)> native) : type(VType::Function) {
    auto func = std::make_shared<FunctionData>();
    func->nativeFn = std::move(native);
    func->isNative = true;
    new (&data.obj) std::shared_ptr<VyneObject>(std::move(func));
}

Value::Value(std::shared_ptr<VyneStruct> s) : type(VType::Struct) {
    new (&data.obj) std::shared_ptr<VyneObject>(std::move(s));
}

Value::~Value() { if (isObject()) data.obj.~shared_ptr(); }

Value::Value(const Value& other) : type(other.type), isReadOnly(other.isReadOnly), stringId(other.stringId) {
    if (isObject()) {
        new (&data.obj) std::shared_ptr<VyneObject>(other.data.obj);
    } else {
        switch(type) {
            case VType::Float64:   data.f64 = other.data.f64; break;
            case VType::Int64:     data.i64 = other.data.i64; break;
            case VType::Reference: data.ref = other.data.ref; break;
            default:               data.i64 = other.data.i64; break;
        }
    }
}

Value::Value(Value&& other) noexcept : type(other.type), isReadOnly(other.isReadOnly), stringId(other.stringId) {
    if (isObject()) {
        new (&data.obj) std::shared_ptr<VyneObject>(std::move(other.data.obj));
    } else {
        data.i64 = other.data.i64;
    }
    other.type = VType::Null;
    other.stringId = 0;
}

Value& Value::operator=(const Value& other) {
    if (this == &other) return *this;

    this->~Value();

    type = other.type;
    isReadOnly = other.isReadOnly;
    stringId = other.stringId;

    if (isObject()) {
        new (&data.obj) std::shared_ptr<VyneObject>(other.data.obj);
    } else {
        data.i64 = other.data.i64;
    }

    return *this;
}

Value& Value::operator=(Value&& other) noexcept {
    if (this == &other) return *this;

    this->~Value();

    type = other.type;
    isReadOnly = other.isReadOnly;
    stringId = other.stringId;

    if (isObject()) {
        new (&data.obj) std::shared_ptr<VyneObject>(std::move(other.data.obj));
    } else {
        data.i64 = other.data.i64;
    }

    other.type = VType::Null;
    other.stringId = 0;

    return *this;
}

int Value::getType() const {
    switch(type) {
        case VType::Null:    return NONE;
        case VType::Float64: return FLOAT64;
        case VType::Int64:   return INT64;
        case VType::String:  return STRING;
        case VType::Reference: return REFERENCE;
        default: {
            if (!data.obj) return NONE;
            switch (data.obj->objType) {
                case VyneObject::ObjType::Array:    return ARRAY;
                case VyneObject::ObjType::Function: return FUNCTION;
                case VyneObject::ObjType::Module:   return MODULE;
                case VyneObject::ObjType::Struct:   return STRUCT;
                default: return NONE;
            }
        }
    }
}

std::string Value::getTypeName() const {
    int t = getType();
    switch(t) {
        case FLOAT64: return "Float64";
        case INT64:   return "Int64";
        case STRING:  return "String";
        case ARRAY:   return "Array";
        case FUNCTION: return "Function";
        case MODULE:  return "Module";
        case STRUCT:  return std::static_pointer_cast<VyneStruct>(data.obj)->typeName;
        default:      return "null";
    }
}

double Value::asFloat() const {
    if (type == VType::Float64) return data.f64;
    if (type == VType::Int64) return static_cast<double>(data.i64);
    return 0.0;
}

int64_t Value::asInt() const {
    if (type == VType::Int64) return data.i64;
    if (type == VType::Float64) return static_cast<int64_t>(data.f64);
    return 0;
}

const std::string& Value::asString() const {
    if (type != VType::String) throw std::runtime_error("Type Error: Expected String");
    return StringPool::get(this->stringId);
}

std::vector<Value>& Value::asList() {
    if (type == VType::Reference) return data.ref->asList();
    return static_cast<VyneArray*>(data.obj.get())->elements;
}

const std::vector<Value>& Value::asList() const {
    if (type == VType::Reference) return data.ref->asList();
    return static_cast<VyneArray*>(data.obj.get())->elements;
}

std::shared_ptr<FunctionData> Value::asFunction() const {
    return std::static_pointer_cast<FunctionData>(data.obj);
}

const std::string& Value::asModule() const {
    return static_cast<ModuleData*>(data.obj.get())->name;
}

std::shared_ptr<VyneStruct> Value::asStruct() const {
    return std::static_pointer_cast<VyneStruct>(data.obj);
}

Value* Value::getPointer() const { return isReference() ? data.ref : nullptr; }

long Value::getRefCount() const { return isObject() ? data.obj.use_count() : 0; }

void Value::print(std::ostream& os) const {
    switch (type) {
        case VType::Null: os << "null"; break;
        case VType::Float64: {
            std::ostringstream oss; oss << data.f64;
            std::string s = oss.str();
            if (s.find('.') == std::string::npos && s.find('e') == std::string::npos) s += ".0";
            os << s; break;
        }
        case VType::Int64: os << data.i64; break;
        case VType::String: os << asString(); break;
        default: {
            if (!isObject()) { os << "null"; break; }
            switch (data.obj->objType) {
                case VyneObject::ObjType::Array: {
                    auto& list = static_cast<VyneArray*>(data.obj.get())->elements;
                    os << "[";
                    for (size_t i = 0; i < list.size(); ++i) {
                        list[i].print(os); if (i < list.size() - 1) os << ", ";
                    }
                    os << "]"; break;
                }
                case VyneObject::ObjType::Function:
                    os << (static_cast<FunctionData*>(data.obj.get())->isNative ? "<native function>" : "<function>"); break;
                case VyneObject::ObjType::Module:
                    os << "<module '" << static_cast<ModuleData*>(data.obj.get())->name << "'>"; break;
                case VyneObject::ObjType::Struct: {
                    auto s = std::static_pointer_cast<VyneStruct>(data.obj);
                    os << s->typeName << " { ";
                    for (auto it = s->fields.begin(); it != s->fields.end();) {
                        os << StringPool::get(it->first) << ": "; it->second.print(os);
                        if (++it != s->fields.end()) os << ", ";
                    }
                    os << " }"; break;
                }
            }
        }
    }
}

size_t Value::getDeepBytes() const {
    if (!isObject()) return 8;
    size_t total = 16;
    if (data.obj->objType == VyneObject::ObjType::Array) {
        auto arr = static_cast<VyneArray*>(data.obj.get());
        total += sizeof(VyneArray) + (arr->elements.capacity() * sizeof(Value));
        for (const auto& item : arr->elements) total += item.getDeepBytes();
    }
    return total;
}

size_t Value::getShallowBytes() const { return isObject() ? 16 + 8 : 8; }

bool Value::isTruthy() const {
    switch(type) {
        case VType::Float64: return data.f64 != 0;
        case VType::Int64:   return data.i64 != 0;
        case VType::String:  return stringId != 0;
        case VType::Null:    return false;
        default:             return true;
    }
}

bool Value::operator==(const Value& other) const {
    if (type != other.type) return false;
    switch (type) {
        case VType::Null:    return true;
        case VType::Float64: return data.f64 == other.data.f64;
        case VType::Int64:   return data.i64 == other.data.i64;
        case VType::String:  return stringId == other.stringId;
        default:             return data.obj == other.data.obj;
    }
}

bool Value::operator!=(const Value& other) const { return !(*this == other); }

bool Value::operator<(const Value& other) const {
    if (type != other.type) return type < other.type;
    switch (type) {
        case VType::Float64: return data.f64 < other.data.f64;
        case VType::Int64:   return data.i64 < other.data.i64;
        case VType::String:  return stringId < other.stringId;
        default:             return data.obj < other.data.obj;
    }
}

uint32_t StringPool::intern(std::string_view s) {
    auto& pool = instance();
    auto it = pool.strToId.find(s);
    if (it != pool.strToId.end()) return it->second;
    uint32_t id = static_cast<uint32_t>(pool.idToStr.size());
    pool.idToStr.emplace_back(s);
    pool.strToId[pool.idToStr.back()] = id;
    return id;
}

const std::string& StringPool::get(uint32_t id) { return instance().idToStr.at(id); }

std::string Value::toString() const {
    if (type == VType::String) return asString();
    std::stringstream ss; print(ss); return ss.str();
}

int Value::toNumber() const {
    if (type == VType::Int64) return (int)data.i64;
    if (type == VType::Float64) return (int)data.f64;
    return 0;
}