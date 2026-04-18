#pragma once
#include <iostream>
#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <set>
#include <algorithm>
#include <stdexcept>
#include <variant>
#include <sstream>
#include <functional>
#include <cstdint>
#include <cmath>
#include <filesystem>

#include "../lexer/lexer.h"
#include "../types.h"
#include "../../utils/file_utils.h"
#include "../types.h"
#include "../../runtime/diagnostics.h"
#include "value.h"

class Emitter;
class Parser;
struct Value;
class ASTNode;
using SymbolTable = std::unordered_map<uint32_t, Value>;
class SymbolContainer {
    std::unordered_map<uint32_t, SymbolTable> table;
    std::vector<std::string> deployedModules;
    std::string currentSourceDir = ".";

    mutable std::unordered_map<uint32_t, bool> variableUsage;

    // Helper to intern strings to uint32_t
    uint32_t intern(const std::string& s) const {
        return StringPool::instance().intern(s);
    }

public:
    // Key operations - now using uint32_t
    SymbolTable& operator[](uint32_t id) { return table[id]; }
    auto find(uint32_t id) { return table.find(id); }
    auto find(uint32_t id) const { return table.find(id); }
    bool contains(uint32_t id) const { return table.find(id) != table.end(); }
    size_t count(uint32_t id) const { return table.count(id); }
    size_t erase(uint32_t id) { return table.erase(id); }
    const SymbolTable& at(uint32_t id) const { return table.at(id); }

    // Convenience methods that accept strings (they intern automatically)
    SymbolTable& operator[](const std::string& key) { 
        return table[intern(key)]; 
    }
    
    auto find(const std::string& key) { 
        return table.find(intern(key)); 
    }
    
    auto find(const std::string& key) const { 
        return table.find(intern(key)); 
    }
    
    bool contains(const std::string& key) const { 
        return table.find(intern(key)) != table.end(); 
    }
    
    size_t count(const std::string& key) const { 
        return table.count(intern(key)); 
    }
    
    size_t erase(const std::string& key) { 
        return table.erase(intern(key)); 
    }
    
    const SymbolTable& at(const std::string& key) const { 
        return table.at(intern(key)); 
    }

    // Iterator support
    auto begin() { return table.begin(); } 
    auto begin() const { return table.begin(); }
    auto end() { return table.end(); }
    auto end() const { return table.end(); }

    // Group/Module checks
    bool hasGroup(const std::string& name) const {
        return contains(name);
    }

    bool hasModule(const std::string& name) const {
        return contains(name);
    }

    void markUsed(uint32_t varId) const {
        variableUsage[varId] = true;
    }
    
    bool wasUsed(uint32_t varId) const {
        auto it = variableUsage.find(varId);
        return it != variableUsage.end() && it->second;
    }
    
    const auto& getUsageMap() const { return variableUsage; }

    // Member access
    Value getGroupMember(const std::string& groupName, const std::string& memberName) {
        uint32_t groupId = intern(groupName);
        auto moduleIt = table.find(groupId);
        if (moduleIt == table.end()) {
            throw std::runtime_error("Runtime Error: Group/Namespace '" + groupName + "' not found.");
        }

        uint32_t memberId = intern(memberName);
        
        SymbolTable& scope = moduleIt->second;
        auto varIt = scope.find(memberId);

        if (varIt == scope.end()) {
            throw std::runtime_error("Runtime Error: '" + groupName + "' has no member '" + memberName + "'");
        }

        return varIt->second;
    }

    Value getModuleMember(uint32_t moduleId, const std::string& memberName) {
        auto moduleIt = table.find(moduleId);
        if (moduleIt == table.end()) {
            std::string moduleName = StringPool::instance().get(moduleId);
            throw std::runtime_error("Runtime Error: Module '" + moduleName + "' not found.");
        }

        uint32_t memberId = StringPool::instance().intern(memberName);
        
        SymbolTable& scope = moduleIt->second;
        auto varIt = scope.find(memberId);

        if (varIt == scope.end()) {
            std::string moduleName = StringPool::instance().get(moduleId);
            throw std::runtime_error("Runtime Error: '" + moduleName + "' has no member '" + memberName + "'");
        }

        return varIt->second;
    }

    Value* getInternalPointer(uint32_t moduleId, uint32_t varId) {
        auto moduleIt = table.find(moduleId);
        if (moduleIt == table.end()) {
            std::string moduleName = StringPool::instance().get(moduleId);
            throw std::runtime_error("Module '" + moduleName + "' not found.");
        }

        SymbolTable& scope = moduleIt->second;
        auto varIt = scope.find(varId);
        if (varIt == scope.end()) {
            throw std::runtime_error("Variable ID '" + std::to_string(varId) + "' not found.");
        }

        return &(varIt->second);
    }
    
    Value* getInternalPointer(const std::string& moduleName, uint32_t varId) {
        uint32_t moduleId = StringPool::instance().intern(moduleName);
        return getInternalPointer(moduleId, varId);
    }

    void deploy(const std::string& moduleName) {
        deployedModules.emplace_back(moduleName);
        uint32_t moduleId = intern(moduleName);
        if (table.find(moduleId) == table.end()) {
            table[moduleId] = SymbolTable();
        }
    }

    const std::vector<std::string>& getDeployedList() const { 
        return deployedModules; 
    }

    void setSourceDir(const std::string& path) { 
        currentSourceDir = std::filesystem::path(path).parent_path().string(); 
    }
    
    std::string getSourceDir() const { 
        return currentSourceDir; 
    }

    // Utility
    size_t size() const { return table.size(); }
    bool empty() const { return table.empty(); }
};

// exception signals
struct ReturnException {
    Value value;
};

struct BreakException {};
struct ContinueException {};

enum class NodeType {
    PROGRAM,
    GROUP,

    NUMBER,
    STRING,
    BOOLEAN,
    VARIABLE,
    ASSIGNMENT,
    INDEX_ASSIGNMENT,

    BINARY_OP,
    POSTFIX,
    UNARY,

    ARRAY,
    RANGE,
    INDEX_ACCESS,

    FUNCTION,
    FUNCTION_CALL,
    BUILTIN_CALL,
    METHOD_CALL,
    MEMBER_ACCESS,
    RETURN,

    WHILE,
    FOR,
    BLOCK,
    IF,

    MODULE,
    DISMISS,
    DEPLOY,
    IMPORT,
    INTERFACE,

    NULLTYPE,
    MEMBER_ASSIGNMENT,

    RULESET, 

    BREAK,
    CONTINUE
};


class ASTNode {
public:
    int lineNumber = 0;
    NodeType nodeType;

    ASTNode(NodeType t) : nodeType(t) {}

    virtual ~ASTNode() = default;

    NodeType type() const { return nodeType; }
    virtual VType getStaticType() const { return VType::Unknown; }
    virtual Value evaluate(SymbolContainer& env, const std::string& currentGroup = "global") const = 0;
    virtual void compile(Emitter& e) const = 0;
};

class ProgramNode : public ASTNode {
public:
    std::vector<std::shared_ptr<ASTNode>> statements;

    ProgramNode(std::vector<std::shared_ptr<ASTNode>> stmts) 
        : ASTNode(NodeType::PROGRAM), statements(std::move(stmts)) {}

    Value evaluate(SymbolContainer& env, const std::string& currentGroup = "global") const override;
    void compile(Emitter& e) const override;
};

class GroupNode : public ASTNode {
    const std::string groupName;
    const std::string targetModule;
    std::vector<std::unique_ptr<ASTNode>> statements;

public:
    GroupNode(std::string name, std::vector<std::unique_ptr<ASTNode>> stmts, std::string targetMod = "")
        : ASTNode(NodeType::GROUP), 
          groupName(name), 
          targetModule(targetMod), 
          statements(std::move(stmts)) {
    }

    Value evaluate(SymbolContainer& env, const std::string& currentGroup) const override;
    void compile(Emitter& e) const override;
};

class NumberNode : public ASTNode {
    // Storing as a Value allows the parser to decide if it's 
    // an INT64 or a FLOAT64 immediately upon creation.
    Value value; 
public:
    NumberNode(Value val) : ASTNode(NodeType::NUMBER), value(std::move(val)) {}
    
    Value evaluate(SymbolContainer& env, const std::string& currentGroup = "global") const override {
        return value;
    }
    
    void compile(Emitter& e) const override;
    VType getStaticType() const override { 
        return (value.getType() == Value::INT64) ? VType::Int64 : VType::Float64; 
    }
};

class VariableNode : public ASTNode {
    uint32_t nameId;
    std::string originalName;
    std::vector<std::string> specificGroup;
    VType explicitType;
    bool isReference;

public:
    VariableNode(
        uint32_t id, 
        std::string name, 
        VType et = VType::Unknown, 
        std::vector<std::string> group = {},
        bool isRef = false)
        : 
        ASTNode(NodeType::VARIABLE), 
        nameId(id), 
        originalName(std::move(name)), 
        explicitType(std::move(et)),
        specificGroup(std::move(group)),
        isReference(isRef) {}

    Value evaluate(SymbolContainer& env, const std::string& currentGroup = "global") const override;
    void compile(Emitter& e) const override;

    const std::vector<std::string>& getScope() const { return specificGroup; }
    uint32_t getNameId() const { return nameId; }
    const std::string& getOriginalName() const { return originalName; }
    bool isRefVar() const { return isReference; }
    VType getStaticType() const override { return explicitType; }
};

class AssignmentNode : public ASTNode {
    uint32_t identifierId;
    std::string originalName;
    std::unique_ptr<ASTNode> rhs;
    std::unique_ptr<ASTNode> indexExpr;
    std::vector<std::string> scopePath;
    bool isConstant;
    bool isReference;
    VType expectedType;

public:
    AssignmentNode(uint32_t id, 
                   std::string on, 
                   std::unique_ptr<ASTNode> rhs_ptr, 
                   bool ic,
                   bool ir,
                   VType vt,
                   std::vector<std::string> path = {},
                   std::unique_ptr<ASTNode> idx_ptr = nullptr)
        : ASTNode(NodeType::ASSIGNMENT),
          identifierId(id), 
          originalName(std::move(on)), 
          rhs(std::move(rhs_ptr)), 
          indexExpr(std::move(idx_ptr)),
          scopePath(std::move(path)),
          isConstant(ic),
          isReference(ir),
          expectedType(std::move(vt)) {}

    void compile(Emitter& e) const override;
    Value evaluate(SymbolContainer& env, const std::string& currentGroup = "global") const override;
};

class MemberAssignmentNode : public ASTNode {
    std::unique_ptr<ASTNode> receiver;
    std::string memberName;
    std::unique_ptr<ASTNode> rhs;

public:
    MemberAssignmentNode(std::unique_ptr<ASTNode> rec, std::string mem, std::unique_ptr<ASTNode> r)
        : ASTNode(NodeType::MEMBER_ASSIGNMENT), 
          receiver(std::move(rec)), 
          memberName(std::move(mem)), 
          rhs(std::move(r)) {}

    Value evaluate(SymbolContainer& env, const std::string& currentGroup) const override;

    void compile(Emitter& e) const override;
};

class BinOpNode : public ASTNode {
    VTokenType op;
    std::unique_ptr<ASTNode> left;
    std::unique_ptr<ASTNode> right;
public:
    BinOpNode(VTokenType op, std::unique_ptr<ASTNode> l, std::unique_ptr<ASTNode> r)
        : ASTNode(NodeType::BINARY_OP), op(op), left(std::move(l)), right(std::move(r)) {
    }
    
    Value evaluate(SymbolContainer& env, const std::string& currentGroup = "global") const override;
    void compile(Emitter& e) const override;

    VType getStaticType() const override {
        switch(op) {
            case VTokenType::Add:
            case VTokenType::Substract:
            case VTokenType::Multiply:
            case VTokenType::Division:
                return VType::Float64;
            case VTokenType::Floor_Divide:
            case VTokenType::Modulo:
                return VType::Int64;
            case VTokenType::And:
            case VTokenType::Or:
            case VTokenType::Double_Equals:
            case VTokenType::Greater:
            case VTokenType::Smaller:
                return VType::Int64;
            default:
                return VType::Unknown;
        }
    }
};

class PostFixNode : public ASTNode {
    VTokenType op;
    std::unique_ptr<ASTNode> left;
public:
    PostFixNode(VTokenType op, std::unique_ptr<ASTNode> lhs)
        : ASTNode(NodeType::POSTFIX), op(op), left(std::move(lhs)) {}
    
    Value evaluate(SymbolContainer& env, const std::string& currentGroup = "global") const override;
    void compile(Emitter& e) const override;
    VType getStaticType() const override { 
        return left->getStaticType(); 
    }
};

class UnaryNode : public ASTNode {
    VTokenType op;
    std::unique_ptr<ASTNode> right;

public: 
    UnaryNode(VTokenType op, std::unique_ptr<ASTNode> rhs)
        : ASTNode(NodeType::UNARY), op(op), right(std::move(rhs)) {}
    
    Value evaluate(SymbolContainer& env, const std::string& currentGroup = "global") const override;
    void compile(Emitter& e) const override;
    VType getStaticType() const override { 
        if (op == VTokenType::Addresser) {
            return VType::Int64;
        }
        
        return right->getStaticType(); 
    }
};   

class BuiltInCallNode : public ASTNode {
    enum class BuiltInType {
        PRINT, EXIT, TYPE, STRING, INT64, FLOAT64, SIZEOF, SEQUENCE, UNKNOWN
    };

    std::string funcName;
    std::vector<std::unique_ptr<ASTNode>> arguments;
    BuiltInType builtInType;

    static BuiltInType resolveBuiltIn(const std::string& name) {
        static const std::unordered_map<std::string, BuiltInType> map = {
            {"out", BuiltInType::PRINT},
            {"exit", BuiltInType::EXIT},
            {"type", BuiltInType::TYPE},
            {"string", BuiltInType::STRING},
            {"int64", BuiltInType::INT64},
            {"float64", BuiltInType::FLOAT64},
            {"sizeof", BuiltInType::SIZEOF},
            {"sequence", BuiltInType::SEQUENCE}
        };
        
        auto it = map.find(name);
        return it != map.end() ? it->second : BuiltInType::UNKNOWN;
    }
    
    Value handleInt64(const std::vector<Value>& args) const {
        if (args.size() != 1) 
            throw std::runtime_error("Argument Error: int64() expects 1 argument...");
        
        if (args[0].getType() == Value::FLOAT64) {
            return Value(static_cast<int64_t>(args[0].asFloat()));
        }
        else if (args[0].getType() == Value::STRING) {
            return Value(static_cast<int64_t>(std::stoll(args[0].asString())));
        }
        else if (args[0].getType() == Value::INT64) {
            return args[0];
        }
        throw std::runtime_error("Type Error: Cannot convert to int64...");
    }
    
    Value handleFloat64(const std::vector<Value>& args) const {
        if (args.size() != 1) 
            throw std::runtime_error("Argument Error: float64() expects 1 argument...");
        
        if (args[0].getType() == Value::INT64) {
            return Value(static_cast<double>(args[0].asInt()));
        }
        else if (args[0].getType() == Value::STRING) {
            return Value(std::stod(args[0].asString()));
        }
        else if (args[0].getType() == Value::FLOAT64) {
            return args[0];
        }
        throw std::runtime_error("Type Error: Cannot convert to float64...");
    }
    
    Value handleSequence(const std::vector<Value>& args) const {
        if (args.size() != 2) 
            throw std::runtime_error("Argument Error: sequence() expects 2 args.");
        
        std::vector<Value> sequence;
        if (args[0].getType() == Value::INT64 && args[1].getType() == Value::INT64) {
            for (int64_t i = args[0].asInt(); i < args[1].asInt(); ++i) 
                sequence.emplace_back(i);
        } else {
            for (double i = args[0].asFloat(); i < args[1].asFloat(); ++i) 
                sequence.emplace_back(i);
        }
        return Value(sequence);
    }

public:
    BuiltInCallNode(std::string name, std::vector<std::unique_ptr<ASTNode>> args) 
        : 
        ASTNode(NodeType::BUILTIN_CALL), 
        builtInType(resolveBuiltIn(name)), 
        arguments(std::move(args)) 
        {
            funcName = std::move(name);
        }

    Value evaluate(SymbolContainer& env, const std::string& currentGroup) const override;
    void compile(Emitter& e) const override;
};

class StringNode : public ASTNode {
    std::string text;
public:
    StringNode(std::string t) : ASTNode(NodeType::STRING), text(std::move(t)) {}
    
    Value evaluate(SymbolContainer& env, const std::string& currentGroup = "global") const override {
        return Value(text);
    }

    void compile(Emitter& e) const override;
    VType getStaticType() const override { return VType::String; }
};

class BooleanNode : public ASTNode {
    bool condition;
public :
    BooleanNode(bool c) : ASTNode(NodeType::BOOLEAN), condition(c) {}

    Value evaluate(SymbolContainer& env, const std::string& currentGroup = "global") const override {
        return Value(condition);
    };
    void compile(Emitter& e) const override;
    VType getStaticType() const override { return VType::Bool; }
};

class ArrayNode : public ASTNode {
    std::vector<std::unique_ptr<ASTNode>> elements;
public:
    ArrayNode(std::vector<std::unique_ptr<ASTNode>> elm) : ASTNode(NodeType::ARRAY), elements(std::move(elm)) {}

    Value evaluate(SymbolContainer& env, const std::string& currentGroup = "global") const override;
    void compile(Emitter& e) const override;
    VType getStaticType() const override { return VType::Array; }
};

class RangeNode : public ASTNode {
    std::unique_ptr<ASTNode> left;
    std::unique_ptr<ASTNode> right;
public:
    RangeNode(std::unique_ptr<ASTNode> l, std::unique_ptr<ASTNode> r) :
    ASTNode(NodeType::RANGE),
    left(std::move(l)), right(std::move(r)) {}

    Value evaluate(SymbolContainer& env, const std::string& currentGroup = "global") const override;
    void compile(Emitter& e) const override;
    VType getStaticType() const override { return VType::Array; }
};

class IndexAccessNode : public ASTNode {
    std::unique_ptr<ASTNode> base;
    std::unique_ptr<ASTNode> index;

public:
    IndexAccessNode(std::unique_ptr<ASTNode> b, std::unique_ptr<ASTNode> idx)
        : ASTNode(NodeType::INDEX_ACCESS), base(std::move(b)), index(std::move(idx)) {}

    IndexAccessNode(uint32_t n, std::string on, std::vector<std::string> s, std::unique_ptr<ASTNode> idx)
        : ASTNode(NodeType::INDEX_ACCESS), index(std::move(idx)) {
        base = std::make_unique<VariableNode>(n, std::move(on), VType::Unknown, std::move(s), false);
    }

    Value evaluate(SymbolContainer& env, const std::string& currentGroup) const override;
    void compile(Emitter& e) const override;
    std::unique_ptr<ASTNode> takeBase() { return std::move(base); }
    std::unique_ptr<ASTNode> takeIndex() { return std::move(index); }
};

class IndexAssignmentNode : public ASTNode {
    std::unique_ptr<ASTNode> base;
    std::unique_ptr<ASTNode> index;
    std::unique_ptr<ASTNode> rhs;
     std::string arrayName;
    bool isConstant;

public:
    IndexAssignmentNode(std::unique_ptr<ASTNode> b, 
                        std::unique_ptr<ASTNode> idx, 
                        std::unique_ptr<ASTNode> r,
                        bool ic)
        : ASTNode(NodeType::INDEX_ASSIGNMENT), 
          base(std::move(b)), 
          index(std::move(idx)), 
          rhs(std::move(r)),
          isConstant(ic) 
        {
            if (base->type() == NodeType::VARIABLE) {
                auto* var = static_cast<VariableNode*>(base.get());
                arrayName = var->getOriginalName();
            } else {
                arrayName = "anonymous";
            }
        }

    Value evaluate(SymbolContainer& env, const std::string& currentGroup) const override;
    
    void compile(Emitter& e) const override;
};

class FunctionNode : public ASTNode {
    std::string targetModule;
    uint32_t funcNameId;
    std::string originalName;
    std::vector<Parameter> parameters;
    std::vector<std::shared_ptr<ASTNode>> body;
    VType returnType;

public:
    FunctionNode(std::string tm, uint32_t n, std::string on, std::vector<Parameter> pid, 
                 std::vector<std::shared_ptr<ASTNode>> body_vec, VType retType)
        : 
        ASTNode(NodeType::FUNCTION), 
        targetModule(std::move(tm)), 
        funcNameId(n), 
        originalName(std::move(on)), 
        parameters(std::move(pid)), 
        body(std::move(body_vec)), 
        returnType(retType) {}

    Value evaluate(SymbolContainer& env, const std::string& currentGroup) const override;
    void compile(Emitter& e) const override;
    VType getStaticType() const override { return VType::Function; }
    const std::string& getOriginalName() const { return originalName; }

    const std::vector<Parameter>& getParameters() const { return parameters; }
    VType getReturnType() const { return returnType; }
};
class FunctionCallNode : public ASTNode {
    uint32_t funcNameId;
    std::string originalName;
    std::vector<std::unique_ptr<ASTNode>> arguments;

public:
    FunctionCallNode(uint32_t fn, std::string name, std::vector<std::unique_ptr<ASTNode>> args)
        : 
        ASTNode(NodeType::FUNCTION_CALL), 
        funcNameId(fn), 
        originalName(std::move(name)), 
        arguments(std::move(args))
    {}

    Value evaluate(SymbolContainer& env, const std::string& currentGroup = "global") const override;
    void compile(Emitter& e) const override;
    static Value deepCopyArray(const Value& arr) {
        auto& originalVec = arr.asList();
        std::vector<Value> copiedVec;
        
        for (const auto& element : originalVec) {
            if (element.getType() == Value::ARRAY) {
                copiedVec.emplace_back(deepCopyArray(element));
            } else {
                copiedVec.emplace_back(element);
            }
        }
        
        return Value(std::move(copiedVec));
    }
};

class ReturnNode : public ASTNode {
    std::unique_ptr<ASTNode> expression;
public:
    ReturnNode(std::unique_ptr<ASTNode> expr) : ASTNode(NodeType::RETURN), expression(std::move(expr)) {}

    Value evaluate(SymbolContainer& env, const std::string& currentGroup = "global") const override;
    void compile(Emitter& e) const override;
};

class MethodCallNode : public ASTNode {
    std::unique_ptr<ASTNode> receiver;
    std::string methodName;
    std::vector<std::unique_ptr<ASTNode>> arguments;

public:
    MethodCallNode(std::unique_ptr<ASTNode> recv, std::string method, 
                   std::vector<std::unique_ptr<ASTNode>> args)
        : ASTNode(NodeType::METHOD_CALL),
        receiver(std::move(recv)), methodName(std::move(method)), arguments(std::move(args)) {}

    Value evaluate(SymbolContainer& env, const std::string& currentGroup = "global") const override;
    void compile(Emitter& e) const override;
};

class WhileNode : public ASTNode {
    std::unique_ptr<ASTNode> condition;
    std::unique_ptr<ASTNode> body;

public:
    WhileNode(std::unique_ptr<ASTNode> c, std::unique_ptr<ASTNode> b)
        : ASTNode(NodeType::WHILE), condition(std::move(c)), body(std::move(b)) {}

    Value evaluate(SymbolContainer& env, const std::string& currentGroup = "global") const override;
    void compile(Emitter& e) const override;
};

class ForNode : public ASTNode {
    enum class ForMode { LOOP, COLLECT, FILTER, EVERY, UNIQUE };
    std::unique_ptr<ASTNode> iterable;
    std::unique_ptr<ASTNode> body;
    std::string iteratorName;
    ForMode mode;

public:
    ForNode(std::unique_ptr<ASTNode> i, std::unique_ptr<ASTNode> b, std::string in, ForMode m)
        : ASTNode(NodeType::FOR), iterable(std::move(i)), body(std::move(b)), iteratorName(std::move(in)), mode(m) {}

    Value evaluate(SymbolContainer& env, const std::string& currentGroup = "global") const override;
    void compile(Emitter& e) const override;

    static ForMode getForMode(const std::string& modeStr){
        if (modeStr == "collect") return ForNode::ForMode::COLLECT;
        else if (modeStr == "filter") return ForNode::ForMode::FILTER;
        else if (modeStr == "every") return ForNode::ForMode::EVERY;
        else if (modeStr == "unique") return ForNode::ForMode::UNIQUE;
        else return ForNode::ForMode::LOOP;
    }
};

class BlockNode : public ASTNode {
public:
    std::vector<std::shared_ptr<ASTNode>> statements;

    BlockNode(std::vector<std::shared_ptr<ASTNode>> stmts) 
        : ASTNode(NodeType::BLOCK), statements(std::move(stmts)) {}

    Value evaluate(SymbolContainer& env, const std::string& currentGroup = "global") const override;
    void compile(Emitter& e) const override;
};

class ModuleNode : public ASTNode {
public:
    uint32_t moduleId;
    std::string originalName;

    ModuleNode(uint32_t mId, std::string mName) : ASTNode(NodeType::MODULE), moduleId(mId), originalName(std::move(mName)) {}

    Value evaluate(SymbolContainer& env, const std::string& currentGroup = "global") const override;
    void compile(Emitter& e) const override;
    VType getStaticType() const override { return VType::Module; }
};

class ImportNode : public ASTNode {
    std::string filePath;
    std::string alias;
    bool isExtern;
public:
   ImportNode(std::string path, bool ie, std::string al = "")
    : ASTNode(NodeType::IMPORT), filePath(std::move(path)), alias(std::move(al)), isExtern(ie) {}

    Value evaluate(SymbolContainer& env, const std::string& currentGroup = "global") const override;
    void compile(Emitter& e) const override;
};

class DeployNode : public ASTNode {
    std::string moduleName;
public:
    DeployNode(std::string name) : ASTNode(NodeType::DEPLOY), moduleName(std::move(name)) {}

    Value evaluate(SymbolContainer& env, const std::string& currentGroup = "global") const override;
    
    void compile(Emitter& e) const override {}
};

class DismissNode : public ASTNode {
public:
    uint32_t moduleId;
    std::string originalName;

    DismissNode(uint32_t mId, std::string mName) : ASTNode(NodeType::DISMISS), moduleId(mId), originalName(std::move(mName)) {}

    Value evaluate(SymbolContainer& env, const std::string& currentGroup = "global") const override;
    void compile(Emitter& e) const override;
};

class IfNode : public ASTNode {
public:
    std::unique_ptr<ASTNode> condition;
    std::unique_ptr<ASTNode> body;
    std::unique_ptr<ASTNode> elseBody;

    IfNode(std::unique_ptr<ASTNode> c, std::unique_ptr<ASTNode> b, std::unique_ptr<ASTNode> eb = nullptr) : 
    ASTNode(NodeType::IF), condition(std::move(c)), body(std::move(b)), elseBody(std::move(eb)) {}

    Value evaluate(SymbolContainer& env, const std::string& currentGroup = "global") const override;
    void compile(Emitter& e) const override;
};

struct InterfaceMember {
    std::string name;
    VType type;
    size_t offset;
};

class InterfaceNode : public ASTNode {
    struct MethodSignature {
        std::string name;
        std::vector<VType> paramTypes;
        VType returnType;
        std::vector<std::string> paramNames;
    };
    std::vector<MethodSignature> methodSignatures;

    std::string interfaceName;
    std::vector<InterfaceMember> members;
    std::vector<std::shared_ptr<ASTNode>> methods;
    std::string moduleName;
public:

    InterfaceNode(std::string in, std::vector<InterfaceMember> m, std::vector<std::shared_ptr<ASTNode>> meth) 
    : 
    ASTNode(NodeType::INTERFACE), 
    interfaceName(std::move(in)), 
    members(std::move(m)),
    methods(std::move(meth))
    {
        for (const auto& method : methods) {
            if (auto* funcNode = dynamic_cast<FunctionNode*>(method.get())) {
                MethodSignature sig;
                sig.name = funcNode->getOriginalName();
                sig.returnType = funcNode->getReturnType();
                
                for (const auto& param : funcNode->getParameters()) {
                    sig.paramTypes.push_back(param.type);
                    sig.paramNames.push_back(param.name);
                }
                
                methodSignatures.push_back(sig);
            }
        }
    }

    Value evaluate(SymbolContainer& env, const std::string& currentGroup = "global") const override;
    void compile(Emitter& e) const override;
    void setModuleName(const std::string& name) { moduleName = name; }
};

class MemberAccessNode : public ASTNode {
    std::unique_ptr<ASTNode> receiver;
    std::string memberName;
public:
    MemberAccessNode(std::unique_ptr<ASTNode> rec, std::string mem) : 
        ASTNode(NodeType::MEMBER_ACCESS), receiver(std::move(rec)), memberName(std::move(mem)) {}

    std::unique_ptr<ASTNode> getReceiver() { return std::move(receiver); }
    const std::string& getMemberName() const { return memberName; }

    Value evaluate(SymbolContainer& env, const std::string& currentGroup = "global") const override;
    void compile(Emitter& e) const override;
    std::string getPath() const {
        if (auto var = dynamic_cast<VariableNode*>(receiver.get())) {
            return var->getOriginalName() + "." + memberName;
        } else if (auto mem = dynamic_cast<MemberAccessNode*>(receiver.get())) {
            return mem->getPath() + "." + memberName;
        }
        return memberName; 
    }

    std::string getReceiverPath() const {
        if (auto var = dynamic_cast<VariableNode*>(receiver.get())) {
            return var->getOriginalName();
        } else if (auto mem = dynamic_cast<MemberAccessNode*>(receiver.get())) {
            return mem->getPath();
        }
        return "";
    }
};

class NullNode : public ASTNode {
    std::string typeName;
public:
    NullNode() : 
            ASTNode(NodeType::NULLTYPE) {}
    NullNode(std::string tn) : 
        ASTNode(NodeType::NULLTYPE), typeName(std::move(tn)) {}

    Value evaluate(SymbolContainer& env, const std::string& currentGroup = "global") const override;
    void compile(Emitter& e) const override;
};

// exception structs
struct BreakNode : public ASTNode {
    BreakNode() : ASTNode(NodeType::BREAK) {}

    Value evaluate(SymbolContainer& env, const std::string& currentGroup = "global") const override {
        throw BreakException();
    }
    void compile(Emitter& e) const override;
};

struct ContinueNode : public ASTNode {
    ContinueNode() : ASTNode(NodeType::CONTINUE) {}

    Value evaluate(SymbolContainer& env, const std::string& currentGroup = "global") const override {
        throw ContinueException();
    }
    void compile(Emitter& e) const override;
};

std::string resolvePath(std::vector<std::string> scope, const std::string& currentGroup = "global");