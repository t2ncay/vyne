#include "ast.h"
#include "ast_helpers.h"

#include "../../modules/common/vcore/vcore.h"
#include "../../modules/common/vglib/vglib.h"
#include "../../modules/common/vmem/vmem.h"
#include "../../modules/common/vmath/vmath.h"
#include "../../modules/common/vfs/vfs.h"
#include "../../modules/common/vurage/vurage.h"
#include "../../modules/common/vcv/vcv.h"
#include "../../modules/common/vaudio/vaudio.h"

#include "../parser/parser.h"
#include "../lexer/lexer.h"

Value ProgramNode::evaluate(SymbolContainer& env, uint32_t currentGroupId) const {
    Value lastValue;
    for (const auto& statement : statements) {
        lastValue = statement->evaluate(env, currentGroupId);
    }
    return lastValue; 
}

/**
 * @brief Retrieves a variable's value from the SymbolContainer.
 * * @details Performs a scoped lookup:
 * 1. Checks the specific group (if provided, e.g., tracker.lineCount).
 * 2. Checks the current local group.
 * 3. Falls back to the "global" group if not found locally.
 * @see AssignmentNode::evaluate
 * * @throw std::runtime_error If the variable cannot be found in any accessible scope.
 * @return Value The stored value of the variable.
 */

Value VariableNode::evaluate(SymbolContainer& env, uint32_t currentGroupId) const {
    static uint32_t globalId = getGlobalId();
    uint32_t targetId = (specificGroupId != 0) ? specificGroupId : currentGroupId;
    
    Value* valPtr = lookupSymbol(env, targetId, nameId);
    env.markUsed(nameId);
    
    if (!valPtr && targetId != globalId) {
        valPtr = lookupSymbol(env, globalId, nameId);
    }

    if (!valPtr) {
        throw std::runtime_error("Runtime Error: Variable '" + originalName + 
                                "' not found [ line " + std::to_string(lineNumber) + " ]");
    }

    return valPtr->isReference() ? *(valPtr->getPointer()) : *valPtr;
}

/**
 * @brief Handles variable assignment and updates the SymbolContainer.
 * * @note Throws a runtime_error if attempting to reassign a Read-Only value.
 * @see VariableNode::evaluate
 * * @return Value The value being assigned (allows for chained assignments like a = b = 1).
 */

Value AssignmentNode::evaluate(SymbolContainer& env, uint32_t currentGroupId) const {
    static uint32_t globalId = getGlobalId();
    uint32_t targetGroupId = (scopeGroupId != 0) ? scopeGroupId : currentGroupId;
    
    if (targetGroupId != globalId && env[targetGroupId].find(identifierId) == env[targetGroupId].end()) {
        if (env[globalId].find(identifierId) != env[globalId].end()) {
            targetGroupId = globalId;
        }
    }

    auto& table = env[targetGroupId];
    auto it_existing = table.find(identifierId);

    if (this->isReference) {
        if (rhs->type() != NodeType::VARIABLE) {
            throw std::runtime_error("Referencer Error: Must bind to a variable (L-Value) [ line " + std::to_string(lineNumber) + " ]");
        }
        
        auto* varNode = static_cast<VariableNode*>(rhs.get());
        Value* sourcePtr = env.getInternalPointer(targetGroupId, varNode->getNameId());
        env.markUsed(varNode->getNameId());
        
        if (!sourcePtr) throw std::runtime_error("Reference Error: Target variable '" + varNode->getOriginalName() + "' not found.");

        Value refValue(sourcePtr);
        if (isConstant) refValue.setReadOnly();
        
        table[identifierId] = refValue;
        return refValue;
    }

    Value val = rhs->evaluate(env, currentGroupId);

    if (Vyne::isTypeStrict() && it_existing != table.end()) {
        int existingType = it_existing->second.getType();
        int newType = val.getType();
        
        if (existingType != Value::NONE && existingType != newType) {
            throw std::runtime_error(
                "Type Error: Cannot assign " + val.getTypeName() + 
                " to '" + originalName + "' of type " + it_existing->second.getTypeName() +
                " in strict mode [ line " + std::to_string(lineNumber) + " ]"
            );
        }
    }

    if (it_existing != table.end() && it_existing->second.isReference()) {
        checkReadOnly(it_existing->second, originalName, lineNumber);
        Value* targetPtr = it_existing->second.getPointer();
        
        val = convertIfNeeded(val, targetPtr->getType(), lineNumber);
        env.markUsed(identifierId);
        
        if (targetPtr->getType() != Value::NONE && targetPtr->getType() != val.getType()) {
            throw std::runtime_error("Type Error: Reference target mismatch for '" + originalName + "'");
        }

        *targetPtr = val;
        return *targetPtr;
    }

    if (expectedType != VType::Unknown) {
        val = convertIfNeeded(val, static_cast<int>(expectedType), lineNumber);
        if (static_cast<int>(expectedType) != val.getType()) {
            throw std::runtime_error(
                "Type Error : Type mismatch for '" + originalName + "': expected " + 
                VTypeToString(expectedType) + " but got " + val.getTypeName() + 
                " [line " + std::to_string(lineNumber) + "]"
            );
        }
    }

    if (it_existing != table.end()) {
        checkReadOnly(it_existing->second, originalName, lineNumber);
        val = convertIfNeeded(val, it_existing->second.getType(), lineNumber);
        env.markUsed(identifierId);
        
        if (it_existing->second.getType() != Value::NONE && 
            it_existing->second.getType() != val.getType()) {
            
            throw std::runtime_error(
                "Type Error: Cannot assign " + val.getTypeName() + " value to '" + originalName + 
                "', which expects " + it_existing->second.getTypeName() + 
                " [ line " + std::to_string(lineNumber) + " ]"
            );
        }
    }

    if (indexExpr) {
        if (it_existing == table.end()) 
            throw std::runtime_error("Runtime Error: Array '" + originalName + "' not found");

        Value& arrayVal = it_existing->second;
        auto& vec = getArrayRef(arrayVal, originalName, lineNumber);
        Value idxValue = indexExpr->evaluate(env, currentGroupId);
        size_t idx = validateIndex(idxValue, vec.size(), lineNumber);
        
        vec[idx] = val;
        return val;
    }

    if (isConstant) val.setReadOnly();
    if (Vyne::getMemoryLimitEnabled()) Vyne::checkMemoryUsage();

    table[identifierId] = val;
    return val;
}

Value GroupNode::evaluate(SymbolContainer& env, uint32_t currentGroupId) const {
    static uint32_t globalId = getGlobalId();

    uint32_t effectiveParentId = (targetModuleId != 0) ? targetModuleId : currentGroupId;

    uint32_t fullNameId;
    std::string fullName;

    if (effectiveParentId == globalId) {
        fullNameId = groupNameId;
        fullName = groupName;
    } else {
        std::string parentName = StringPool::instance().get(effectiveParentId);
        fullName = parentName + "." + groupName;
        fullNameId = StringPool::instance().intern(fullName);
    }

    env[effectiveParentId][groupNameId] = Value(fullNameId, fullName, true);

    if (!env.contains(fullNameId)) {
        env[fullNameId] = SymbolTable();
    }

    for (const auto& stmt : statements) {
        stmt->evaluate(env, fullNameId);
    }
    return Value();
}

/**
 * @brief Evaluates binary operations between two AST nodes.
 * * This method implements the core logic for binary operators, including arithmetic,
 * comparisons, logical short-circuiting, and type-specific operations (like string 
 * concatenation or array merging).
 * * @param env The SymbolContainer providing access to the current variable environment.
 * @param currentGroup The string identifier for the current scope/group (defaults to "global").
 * * @details 
 * ### Execution Flow:
 * 1. **Short-Circuit Logic**: For `AND` and `OR`, the right-hand side is only evaluated if 
 * the left-hand side does not determine the final result.
 * 2. **String Concatenation**: If the operator is `+` and either operand is a string, 
 * both are converted to strings and concatenated.
 * 3. **Array Merging**: If both operands are arrays and the operator is `+`, a new array 
 * is returned containing elements of both.
 * 4. **Numeric Operations**: Standard arithmetic and comparison operations for doubles.
 * * 
 * * ### Supported Operators:
 * | Category   | Tokens |
 * | :---       | :---   |
 * | **Logical**| `And`, `Or` |
 * | **Arithmetic** | `Add`, `Substract`, `Multiply`, `Division`, `Floor_Divide`, `Modulo` |
 * | **Comparison** | `Smaller`, `Smaller_Or_Equal`, `Greater`, `Greater_Or_Equal`, `Double_Equals` |
 * * @return Value The result of the binary operation.
 * * @throw std::runtime_error Thrown in the following scenarios:
 * - Division or Modulo by zero.
 * - Type mismatch (e.g., trying to subtract a string from a number).
 * - Unsupported operator for the given operand types.
 * * @note 
 * - **Short-circuiting**: `And` and `Or` operators do not evaluate the right-hand side 
 * if the result is determined by the left-hand side.
 * - **String Promotion**: The `+` operator favors string concatenation if any operand 
 * is a string, effectively "promoting" the other operand to a string type.
 * - **Numeric Precision**: All numeric operations are performed using `double` 
 * precision; comparison operations return `1.0` for true and `0.0` for false to 
 * maintain type consistency within the `Value` system.
 */

Value BinOpNode::evaluate(SymbolContainer& env, uint32_t currentGroupId) const {
    const Value& l = leftNode->evaluate(env, currentGroupId);

    if (op == VTokenType::And) {
        if (!l.isTruthy()) return Value(static_cast<int64_t>(0));
        return Value(static_cast<int64_t>(rightNode->evaluate(env, currentGroupId).isTruthy() ? 1 : 0));
    }

    if (op == VTokenType::Or) {
        if (l.isTruthy()) return Value(static_cast<int64_t>(1));
        return Value(static_cast<int64_t>(rightNode->evaluate(env, currentGroupId).isTruthy() ? 1 : 0));
    }

    const Value& r = rightNode->evaluate(env, currentGroupId);
    int lType = l.getType();
    int rType = r.getType();

    if ((op == VTokenType::Add) && (l.getType() == Value::STRING && r.getType() == Value::STRING)) {
        return Value(l.toString() + r.toString()); 
    }

    if (l.getType() == Value::ARRAY && r.getType() == Value::ARRAY) {
        if (op == VTokenType::Add) {
            Value result = l;
            auto& resList = result.asList();
            auto& rList = r.asList();
            resList.insert(resList.end(), rList.begin(), rList.end());
            return result;
        }
    }

    bool isFloatMath = (lType == Value::FLOAT64 || rType == Value::FLOAT64);

    if (isFloatMath) {
        double lv = l.asFloat();
        double rv = r.asFloat();
        switch (op) {
            case VTokenType::Add:      return Value(lv + rv);
            case VTokenType::Substract: return Value(lv - rv);
            case VTokenType::Multiply:  return Value(lv * rv);
            case VTokenType::Division:
                if (rv == 0) throw std::runtime_error("Division by zero!");
                return Value(lv / rv);
            case VTokenType::Smaller:   return Value(lv < rv);
            case VTokenType::Double_Equals: return Value(l == r);
            case VTokenType::Smaller_Or_Equal: return Value(lv <= rv);
            case VTokenType::Greater:       return Value(lv > rv);
            case VTokenType::Greater_Or_Equal: return Value(lv >= rv);
            case VTokenType::Modulo:
                if (rv == 0) throw std::runtime_error("Modulo by zero!");
                return Value(std::fmod(lv, rv));
            default: break;
        }
    } else if (lType == Value::INT64 && rType == Value::INT64) {
        int64_t lv = l.asInt();
        int64_t rv = r.asInt();
        switch (op) {
            case VTokenType::Add:       return Value(lv + rv);
            case VTokenType::Substract: return Value(lv - rv);
            case VTokenType::Multiply:  return Value(lv * rv);
            case VTokenType::Division:
                if (rv == 0) throw std::runtime_error("Division by zero!");
                return Value(lv / rv); 
            case VTokenType::Modulo:
                if (rv == 0) throw std::runtime_error("Modulo by zero!");
                return Value(static_cast<int64_t>(lv % rv));
            case VTokenType::Power : return Value(std::pow(lv, rv));
            case VTokenType::Smaller:   return Value(lv < rv);
            case VTokenType::Double_Equals: return Value(lv == rv);
            case VTokenType::Smaller_Or_Equal: return Value(lv <= rv);
            case VTokenType::Greater:       return Value(lv > rv);
            case VTokenType::Greater_Or_Equal: return Value(lv >= rv);
            default: break;
        }
    }

    if (op == VTokenType::Double_Equals) return Value(l == r);
    if (op == VTokenType::Not_Equal) return Value(l != r);

    throw std::runtime_error("Type Error: Invalid operation " + VTokenTypeToString(op) + " between " + l.getTypeName() + " and " + r.getTypeName() + "[ " + std::to_string(lineNumber) + " ]");
}

Value PostFixNode::evaluate(SymbolContainer& env, uint32_t currentGroupId) const {
    Value oldValue = left->evaluate(env, currentGroupId);
    Value newVal;

    if (oldValue.getType() == Value::INT64) {
        int64_t raw = oldValue.asInt();
        newVal = Value(op == VTokenType::Double_Increment ? raw + 1 : raw - 1);
    } else if (oldValue.getType() == Value::FLOAT64) {
        double raw = oldValue.asFloat();
        newVal = Value(op == VTokenType::Double_Increment ? raw + 1.0 : raw - 1.0);
    } else {
        throw std::runtime_error("Type Error: Cannot increment/decrement non-numeric type [ line " + std::to_string(lineNumber) + " ]");
    }

    if (left->type() == NodeType::MEMBER_ACCESS) [[likely]] {
        auto* memNode = static_cast<MemberAccessNode*>(left.get());
        
        uint32_t memberId = memNode->getMemberId();
        
        Value receiverVal = memNode->getReceiver()->evaluate(env, currentGroupId);

        if (receiverVal.getType() == Value::STRUCT) {
            auto structPtr = receiverVal.asStruct();
            structPtr->fields[memberId] = newVal;
        } else {
            // Əgər bu bir modul və ya statik qrupdursa
            uint32_t receiverId = memNode->getReceiverPathId();
            Value* internalVal = env.getInternalPointer(receiverId, memberId);
            *internalVal = newVal;
        }
    }
    else if (left->type() == NodeType::VARIABLE) [[likely]] {
        auto* varNode = static_cast<VariableNode*>(left.get());
        
        uint32_t scopeId = currentGroupId; 
        
        if (varNode->getScope().empty() == false) {
            scopeId = resolvePathId(varNode->getScope(), currentGroupId);
        }

        Value* internalVal = env.getInternalPointer(scopeId, varNode->getNameId());
        
        if (!internalVal) {
            throw std::runtime_error("Runtime Error: Cannot assign to undefined variable '" + 
                                    varNode->getOriginalName() + "'");
        }
        
        *internalVal = newVal;
    }
    else {
        throw std::runtime_error("Type Error: L-value required as increment operand [ line " + std::to_string(lineNumber) + " ]");
    }

    return oldValue; 
}

Value UnaryNode::evaluate(SymbolContainer& env, uint32_t currentGroupId) const {
    Value val = right->evaluate(env, currentGroupId);

    switch(op){
        case VTokenType::Exclamatory : 
            return Value(!val.isTruthy());
        
        case VTokenType::Substract : 
            if (val.getType() == Value::INT64) return Value(-val.asInt());
            return Value(-val.asFloat());
        
        case VTokenType::Addresser : {
            auto varNode = dynamic_cast<VariableNode*>(right.get());
            if (!varNode) throw std::runtime_error("Cannot take address of a non-variable.");

            uint32_t scopeId = currentGroupId;
            Value* internalPtr = env.getInternalPointer(scopeId, varNode->getNameId()); 
            
            return Value(reinterpret_cast<int64_t>(internalPtr));
        }
        default: return Value();
    }
}

Value ArrayNode::evaluate(SymbolContainer& env, uint32_t currentGroupId) const {
    std::vector<Value> results;
    for (const auto& node : elements) results.emplace_back(node->evaluate(env, currentGroupId));

    if (Vyne::getMemoryLimitEnabled()) Vyne::checkMemoryUsage();

    return Value(std::move(results));
}

Value RangeNode::evaluate(SymbolContainer& env, uint32_t currentGroupId) const {
    Value startVal = left->evaluate(env, currentGroupId);
    Value endVal = right->evaluate(env, currentGroupId);
    
    std::vector<Value> rangeArray;

    if (startVal.getType() == Value::INT64 && endVal.getType() == Value::INT64) {
        int64_t start = startVal.asInt();
        int64_t end = endVal.asInt();
        for (int64_t i = start; i <= end; ++i) rangeArray.emplace_back(i);
    } else {
        double start = startVal.asFloat();
        double end = endVal.asFloat();
        for (double i = start; i <= end; ++i) rangeArray.emplace_back(i);
    }
    
    return Value(rangeArray);
}

Value BuiltInCallNode::evaluate(SymbolContainer& env, uint32_t currentGroupId) const {
    std::vector<Value> argValues;

    for (auto& arg : arguments) {
        if (arg->type() == NodeType::VARIABLE) {
            auto* varNode = static_cast<VariableNode*>(arg.get());
            env.markUsed(varNode->getNameId());
        }
    }
    
    for(auto& arg : arguments){
        argValues.emplace_back(arg->evaluate(env, currentGroupId));
    }

    switch (builtInType) {
        case BuiltInType::PRINT:
            if (!argValues.empty()) { 
                argValues[0].print(); 
                std::cout << "\n"; 
            }
            
            return Value();
            
        case BuiltInType::EXIT:
            if (!argValues.empty()) { 
                throw std::runtime_error(argValues[0].asString()); 
            }
            return Value();
            
        case BuiltInType::TYPE:
            return Value(argValues[0].getTypeName());
            
        case BuiltInType::STRING:
            if (argValues.size() != 1) 
                throw std::runtime_error("Argument Error: string() expects 1 argument...");
            return Value(argValues[0].toString());
            
        case BuiltInType::INT64:
            return handleInt64(argValues);
            
        case BuiltInType::FLOAT64:
            return handleFloat64(argValues);
            
        case BuiltInType::SIZEOF:
            if (argValues.size() != 1)
                throw std::runtime_error("Argument Error: sizeof() expects 1 arg.");
            return Value(static_cast<int64_t>(argValues[0].getShallowBytes()));
            
        case BuiltInType::SEQUENCE:
            return handleSequence(argValues);
            
        default:
            throw std::runtime_error("Syntax Error : Unknown built-in function [ line " + std::to_string(lineNumber) + " ]");
    }
}

Value IndexAccessNode::evaluate(SymbolContainer& env, uint32_t currentGroupId) const {
    Value baseVal = base->evaluate(env, currentGroupId);
    Value idxVal  = index->evaluate(env, currentGroupId);
    
    if (baseVal.getType() == Value::ARRAY) {
        auto& vec = baseVal.asList();
        size_t idx = validateIndex(idxVal, vec.size(), lineNumber);
        return vec[idx];
    }
    
    else if (baseVal.getType() == Value::STRING) {
        const std::string& str = baseVal.asString();
        size_t idx = validateIndex(idxVal, str.length(), lineNumber);
        return Value(std::string(1, str[idx]));
    }
    
    throw std::runtime_error("Type Error: Cannot index non-array, non-string type [ line " + std::to_string(lineNumber) + " ]");
}

Value IndexAssignmentNode::evaluate(SymbolContainer& env, uint32_t currentGroupId) const {
    Value baseVal = base->evaluate(env, currentGroupId);
    Value idxVal = index->evaluate(env, currentGroupId);
    Value val = rhs->evaluate(env, currentGroupId);
    
    if (baseVal.getType() == Value::ARRAY) {
        auto& vec = baseVal.asList();
        size_t idx = validateIndex(idxVal, vec.size(), lineNumber);
        
        checkReadOnly(baseVal, arrayName, lineNumber);
        
        vec[idx] = val;
        return val;
    }
    
    else if (baseVal.getType() == Value::STRING) {
        throw std::runtime_error("Runtime Error: Strings are immutable...");
    }
    
    throw std::runtime_error("Type Error: Cannot assign to index of non-array type...");
}

Value FunctionNode::evaluate(SymbolContainer& env, uint32_t currentGroupId) const {
    Value funcValue(
        parameters, 
        body,
        VTypeToString(returnType)
    );

    uint32_t destinationId = (targetModuleId != 0) ? targetModuleId : currentGroupId;

    env[destinationId][funcNameId] = funcValue;

    return funcValue;
}

Value FunctionCallNode::evaluate(SymbolContainer& env, uint32_t currentGroupId) const {
    static uint32_t globalId = getGlobalId();

    if (!env.contains(targetGroupId)) {
        std::string groupName = StringPool::instance().get(targetGroupId);
        throw std::runtime_error("Runtime Error: Group '" + groupName + 
            "' not found for call '" + originalName + "' [ line " + std::to_string(lineNumber) + " ]");
    }

    auto& groupMap = env[targetGroupId];
    auto it = groupMap.find(targetNameId);

    if (it == groupMap.end()) {
        if (!isNamespaced) { 
            auto& globalMap = env[globalId];
            it = globalMap.find(targetNameId);
        }

        if (it == (isNamespaced ? groupMap.end() : env[globalId].end())) {
            throw std::runtime_error("Runtime Error: '" + originalName + 
                "' is not defined [ line " + std::to_string(lineNumber) + " ]");
        }
    }

    Value funcVal = it->second;

    if (funcVal.getType() != Value::FUNCTION) {
        throw std::runtime_error("Type Error: '" + originalName + 
            "' is not a callable function or constructor [ line " + std::to_string(lineNumber) + " ]");
    }

    auto funcData = funcVal.asFunction();
    std::vector<Value> evaluatedArgs;
    evaluatedArgs.reserve(arguments.size());
    
    for (const auto& arg : arguments) {
        if (arg) evaluatedArgs.emplace_back(arg->evaluate(env, currentGroupId));
    }

    std::string currentGroupNameStr = StringPool::instance().get(currentGroupId);
    CallContext ctx{env, currentGroupNameStr, lineNumber, originalName};

    if (funcData->isNative) {
        if (funcData->arity != -1) {
            checkArgumentCount(funcData->arity, evaluatedArgs.size(), ctx);
        }
        return funcData->nativeFn(evaluatedArgs);
    }

    checkArgumentCount(funcData->params.size(), evaluatedArgs.size(), ctx);

    std::string localScopeName = createLocalScope("call", originalName);
    ScopedEnvironment scope(env, localScopeName, currentGroupId);
    uint32_t localScopeId = scope.getScopeId();

    if (Vyne::isTypeStrict()) {
        for (size_t i = 0; i < funcData->params.size(); ++i) {
            const auto& param = funcData->params[i];
            
            if (param.type != VType::Unknown) {
                int expectedType = static_cast<int>(param.type);
                int actualType = evaluatedArgs[i].getType();
                
                if (expectedType != actualType) {
                    throw std::runtime_error(
                        "Type Error: Parameter '" + param.name + "' expects " +
                        VTypeToString(param.type) + ", but got " + evaluatedArgs[i].getTypeName() +
                        " in strict mode [ line " + std::to_string(lineNumber) + " ]"
                    );
                }
            }
        }
    }

    for (size_t i = 0; i < funcData->params.size(); ++i) {
        const auto& param = funcData->params[i];
        
        if (param.isReference) {
            Value* sourcePtr = nullptr;
            
            if (arguments[i]->type() == NodeType::VARIABLE) {
                auto* varNode = static_cast<VariableNode*>(arguments[i].get());
                sourcePtr = env.getInternalPointer(currentGroupId, varNode->getNameId());
                env.markUsed(varNode->getNameId());
            }
            else if (arguments[i]->type() == NodeType::MEMBER_ACCESS) {
                auto* memNode = static_cast<MemberAccessNode*>(arguments[i].get());
                Value memberVal = memNode->evaluate(env, currentGroupId);
                if (memberVal.isReference()) {
                    sourcePtr = memberVal.getPointer();
                }
            }
            
            if (!sourcePtr) {
                throw std::runtime_error("Reference Error: Cannot bind to non-variable for reference parameter '" + 
                                        param.name + "' [ line " + std::to_string(lineNumber) + " ]");
            }
            
            Value refValue(sourcePtr);
            scope.bind(param.id, refValue);
        }
        else {
            if (evaluatedArgs[i].getType() == Value::ARRAY) {
                scope.bind(param.id, deepCopyValue(evaluatedArgs[i]));
            } else {
                scope.bind(param.id, evaluatedArgs[i]);
            }
        }
    }

    return executeFunction(funcData, evaluatedArgs, env, localScopeId, lineNumber);
}

Value ReturnNode::evaluate(SymbolContainer& env, uint32_t currentGroupId) const {
    throw ReturnException{expression->evaluate(env, currentGroupId)};
}

/**
 * @brief Dispatches and executes method calls on a receiver object.
 * * @details This method serves as the central hub for "Dot Notation" syntax. 
 * It supports two primary execution paths:
 * * 1. **Module Calls:** When the receiver evaluates to a MODULE, it searches 
 * the module's namespace for a matching FUNCTION (Native or Vyne-defined).
 * * 2. **Built-in Array Methods:** When the receiver is an ARRAY, it provides 
 * access to the built-in standard library, including:
 * - `size()`: Returns element count.
 * - `push(val)`: Appends elements to the array.
 * - `pop()`: Removes the last element.
 * - `delete(val)`: Erases a specific value.
 * - `sort()`, `reverse()`, `clear()`, `place_all(val, count)`.
 * * @note Array methods require the receiver to be a named variable (L-Value) 
 * to allow for in-place modification.
 * * @param env The current SymbolContainer holding global and scoped variables.
 * @param currentGroup The active namespace/group context of the caller.
 * * @throw std::runtime_error If the method is unknown, the module is missing, 
 * or if type/argument constraints are violated.
 * * @return Value The result of the function execution or the modified receiver object.
 */

Value MethodCallNode::evaluate(SymbolContainer& env, uint32_t currentGroupId) const {
    if (receiver->type() == NodeType::VARIABLE) {
        auto* varNode = static_cast<VariableNode*>(receiver.get());
        env.markUsed(varNode->getNameId());
    }

    for (auto& arg : arguments) {
        if (arg->type() == NodeType::VARIABLE) {
            auto* varNode = static_cast<VariableNode*>(arg.get());
            env.markUsed(varNode->getNameId());
        }
    }

    Value receiverVal = receiver->evaluate(env, currentGroupId);

    static const uint32_t lengthId   = StringPool::intern("length");
    static const uint32_t replaceId  = StringPool::intern("replace");
    static const uint32_t sizeId     = StringPool::intern("size");
    static const uint32_t pushId     = StringPool::intern("push");
    static const uint32_t popId      = StringPool::intern("pop");
    static const uint32_t popFrontId = StringPool::intern("pop_front");
    static const uint32_t backId     = StringPool::intern("back");
    static const uint32_t deleteId   = StringPool::intern("delete");
    static const uint32_t sortId     = StringPool::intern("sort");
    static const uint32_t reverseId  = StringPool::intern("reverse");
    static const uint32_t clearId    = StringPool::intern("clear");
    static const uint32_t placeAllId = StringPool::intern("place_all");
    static const uint32_t fieldsId   = StringPool::intern("fields");

    if (receiverVal.getType() == Value::STRING) {
        std::string str = receiverVal.asString();

        if (methodId == replaceId) {
            if (arguments.size() < 2) throw std::runtime_error("replace() expects 2 arguments");
            
            std::string oldS = arguments[0]->evaluate(env, currentGroupId).asString();
            std::string newS = arguments[1]->evaluate(env, currentGroupId).asString();

            // C++ daxilində string əvəzləmə məntiqi
            size_t pos = 0;
            while ((pos = str.find(oldS, pos)) != std::string::npos) {
                str.replace(pos, oldS.length(), newS);
                pos += newS.length();
            }
            return Value(str);
        }
    }

    if (receiverVal.getType() == Value::MODULE) {
        uint32_t modId;
        std::string modName;

        if (receiverVal.isObject()) {
            auto mod = static_cast<ModuleData*>(receiverVal.data.obj.get());
            modName = mod->name;
            modId = StringPool::instance().intern(modName);
        } else {
            modName = receiverVal.asString();
            modId = StringPool::instance().intern(modName);
        }

        if (env.contains(modId) && env[modId].count(methodId)) {
            Value& funcVal = env[modId][methodId];

            if (funcVal.getType() == Value::FUNCTION) {
                auto func = funcVal.asFunction();

                if (arguments.empty() && func->isNative) {
                    static std::vector<Value> emptyArgs; 
                    return func->nativeFn(emptyArgs);
                }

            std::vector<Value> argValues;
            argValues.reserve(arguments.size());
            for (auto& arg : arguments) {
                argValues.emplace_back(arg->evaluate(env, currentGroupId));
            }

            if (func->isNative) {
                return func->nativeFn(argValues); 
            }

            std::string localScopeName = createLocalScope("modcall", methodName);
            ScopedEnvironment scope(env, localScopeName, currentGroupId);
            
            for (size_t i = 0; i < func->params.size() && i < argValues.size(); ++i) {
                const Parameter& param = func->params[i];
                Value& providedArg = argValues[i];

                if (param.type != VType::Unknown) {
                    if (static_cast<VType>(providedArg.getType()) != param.type) {
                        throw std::runtime_error(
                            "Type Mismatch: Parameter '" + param.name + 
                            "' expects " + VTypeToString(param.type) + 
                            " but got " + providedArg.getTypeName() + 
                            " at line " + std::to_string(lineNumber)
                        );
                    }
                }

                scope.bind(param.id, providedArg);
            }

            return executeFunction(func, argValues, env, scope.getScopeId(), lineNumber);
        }
    }
    
    throw std::runtime_error("Module Error: Method '" + methodName + "' not found in module " + modName + " [ line " + std::to_string(lineNumber) + " ]");
}

    // --- ARRAY METHODS ---
    // TODO USE STRING_VIEW TO COMPARE INPUT METHOD WITH AVAILABLE METHOD OPTIONS
    if (receiverVal.getType() == Value::ARRAY || receiverVal.getType() == Value::STRING) {
        if (methodName == "length" || methodName == "size") {
            if (receiverVal.getType() == Value::STRING) {
                return Value(static_cast<uint64_t>(receiverVal.asString().size()));
            } else {
                return Value(static_cast<uint64_t>(receiverVal.asList().size()));
            }
        }

        Value* target = nullptr;

        if (receiver->type() == NodeType::VARIABLE) {
            auto* var = static_cast<VariableNode*>(receiver.get());
            static uint32_t globalId = getGlobalId();
            
            uint32_t targetGroupId = resolvePathId(var->getScope(), currentGroupId);
            uint32_t varNameId = var->getNameId();

            if (env.count(targetGroupId) && env[targetGroupId].count(varNameId)) {
                target = &env[targetGroupId][varNameId];
                env.markUsed(varNameId);
            } 
            else if (targetGroupId != globalId && env.count(globalId) && env[globalId].count(varNameId)) {
                target = &env[globalId][varNameId];
                env.markUsed(varNameId);
            }
        }

        if (!target && (methodId == pushId || methodId == popId || methodId == clearId)) {
            throw std::runtime_error("Runtime Error: Cannot call mutating method '" + methodName + "' on anonymous array [ line " + std::to_string(lineNumber) + " ]");
        }

        auto& vec = target ? target->asList() : receiverVal.asList();

        if (methodId == pushId) {
            Value* actualTarget = nullptr;
            
            if (target->getType() == Value::ARRAY) {
                actualTarget = target;
            }
            else if (target->isReference() && target->getPointer()->getType() == Value::ARRAY) {
                actualTarget = target->getPointer();
            }
            else {
                throw std::runtime_error("Type Error: Called method push() on non-array [ line " + 
                                        std::to_string(lineNumber) + " ]");
            }
            
            if (receiver->type() == NodeType::VARIABLE) {
                auto* var = static_cast<VariableNode*>(receiver.get());
                env.markUsed(var->getNameId());
            }
            
            for(auto& arg : arguments){
                Value val = arg->evaluate(env, currentGroupId);
                actualTarget->asList().emplace_back(val);
            }

            if (Vyne::getMemoryLimitEnabled()) Vyne::checkMemoryUsage();
            
            return receiverVal;
        }

        if (methodId == popId) {
            if (target->getType() != Value::ARRAY) 
                throw std::runtime_error("Type Error : Called method pop() on non-array [ line " + std::to_string(lineNumber) + " ]");
            
            auto& vec = target->asList(); // Referans alırıq ki, kod qısa olsun
            
            if (vec.empty()) 
                throw std::runtime_error("Index Error: pop() from empty array [ line " + std::to_string(lineNumber) + " ]");
            
            if (!arguments.empty()) 
                throw std::runtime_error("Argument Error: pop() expects 0 arguments [ line " + std::to_string(lineNumber) + " ]");

            Value poppedValue = vec.back();
            
            vec.pop_back();
            
            return poppedValue;
        }

        if (methodId == popFrontId) {
            if (target->getType() != Value::ARRAY) 
                throw std::runtime_error("Type Error: Called method pop_front() on non-array [ line " + std::to_string(lineNumber) + " ]");
            
            auto& vec = target->asList();
            
            if (vec.empty()) 
                throw std::runtime_error("Index Error: pop_front() from empty array [ line " + std::to_string(lineNumber) + " ]");
            
            if (!arguments.empty()) 
                throw std::runtime_error("Argument Error: pop_front() expects 0 arguments [ line " + std::to_string(lineNumber) + " ]");

            Value frontValue = vec.front();
            vec.erase(vec.begin()); // Erase first element from std::vector<Value>
            
            return frontValue;
        }

        if (methodId == backId) {
            if (target->getType() != Value::ARRAY) throw std::runtime_error("Type Error : Called method back() on non-array [ line " + std::to_string(lineNumber) + " ]");
            if (target->asList().empty()) throw std::runtime_error("Index Error: back() from empty array [ line " + std::to_string(lineNumber) + " ]");
            if (!arguments.empty()) throw std::runtime_error("Argument Error: back() expects 0 arguments, but got " + std::to_string(arguments.size()) + " [ line " + std::to_string(lineNumber) + " ]");

            return Value(target->asList().back());
        }

        if (methodId == deleteId) {
            if (target->getType() != Value::ARRAY) throw std::runtime_error("Type Error : Called method delete() on non-array [ line " + std::to_string(lineNumber) + " ]");
            if (arguments.size() != 1) throw std::runtime_error("Argument Error: delete() expects exactly 1 argument, but got " + std::to_string(arguments.size()) + " instead [ line " + std::to_string(lineNumber) + " ]");

            Value val = arguments[0]->evaluate(env, currentGroupId);
            auto it = std::find(target->asList().begin(), target->asList().end(), val);
            if (it == std::end(target->asList())) throw std::runtime_error("Value error : Could not find given value in array! [ line " + std::to_string(lineNumber) + " ]");
            target->asList().erase(it);
            return Value(true);
        }

        if (methodId == sortId) {
            if (target->getType() != Value::ARRAY) throw std::runtime_error("Type Error: sort() called on non-array [ line " + std::to_string(lineNumber) + " ]");
            if (!arguments.empty()) throw std::runtime_error("Argument Error: sort() expects 0 arguments [ line " + std::to_string(lineNumber) + " ]");
            for (auto& el : target->asList()) {
                if (el.getType() != Value::INT64 && el.getType() != Value::FLOAT64) throw std::runtime_error("Value Error: Cannot sort non-numeric values [ line " + std::to_string(lineNumber) + " ]");
            }
            std::sort(target->asList().begin(), target->asList().end());
            return Value(*target); 
        }

        if (methodId == placeAllId) {
            if (target->getType() != Value::ARRAY) 
                throw std::runtime_error("Type Error: place_all() called on non-array [ line " + std::to_string(lineNumber) + " ]");

            if (arguments.size() < 2)
                throw std::runtime_error("Argument Error: place_all(element, count) expects 2 arguments [ line " + std::to_string(lineNumber) + " ]");

            Value element = arguments[0]->evaluate(env, currentGroupId);
            Value countVal = arguments[1]->evaluate(env, currentGroupId);
            
            int64_t count = countVal.asInt();
            if (count < 0) {
                throw std::runtime_error("Runtime Error: Cannot resize array to a negative count [ line " + std::to_string(lineNumber) + " ]");
            }

            auto& targetVec = target->asList();
            
            targetVec.clear();
            targetVec.reserve(static_cast<size_t>(count));
            
            for (int64_t i = 0; i < count; i++) {
                targetVec.emplace_back(element);
            }

            return *target; 
        }

        if (methodId == reverseId) {
            if (target->getType() != Value::ARRAY) throw std::runtime_error("Type Error: reverse() called on non-array [ line " + std::to_string(lineNumber) + " ]");
            if (arguments.size() > 0) throw std::runtime_error("Argument Error: reverse() expects 0 arguments, but got " + std::to_string(arguments.size()) + " instead [ line " + std::to_string(lineNumber) + " ]");
            std::reverse(target->asList().begin(), target->asList().end());
            return Value(*target);
        }

        if (methodId == clearId) {
            if (target->getType() != Value::ARRAY) throw std::runtime_error("Type Error: clear() called on non-array [ line " + std::to_string(lineNumber) + " ]");
            if (arguments.size() > 0) throw std::runtime_error("Argument Error: clear() expects 0 arguments, but got " + std::to_string(arguments.size()) + " instead [ line " + std::to_string(lineNumber) + " ]");
            target->asList().clear();
            return Value(*target);
        }
    }

    if (receiverVal.getType() == Value::STRUCT) {
        if (methodId == fieldsId) {
            auto baseObj = receiverVal.asStruct();
            auto structPtr = std::static_pointer_cast<VyneStruct>(baseObj);
            
            std::vector<Value> fieldNames;
            for (auto const& [id, val] : structPtr->fields) {
                fieldNames.emplace_back(StringPool::instance().get(id));
            }
            
            return Value(fieldNames);
        }
        
        auto structPtr = receiverVal.asStruct();
        std::string currentGroupNameStr = StringPool::instance().get(currentGroupId);
        CallContext ctx{env, currentGroupNameStr, lineNumber, methodName};

        auto methodNodeIt = structPtr->methodNodes.find(methodId);
        if (methodNodeIt != structPtr->methodNodes.end()) {
            FunctionNode* funcNode = methodNodeIt->second;

            std::vector<Value> argValues;
            argValues.reserve(arguments.size());
            for (auto& arg : arguments) {
                argValues.emplace_back(arg->evaluate(env, currentGroupId));
            }

            // Step 2: Validate the call using the evaluated arguments
            if (arguments.size() != funcNode->getParameters().size()) {
                throw std::runtime_error(
                    "Type Error: Method '" + methodName + 
                    "' expects " + std::to_string(funcNode->getParameters().size()) + 
                    " argument(s), but got " + std::to_string(arguments.size()) + 
                    " [ line " + std::to_string(lineNumber) + " ]"
                );
            }
            
            for (size_t i = 0; i < argValues.size(); ++i) {
                VType expectedType = funcNode->getParameters()[i].type;
                
                if (expectedType != VType::Unknown) {
                    int argType = argValues[i].getType();
                    bool typeMatch = false;
                    
                    if (expectedType == VType::Float64 && argType == Value::INT64) {
                        typeMatch = true;
                    }
                    else if (static_cast<int>(expectedType) == argType) {
                        typeMatch = true;
                    }
                    
                    if (!typeMatch) {
                        std::string expectedStr = VTypeToString(expectedType);
                        throw std::runtime_error(
                            "Type Error: Argument " + std::to_string(i+1) + 
                            " of method '" + methodName + "' expects " + expectedStr + 
                            ", but got " + argValues[i].getTypeName() + 
                            " [ line " + std::to_string(lineNumber) + " ]"
                        );
                    }
                }
            }

            Value funcVal = funcNode->evaluate(env, currentGroupId);
            structPtr->methods[methodId] = funcVal;
            
            auto funcData = funcVal.asFunction();
            
            std::vector<Value> allArgs = { receiverVal };
            allArgs.insert(allArgs.end(), argValues.begin(), argValues.end());
            
            if (funcData->isNative) {
                if (funcData->arity != -1) {
                    checkArgumentCount(funcData->arity, allArgs.size() - 1, ctx);
                }
                return funcData->nativeFn(allArgs);
            }
            
            checkArgumentCount(funcData->params.size(), allArgs.size() - 1, ctx);
            
            std::string localScope = createLocalScope("method", methodName);
            ScopedEnvironment scope(env, localScope, currentGroupId);
            static uint32_t selfId = StringPool::instance().intern("self");

            scope.bind(selfId, receiverVal);
            
            for (size_t i = 1; i < allArgs.size(); ++i) {
                if (i-1 < funcData->params.size()) {
                    scope.bind(funcData->params[i-1].id, allArgs[i]);
                }
            }
            
            Value result = executeFunction(funcData, allArgs, env, scope.getScopeId(), lineNumber);
            
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
            
            return result;
        }
        
        auto methodIt = structPtr->methods.find(methodId);
        if (methodIt != structPtr->methods.end()) {
            Value funcVal = methodIt->second;
            checkCallableType(funcVal, ctx);
            
            auto funcData = funcVal.asFunction();
            
            std::vector<Value> argValues;
            argValues.reserve(arguments.size());
            for (auto& arg : arguments) {
                argValues.emplace_back(arg->evaluate(env, currentGroupId));
            }
            
            std::vector<Value> allArgs = { receiverVal };
            allArgs.insert(allArgs.end(), argValues.begin(), argValues.end());
            
            if (funcData->isNative) {
                if (funcData->arity != -1) {
                    checkArgumentCount(funcData->arity, allArgs.size() - 1, ctx);
                }
                return funcData->nativeFn(allArgs);
            }
            
            checkArgumentCount(funcData->params.size(), allArgs.size() - 1, ctx);
            
            std::string localScopeName = createLocalScope("method", methodName);
            ScopedEnvironment scope(env, localScopeName, currentGroupId);

            static uint32_t selfId = StringPool::instance().intern("self");
            scope.bind(selfId, receiverVal);
            
            for (size_t i = 1; i < allArgs.size(); ++i) {
                if (i-1 < funcData->params.size()) {
                    scope.bind(funcData->params[i-1].id, allArgs[i]);
                }
            }
            
            return executeFunction(funcData, allArgs, env, scope.getScopeId(), lineNumber);
        }
        
        auto fieldIt = structPtr->fields.find(methodId);
        if (fieldIt != structPtr->fields.end()) {
            return fieldIt->second;
        }
        
        throw std::runtime_error("Runtime Error: Struct '" + structPtr->typeName + 
            "' has no member '" + methodName + "' [ line " + std::to_string(lineNumber) + " ]");
    }
    
    throw std::runtime_error("Unknown method: " + methodName + " [ line " + std::to_string(lineNumber) + " ]");
}

/**
 * @brief Executes a block of code repeatedly while a condition is truthy.
 * * This implementation supports:
 * - @b Break: Caught via BreakException to exit the loop.
 * - @b Continue: Caught via ContinueException to skip to the next iteration.
 * * */

Value WhileNode::evaluate(SymbolContainer& env, uint32_t currentGroupId) const {
    Value lastResult;
    
    while (condition->evaluate(env, currentGroupId).isTruthy()) {
        try {
            lastResult = body->evaluate(env, currentGroupId);
        } catch (const BreakException&) { break; }
        catch (const ContinueException&) { continue; }
    }
    return lastResult;
}

Value ForNode::evaluate(SymbolContainer& env, uint32_t currentGroupId) const {
    Value collection = iterable->evaluate(env, currentGroupId);
    if (collection.getType() != Value::ARRAY) {
        throw std::runtime_error("Runtime Error: 'through' requires a sequence or range...");
    }

    const auto& elements = collection.asList();
    auto& scope = env[currentGroupId];  // uses current group directly!!!!
    uint32_t itId = StringPool::instance().intern(iteratorName);

    Value savedIt;
    bool hadIt = (scope.find(itId) != scope.end());
    if (hadIt) savedIt = scope[itId];

    std::vector<Value> resultList;
    std::unordered_set<uint32_t> seenIds;
    Value lastVal;

    for (const auto& element : elements) {
        scope[itId] = element;  // direct assignment in current group ( for now )
        
        try {
            Value currentResult = body->evaluate(env, currentGroupId);

            switch(mode) {
                case ForMode::COLLECT: 
                    resultList.emplace_back(currentResult); 
                    if (Vyne::getMemoryLimitEnabled()) Vyne::checkMemoryUsage(); 
                    break;
                case ForMode::FILTER:
                    if (currentResult.isTruthy()) resultList.emplace_back(element);
                    break;
                case ForMode::LOOP: lastVal = currentResult; break;
                case ForMode::UNIQUE: {
                    uint32_t elementId = StringPool::instance().intern(element.toString());
                    if (seenIds.insert(elementId).second) {
                        resultList.emplace_back(element);
                    }
                    break;
                }
                default: lastVal = currentResult; break;
            }

        } catch (const BreakException&) { break; }
        catch (const ContinueException&) { continue; }
    }

    if (hadIt) scope[itId] = savedIt;
    else scope.erase(itId);

    if (mode == ForMode::COLLECT || mode == ForMode::FILTER || mode == ForMode::UNIQUE) {
        return Value(resultList);
    }
    return lastVal;
}

Value IfNode::evaluate(SymbolContainer& env, uint32_t currentGroupId) const {
    try{
        if(condition->evaluate(env, currentGroupId).isTruthy()){
            return body->evaluate(env, currentGroupId);
        } else if (elseBody) {
            return elseBody->evaluate(env, currentGroupId);
        }
    } catch (const BreakException& breakException){
        throw;
    }
    return Value();
}

Value InterfaceNode::evaluate(SymbolContainer& env, uint32_t currentGroupId) const {
    auto funcData = std::make_shared<FunctionData>();
    funcData->isNative = true;
    funcData->arity = -1; 
    funcData->params = {};

    auto name = this->interfaceName;
    auto localMembers = this->members; 
    auto localMethods = this->methods;

    funcData->nativeFn = [name, localMembers, localMethods](std::vector<Value>& args) -> Value {
        auto instance = std::make_shared<VyneStruct>(name);
        
        for (size_t i = 0; i < localMembers.size(); ++i) {
            uint32_t fieldId = StringPool::instance().intern(localMembers[i].name);
            
            if (i < args.size()) {
                if (localMembers[i].type == VType::Reference && 
                    i + 1 < args.size() && 
                    args[i].getType() == Value::STRUCT) {
                    
                    instance->fields[fieldId] = args[i];
                } else {
                    instance->fields[fieldId] = args[i];
                }
            } else {
                switch (localMembers[i].type) {
                    case VType::String:  
                        instance->fields[fieldId] = Value("");
                        break;
                    case VType::Int64:   
                        instance->fields[fieldId] = Value(static_cast<int64_t>(0)); 
                        break;
                    case VType::Float64: 
                        instance->fields[fieldId] = Value(0.0); 
                        break;
                    case VType::Array:   
                        instance->fields[fieldId] = Value(std::vector<Value>{}); 
                        break;
                    default:             
                        instance->fields[fieldId] = Value();
                        break;
                }
            }
        }
        
        for (auto& method : localMethods) {
            if (auto* funcNode = dynamic_cast<FunctionNode*>(method.get())) {
                uint32_t methodId = StringPool::instance().intern(funcNode->getOriginalName());
                instance->methodNodes[methodId] = funcNode;
            }
        }
        
        return Value(instance);
    };

    uint32_t id = StringPool::instance().intern(interfaceName);
    env[currentGroupId][id] = Value(funcData);
    
    if (!moduleName.empty()) {
        if (env.find(moduleName) == env.end()) {
            env[moduleName] = SymbolTable();
        }
        env[moduleName][id] = Value(funcData);
    }
    
    // methods will be evaluated lazily
    // for (auto& method : methods) {
    //     method->evaluate(env, currentGroup);
    // }

    return Value();
}

Value MemberAccessNode::evaluate(SymbolContainer& env, uint32_t currentGroupId) const {
    if (isReceiverStatic) {
        if (currentGroupId != 0) { 
            std::string currentGroupName = StringPool::instance().get(currentGroupId);
            uint32_t localContainerId = StringPool::instance().intern(currentGroupName + "." + getReceiverPath());
            
            if (env.contains(localContainerId)) {
                auto& table = env[localContainerId];
                auto it = table.find(memberId);
                if (it != table.end()) return it->second;
            }
        }

        if (env.contains(receiverPathId)) {
            auto& table = env[receiverPathId];
            auto it = table.find(memberId);
            if (it != table.end()) return it->second;
        }

        // Global fallback üçün "global." prefix-ini bir dəfə intern edib saxlamaq olar
        // Amma indiki halda belə fix edirik:
        uint32_t globalContainerId = StringPool::instance().intern("global." + getReceiverPath());
        if (env.contains(globalContainerId)) {
            auto& table = env[globalContainerId];
            auto it = table.find(memberId);
            if (it != table.end()) return it->second;
        }
    }

    Value receiverVal = receiver->evaluate(env, currentGroupId);
    
    if (receiverVal.getType() == Value::NONE) {
        throw std::runtime_error("Runtime Error: Cannot access member '" + memberName + 
            "' on undefined/null [ line " + std::to_string(lineNumber) + " ]");
    }

    int type = receiverVal.getType();

    if (type == Value::MODULE) {
        auto obj = receiverVal.data.obj;
        auto mod = static_cast<ModuleData*>(obj.get());
        
        // Modulun öz ID-si ModuleData-da varsa onu istifadə et, yoxdursa:
        uint32_t moduleId = StringPool::instance().intern(mod->name);
        
        if (env.contains(moduleId)) {
            auto& moduleTable = env[moduleId];
            auto it = moduleTable.find(memberId);
            if (it != moduleTable.end()) return it->second;
        }
        
        throw std::runtime_error("Runtime Error: Module '" + mod->name + 
            "' has no member '" + memberName + "' [ line " + std::to_string(lineNumber) + " ]");
    }

    if (type == Value::STRUCT) {
        auto strct = receiverVal.asStruct(); 
        
        auto it = strct->fields.find(memberId);
        if (it != strct->fields.end()) {
            return it->second;
        }
        
        throw std::runtime_error("Runtime Error: Struct '" + strct->typeName + 
            "' has no member '" + memberName + "' [ line " + std::to_string(lineNumber) + " ]");
    }

    throw std::runtime_error("Type Error: Member access '" + memberName + 
        "' not supported for type " + receiverVal.getTypeName() + " [ line " + std::to_string(lineNumber) + " ]");
}

Value MemberAssignmentNode::evaluate(SymbolContainer& env, uint32_t currentGroupId) const {
    Value newValue = rhs->evaluate(env, currentGroupId);
    Value recVal = receiver->evaluate(env, currentGroupId);
    
    uint32_t memberId = StringPool::instance().intern(memberName);

    if (recVal.getType() == Value::STRUCT) {
        auto structPtr = recVal.asStruct();
        
        auto fieldIt = structPtr->fields.find(memberId);
        if (fieldIt != structPtr->fields.end() && fieldIt->second.isReadOnly) {
            throw std::runtime_error("Runtime Error: Cannot assign to read-only struct field '" + 
                memberName + "' [ line " + std::to_string(lineNumber) + " ]");
        }

        structPtr->fields[memberId] = newValue;
        return newValue;
    }
    
    else if (recVal.getType() == Value::MODULE || recVal.getType() == Value::NONE) {
        std::string moduleName = "";
        uint32_t moduleId = 0;

        if (recVal.getType() == Value::MODULE && recVal.isObject()) {
            auto& obj = recVal.data.obj;
            if (auto mod = static_cast<ModuleData*>(obj.get())) {
                moduleName = mod->name;
                moduleId = StringPool::instance().intern(moduleName);
            }
        } else if (recVal.getType() == Value::MODULE) {
            moduleName = recVal.asString();
            moduleId = StringPool::instance().intern(moduleName);
        } else if (receiver->type() == NodeType::VARIABLE) {
            moduleId = static_cast<VariableNode*>(receiver.get())->getNameId();
            moduleName = static_cast<VariableNode*>(receiver.get())->getOriginalName();
        }

        if (moduleId == 0 && receiver->type() == NodeType::MEMBER_ACCESS) {
            auto* memAccess = static_cast<MemberAccessNode*>(receiver.get());
            moduleId = memAccess->getFullPathId();
            moduleName = memAccess->getFullPath();
        }

        if (moduleId != 0 && env.contains(moduleId)) {
            auto& moduleTable = env[moduleId];
            auto it = moduleTable.find(memberId);
            
            if (it == moduleTable.end()) {
                throw std::runtime_error("Runtime Error: '" + moduleName + "' has no member '" + 
                    memberName + "' [ line " + std::to_string(lineNumber) + " ]");
            }
            
            if (it->second.isReadOnly || it->second.isReadOnly) {
                throw std::runtime_error("Runtime Error: Cannot assign to read-only member '" + 
                    moduleName + "." + memberName + "' [ line " + std::to_string(lineNumber) + " ]");
            }

            if (Vyne::isTypeStrict() && it->second.getType() != Value::NONE) {
                if (it->second.getType() != newValue.getType()) {
                    throw std::runtime_error(
                        "Type Error: Cannot assign " + newValue.getTypeName() + 
                        " to '" + moduleName + "." + memberName + "' of type " + it->second.getTypeName() +
                        " in strict mode [ line " + std::to_string(lineNumber) + " ]"
                    );
                }
            }

            moduleTable[memberId] = newValue;
            return newValue;
        }

        if (!moduleName.empty()) {
            throw std::runtime_error("Runtime Error: Module/group '" + moduleName + 
                "' not found in environment [ line " + std::to_string(lineNumber) + " ]");
        }
    }
    
    throw std::runtime_error("Type Error: Cannot assign to member of non-struct, non-module type " + 
        recVal.getTypeName() + " [ line " + std::to_string(lineNumber) + " ]");
}

Value BlockNode::evaluate(SymbolContainer& env, uint32_t currentGroupId) const {
    // TEMPORARY FIX: we won't create new scopes for now
    // TODO ADD IT
    Value lastValue;
    for (const auto& statement : statements) {
        lastValue = statement->evaluate(env, currentGroupId);
    }
    return lastValue; 
}

/**
 * @brief Registers and initializes external modules within the current scope.
 * * When a `module` keyword is encountered, this node triggers the setup functions 
 * for native libraries (like vcore or vglib).
 * * @param env The global symbol container mapping scope paths to symbol tables.
 * @param currentGroup The current hierarchical scope path (e.g., "global.main").
 * @return Value The Module-typed value representing the loaded library.
 */

Value ModuleNode::evaluate(SymbolContainer& env, uint32_t currentGroupId) const {
    if (originalName == "vcore")  setupVCore(env, StringPool::instance());
    if (originalName == "vglib")  setupVGLib(env, StringPool::instance());
    if (originalName == "vmem")   setupVMem(env, StringPool::instance());
    if (originalName == "vmath")  setupVMath(env, StringPool::instance());
    if (originalName == "vfs")    setupVFs(env, StringPool::instance());
    if (originalName == "vurage") setupVurage(env, StringPool::instance());
    if (originalName == "vcv")    setupVCV(env, StringPool::instance());
    if (originalName == "vaudio") setupVAudio(env, StringPool::instance());

    auto& groupTable = env[currentGroupId]; 

    groupTable[moduleId] = Value(moduleId, originalName, true); 

    if (env.find(originalName) == env.end()) {
        env[originalName] = {}; 
    }

    return groupTable[moduleId];
}

Value ImportNode::evaluate(SymbolContainer& env, uint32_t currentGroupId) const {
    static uint32_t globalId = getGlobalId();
    std::filesystem::path finalPath;
    
    std::string cleanPath = filePath;
    if (!cleanPath.empty() && (cleanPath[0] == '/' || cleanPath[0] == '\\')) {
        cleanPath.erase(0, 1);
    }

    if (isExtern) {
        finalPath = std::filesystem::path(FileUtils::exeDir) / "vyne" / "modules" / "external" / cleanPath;
    } else {
        finalPath = std::filesystem::path(env.getSourceDir()) / cleanPath;
    }

    finalPath = std::filesystem::weakly_canonical(finalPath);

    if (!std::filesystem::exists(finalPath) || std::filesystem::is_directory(finalPath)) {
        std::stringstream ss;
        ss << "Vyne Error: " << (isExtern ? "Extern Module " : "File ") 
           << "'" << cleanPath << "' not found at: " << finalPath.string();
        throw std::runtime_error(ss.str());
    }

    const std::string& source = FileUtils::readFile(finalPath.string());
    auto tokens = tokenize(source);
    Parser parser(std::move(tokens));
    auto externalAst = parser.parseProgram(env);

    SymbolContainer externalEnv;
    externalEnv.setSourceDir(finalPath.parent_path().string());

    try {
        externalAst->evaluate(externalEnv, globalId);
    } catch (const std::runtime_error& e) {
        throw std::runtime_error("In " + cleanPath + ": " + e.what());
    }

    if (alias.empty()) {
        std::string stem = finalPath.stem().string();
        uint32_t stemId = StringPool::instance().intern(stem);

        if (!env.contains(stemId)) {
            env[stemId] = SymbolTable();
        }

        for (auto it = externalEnv.begin(); it != externalEnv.end(); ++it) {
            uint32_t groupId = it->first;

            if (groupId == globalId) {
                for (auto const& [id, val] : it->second) {
                    env[globalId][id] = val;
                    env[stemId][id]   = val;
                }
            } else {
                env[groupId] = std::move(it->second);
            }
        }

        env[globalId][stemId] = Value(stemId, stem, true);
    } else {
        uint32_t aliasId = StringPool::instance().intern(alias);
        env[aliasId] = {};

        auto globalIt = externalEnv.find(globalId);
        if (globalIt != externalEnv.end()) {
            for (auto const& [id, val] : globalIt->second) {
                env[aliasId][id] = val;
            }
        }

        for (auto it = externalEnv.begin(); it != externalEnv.end(); ++it) {
            uint32_t groupId = it->first;
            if (groupId == globalId) continue;

            std::string groupName = StringPool::instance().get(groupId);
            uint32_t aliasGroupId = StringPool::instance().intern(alias + "." + groupName);
            env[aliasGroupId] = std::move(it->second);
        }

        env[globalId][aliasId] = Value(aliasId, alias, true);
    }

    return Value(true);
}

Value DeployNode::evaluate(SymbolContainer& env, uint32_t currentGroupId) const {
    
    if (!env.contains(moduleName)) {
        throw std::runtime_error("Runtime Error: Module '" + moduleName + 
            "' not found in environment [ line " + std::to_string(lineNumber) + " ]");
    }

    env.deploy(moduleName); 
    return Value(true); 
}

Value DismissNode::evaluate(SymbolContainer& env, uint32_t currentGroupId) const {
    static uint32_t globalId = getGlobalId();

    bool erasedSomething = false;
    uint32_t nameId = StringPool::instance().intern(originalName);
    
    uint32_t moduleGroupId = StringPool::instance().intern(originalName);
    if (env.erase(moduleGroupId)) {
        erasedSomething = true;
    }
    
    std::vector<uint32_t> keysToErase;
    std::string prefix = "global." + originalName;
    
    for (const auto& [groupId, table] : env) {
        const std::string& groupName = StringPool::instance().get(groupId);
        if (groupName.find(prefix) == 0) {
            keysToErase.push_back(groupId);
        }
    }
    
    for (uint32_t key : keysToErase) {
        env.erase(key);
        erasedSomething = true;
    }
    
    uint32_t globalDotNameId = StringPool::instance().intern(prefix);
    if (env.erase(globalDotNameId)) erasedSomething = true;
    
    if (env.count(currentGroupId)) {
        if (env[currentGroupId].erase(nameId)) erasedSomething = true;
    }
    
    if (currentGroupId != globalId && env.count(globalId)) {
        if (env[globalId].erase(nameId)) erasedSomething = true;
    }
    
    if (env.count(nameId)) {
        env.erase(nameId);
        erasedSomething = true;
    }
    
    if (erasedSomething) return Value(true);
    
    throw std::runtime_error("Module Error: Could not dismiss '" + originalName + 
                            "' [ line " + std::to_string(lineNumber) + " ]");
}

Value EnumNode::evaluate(SymbolContainer& env, uint32_t currentGroupId) const {
    uint32_t enumId = StringPool::instance().intern(enumName);

    if (!env.contains(enumId)) {
        env[enumId] = SymbolTable();
    }

    env[currentGroupId][enumId] = Value(enumId, enumName, true);

    for (const auto& [memberName, value] : members) {
        uint32_t memberId = StringPool::instance().intern(memberName);
        
        Value constValue(static_cast<int64_t>(value));
        constValue.setReadOnly();

        env[enumId][memberId] = constValue;
    }

    return Value();
}

Value NullNode::evaluate(SymbolContainer& env, uint32_t currentGroupId) const {
    return Value();
}

uint32_t resolvePathId(const std::vector<std::string>& scope, uint32_t currentGroupId) {
    if (scope.empty()) return currentGroupId;
    
    // Məcbur qalıb string birləşdiririksə, bunu ancaq mürəkkəb yollarda edirik
    std::string path = "global";
    for (const auto& part : scope) {
        path += "." + part;
    }
    return StringPool::instance().intern(path);
}