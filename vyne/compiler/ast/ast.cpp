#include "ast.h"

#include "../../modules/common/vcore/vcore.h"
#include "../../modules/common/vglib/vglib.h"
#include "../../modules/common/vmem/vmem.h"
#include "../../modules/common/vmath/vmath.h"

#include "../parser/parser.h"
#include "../lexer/lexer.h"

Value ProgramNode::evaluate(SymbolContainer& env, const std::string& currentGroup) const {
    Value lastValue;
    for (const auto& statement : statements) lastValue = statement->evaluate(env, currentGroup);
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

Value VariableNode::evaluate(SymbolContainer& env, const std::string& currentGroup) const {
    std::string targetGroup = specificGroup.empty() ? currentGroup : "global";
    if (!specificGroup.empty()) {
        for (const auto& g : specificGroup) targetGroup += "." + g;
    }

    auto groupIt = env.find(targetGroup);
    if (groupIt != env.end()) {
        auto varIt = groupIt->second.find(nameId);
        if (varIt != groupIt->second.end()) return varIt->second;
    }

    if (targetGroup != "global") {
        auto globalIt = env.find("global");
        if (globalIt != env.end()) {
            auto varIt = globalIt->second.find(nameId);
            if (varIt != globalIt->second.end()) return varIt->second;
        }
    }

    throw std::runtime_error("Runtime Error: Variable '" + originalName + "' not found [ line " + std::to_string(lineNumber) + " ]");
}

/**
 * @brief Handles variable assignment and updates the SymbolContainer.
 * * @note Throws a runtime_error if attempting to reassign a Read-Only value.
 * @see VariableNode::evaluate
 * * @return Value The value being assigned (allows for chained assignments like a = b = 1).
 */

Value AssignmentNode::evaluate(SymbolContainer& env,     const std::string& currentGroup) const {
    Value val = rhs->evaluate(env, currentGroup);

    if (expectedType != VType::Unknown) {
    const std::string& expectedName = VTypeToString(expectedType);
    const std::string& actualName = val.getTypeName(); 

    if (expectedName != actualName) {
        if (expectedType == VType::Int64 && val.getType() == Value::FLOAT64) {
            val = Value(val.asInt());
        } 
        else if (expectedType == VType::Float64 && val.getType() == Value::INT64) {
            val = Value(val.asFloat());
        } 
        else {
            throw std::runtime_error("Type Error: Explicit type mismatch. Expected " + 
                expectedName + ", but got " + actualName + 
                " [ line " + std::to_string(lineNumber) + " ]");
        }
    }
}

    if (isConstant) {
        val.setReadOnly();
    }

    std::string targetGroup = resolvePath(scopePath, currentGroup);
    auto& table = env[targetGroup];

    auto it_existing = table.find(identifierId); 

    if (it_existing != table.end()) {
        int existingType = it_existing->second.getType();
        int newType = val.getType();

        if (existingType != 0 && existingType != newType) {
            throw std::runtime_error("Type Error: Cannot assign type " + val.getTypeName() + 
                                    " to variable '" + originalName + 
                                    "' defined as " + it_existing->second.getTypeName() + 
                                    " [ line " + std::to_string(lineNumber) + " ]");
        }
    }

    if (indexExpr) {
        auto it = table.find(identifierId);
        if (it == table.end()) {
            throw std::runtime_error("Runtime Error: Array '" + originalName + "' not found [ line " + std::to_string(lineNumber) + " ]");
        }

        Value& arrayVal = it->second; 
        
        if (arrayVal.getType() != Value::ARRAY) {
            throw std::runtime_error("Runtime Error: Cannot index into non-array '" + originalName + "' [ line " + std::to_string(lineNumber) + " ]");
        }

        Value idxValue = indexExpr->evaluate(env, currentGroup);
        
        int64_t rawIdx = idxValue.asInt();

        if (rawIdx < 0) {
            throw std::runtime_error("Runtime Error: Negative array index " + std::to_string(rawIdx) + " [ line " + std::to_string(lineNumber) + " ]");
        }

        size_t idx = static_cast<size_t>(rawIdx);
        auto& vec = arrayVal.asList();
        
        if (idx < vec.size()) {
            vec[idx] = val;
        } else {
            throw std::runtime_error("Runtime Error: Index " + std::to_string(idx) + " out of bounds (size " + std::to_string(vec.size()) + ") [ line " + std::to_string(lineNumber) + " ]");
        }
        
        return val;
    }

    auto it = table.find(identifierId);
    if (it != table.end() && it->second.isReadOnly) {
        throw std::runtime_error("Runtime Error: Cannot reassign read-only '" + originalName + "' [ line " + std::to_string(lineNumber) + " ]");
    }

    table[identifierId] = val; 
    return val;
}

Value GroupNode::evaluate(SymbolContainer& env, const std::string& currentGroup) const {
    std::string nextGroup = currentGroup + "." + groupName;
    for (const auto& stmt : statements) {
        stmt->evaluate(env, nextGroup);
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

Value BinOpNode::evaluate(SymbolContainer& env, const std::string& currentGroup) const {
    Value l = left->evaluate(env, currentGroup);

    if (op == VTokenType::And) {
        if (!l.isTruthy()) return Value(static_cast<int64_t>(0));
        return Value(static_cast<int64_t>(right->evaluate(env, currentGroup).isTruthy() ? 1 : 0));
    }
    if (op == VTokenType::Or) {
        if (!l.isTruthy()) return Value(static_cast<int64_t>(1));
        return Value(static_cast<int64_t>(right->evaluate(env, currentGroup).isTruthy() ? 1 : 0));
    }

    Value r = right->evaluate(env, currentGroup);
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
                return Value(lv % rv);
            case VTokenType::Smaller:   return Value(lv < rv);
            case VTokenType::Double_Equals: return Value(lv == rv);
            default: break;
        }
    }

    if (op == VTokenType::Double_Equals) return Value(l == r);
    if (op == VTokenType::Not_Equal) return Value(l != r);

    throw std::runtime_error("Type Error: Invalid operation " + VTokenTypeToString(op) + " between " + l.getTypeName() + " and " + r.getTypeName() + "[ " + std::to_string(lineNumber) + " ]");
}

Value PostFixNode::evaluate(SymbolContainer& env, const std::string& currentGroup) const {
    if (left->type() != NodeType::VARIABLE) {
        throw std::runtime_error("Type Error: Cannot increment a non-variable [ line " + std::to_string(lineNumber) + " ]");
    }

    auto* varNode = static_cast<VariableNode*>(left.get());

    Value oldValue = left->evaluate(env, currentGroup);
    Value newVal;

    if (oldValue.getType() == Value::INT64) {
        int64_t raw = oldValue.asInt();
        newVal = Value(op == VTokenType::Double_Increment ? raw + 1 : raw - 1);
    } else {
        double raw = oldValue.asFloat();
        newVal = Value(op == VTokenType::Double_Increment ? raw + 1.0 : raw - 1.0);
    }

    env[currentGroup][varNode->getNameId()] = newVal;
    return newVal;
}

Value UnaryNode::evaluate(SymbolContainer& env, const std::string& currentGroup) const {
    Value val = right->evaluate(env, currentGroup);

    switch(op){
        case VTokenType::Exclamatory : 
            return Value(!val.isTruthy());
        
        case VTokenType::Substract : 
            if (val.getType() == Value::INT64) return Value(-val.asInt());
            return Value(-val.asFloat());
        
        case VTokenType::Addresser : {
            auto varNode = dynamic_cast<VariableNode*>(right.get());
            if (!varNode) throw std::runtime_error("Cannot take address of a non-variable.");

            Value* internalPtr = env.getInternalPointer(currentGroup, varNode->getNameId()); 
            
            return Value(reinterpret_cast<int64_t>(internalPtr)); // remember here
        }
        default: return Value();
    }
}

Value ArrayNode::evaluate(SymbolContainer& env, const std::string& currentGroup) const {
    std::vector<Value> results;
    for (const auto& node : elements) results.emplace_back(node->evaluate(env, currentGroup));
    return Value(results);
}

Value RangeNode::evaluate(SymbolContainer& env, const std::string& currentGroup) const {
    Value startVal = left->evaluate(env, currentGroup);
    Value endVal = right->evaluate(env, currentGroup);
    
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

Value BuiltInCallNode::evaluate(SymbolContainer& env, const std::string& currentGroup) const {
    std::vector<Value> argValues;
    for (auto& arg : arguments) argValues.emplace_back(arg->evaluate(env, currentGroup));

    if (funcName == "out") {
        if (!argValues.empty()) { argValues[0].print(std::cout); std::cout << std::endl; }
        return Value();
    }

    if (funcName == "type") return argValues[0].getTypeName();

    if (funcName == "string") {
        if (argValues.size() != 1) throw std::runtime_error("Argument Error: string() expects 1 arg.");
        return Value(argValues[0].toString());
    }

    if (funcName == "int64") {
        if (argValues.size() != 1) throw std::runtime_error("Argument Error: int64() expects 1 arg.");
        
        int64_t result = std::stoll(argValues[0].asString());
        return Value(result); 
    }

    if (funcName == "float64") {
        if (argValues.size() != 1) throw std::runtime_error("Argument Error: float64() expects 1 arg.");
        
        double result = std::stod(argValues[0].asString());
        return Value(result);
    }

    if (funcName == "sizeof") {
        if (argValues.size() != 1) throw std::runtime_error("Argument Error: sizeof() expects 1 arg.");
        return Value(static_cast<int64_t>(argValues[0].getShallowBytes()));
    }

    if (funcName == "sequence") {
        if (argValues.size() != 2) throw std::runtime_error("Argument Error: sequence() expects 2 args.");
        
        std::vector<Value> sequence;
        if (argValues[0].getType() == Value::INT64 && argValues[1].getType() == Value::INT64) {
            for (int64_t i = argValues[0].asInt(); i < argValues[1].asInt(); ++i) 
                sequence.emplace_back(i);
        } else {
            for (double i = argValues[0].asFloat(); i < argValues[1].asFloat(); ++i) 
                sequence.emplace_back(i);
        }
        return Value(sequence);
    } 

    throw std::runtime_error("Unknown built-in: " + funcName);
}

Value IndexAccessNode::evaluate(SymbolContainer& env, const std::string& currentGroup) const {
    std::string targetGroup = resolvePath(scope, currentGroup);
    
    auto getFromTable = [&](const std::string& group) -> Value* {
        if (env.count(group) && env[group].count(nameId)) {
            return &env[group][nameId];
        }
        return nullptr;
    };

    Value* arrayValPtr = getFromTable(targetGroup);
    if (!arrayValPtr && targetGroup != "global") {
        arrayValPtr = getFromTable("global");
    }

    if (arrayValPtr) {
        if (arrayValPtr->getType() != Value::ARRAY) {
            throw std::runtime_error("Type Error: '" + originalName + "' is not an array [ line " + std::to_string(lineNumber) + " ]");
        }

        Value idxVal = index->evaluate(env, currentGroup);
        size_t idx = static_cast<size_t>(idxVal.asInt());
        auto& vec = arrayValPtr->asList();

        if (idx >= vec.size()) {
            throw std::runtime_error("Index Error: Out of bounds (" + std::to_string(idx) + ") on '" + originalName + "' [ line " + std::to_string(lineNumber) + " ]");
        }
        return vec[idx];
    }

    throw std::runtime_error("Runtime Error: Array '" + originalName + "' not found [ line " + std::to_string(lineNumber) + " ]");
}

Value FunctionNode::evaluate(SymbolContainer& env, const std::string& currentGroup) const {
    Value funcValue(parameterIds, body);

    std::string destination = targetModule.empty() ? currentGroup : "global." + targetModule;

    env[destination][funcNameId] = funcValue; 

    return funcValue;
}

Value FunctionCallNode::evaluate(SymbolContainer& env, const std::string& currentGroup) const {
    auto it = env["global"].find(funcNameId);
    
    if (it == env["global"].end()) {
        throw std::runtime_error("Runtime Error: " + originalName + " is not defined in global scope [ line " + std::to_string(lineNumber) + " ]");
    }

    Value funcVal = it->second;

    if (funcVal.getType() != Value::FUNCTION) {
        throw std::runtime_error("Type Error: " + originalName + " is not a function [ line " + std::to_string(lineNumber) + " ]");
    }

    std::vector<Value> evaluatedArgs;
    evaluatedArgs.reserve(arguments.size());

    for (const auto& arg : arguments) {
        evaluatedArgs.emplace_back(arg->evaluate(env, currentGroup));
    }

    std::string localScope = "call_" + originalName + "_" + std::to_string(rand());

    auto& params = funcVal.asFunction()->params;

    if (params.size() != evaluatedArgs.size()) {
        throw std::runtime_error("Argument Error: Argument count mismatch on function call " + originalName + " [ line " + std::to_string(lineNumber) + " ]");
    }
    
    for (size_t i = 0; i < params.size(); ++i) {
        env[localScope][params[i]] = std::move(evaluatedArgs[i]);
    }

    Value result;
    try {
        for (const auto& bodyNode : funcVal.asFunction()->body) {
            result = bodyNode->evaluate(env, localScope);
        }
    } catch (const ReturnException& e) {
        result = e.value; 
    }

    if (env.find(localScope) != env.end()) {
        env[localScope].clear();
        env.erase(localScope);
    }

    return result;
}

Value ReturnNode::evaluate(SymbolContainer& env, const std::string& currentGroup) const {
    throw ReturnException{expression->evaluate(env, currentGroup)};
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

Value MethodCallNode::evaluate(SymbolContainer& env, const std::string& currentGroup) const {
    Value receiverVal = receiver->evaluate(env, currentGroup);
    uint32_t methodId = StringPool::instance().intern(methodName);

    if (receiverVal.getType() == Value::MODULE) {
        auto obj = std::get<std::shared_ptr<VyneObject>>(receiverVal.data);
    
        auto mod = static_cast<ModuleData*>(obj.get());
        std::string modName = mod->name;
        std::string modPath = "global." + modName;

        if (env.count(modPath) && env[modPath].count(methodId)) {
            Value& funcVal = env[modPath][methodId];

            if (funcVal.getType() == Value::FUNCTION) {
                auto func = funcVal.asFunction();

                if (arguments.empty() && func->isNative) {
                    static std::vector<Value> emptyArgs; 
                    return func->nativeFn(emptyArgs);
                }

                std::vector<Value> argValues;
                argValues.reserve(arguments.size());
                for (auto& arg : arguments) {
                    argValues.emplace_back(arg->evaluate(env, currentGroup));
                }

                if (func->isNative) {
                    return func->nativeFn(argValues); 
                }

                const std::string& localCallScope = modPath + ".call_" + std::to_string(rand());

                for (size_t i = 0; i < func->params.size() && i < argValues.size(); ++i) {
                    env[localCallScope][func->params[i]] = std::move(argValues[i]);
                }

                Value result(0.0); 
                try {
                    for (auto& stmt : func->body) {
                        result = stmt->evaluate(env, localCallScope);
                    }
                } catch (const ReturnException& e) {
                    result = e.value;
                }

                env.erase(localCallScope);

                return result;
            }
        }
        throw std::runtime_error("Module Error: Method '" + methodName + "' not found in module " + modName + " [ line " + std::to_string(lineNumber) + " ]");
    }

    // --- ARRAY METHODS ---
    if (receiverVal.getType() == Value::ARRAY) {
        if (methodName == "size") {
            return Value(static_cast<double>(receiverVal.asList().size()));
        }

        Value* target = nullptr;

        if (receiver->type() == NodeType::VARIABLE) {
            auto* var = static_cast<VariableNode*>(receiver.get());
            std::string targetGroup = resolvePath(var->getScope(), currentGroup);
            if (env.count(targetGroup) && env[targetGroup].count(var->getNameId())) {
                target = &env[targetGroup][var->getNameId()];
            } else if (targetGroup != "global" && env["global"].count(var->getNameId())) {
                target = &env["global"][var->getNameId()];
            }
        } 
        else if (receiver->type() == NodeType::INDEX_ACCESS) {
            target = &receiverVal;
        }

        if (!target && (methodName == "push" || methodName == "pop" || methodName == "clear")) {
            throw std::runtime_error("Runtime Error: Cannot call mutating method '" + methodName + "' on anonymous array [ line " + std::to_string(lineNumber) + " ]");
        }

        auto& vec = target ? target->asList() : receiverVal.asList();

        if (methodName == "push") {
            if (target->getType() != Value::ARRAY) throw std::runtime_error("Type Error : Called method push() on non-array [ line " + std::to_string(lineNumber) + " ]");

            for(auto& arg : arguments){
                Value val = arg->evaluate(env, currentGroup);
                target->asList().emplace_back(val);
            }

            return receiverVal;
        }

        if (methodName == "pop") {
            if (target->getType() != Value::ARRAY) throw std::runtime_error("Type Error : Called method pop() on non-array [ line " + std::to_string(lineNumber) + " ]");
            if (target->asList().empty()) throw std::runtime_error("Index Error: pop() from empty array [ line " + std::to_string(lineNumber) + " ]");
            if (!arguments.empty()) throw std::runtime_error("Argument Error: pop() expects 0 arguments, but got " + std::to_string(arguments.size()) + " [ line " + std::to_string(lineNumber) + " ]");

            target->asList().pop_back();
            return Value(true);
        }

        if (methodName == "delete") {
            if (target->getType() != Value::ARRAY) throw std::runtime_error("Type Error : Called method delete() on non-array [ line " + std::to_string(lineNumber) + " ]");
            if (arguments.size() != 1) throw std::runtime_error("Argument Error: delete() expects exactly 1 argument, but got " + std::to_string(arguments.size()) + " instead [ line " + std::to_string(lineNumber) + " ]");

            Value val = arguments[0]->evaluate(env, currentGroup);
            auto it = std::find(target->asList().begin(), target->asList().end(), val);
            if (it == std::end(target->asList())) throw std::runtime_error("Value error : Could not find given value in array! [ line " + std::to_string(lineNumber) + " ]");
            target->asList().erase(it);
            return Value(true);
        }

        if (methodName == "sort") {
            if (target->getType() != Value::ARRAY) throw std::runtime_error("Type Error: sort() called on non-array [ line " + std::to_string(lineNumber) + " ]");
            if (!arguments.empty()) throw std::runtime_error("Argument Error: sort() expects 0 arguments [ line " + std::to_string(lineNumber) + " ]");
            for (auto& el : target->asList()) {
                if (el.getType() != Value::INT64 && el.getType() != Value::FLOAT64) throw std::runtime_error("Value Error: Cannot sort non-numeric values [ line " + std::to_string(lineNumber) + " ]");
            }
            std::sort(target->asList().begin(), target->asList().end());
            return Value(*target); 
        }

        if (methodName == "place_all") {
            if (target->getType() != Value::ARRAY) 
                throw std::runtime_error("Type Error: place_all() called on non-array [ line " + std::to_string(lineNumber) + " ]");

            if (arguments.size() < 2)
                throw std::runtime_error("Argument Error: place_all(element, count) expects 2 arguments [ line " + std::to_string(lineNumber) + " ]");

            Value element = arguments[0]->evaluate(env, currentGroup);
            Value countVal = arguments[1]->evaluate(env, currentGroup);
            
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

        if (methodName == "reverse") {
            if (target->getType() != Value::ARRAY) throw std::runtime_error("Type Error: reverse() called on non-array [ line " + std::to_string(lineNumber) + " ]");
            if (arguments.size() > 0) throw std::runtime_error("Argument Error: reverse() expects 0 arguments, but got " + std::to_string(arguments.size()) + " instead [ line " + std::to_string(lineNumber) + " ]");
            std::reverse(target->asList().begin(), target->asList().end());
            return Value(*target);
        }

        if (methodName == "clear") {
            if (target->getType() != Value::ARRAY) throw std::runtime_error("Type Error: clear() called on non-array [ line " + std::to_string(lineNumber) + " ]");
            if (arguments.size() > 0) throw std::runtime_error("Argument Error: clear() expects 0 arguments, but got " + std::to_string(arguments.size()) + " instead [ line " + std::to_string(lineNumber) + " ]");
            target->asList().clear();
            return Value(*target);
        }
    }
    
    throw std::runtime_error("Unknown method: " + methodName + " [ line " + std::to_string(lineNumber) + " ]");
}

/**
 * @brief Executes a block of code repeatedly while a condition is truthy.
 * * This implementation supports:
 * - @b Break: Caught via BreakException to exit the loop.
 * - @b Continue: Caught via ContinueException to skip to the next iteration.
 * * */

Value WhileNode::evaluate(SymbolContainer& env, const std::string& currentGroup) const {
    Value lastResult;
    while (condition->evaluate(env, currentGroup).isTruthy()) {
        try {
            lastResult = body->evaluate(env, currentGroup);
        } catch (const BreakException&) { break; }
        catch (const ContinueException&) { continue; }
    }
    return lastResult;
}

Value ForNode::evaluate(SymbolContainer& env, const std::string& currentGroup) const {
    Value collection = iterable->evaluate(env, currentGroup);
    if (collection.getType() != Value::ARRAY) {
        throw std::runtime_error("Runtime Error: 'through' requires a sequence or range [ line " + std::to_string(lineNumber) + " ]");
    }

    const auto& elements = collection.asList();
    auto& scope = env[currentGroup];
    uint32_t itId = StringPool::instance().intern(iteratorName);

    Value savedIt;
    bool hadIt = (scope.find(itId) != scope.end());
    if (hadIt) savedIt = scope[itId];

    std::vector<Value> resultList;
    std::set<Value> seen;
    Value lastVal;

    for (const auto& element : elements) {
        scope[itId] = element;
        
        try {
            Value currentResult = body->evaluate(env, currentGroup);

            switch(mode) {
                case ForMode::COLLECT:          
                    resultList.emplace_back(currentResult);
                    break;
                case ForMode::FILTER:
                    if (currentResult.isTruthy()) resultList.emplace_back(element);
                    break;
                case ForMode::LOOP: 
                    lastVal = currentResult;
                    break;
                case ForMode::UNIQUE : {
                    if(seen.find(element) == seen.end()){
                        seen.insert(element);
                        resultList.emplace_back(element);
                    }
                    break;
                }
                case ForMode::EVERY:
                default:
                    lastVal = currentResult;
                    break;
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

Value IfNode::evaluate(SymbolContainer& env, const std::string& currentGroup) const {
    try{
        if(condition->evaluate(env, currentGroup).isTruthy()){
            return body->evaluate(env, currentGroup);
        } else if (elseBody) {
            return elseBody->evaluate(env, currentGroup);
        }
    } catch (const BreakException& breakException){
        throw;
    }
    return Value();
}

Value BlockNode::evaluate(SymbolContainer& env, const std::string& currentGroup) const {
    Value lastValue;
    for (const auto& statement : statements) lastValue = statement->evaluate(env, currentGroup);
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

Value ModuleNode::evaluate(SymbolContainer& env, const std::string& currentGroup) const {
    if (originalName == "vcore") {
        setupVCore(env, StringPool::instance());
    }

    if (originalName == "vglib") {
        setupVGLib(env, StringPool::instance());
    }

    if (originalName == "vmem") {
        setupVMem(env, StringPool::instance());
    }

    if (originalName == "vmath") {
        setupVMath(env, StringPool::instance());
    }
    
    // TODO this shit clashes with group names
    env[currentGroup][moduleId] = Value(moduleId, originalName, true); 
    
    return env[currentGroup][moduleId];
}

Value ImportNode::evaluate(SymbolContainer& env, const std::string& currentGroup) const {
    std::filesystem::path finalPath;

    if (isExtern) {
        finalPath = std::filesystem::path(FileUtils::exeDir) / "vyne" / "modules" / "external" / filePath;
    } else {
        finalPath = std::filesystem::path(env.getSourceDir()) / filePath;
    }

    finalPath = std::filesystem::weakly_canonical(finalPath);

    if (!std::filesystem::exists(finalPath)) {
        if (!std::filesystem::exists(finalPath)) {
            std::stringstream ss;
            ss << (isExtern ? "Extern Module" : "File") 
            << " not found: " << finalPath.string();
            throw std::runtime_error(ss.str());
        }
    }

    const std::string& source = FileUtils::readFile(finalPath.string());
    auto tokens = tokenize(source);
    Parser parser(std::move(tokens));
    auto externalAst = parser.parseProgram();

    SymbolContainer externalEnv;
    externalEnv.setSourceDir(finalPath.parent_path().string());

    try {
        externalAst->evaluate(externalEnv, "global");
        std::cout << "[DEBUG] External file '" << filePath << "' evaluated successfully.\n";
    } catch (const std::runtime_error& e) {
        throw std::runtime_error("In " + filePath + ": " + e.what());
    }

    for (auto const& [id, val] : externalEnv["global"]) {
        env["global"][id] = val;
    }

    for (auto it = externalEnv.begin(); it != externalEnv.end(); ++it) {
        const std::string& scopeName = it->first;

        if (scopeName == "global") continue;

        std::string finalScopeName;
        if (alias.empty()) {
            finalScopeName = scopeName;
        } else {
            if (scopeName.rfind("global.", 0) == 0) {
                finalScopeName = "global." + alias + "." + scopeName.substr(7);
            } else {
                finalScopeName = alias + "." + scopeName;
            }
        }

        std::cout << "[DEBUG] Transferring Scope: " << scopeName << " -> " << finalScopeName 
                  << " (" << it->second.size() << " symbols)\n";

        env[finalScopeName] = std::move(it->second);
    }

    for (const auto& modName : externalEnv.getDeployedList()) {
        const std::string& targetMod = alias.empty() ? modName : alias + "." + modName;
        env.deploy(targetMod);
    }

    return Value(true);
}

Value DeployNode::evaluate(SymbolContainer& env, const std::string& currentGroup) const {
    uint32_t modId = StringPool::instance().intern(moduleName); 

    if (env["global"].find(modId) == env["global"].end()) {
        throw std::runtime_error("Runtime Error: Module '" + moduleName + "' not found [ line " + std::to_string(lineNumber) + " ]");
    }

    env.deploy(moduleName); 
    
    return Value(true); 
}

Value DismissNode::evaluate(SymbolContainer& env, const std::string& currentGroup) const {
    bool erasedSomething = false;
    uint32_t nameId = StringPool::instance().intern(originalName);

    if (env.erase("global." + originalName)) erasedSomething = true;

    if (env.count(currentGroup)) {
        if (env[currentGroup].erase(nameId)) erasedSomething = true;
    }

    if (currentGroup != "global" && env.count("global")) {
        if (env["global"].erase(nameId)) erasedSomething = true;
    }

    if (erasedSomething) return Value();

    throw std::runtime_error("Module Error: Could not dismiss '" + originalName + "' [ line " + std::to_string(lineNumber) + " ]");
}

std::string resolvePath(std::vector<std::string> scope, const std::string& currentGroup) {
    if (scope.empty()) {
        return currentGroup;
    }
    
    std::string path = "global";
    for (const auto& segment : scope) {
        path += "." + segment;
    }
    return path;
}
