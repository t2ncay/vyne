#include "value.h"

int Value::getType() const {
    if (std::holds_alternative<std::monostate>(data)) return NONE;
    if (std::holds_alternative<double>(data))         return FLOAT64;
    if (std::holds_alternative<int64_t>(data))        return INT64;
    if (std::holds_alternative<uint32_t>(data))       return STRING; // Interned ID

    if (std::holds_alternative<std::shared_ptr<VyneObject>>(data)) {
        auto ptr = std::get<std::shared_ptr<VyneObject>>(data);
        if (!ptr) return NONE;
        
        switch (ptr->objType) {
            case VyneObject::ObjType::Array:    return ARRAY;
            case VyneObject::ObjType::Function: return FUNCTION;
            case VyneObject::ObjType::Module:   return MODULE;
            case VyneObject::ObjType::Struct:   return STRUCT;
        }
    }
    return NONE;
}

std::string Value::getTypeName() const { 
    switch(getType()) {
        case Value::FLOAT64:  return "Float64";
        case Value::INT64:    return "Int64";
        case Value::STRING:   return "String";
        case Value::ARRAY:    return "Array";
        case Value::FUNCTION: return "Function";
        case Value::MODULE:   return "Module";
        case Value::STRUCT: {
            auto obj = std::get<std::shared_ptr<VyneObject>>(data);
            auto structPtr = std::static_pointer_cast<VyneStruct>(obj);
            return structPtr->typeName;
        }
        default: return "null";
    }
}

double Value::asFloat() const { 
    if (data.index() == 1) return std::get<double>(data);
    if (data.index() == 2) return static_cast<double>(std::get<int64_t>(data));
    return 0.0;
}

int64_t Value::asInt() const { 
    if (data.index() == 2) return std::get<int64_t>(data);
    if (data.index() == 1) return static_cast<int64_t>(std::get<double>(data));
    return 0;
}

const std::string& Value::asString() const {
    if (auto* id = std::get_if<uint32_t>(&data)) {
        return StringPool::instance().get(*id);
    }
    if (auto* id64 = std::get_if<int64_t>(&data)) {
        return StringPool::instance().get(static_cast<uint32_t>(*id64));
    }
    throw std::runtime_error("Type Error: Expected String, found " + getTypeName());
}

std::vector<Value>& Value::asList() { 
    auto obj = std::get<std::shared_ptr<VyneObject>>(this->data);
    return static_cast<VyneArray*>(obj.get())->elements; 
}

const std::vector<Value>& Value::asList() const { 
    auto obj = std::get<std::shared_ptr<VyneObject>>(this->data);
    return static_cast<VyneArray*>(obj.get())->elements; 
}

std::shared_ptr<FunctionData> Value::asFunction() const { 
    auto obj = std::get<std::shared_ptr<VyneObject>>(this->data);
    return std::static_pointer_cast<FunctionData>(obj);
}
const std::string& Value::asModule() const { 
    auto obj = std::get<std::shared_ptr<VyneObject>>(this->data);
    return static_cast<ModuleData*>(obj.get())->name;
}

Value& Value::setReadOnly(){
    isReadOnly = true;
    return *this;
}

long Value::getRefCount() const {
    if (data.index() == 4) {
        return std::get<std::shared_ptr<VyneObject>>(data).use_count();
    }
    return 0; 
}

void Value::print(std::ostream& os) const {
    switch (data.index()) {
        case 0: os << "null"; break;
        case 1: {
            double val = std::get<double>(data);
            std::ostringstream oss;
            
            oss << val;
            std::string s = oss.str();
            
            if (s.find('.') == std::string::npos && s.find('e') == std::string::npos) {
                s += ".0";
            }
            
            os << s;
            break;
        }
        case 2: os << std::get<int64_t>(data); break;
        case 3: os << asString(); break;
        case 4: {
            auto obj = std::get<std::shared_ptr<VyneObject>>(data);
            if (!obj) { os << "null"; break; }
            switch (obj->objType) {
                case VyneObject::ObjType::Array: {
                    const auto& list = static_cast<VyneArray*>(obj.get())->elements;
                    os << "[";
                    for (size_t i = 0; i < list.size(); ++i) {
                        list[i].print(os);
                        if (i < list.size() - 1) os << ", ";
                    }
                    os << "]";
                    break;
                }
                case VyneObject::ObjType::Function: {
                    auto func = static_cast<FunctionData*>(obj.get());
                    os << (func->isNative ? "<native function>" : "<function>");
                    break;
                }
                case VyneObject::ObjType::Module: {
                    os << "<module '" << static_cast<ModuleData*>(obj.get())->name << "'>";
                    break;
                }
                case VyneObject::ObjType::Struct: {
                    auto structPtr = std::static_pointer_cast<VyneStruct>(obj);
                    
                    os << structPtr->typeName << " { ";
                    
                    auto it = structPtr->fields.begin();
                    while (it != structPtr->fields.end()) {
                        std::string fieldName = StringPool::instance().get(it->first);
                        os << fieldName << ": ";
                        it->second.print(os);
                        
                        if (++it != structPtr->fields.end()) {
                            os << ", ";
                        }
                    }
                    
                    os << " }";
                    break;
                }
            }
            break;
        }
    }
}

size_t Value::getDeepBytes() const {
    switch(data.index()) {
        case 1: return sizeof(double);
        case 2: return sizeof(int64_t);
        case 3: return sizeof(uint32_t);
        case 4: {
            auto obj = std::get<std::shared_ptr<VyneObject>>(data);
            if (!obj) return 0;
            size_t total = 16;
            if (obj->objType == VyneObject::ObjType::Array) {
                auto arr = static_cast<VyneArray*>(obj.get());
                total += sizeof(VyneArray) + (arr->elements.capacity() * sizeof(Value));
                for (const auto& item : arr->elements) total += item.getDeepBytes();
            } else if (obj->objType == VyneObject::ObjType::Module) {
                total += sizeof(ModuleData) + static_cast<ModuleData*>(obj.get())->name.capacity();
            }
            return total;
        }
        default: return 0;
    }
}

size_t Value::getShallowBytes() const {
    switch(data.index()) {
        case 0: return 0;
        case 1: return sizeof(double);
        case 2: return sizeof(int64_t);
        case 3: return sizeof(uint32_t);
        case 4: {
            auto obj = std::get<std::shared_ptr<VyneObject>>(data);
            if (!obj) return 0;

            size_t baseSize = sizeof(std::shared_ptr<VyneObject>);
            switch(obj->objType) {
                case VyneObject::ObjType::Array:
                    return baseSize + sizeof(VyneArray);
                case VyneObject::ObjType::Function:
                    return baseSize + sizeof(FunctionData);
                case VyneObject::ObjType::Module:
                    return baseSize + sizeof(ModuleData);
            }
            return baseSize;
        }
        default: return 0;
    }
}

bool Value::equals(const Value& other) const {
    return *this == other;
}

std::string Value::toString() const {
    switch(data.index()) {
        case 1: {
            char buffer[64];
            auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), std::get<double>(data));
            return (ec == std::errc()) ? std::string(buffer, ptr - buffer) : "0";
        }
        case 2: return std::to_string(std::get<int64_t>(data));
        case 3: return asString();
        case 0: return "null";
        default: {
            std::stringstream ss;
            this->print(ss);
            return ss.str();
        }
    }
}

int Value::toNumber() const {
    switch(data.index()){
        case 0: return 0;
        case 1: return static_cast<int>(std::get<double>(data));
        case 2: return static_cast<int>(std::get<int64_t>(data));
        case 3: {
            try {
                return static_cast<int>(std::stod(asString()));
            } catch (...) { return 0; }
        }
        default: return 0;
    }
}

bool Value::isTruthy() const {
    switch(data.index()) {
        case 1: return std::get<double>(data) != 0;
        case 2: return std::get<int64_t>(data) != 0;
        case 3: return !asString().empty();
        case 4: return true;
        default: return false;
    }
}

bool Value::operator==(const Value& other) const {
    if (data.index() != other.data.index()) return false;
    switch (data.index()) {
        case 0: return true; // null == null
        case 1: return std::get<double>(data) == std::get<double>(other.data);
        case 2: return std::get<int64_t>(data) == std::get<int64_t>(other.data);
        case 3: return std::get<uint32_t>(data) == std::get<uint32_t>(other.data);
        case 4: return std::get<std::shared_ptr<VyneObject>>(data) == std::get<std::shared_ptr<VyneObject>>(other.data);
        default: return true;
    }
}

bool Value::operator!=(const Value& other) const {
    return !(*this == other);
}

bool Value::operator<(const Value& other) const {
    if (data.index() != other.data.index()) {
        return data.index() < other.data.index();
    }
    
    switch (data.index()) {
        case 0: return false;
        case 1: return std::get<double>(data) < std::get<double>(other.data);
        case 2: return std::get<int64_t>(data) < std::get<int64_t>(other.data);
        case 3: return std::get<uint32_t>(data) < std::get<uint32_t>(other.data); // Interned string ID comparison
        case 4: {
            return std::get<std::shared_ptr<VyneObject>>(data) < std::get<std::shared_ptr<VyneObject>>(other.data);
        }
        default: return false;
    }
}

uint32_t StringPool::intern(const std::string& s) {
    StringPool& pool = StringPool::instance();

    auto it = pool.strToId.find(s);
    if (it != pool.strToId.end()) return it->second;

    uint32_t newId = static_cast<uint32_t>(pool.idToStr.size());
    pool.idToStr.emplace_back(s);
    pool.strToId[s] = newId;

    return newId;
}
