#include "value.h"

int Value::getType() const { 
    size_t idx = data.index();
    if (idx == 0) return NONE;
    if (idx == 1) return NUMBER;
    if (idx == 2) return STRING;
    if (idx == 3) {
        auto obj = std::get<std::shared_ptr<VyneObject>>(data);
        if (!obj) return NONE;
        if (obj->objType == VyneObject::ObjType::Array) return ARRAY;
        if (obj->objType == VyneObject::ObjType::Function) return FUNCTION;
        if (obj->objType == VyneObject::ObjType::Module) return MODULE;
    }
    return NONE;
}

std::string Value::getTypeName() const { 
    int type = getType();
    switch(type) {
        case Value::NUMBER:   return "Number";
        case Value::STRING:   return "String";
        case Value::ARRAY:    return "Array";
        case Value::FUNCTION: return "Function";
        case Value::MODULE:   return "Module";
        default:              return "Unknown";
    }
}

double Value::asNumber() const { 
    return std::get<double>(this->data); 
}

const std::string& Value::asString() const { 
    return StringPool::instance().get(std::get<uint32_t>(this->data)); 
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

void Value::print(std::ostream& os) const {
    switch (data.index()) {
        case 0:
            os << "null";
            break;

        case 1:
            os << std::get<double>(data);
            break;

        case 2:
            os << "\"" << asString() << "\"";
            break;

        case 3: {
            auto obj = std::get<std::shared_ptr<VyneObject>>(data);
            if (!obj) {
                os << "null";
                break;
            }

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
                    if (func->isNative) os << "<native function>";
                    else os << "<function>";
                    break;
                }
                case VyneObject::ObjType::Module: {
                    auto mod = static_cast<ModuleData*>(obj.get());
                    os << "<module '" << mod->name << "'>";
                    break;
                }
            }
            break;
        }
        default:
            os << "<unknown>";
            break;
    }
}

size_t Value::getDeepBytes() const {
    switch(data.index()) {
        case 1:
            return sizeof(double);

        case 2:
            return sizeof(uint32_t);

        case 3: { // HEAP OBJECTS
            auto obj = std::get<std::shared_ptr<VyneObject>>(data);
            if (!obj) return 0;

            size_t total = 16; 

            switch(obj->objType) {
                case VyneObject::ObjType::Array: {
                    auto arr = static_cast<VyneArray*>(obj.get());
                    total += sizeof(VyneArray);
                    total += arr->elements.capacity() * sizeof(Value);
                    for (const auto& item : arr->elements) {
                        total += item.getDeepBytes();
                    }
                    break;
                }
                case VyneObject::ObjType::Function: {
                    auto func = static_cast<FunctionData*>(obj.get());
                    total += sizeof(FunctionData);
                    total += func->params.capacity() * sizeof(uint32_t);
                    total += func->body.capacity() * sizeof(std::shared_ptr<ASTNode>);
                    break;
                }
                case VyneObject::ObjType::Module: {
                    auto mod = static_cast<ModuleData*>(obj.get());
                    total += sizeof(ModuleData);
                    total += mod->name.capacity();
                    break;
                }
            }
            return total;
        }
        default: return 0;
    }
}

size_t Value::getShallowBytes() const {
    switch(data.index()) {
        case 1:
            return sizeof(double);

        case 2:
            return sizeof(uint32_t);

        case 3: {
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
        default:
            return 0;
    }
}

bool Value::equals(const Value& other) const {
    if (getType() != other.getType()) return false;

    switch(getType()) {
        case 0: return true;
        case 1: return asNumber() == other.asNumber();
        case 2: return asString() == other.asString();
        default: return false;
    }
}

std::string Value::toString() const {
    switch(data.index()) {
        case 1: {
            char buffer[64];
            auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), std::get<double>(data));
            if (ec == std::errc()) {
                return std::string(buffer, ptr - buffer);
            }
            return "0";
        }
        case 2:
            return StringPool::instance().get(std::get<uint32_t>(data));

        case 0:
            return "null";

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
        case 2: {
            try {
                const std::string& s = StringPool::instance().get(std::get<uint32_t>(data));
                return static_cast<int>(std::stod(s));
            } catch (...) {
                return 0; 
            }
        }
        default: return 0;
    }
}

bool Value::isTruthy() const {
    switch(getType()) {
        case Value::NUMBER:  return asNumber() != 0;
        case Value::STRING:  return !asString().empty();
        case Value::ARRAY:   return !asList().empty();
        default:             return false;
    }
}


bool Value::operator==(const Value& other) const {
    int type = getType();
    if (type != other.getType()) return false;

    switch (type) {
        case NUMBER: return asNumber() == other.asNumber();
        case STRING: return std::get<uint32_t>(data) == std::get<uint32_t>(other.data); // Fast ID comparison!
        case ARRAY:  return asList() == other.asList();
        default:     return false; 
    }
}

bool Value::operator!=(const Value& other) const {
    if (this->data.index() != other.data.index()) return true;

    switch (this->data.index()) {
        case 0: return false;
        case 1: return asNumber() != other.asNumber();
        case 2: return std::get<uint32_t>(data) != std::get<uint32_t>(other.data);
        case 3: return asList() != other.asList();
        default: return true; 
    }
}

bool Value::operator<(const Value& other) const {
    if (data.index() != other.data.index()) {
        return data.index() < other.data.index();
    }

    switch (data.index()) {
        case 1:
            return std::get<double>(data) < std::get<double>(other.data);
        case 2:
            return std::get<uint32_t>(data) < std::get<uint32_t>(other.data);
        default:
            return false;
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