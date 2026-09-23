#include "codegen.h"

// ============================================================
// LITERALS
// ============================================================

std::string NumberNode::getCExpr(C_Emitter& e) const {
    if (value.getType() == Value::INT64)
        return "vyne_int(" + std::to_string(value.asInt()) + ")";
    return "vyne_float(" + std::to_string(value.asFloat()) + ")";
}

void NumberNode::compile(C_Emitter& e) const { /* literals are expressions only */ }

std::string StringNode::getCExpr(C_Emitter& e) const {
    (void)e;
    std::string escaped;
    escaped.reserve(text.size() * 2);
    for (unsigned char c : text) {
        switch (c) {
            case '"':  escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\n': escaped += "\\n";  break;
            case '\r': escaped += "\\r";  break;
            case '\t': escaped += "\\t";  break;
            default:
                if (c < 0x20 || c == 0x7F) {
                    // Non-printable byte → \xHH
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\x%02x", c);
                    escaped += buf;
                } else {
                    escaped += static_cast<char>(c);
                }
        }
    }
    return "vyne_string(\"" + escaped + "\")";
}

void StringNode::compile(C_Emitter& e) const {}

std::string BooleanNode::getCExpr(C_Emitter& e) const {
    return condition ? "vyne_bool(1)" : "vyne_bool(0)";
}

void BooleanNode::compile(C_Emitter& e) const {}

std::string NullNode::getCExpr(C_Emitter& e) const {
    return "vyne_null()";
}

void NullNode::compile(C_Emitter& e) const {}

// ============================================================
// VARIABLES AND ASSIGNMENTS
// ============================================================

std::string VariableNode::getCExpr(C_Emitter& e) const {
    std::string sanitized = originalName;
    std::replace(sanitized.begin(), sanitized.end(), '.', '_');

    std::string prefix = e.getActiveFunctionPrefix();
    std::string resolved;

    if (!prefix.empty()) {
        std::string localName = "v_" + prefix + "_" + sanitized;
        if (e.isLocalDeclared(localName)) {
            resolved = localName;
        } else {
            resolved = "v_" + sanitized;
        }
    } else {
        resolved = "v_" + sanitized;
    }

    if (e.isReference(resolved)) {
        return "(*" + resolved + ")";
    }
    return resolved;
}

void VariableNode::compile(C_Emitter& e) const {
    // Bare variable reference as a statement is a no-op in C
}

std::string AssignmentNode::getCExpr(C_Emitter& e) const {
    std::string name = "v_" + originalName;
    std::replace(name.begin(), name.end(), '.', '_');
    return name;
}

void AssignmentNode::compile(C_Emitter& e) const {
    if (isReference) {
        throw std::runtime_error(
            "Compile Error: Reference variables are not supported by the C backend "
            "(line " + std::to_string(lineNumber) + "). Use the interpreter instead.");
    }

    std::string sanitized = originalName;
    std::replace(sanitized.begin(), sanitized.end(), '.', '_');

    std::string prefix = e.getActiveFunctionPrefix();
    std::string bareName = "v_" + sanitized;
    bool hasGlobal = e.getGlobalVars().count(bareName) > 0;
    bool isDeclaration = (expectedType != VType::Unknown);

    bool useGlobal;
    if (prefix.empty()) {
        useGlobal = true;                      // top-level → global
    } else if (!isDeclaration && hasGlobal) {
        useGlobal = true;                      // inside fn, assigning existing global
    } else {
        useGlobal = false;                     // inside fn, local
    }

    std::string varName = useGlobal ? bareName
                                    : ("v_" + prefix + "_" + sanitized);

    if (useGlobal) {
        if (!hasGlobal) {
            e.registerDeclaration(bareName);
            e.emitGlobalDecl("VyneValue " + bareName + ";");
        }
        std::string val = rhs->getCExpr(e);
        e.emit(bareName + " = " + val + ";");
    } else {
        if (!e.isLocalDeclared(varName)) {
            e.registerDeclaration(varName);
            std::string val = rhs->getCExpr(e);
            e.emit("VyneValue " + varName + " = " + val + ";");
        } else {
            std::string val = rhs->getCExpr(e);
            e.emit(varName + " = " + val + ";");
        }
    }
}

// ============================================================
// BINARY / UNARY / POSTFIX
// ============================================================

std::string BinOpNode::getCExpr(C_Emitter& e) const {
    std::string l = leftNode->getCExpr(e);
    std::string r = rightNode->getCExpr(e);
    std::string temp = e.newTemp("bin");
    int opCode = static_cast<int>(op);
    
    // Map VTokenType to runtime op codes
    switch (op) {
        case VTokenType::Add:       opCode = 29; break;
        case VTokenType::Substract: opCode = 30; break;
        case VTokenType::Multiply:  opCode = 31; break;
        case VTokenType::Division:  opCode = 32; break;
        case VTokenType::Modulo:    opCode = 36; break;
        case VTokenType::Power:     opCode = 37; break;
        case VTokenType::Double_Equals: opCode = 43; break;
        case VTokenType::Not_Equal: opCode = 44; break;
        case VTokenType::Greater:   opCode = 45; break;
        case VTokenType::Smaller:   opCode = 46; break;
        case VTokenType::Greater_Or_Equal: opCode = 47; break;
        case VTokenType::Smaller_Or_Equal: opCode = 48; break;
        case VTokenType::And:       opCode = 49; break;
        case VTokenType::Or:        opCode = 50; break;
        case VTokenType::Floor_Divide: opCode = 51; break;
        default: break;
    }
    
    e.emit("VyneValue " + temp + " = vyne_binop(" + l + ", " + r +
           ", " + std::to_string(opCode) + ");");
    return temp;
}

void BinOpNode::compile(C_Emitter& e) const { getCExpr(e); }

std::string UnaryNode::getCExpr(C_Emitter& e) const {
    if (op == VTokenType::Addresser) {
        throw std::runtime_error(
            "Compile Error: '&' (address-of) is not supported by the C backend "
            "(line " + std::to_string(lineNumber) + "). Use the interpreter instead.");
    }

    std::string val = right->getCExpr(e);
    std::string temp = e.newTemp("un");
    int opCode = static_cast<int>(op);

    if (op == VTokenType::Exclamatory) opCode = 44;
    else if (op == VTokenType::Substract) opCode = 30;

    e.emit("VyneValue " + temp + " = vyne_unary(" + val +
           ", " + std::to_string(opCode) + ");");
    return temp;
}

void UnaryNode::compile(C_Emitter& e) const { getCExpr(e); }

std::string PostFixNode::getCExpr(C_Emitter& e) const {
    std::string var = left->getCExpr(e);
    std::string temp = e.newTemp("post");
    e.emit("VyneValue " + temp + " = " + var + ";");
    if (op == VTokenType::Double_Increment) {
        e.emit("if (" + var + ".type == V_INT64)   " + var + ".as.i64++;");
        e.emit("else if (" + var + ".type == V_FLOAT64) " + var + ".as.f64++;");
    } else {
        e.emit("if (" + var + ".type == V_INT64)   " + var + ".as.i64--;");
        e.emit("else if (" + var + ".type == V_FLOAT64) " + var + ".as.f64--;");
    }
    return temp;
}

void PostFixNode::compile(C_Emitter& e) const { getCExpr(e); }

// ============================================================
// CONTROL FLOW
// ============================================================

void IfNode::compile(C_Emitter& e) const {
    std::string cond = condition->getCExpr(e);
    e.emitBlockOpen("if (vyne_is_truthy(" + cond + ")) {");
    if (body) body->compile(e);
    e.emitBlockClose();
    if (elseBody) {
        e.emitBlockOpen("else {");
        elseBody->compile(e);
        e.emitBlockClose();
    }
}

std::string IfNode::getCExpr(C_Emitter& e) const {
    compile(e);
    return "vyne_null()";
}

void WhileNode::compile(C_Emitter& e) const {
    e.emitBlockOpen("while (1) {");
    std::string cond = condition->getCExpr(e);
    e.emit("if (!vyne_is_truthy(" + cond + ")) break;");
    if (body) body->compile(e);
    e.emitBlockClose();
}

std::string WhileNode::getCExpr(C_Emitter& e) const {
    compile(e);
    return "vyne_null()";
}

void ReturnNode::compile(C_Emitter& e) const {
    std::string expr = expression ? expression->getCExpr(e) : "vyne_null()";

    if (e.hasTryCleanup() && e.hasReturnVars()) {
        e.emit(e.getReturnVar() + " = " + expr + ";");
        e.emit(e.getReturningVar() + " = 1;");
        e.emit("goto " + e.currentTryCleanup() + ";");
    } else if (e.hasDeferContext() && e.hasReturnVars()) {
        e.emit(e.getReturnVar() + " = " + expr + ";");
        e.emit("goto " + e.getDeferCleanupLabel() + ";");
    } else {
        e.emit("return " + expr + ";");
    }
}

std::string ReturnNode::getCExpr(C_Emitter& e) const {
    compile(e);
    return "vyne_null()";
}

void BreakNode::compile(C_Emitter& e) const {
    if (e.hasTryCleanup()) {
        throw std::runtime_error(
            "Compile Error: 'break' inside try/catch/finally is not supported by "
            "the C backend (line " + std::to_string(lineNumber) + "). "
            "Use a flag variable and break outside the try.");
    }
    e.emit("break;");
}
std::string BreakNode::getCExpr(C_Emitter& e) const { return "vyne_null()"; }

void ContinueNode::compile(C_Emitter& e) const {
    if (e.hasTryCleanup()) {
        throw std::runtime_error(
            "Compile Error: 'continue' inside try/catch/finally is not supported by "
            "the C backend (line " + std::to_string(lineNumber) + ").");
    }
    e.emit("continue;");
}
std::string ContinueNode::getCExpr(C_Emitter& e) const { return "vyne_null()"; }

// ============================================================
// FOR NODE
// ============================================================

void ForNode::compile(C_Emitter& e) const {
    std::string collection = iterable->getCExpr(e);
    std::string iTemp = e.newTemp("i");
    std::string sizeTemp = e.newTemp("sz");
    std::string elemVar = "v_" + iteratorName;

    e.emitBlockOpen("if (" + collection + ".type == V_ARRAY) {");
    e.emit("int64_t " + sizeTemp + " = " + collection + ".as.arr->size;");
    e.emitBlockOpen("for (int64_t " + iTemp + " = 0; " +
                    iTemp + " < " + sizeTemp + "; " + iTemp + "++) {");
    e.emit("VyneValue " + elemVar + " = vyne_array_get(" +
           collection + ", vyne_int(" + iTemp + "));");
    if (body) body->compile(e);
    e.emitBlockClose();
    e.emitBlockClose();
}

std::string ForNode::getCExpr(C_Emitter& e) const {
    if (mode == ForMode::LOOP) {
        compile(e);
        return "vyne_null()";
    }

    std::string collection = iterable->getCExpr(e);
    std::string iTemp = e.newTemp("i");
    std::string sizeTemp = e.newTemp("sz");
    std::string elemVar = "v_" + iteratorName;

    // --- EVERY mode: short-circuiting boolean AND over elements ---
    if (mode == ForMode::EVERY) {
        std::string everyTemp = e.newTemp("every");
        std::string resTemp = e.newTemp("every_res");
        e.emit("bool " + everyTemp + " = true;");
        e.emitBlockOpen("if (" + collection + ".type == V_ARRAY) {");
        e.emit("int64_t " + sizeTemp + " = " + collection + ".as.arr->size;");
        e.emitBlockOpen("for (int64_t " + iTemp + " = 0; " +
                        iTemp + " < " + sizeTemp + "; " + iTemp + "++) {");
        e.emit("VyneValue " + elemVar + " = vyne_array_get(" +
               collection + ", vyne_int(" + iTemp + "));");
        std::string cond = body->getCExpr(e);
        e.emitBlockOpen("if (!vyne_is_truthy(" + cond + ")) {");
        e.emit(everyTemp + " = false;");
        e.emit("break;");
        e.emitBlockClose();
        e.emitBlockClose();
        e.emitBlockClose();
        e.emit("VyneValue " + resTemp + " = vyne_bool(" + everyTemp + ");");
        return resTemp;
    }

    // --- COLLECT / FILTER / UNIQUE mode ---
    std::string listTemp = e.newTemp("res");
    e.emit("VyneValue " + listTemp + " = vyne_array_create(0);");
    e.emitBlockOpen("if (" + collection + ".type == V_ARRAY) {");
    e.emit("int64_t " + sizeTemp + " = " + collection + ".as.arr->size;");
    e.emitBlockOpen("for (int64_t " + iTemp + " = 0; " +
                    iTemp + " < " + sizeTemp + "; " + iTemp + "++) {");
    e.emit("VyneValue " + elemVar + " = vyne_array_get(" +
           collection + ", vyne_int(" + iTemp + "));");

    switch (mode) {
        case ForMode::COLLECT: {
            std::string result = body->getCExpr(e);
            e.emit("vyne_array_push(" + listTemp + ", " + result + ");");
            break;
        }
        case ForMode::FILTER: {
            std::string cond = body->getCExpr(e);
            e.emitBlockOpen("if (vyne_is_truthy(" + cond + ")) {");
            e.emit("vyne_array_push(" + listTemp + ", " + elemVar + ");");
            e.emitBlockClose();
            break;
        }
        case ForMode::UNIQUE: {
            std::string dupCheck = e.newTemp("seen");
            e.emit("bool " + dupCheck + " = vyne_array_contains(" +
                   listTemp + ", " + elemVar + ");");
            e.emitBlockOpen("if (!" + dupCheck + ") {");
            e.emit("vyne_array_push(" + listTemp + ", " + elemVar + ");");
            e.emitBlockClose();
            break;
        }
        default: break;
    }

    e.emitBlockClose();
    e.emitBlockClose();
    return listTemp;
}

// ============================================================
// FUNCTIONS
// ============================================================

static void emitFunctionBody(C_Emitter& e,
                             const std::vector<Parameter>& parameters,
                             const std::vector<std::shared_ptr<ASTNode>>& body,
                             const std::string& mangledName) {
    // Parameters
    for (size_t i = 0; i < parameters.size(); ++i) {
        std::string paramSanitized = parameters[i].name;
        std::replace(paramSanitized.begin(), paramSanitized.end(), '.', '_');
        std::string paramName = "v_" + mangledName + "_" + paramSanitized;

        e.registerDeclaration(paramName);
        e.emit("VyneValue " + paramName +
               " = (arg_count > " + std::to_string(i) +
               ") ? args[" + std::to_string(i) + "] : vyne_null();");
    }

    // Return-value slot (shared by defer and try/catch paths).
    std::string retVar = "__ret_" + mangledName;
    std::string retFlag = "__returning_" + mangledName;
    std::string cleanupLabel = "__cleanup_" + mangledName;

    e.emit("VyneValue " + retVar + " = vyne_null();");
    e.emit("int " + retFlag + " = 0;");

    e.setReturnVars(retVar, retFlag);

    // Top-level defers (unchanged collection logic)
    std::vector<DeferNode*> defers;
    for (const auto& s : body) {
        if (s && s->type() == NodeType::DEFER) {
            auto* d = static_cast<DeferNode*>(s.get());
            d->markCollected();
            defers.push_back(d);
        }
    }

    if (!defers.empty()) {
        e.pushDeferContext(cleanupLabel, retVar);
    }

    for (const auto& stmt : body)
        if (stmt) stmt->compile(e);

        if (!defers.empty()) {
        e.emit("goto " + cleanupLabel + ";");

        e.dedent();
        e.emit(cleanupLabel + ":");
        e.indent();

        for (auto it = defers.rbegin(); it != defers.rend(); ++it) {
            (*it)->getBody()->compile(e);
        }

        e.emit("return " + retVar + ";");
        e.popDeferContext();
    } else {
        e.emit("return " + retVar + ";");
    }

    e.clearReturnVars();
}

void FunctionNode::compile(C_Emitter& e) const {
    std::string mangledName = originalName;
    std::replace(mangledName.begin(), mangledName.end(), '.', '_');

    if (!targetModule.empty()) {
        mangledName = targetModule + "_" + mangledName;
    }

    e.emitGlobalDecl("VyneValue fn_" + mangledName + "(int arg_count, VyneValue* args);");
    e.pushFunctionContext();
    e.enterFunction(mangledName);

    e.emit("// fn: " + originalName);
    e.emitBlockOpen("VyneValue fn_" + mangledName +
                    "(int arg_count, VyneValue* args) {");

    emitFunctionBody(e, parameters, body, mangledName);

    e.emitBlockClose();
    e.emit("");

    e.exitFunction();
    e.popFunctionContext();
}

void FunctionNode::compileAs(C_Emitter& e, const std::string& mangledName) const {
    std::string name = mangledName;
    std::replace(name.begin(), name.end(), '.', '_');

    e.emitGlobalDecl("VyneValue fn_" + name + "(int arg_count, VyneValue* args);");
    e.pushFunctionContext();
    e.enterFunction(name);
    e.emit("// fn (aliased): " + mangledName);
    e.emitBlockOpen("VyneValue fn_" + name + "(int arg_count, VyneValue* args) {");

    emitFunctionBody(e, parameters, body, name);

    e.emitBlockClose();
    e.emit("");

    e.exitFunction();
    e.popFunctionContext();
}

std::string FunctionNode::getCExpr(C_Emitter& e) const {
    compile(e);
    std::string name = originalName;
    std::replace(name.begin(), name.end(), '.', '_');
    if (!targetModule.empty()) {
        name = targetModule + "_" + name;
    }
    return "fn_" + name;
}

// ============================================================
// FUNCTION CALL
// ============================================================

std::string FunctionCallNode::getCExpr(C_Emitter& e) const {
    // --- Build ordered argument list (handles named args) ---
    std::vector<ASTNode*> orderedArgs;
    if (hasNamedArguments()) {
        const auto* sig = e.getFunctionSignature(originalName);
        if (!sig) {
            throw std::runtime_error(
                "Compile Error: named arguments used for function '" + originalName +
                "', but its signature is unknown. Define the function before calling it "
                "(line " + std::to_string(lineNumber) + ").");
        }

        std::unordered_map<std::string, ASTNode*> nameToArg;
        for (const auto& [name, arg] : namedArguments) {
            if (nameToArg.count(name)) {
                throw std::runtime_error(
                    "Compile Error: duplicate named argument '" + name +
                    "' in call to '" + originalName + "' (line " +
                    std::to_string(lineNumber) + ").");
            }
            nameToArg[name] = arg.get();
        }

        for (const auto& paramName : *sig) {
            auto it = nameToArg.find(paramName);
            if (it == nameToArg.end()) {
                throw std::runtime_error(
                    "Compile Error: missing argument '" + paramName +
                    "' in call to '" + originalName + "' (line " +
                    std::to_string(lineNumber) + ").");
            }
            orderedArgs.push_back(it->second);
        }

        // Extra-arg check
        for (const auto& [name, _] : nameToArg) {
            bool found = false;
            for (const auto& pn : *sig) if (pn == name) { found = true; break; }
            if (!found) {
                throw std::runtime_error(
                    "Compile Error: unknown argument '" + name +
                    "' in call to '" + originalName + "' (line " +
                    std::to_string(lineNumber) + ").");
            }
        }
    } else {
        for (const auto& a : arguments) orderedArgs.push_back(a.get());
    }

    int argSize = (int)orderedArgs.size();
    std::string retTemp = e.newTemp("ret");

    std::string mangledName = originalName;
    std::replace(mangledName.begin(), mangledName.end(), '.', '_');

    // ----------------------------------------------------------------
    // Interface constructors take their arguments directly — no
    // `args[]` array, no arena allocation, no deep-copy dance.
    // Handle that case first so we don't emit dead argument code.
    // ----------------------------------------------------------------
    if (e.isInterface(originalName) || e.isInterface(mangledName)) {
        if (hasNamedArguments()) {
            throw std::runtime_error(
                "Compile Error: named arguments are not supported for interface constructors "
                "(line " + std::to_string(lineNumber) + ").");
        }

        const std::vector<std::string>* defaults =
            e.getInterfaceDefaults(originalName);
        if (!defaults) defaults = e.getInterfaceDefaults(mangledName);

        std::vector<std::string> argStrs;
        argStrs.reserve(orderedArgs.size());
        for (auto* argNode : orderedArgs) {
            argStrs.push_back(argNode->getCExpr(e));
        }

        if (defaults) {
            for (size_t i = argStrs.size(); i < defaults->size(); ++i) {
                argStrs.push_back((*defaults)[i]);
            }
        }

        std::string directArgs;
        for (size_t i = 0; i < argStrs.size(); ++i) {
            if (i > 0) directArgs += ", ";
            directArgs += argStrs[i];
        }

        e.emit("VyneValue " + retTemp + " = struct_" + mangledName +
               "(" + directArgs + ");");
        return retTemp;
    }

    // ----------------------------------------------------------------
    // Normal function call: build args[] on the arena, deep-copy
    // arrays/maps so callee mutations don't leak back to the caller.
    // ----------------------------------------------------------------
    std::string argArr = e.newTemp("args");

    if (argSize > 0) {
        e.emit("VyneValue* " + argArr + " = (VyneValue*)arena_alloc(sizeof(VyneValue) * " +
               std::to_string(argSize) + ");");
        for (int i = 0; i < argSize; ++i) {
            std::string val = orderedArgs[i]->getCExpr(e);
            std::string copyTmp = e.newTemp("argc");
            e.emit("VyneValue " + copyTmp + " = " + val + ";");
            e.emit("if (" + copyTmp + ".type == V_ARRAY) " + copyTmp + " = vyne_array_deepcopy(" + copyTmp + ");");
            e.emit("else if (" + copyTmp + ".type == V_MAP) " + copyTmp + " = vyne_map_deepcopy(" + copyTmp + ");");
            e.emit(argArr + "[" + std::to_string(i) + "] = " + copyTmp + ";");
        }
    } else {
        e.emit("VyneValue* " + argArr + " = NULL;");
    }

    e.emit("VyneValue " + retTemp + " = fn_" + mangledName +
           "(" + std::to_string(argSize) + ", " + argArr + ");");
    return retTemp;
}

void FunctionCallNode::compile(C_Emitter& e) const { getCExpr(e); }

// ============================================================
// ARRAY AND INDEX ACCESS
// ============================================================

std::string ArrayNode::getCExpr(C_Emitter& e) const {
    std::string temp = e.newTemp("arr");
    int size = (int)elements.size();
    e.emit("VyneValue " + temp + " = vyne_array_create(" +
           std::to_string(size) + ");");
    for (int i = 0; i < size; i++) {
        std::string elem = elements[i]->getCExpr(e);
        e.emit("vyne_array_set(" + temp + ", vyne_int(" +
               std::to_string(i) + "), " + elem + ");");
    }
    return temp;
}

void ArrayNode::compile(C_Emitter& e) const { getCExpr(e); }

std::string IndexAccessNode::getCExpr(C_Emitter& e) const {
    std::string b = base->getCExpr(e);
    std::string idx = index->getCExpr(e);
    std::string temp = e.newTemp("idx");
    e.emit("VyneValue " + temp + " = vyne_index_get(" + b + ", " + idx + ");");
    return temp;
}

void IndexAccessNode::compile(C_Emitter& e) const { getCExpr(e); }

void IndexAssignmentNode::compile(C_Emitter& e) const {
    std::string b = base->getCExpr(e);
    std::string i = index->getCExpr(e);
    std::string r = rhs->getCExpr(e);
    e.emit("vyne_index_set(" + b + ", " + i + ", " + r + ");");
}

std::string IndexAssignmentNode::getCExpr(C_Emitter& e) const {
    compile(e);
    return "vyne_null()";
}

// ============================================================
// RANGE
// ============================================================

std::string RangeNode::getCExpr(C_Emitter& e) const {
    if (!left || !right) return "vyne_null()";
    std::string l = left->getCExpr(e);
    std::string r = right->getCExpr(e);
    std::string temp = e.newTemp("rng");
    e.emit("VyneValue " + temp + " = vyne_range_create(" + l + ", " + r + ");");
    return temp;
}

void RangeNode::compile(C_Emitter& e) const { getCExpr(e); }

// ============================================================
// BUILT-INS
// ============================================================

std::string BuiltInCallNode::getCExpr(C_Emitter& e) const {
    if (funcName == "out") {
        for (const auto& arg : arguments) {
            e.emit("vyne_out(" + arg->getCExpr(e) + ");");
        }
        return "vyne_null()";
    }
    if (funcName == "string") {
        if (arguments.empty()) return "vyne_string(\"\")";
        std::string temp = e.newTemp("str");
        e.emit("VyneValue " + temp + " = vyne_to_string(" +
               arguments[0]->getCExpr(e) + ");");
        return temp;
    }
    if (funcName == "int64") {
        std::string arg = arguments.empty() ? "vyne_null()" : arguments[0]->getCExpr(e);
        return "vyne_to_int(" + arg + ")";
    }
    if (funcName == "float64") {
        if (arguments.empty()) return "vyne_float(0.0)";
        return "vyne_to_float(" + arguments[0]->getCExpr(e) + ")";
    }
    if (funcName == "sizeof") {
        if (arguments.empty()) return "vyne_int(0)";
        std::string arg = arguments[0]->getCExpr(e);
        std::string temp = e.newTemp("sz");
        e.emit("VyneValue " + temp + " = vyne_int(vyne_get_sizeof(" + arg + "));");
        return temp;
    }
    if (funcName == "type") {
        if (arguments.empty()) return "vyne_string(\"null\")";
        std::string temp = e.newTemp("type");
        e.emit("VyneValue " + temp + " = vyne_string(vyne_get_type_name(" +
               arguments[0]->getCExpr(e) + "));");
        return temp;
    }
    if (funcName == "free") {
        if (arguments.empty()) return "vyne_null()";
        // Free is a no-op in the C runtime since we use arena allocation
        e.emit("// free() called on: " + arguments[0]->getCExpr(e));
        return "vyne_null()";
    }
    if (funcName == "exit") {
        if (!arguments.empty()) {
            e.emit("exit((int)" + arguments[0]->getCExpr(e) + ".as.i64);");
        } else {
            e.emit("exit(0);");
        }
        return "vyne_null()";
    }
    if (funcName == "sequence") {
        if (arguments.size() < 2) return "vyne_array_create(0)";
        std::string start = arguments[0]->getCExpr(e);
        std::string end   = arguments[1]->getCExpr(e);
        std::string temp  = e.newTemp("seq");
        std::string iv    = e.newTemp("i");
        e.emit("VyneValue " + temp + " = vyne_array_create(0);");
        e.emitBlockOpen("for (int64_t " + iv + " = (" + start + ").as.i64; "
                        + iv + " < (" + end + ").as.i64; " + iv + "++) {");
        e.emit("vyne_array_push(" + temp + ", vyne_int(" + iv + "));");
        e.emitBlockClose();
        return temp;
    }
    if (funcName == "map") {
        return "vyne_map_create()";
    }

    e.emit("/* unknown built-in: " + funcName + " */");
    return "vyne_null()";
}

void BuiltInCallNode::compile(C_Emitter& e) const { getCExpr(e); }

// ============================================================
// PROGRAM / BLOCK
// ============================================================

void ProgramNode::compile(C_Emitter& e) const {
    for (const auto& stmt : statements) {
        if (stmt && stmt->type() == NodeType::FUNCTION) {
            auto* fn = static_cast<FunctionNode*>(stmt.get());
            std::vector<std::string> paramNames;
            for (const auto& p : fn->getParameters()) paramNames.push_back(p.name);
            e.registerFunctionSignature(fn->getOriginalName(), std::move(paramNames));
        }
    }

    for (const auto& stmt : statements)
        if (stmt) stmt->compile(e);
}

void ProgramNode::compileAliased(C_Emitter& e, const std::string& alias) const {
    e.registerGroup(alias);
    e.pushGlobalContext();
    e.emit("// --- import as " + alias + " ---");

    for (const auto& stmt : statements) {
        if (!stmt) continue;

        if (stmt->type() == NodeType::FUNCTION) {
            auto* fn = static_cast<FunctionNode*>(stmt.get());
            e.popGlobalContext();
            fn->compileAs(e, alias + "_" + fn->getOriginalName());
            e.pushGlobalContext();
        }
        else if (stmt->type() == NodeType::ASSIGNMENT) {
            auto* assign = static_cast<AssignmentNode*>(stmt.get());
            std::string mangled = "v_" + alias + "_" + assign->getOriginalName();
            std::replace(mangled.begin(), mangled.end(), '.', '_');
            e.emit("VyneValue " + mangled + ";");
            e.popGlobalContext();
            std::string val = assign->getRHS()->getCExpr(e);
            e.emit(mangled + " = " + val + ";");
            e.pushGlobalContext();
        }
        else if (stmt->type() == NodeType::IMPORT) {
            continue;
        }
        else {
            e.popGlobalContext();
            stmt->compile(e);
            e.pushGlobalContext();
        }
    }
    e.popGlobalContext();

    std::string modVirtualVar = "v_" + alias;
    std::replace(modVirtualVar.begin(), modVirtualVar.end(), '.', '_');

    if (e.getGlobalVars().count(modVirtualVar) == 0) {
        e.registerDeclaration(modVirtualVar);
        e.emitGlobalDecl("VyneValue " + modVirtualVar + ";");
    }

    e.pushMainContext();
    e.emit("// Virtual Module registration for " + alias);
    e.emitBlockOpen("{");
    std::string tempS = e.newTemp("mod_s");
    e.emit("VyneStruct* " + tempS + " = (VyneStruct*)arena_alloc(sizeof(VyneStruct));");
    e.emit(tempS + "->type_name = \"" + alias + "\";");
    e.emit(tempS + "->field_count = 0;");
    e.emit(tempS + "->fields = NULL;");
    e.emit(tempS + "->methods = NULL;");
    e.emit(tempS + "->method_count = 0;");
    e.emit(modVirtualVar + ".type = V_STRUCT; " + modVirtualVar + ".as.strct = " + tempS + ";");

    for (const auto& stmt : statements) {
        if (stmt && stmt->type() == NodeType::FUNCTION) {
            auto* fn = static_cast<FunctionNode*>(stmt.get());
            std::string fnOrigName = fn->getOriginalName();
            std::string fnRealCName = "fn_" + alias + "_" + fnOrigName;
            std::replace(fnRealCName.begin(), fnRealCName.end(), '.', '_');

            std::string wrapperName = "wrap_" + alias + "_" + fnOrigName;
            std::replace(wrapperName.begin(), wrapperName.end(), '.', '_');

            e.pushFunctionContext();
            e.emitGlobalDecl("VyneValue " + wrapperName + "(int arg_count, VyneValue* args);");
            e.emitBlockOpen("VyneValue " + wrapperName + "(int arg_count, VyneValue* args) {");
            e.emit("if (arg_count > 0) {");
            e.emit("    return " + fnRealCName + "(arg_count - 1, args + 1);");
            e.emit("}");
            e.emit("return " + fnRealCName + "(arg_count, args);");
            e.emitBlockClose();
            e.popFunctionContext();

            e.emit("vyne_register_method(\"" + alias + "\", \"" +
                   fnOrigName + "\", " + wrapperName + ");");
        }
    }
    e.emitBlockClose();
    e.popMainContext();
}

std::string ProgramNode::getCExpr(C_Emitter& e) const {
    compile(e);
    return "vyne_null()";
}

void BlockNode::compile(C_Emitter& e) const {
    e.emitBlockOpen("{");
    for (const auto& stmt : statements)
        if (stmt) stmt->compile(e);
    e.emitBlockClose();
}

std::string BlockNode::getCExpr(C_Emitter& e) const {
    if (statements.empty()) return "vyne_null()";

    if (statements.size() == 1) {
        return statements[0]->getCExpr(e);
    }

    for (size_t i = 0; i + 1 < statements.size(); ++i) {
        if (statements[i]) statements[i]->compile(e);
    }
    return statements.back()->getCExpr(e);
}
// ============================================================
// TERNARY
// ============================================================

std::string TernaryNode::getCExpr(C_Emitter& e) const {
    std::string cond = condition->getCExpr(e);
    std::string tVal = trueExpr->getCExpr(e);
    std::string fVal = falseExpr->getCExpr(e);
    std::string temp = e.newTemp("tern");
    e.emit("VyneValue " + temp + " = vyne_is_truthy(" + cond + ") ? " +
           tVal + " : " + fVal + ";");
    return temp;
}

void TernaryNode::compile(C_Emitter& e) const { getCExpr(e); }

// ============================================================
// MEMBER ACCESS / ASSIGNMENT
// ============================================================

std::string MemberAccessNode::getCExpr(C_Emitter& e) const {
    if (receiver->type() == NodeType::VARIABLE) {
        auto* var = static_cast<VariableNode*>(receiver.get());
        std::string modName = var->getOriginalName();

        std::string native = e.getNativeMapping(modName, memberName, false);
        if (native.find("v_" + modName) == std::string::npos) {
            return native;
        }

        if (e.isGroup(modName)) {
            std::string name = "v_" + modName + "_" + memberName;
            std::replace(name.begin(), name.end(), '.', '_');
            return name;
        }
    }

    std::string recv = receiver->getCExpr(e);
    uint32_t fid = StringPool::intern(memberName);
    return "vyne_struct_get(" + recv + ", " + std::to_string(fid) + ")";
}

void MemberAccessNode::compile(C_Emitter& e) const {
    // Bare member access as statement — no-op
}

void MemberAssignmentNode::compile(C_Emitter& e) const {
    std::string val = rhs->getCExpr(e);

    if (receiver->type() == NodeType::VARIABLE) {
        auto* var = static_cast<VariableNode*>(receiver.get());
        std::string modName = var->getOriginalName();

        if (e.isGroup(modName)) {
            std::string name = "v_" + modName + "_" + memberName;
            std::replace(name.begin(), name.end(), '.', '_');
            e.emit(name + " = " + val + ";");
            return;
        }

        if (modName == "self") {
            uint32_t fid = StringPool::intern(memberName);
            e.emit("vyne_struct_set(v_self, " + std::to_string(fid) + ", \"" + memberName + "\", " + val + ");");
            return;
        }
    }

    std::string recv = receiver->getCExpr(e);
    uint32_t fid = StringPool::intern(memberName);
    e.emit("vyne_struct_set(" + recv + ", " + std::to_string(fid) + ", \"" + memberName + "\", " + val + ");");
}

std::string MemberAssignmentNode::getCExpr(C_Emitter& e) const {
    compile(e);
    if (receiver->type() == NodeType::VARIABLE) {
        auto* var = static_cast<VariableNode*>(receiver.get());
        std::string name = "v_" + var->getOriginalName() + "_" + memberName;
        std::replace(name.begin(), name.end(), '.', '_');
        return name;
    }
    return receiver->getCExpr(e) + "_" + memberName;
}

// ============================================================
// GROUP
// ============================================================

void GroupNode::compile(C_Emitter& e) const {
    e.registerGroup(groupName);
    e.pushGlobalContext();
    e.emit("// --- Group: " + groupName + " ---");

    for (const auto& stmt : statements) {
        if (!stmt) continue;
        if (stmt->type() == NodeType::ASSIGNMENT) {
            auto* assign = static_cast<AssignmentNode*>(stmt.get());
            std::string mangled = "v_" + groupName + "_" + assign->getOriginalName();
            std::replace(mangled.begin(), mangled.end(), '.', '_');
            e.emit("VyneValue " + mangled + ";");
            e.popGlobalContext();
            std::string val = assign->getRHS()->getCExpr(e);
            e.emit(mangled + " = " + val + ";");
            e.pushGlobalContext();
                } else if (stmt->type() == NodeType::FUNCTION) {
            e.popGlobalContext();
            stmt->compile(e);
            e.pushGlobalContext();
        } else if (stmt->type() == NodeType::INTERFACE) {
            e.popGlobalContext();
            e.setGroupPrefix(groupName);
            stmt->compile(e);
            e.clearGroupPrefix();
            e.pushGlobalContext();
        }
    }
    e.popGlobalContext();
}

std::string GroupNode::getCExpr(C_Emitter& e) const {
    compile(e);
    return "vyne_null()";
}

// ============================================================
// MODULE
// ============================================================

void ModuleNode::compile(C_Emitter& e) const {
    std::string base = FileUtils::getExeDir();
    std::filesystem::path moduleBase = std::filesystem::path(base) / "vyne" / "runtime" / "modules";

    if (originalName == "vmath")  e.addInclude((moduleBase / "vmath.h").string());
    if (originalName == "vcore")  e.addInclude((moduleBase / "vcore.h").string());
    if (originalName == "vaudio") e.addInclude((moduleBase / "vaudio.h").string());
    if (originalName == "vglib")  e.addInclude((moduleBase / "vglib.h").string());

    e.registerGroup(originalName);
}

std::string ModuleNode::getCExpr(C_Emitter& e) const { return "vyne_null()"; }

// ============================================================
// INTERFACE / STRUCT
// ============================================================

void InterfaceNode::compile(C_Emitter& e) const {
    // Determine the effective module: explicit moduleName wins, else use
    // the currently active group prefix (set by GroupNode::compile).
    std::string effectiveModule = moduleName;
    if (effectiveModule.empty()) effectiveModule = e.getGroupPrefix();

    std::string fullName = effectiveModule.empty()
        ? interfaceName
        : (effectiveModule + "." + interfaceName);

    std::string cStructName = effectiveModule.empty()
        ? interfaceName
        : (effectiveModule + "_" + interfaceName);
    std::replace(cStructName.begin(), cStructName.end(), '.', '_');
    std::replace(cStructName.begin(), cStructName.end(), '_', '_');

    // Register the interface under its bare name, dotted name, and
    // underscore-mangled name so that every calling convention resolves.
    e.registerInterface(interfaceName);
    if (!effectiveModule.empty()) {
        e.registerInterface(effectiveModule + "." + interfaceName);
        e.registerInterface(effectiveModule + "_" + interfaceName);
    }

    // Per-field defaults (used to pad short constructor calls).
    {
        std::vector<std::string> defaults;
        defaults.reserve(members.size());
        for (const auto& m : members) {
            switch (m.type) {
                case VType::String:  defaults.push_back("vyne_string(\"\")"); break;
                case VType::Int64:   defaults.push_back("vyne_int(0)");        break;
                case VType::Float64: defaults.push_back("vyne_float(0.0)");    break;
                case VType::Array:   defaults.push_back("vyne_array_create(0)"); break;
                case VType::Map:     defaults.push_back("vyne_map_create()");   break;
                default:             defaults.push_back("vyne_null()");         break;
            }
        }
        e.registerInterfaceDefaults(interfaceName, defaults);
        if (!effectiveModule.empty()) {
            e.registerInterfaceDefaults(effectiveModule + "." + interfaceName, defaults);
            e.registerInterfaceDefaults(effectiveModule + "_" + interfaceName, defaults);
        }
    }

    e.pushFunctionContext();

    e.emit("// interface: " + fullName);
    std::string params;
    for (size_t i = 0; i < members.size(); ++i) {
        if (i > 0) params += ", ";
        params += "VyneValue v_" + members[i].name;
    }

    e.emitBlockOpen("VyneValue struct_" + cStructName + "(" + params + ") {");

    std::string temp = e.newTemp("s");
    e.emit("VyneStruct* " + temp + " = (VyneStruct*)arena_alloc(sizeof(VyneStruct));");
    e.emit(temp + "->type_name = \"" + fullName + "\";");
    e.emit(temp + "->field_count = " + std::to_string(members.size()) + ";");
    e.emit(temp + "->fields = (VyneField*)arena_alloc(sizeof(VyneField) * " +
           std::to_string(members.size()) + ");");
    e.emit(temp + "->methods = NULL;");
    e.emit(temp + "->method_count = 0;");

    for (size_t i = 0; i < members.size(); ++i) {
        uint32_t fid = StringPool::intern(members[i].name);
        e.emit(temp + "->fields[" + std::to_string(i) + "].id = " + std::to_string(fid) + ";");
        e.emit(temp + "->fields[" + std::to_string(i) + "].name = \"" + members[i].name + "\";");
        e.emit(temp + "->fields[" + std::to_string(i) + "].value = v_" + members[i].name + ";");
    }

    e.emit("VyneValue res; res.type = V_STRUCT; res.as.strct = " + temp + ";");
    e.emit("return res;");
    e.emitBlockClose();
    e.emit("");

    for (const auto& method : methods) {
        if (!method) continue;
        auto* fn = static_cast<FunctionNode*>(method.get());
        std::string methodName = cStructName + "_" + fn->getOriginalName();
        std::replace(methodName.begin(), methodName.end(), '.', '_');

        e.emitGlobalDecl("VyneValue fn_" + methodName + "(int arg_count, VyneValue* args);");
        e.pushFunctionContext();
        e.emitBlockOpen("VyneValue fn_" + methodName + "(int arg_count, VyneValue* args) {");

        e.emit("VyneValue v_self = (arg_count > 0) ? args[0] : vyne_null();");

        const auto& params2 = fn->getParameters();
        for (size_t i = 0; i < params2.size(); ++i) {
            std::string paramName = "v_" + params2[i].name;
            std::replace(paramName.begin(), paramName.end(), '.', '_');
            e.emit("VyneValue " + paramName +
                   " = (arg_count > " + std::to_string(i + 1) +
                   ") ? args[" + std::to_string(i + 1) + "] : vyne_null();");
        }

        for (const auto& stmt : fn->getBody())
            if (stmt) stmt->compile(e);

        e.emit("return vyne_null();");
        e.emitBlockClose();
        e.emit("");
        e.popFunctionContext();

        e.pushMainContext();
        e.emit("vyne_register_method(\"" + fullName + "\", \"" +
               fn->getOriginalName() + "\", fn_" + methodName + ");");
        e.popMainContext();
    }

    e.popFunctionContext();
}

std::string InterfaceNode::getCExpr(C_Emitter& e) const { return "vyne_null()"; }

// ============================================================
// METHOD CALL
// ============================================================

std::string MethodCallNode::getCExpr(C_Emitter& e) const {
    if (receiver->type() == NodeType::VARIABLE) {
        auto* var = static_cast<VariableNode*>(receiver.get());
        std::string name = var->getOriginalName();

        if (e.isGroup(name)) {
            std::string ifaceDotted = name + "." + this->methodName;
            std::string ifaceMangled = name + "_" + this->methodName;

            if (e.isInterface(ifaceDotted) || e.isInterface(ifaceMangled)) {
                const std::vector<std::string>* defaults =
                    e.getInterfaceDefaults(ifaceDotted);
                if (!defaults) defaults = e.getInterfaceDefaults(ifaceMangled);
                if (!defaults) defaults = e.getInterfaceDefaults(this->methodName);

                std::vector<std::string> argStrs;
                argStrs.reserve(arguments.size());
                for (const auto& argNode : arguments) {
                    argStrs.push_back(argNode->getCExpr(e));
                }
                if (defaults) {
                    for (size_t i = argStrs.size(); i < defaults->size(); ++i) {
                        argStrs.push_back((*defaults)[i]);
                    }
                }

                std::string directArgs;
                for (size_t i = 0; i < argStrs.size(); ++i) {
                    if (i > 0) directArgs += ", ";
                    directArgs += argStrs[i];
                }

                std::string cName = name + "_" + this->methodName;
                std::replace(cName.begin(), cName.end(), '.', '_');

                std::string resTemp = e.newTemp("g_iface");
                e.emit("VyneValue " + resTemp + " = struct_" + cName + "(" + directArgs + ");");
                return resTemp;
            }

            int argSize = (int)arguments.size();
            std::string argArr = e.newTemp("g_args");

            if (argSize > 0) {
                e.emit("VyneValue* " + argArr + " = (VyneValue*)arena_alloc(sizeof(VyneValue) * " +
                       std::to_string(argSize) + ");");
                for (int i = 0; i < argSize; ++i) {
                    e.emit(argArr + "[" + std::to_string(i) + "] = " +
                           arguments[i]->getCExpr(e) + ";");
                }
            } else {
                e.emit("VyneValue* " + argArr + " = NULL;");
            }

            std::string resTemp = e.newTemp("g_ret");
            std::string gMethodName = name + "_" + this->methodName;
            std::replace(gMethodName.begin(), gMethodName.end(), '.', '_');
            e.emit("VyneValue " + resTemp + " = fn_" + gMethodName +
                   "(" + std::to_string(argSize) + ", " + argArr + ");");
            return resTemp;
        }

        std::string nativeFunc = e.getNativeMapping(name, methodName, true);
        if (nativeFunc.find("v_" + name) == std::string::npos) {
            std::string argStr;
            for (size_t i = 0; i < arguments.size(); ++i) {
                if (i > 0) argStr += ", ";
                argStr += arguments[i]->getCExpr(e);
            }
            std::string resTemp = e.newTemp("n_ret");
            e.emit("VyneValue " + resTemp + " = " + nativeFunc + "(" + argStr + ");");
            return resTemp;
        }
    }

    std::string recvRaw = receiver->getCExpr(e);
    std::string recv = e.newTemp("m_recv");
    e.emit("VyneValue " + recv + " = " + recvRaw + ";");

    // Array methods
    if (methodName == "push") {
        for (const auto& argNode : arguments) {
            e.emit("vyne_array_push(" + recv + ", " + argNode->getCExpr(e) + ");");
        }
        return recv;
    }
    if (methodName == "pop") {
        std::string temp = e.newTemp("pop");
        e.emit("VyneValue " + temp + " = vyne_array_pop(" + recv + ");");
        return temp;
    }
    if (methodName == "reverse") {
        e.emit("vyne_array_reverse(" + recv + ");");
        return recv;
    }
    if (methodName == "length" || methodName == "size") {
        std::string temp = e.newTemp("len");
        e.emit("VyneValue " + temp + " = vyne_int(vyne_get_sizeof(" + recv + "));");
        return temp;
    }

    if (methodName == "pop_front") {
        std::string temp = e.newTemp("pf");
        e.emit("VyneValue " + temp + " = vyne_array_pop_front(" + recv + ");");
        return temp;
    }
    if (methodName == "back") {
        std::string temp = e.newTemp("bk");
        e.emit("VyneValue " + temp + " = vyne_array_back(" + recv + ");");
        return temp;
    }
    if (methodName == "delete_at") {
        std::string idx = arguments[0]->getCExpr(e);
        std::string temp = e.newTemp("delat");
        e.emit("VyneValue " + temp + " = vyne_array_delete_at(" + recv +
               ", (" + idx + ").as.i64);");
        return temp;
    }
    if (methodName == "sort") {
        e.emit("vyne_array_sort(" + recv + ");");
        return recv;
    }
    if (methodName == "place_all") {
        std::string v = arguments[0]->getCExpr(e);
        std::string c = arguments[1]->getCExpr(e);
        e.emit("vyne_array_place_all(" + recv + ", " + v +
               ", (" + c + ").as.i64);");
        return recv;
    }

        // --- String methods ---
    if (methodName == "substr") {
        if (arguments.empty()) {
            throw std::runtime_error(
                "Compile Error: substr() requires at least 1 argument (line " +
                std::to_string(lineNumber) + ")");
        }
        std::string s = arguments[0]->getCExpr(e);
        std::string c = (arguments.size() >= 2) ? arguments[1]->getCExpr(e) : "vyne_int(-1)";
        std::string temp = e.newTemp("substr");
        e.emit("VyneValue " + temp + " = vyne_string_substr(" + recv +
               ", (" + s + ").as.i64, (" + c + ").as.i64);");
        return temp;
    }
    if (methodName == "find") {
        if (arguments.empty()) {
            throw std::runtime_error(
                "Compile Error: find() requires 1 argument (line " +
                std::to_string(lineNumber) + ")");
        }
        std::string target = arguments[0]->getCExpr(e);
        std::string temp = e.newTemp("find");
        e.emit("VyneValue " + temp + " = vyne_string_find(" + recv + ", " + target + ");");
        return temp;
    }
    if (methodName == "uppercase") {
        std::string temp = e.newTemp("up");
        e.emit("VyneValue " + temp + " = vyne_string_uppercase(" + recv + ");");
        return temp;
    }
    if (methodName == "lowercase") {
        std::string temp = e.newTemp("lo");
        e.emit("VyneValue " + temp + " = vyne_string_lowercase(" + recv + ");");
        return temp;
    }
    if (methodName == "trim") {
        std::string temp = e.newTemp("tr");
        e.emit("VyneValue " + temp + " = vyne_string_trim(" + recv + ");");
        return temp;
    }
    if (methodName == "replace") {
        if (arguments.size() < 2) {
            throw std::runtime_error(
                "Compile Error: replace() requires 2 arguments (line " +
                std::to_string(lineNumber) + ")");
        }
        std::string o = arguments[0]->getCExpr(e);
        std::string n = arguments[1]->getCExpr(e);
        std::string temp = e.newTemp("rep");
        e.emit("VyneValue " + temp + " = vyne_string_replace(" + recv +
               ", " + o + ", " + n + ");");
        return temp;
    }

    if (methodName == "fields") {
        std::string temp = e.newTemp("flds");
        e.emit("VyneValue " + temp + " = vyne_array_create(0);");
        e.emitBlockOpen("if (" + recv + ".type == V_STRUCT) {");
        e.emit("VyneStruct* __s = " + recv + ".as.strct;");
        e.emitBlockOpen("for (int __i = 0; __i < __s->field_count; __i++) {");
        e.emit("vyne_array_push(" + temp + ", vyne_string(__s->fields[__i].name));");
        e.emitBlockClose();
        e.emitBlockClose();
        return temp;
    }

    if (methodName == "has") {
        std::string arg = arguments[0]->getCExpr(e);
        std::string temp = e.newTemp("has");
        e.emit("VyneValue " + temp + " = vyne_bool(vyne_map_has(" + recv + ", " + arg + "));");
        return temp;
    }
    if (methodName == "keys") {
        std::string temp = e.newTemp("keys");
        e.emit("VyneValue " + temp + " = vyne_map_keys(" + recv + ");");
        return temp;
    }
    if (methodName == "values") {
        std::string temp = e.newTemp("vals");
        e.emit("VyneValue " + temp + " = vyne_map_values(" + recv + ");");
        return temp;
    }
    if (methodName == "set") {
        std::string k = arguments[0]->getCExpr(e);
        std::string v = arguments[1]->getCExpr(e);
        e.emit("vyne_map_set(" + recv + ", " + k + ", " + v + ");");
        return v;
    }
    if (methodName == "delete") {
        std::string k = arguments[0]->getCExpr(e);
        std::string temp = e.newTemp("del");
        e.emit("VyneValue " + temp + " = vyne_delete_any(" + recv + ", " + k + ");");
        return temp;
    }
    if (methodName == "clear") {
        e.emit("vyne_clear_any(" + recv + ");");
        return recv;
    }

    // Struct method call
    {
        std::string temp = e.newTemp("mret");
        int argSize = (int)arguments.size();
        std::string argArr = e.newTemp("m_args");

        e.emit("VyneValue " + temp + " = vyne_null();");
        e.emitBlockOpen("if (" + recv + ".type == V_STRUCT) {");
        e.emit("VyneValue* " + argArr + " = (VyneValue*)arena_alloc(sizeof(VyneValue) * " +
               std::to_string(argSize + 1) + ");");
        e.emit(argArr + "[0] = " + recv + ";");
        for (int i = 0; i < argSize; ++i) {
            e.emit(argArr + "[" + std::to_string(i + 1) + "] = " +
                   arguments[i]->getCExpr(e) + ";");
        }
        e.emit(temp + " = vyne_struct_call(" + recv + ", \"" + methodName + "\", " +
               std::to_string(argSize + 1) + ", " + argArr + ");");
        e.emitBlockClose();
        return temp;
    }

    return "vyne_null()";
}

void MethodCallNode::compile(C_Emitter& e) const { getCExpr(e); }

// ============================================================
// IMPORT
// ============================================================

void ImportNode::compile(C_Emitter& e) const {
    std::filesystem::path finalPath;
    std::string cleanPath = filePath;
    if (!cleanPath.empty() && (cleanPath[0] == '/' || cleanPath[0] == '\\'))
        cleanPath.erase(0, 1);

    if (isExtern) {
        finalPath = std::filesystem::path(FileUtils::getExeDir())
                    / "vyne" / "modules" / "external" / cleanPath;
    } else {
        finalPath = std::filesystem::path(e.getSourceDir()) / cleanPath;
    }
    finalPath = std::filesystem::weakly_canonical(finalPath);

    if (!std::filesystem::exists(finalPath) || std::filesystem::is_directory(finalPath)) {
        throw std::runtime_error("Vyne Error: import '" + cleanPath +
                                 "' not found at: " + finalPath.string());
    }

    if (e.isAlreadyImported(finalPath.string()))
        return;
    e.markImported(finalPath.string());

    const std::string& source = FileUtils::readFile(finalPath.string());
    auto tokens = tokenize(source);

    SymbolContainer parseEnv;
    Parser parser(std::move(tokens));
    std::unique_ptr<ProgramNode> externalAst = parser.parseProgram(parseEnv);
    if (!externalAst)
        throw std::runtime_error("Import Error: failed to parse '" + cleanPath + "'");

    std::string prevDir = e.getSourceDir();
    e.setSourceDir(finalPath.parent_path().string());

    for (const auto& stmt : externalAst->statements) {
        if (!stmt) continue;
        if (stmt->type() == NodeType::INTERFACE) {
            auto* iface = static_cast<InterfaceNode*>(stmt.get());
            e.registerInterface(iface->getInterfaceName());
            if (!iface->getModuleName().empty()) {
                e.registerInterface(iface->getModuleName() + "." + iface->getInterfaceName());
            }
        }
        if (stmt->type() == NodeType::GROUP) {
            auto* grp = static_cast<GroupNode*>(stmt.get());
            e.registerGroup(grp->getGroupName());
        }
        if (stmt->type() == NodeType::MODULE) {
            auto* mod = static_cast<ModuleNode*>(stmt.get());
            e.registerGroup(mod->getOriginalName());
        }
    }

    if (alias.empty()) {
        for (const auto& stmt : externalAst->statements) {
            if (stmt) stmt->compile(e);
        }
    } else {
        e.registerGroup(alias);
        e.pushGlobalContext();
        e.emit("// --- import as " + alias + " ---");

        for (const auto& stmt : externalAst->statements) {
            if (!stmt) continue;
            if (stmt->type() == NodeType::FUNCTION) {
                auto* fn = static_cast<FunctionNode*>(stmt.get());
                e.popGlobalContext();
                fn->compileAs(e, alias + "_" + fn->getOriginalName());
                e.pushGlobalContext();
            } else if (stmt->type() == NodeType::ASSIGNMENT) {
                auto* assign = static_cast<AssignmentNode*>(stmt.get());
                std::string mangled = "v_" + alias + "_" + assign->getOriginalName();
                std::replace(mangled.begin(), mangled.end(), '.', '_');
                e.emit("VyneValue " + mangled + ";");
                e.popGlobalContext();
                std::string val = assign->getRHS()->getCExpr(e);
                e.emit(mangled + " = " + val + ";");
                e.pushGlobalContext();
            } else {
                e.popGlobalContext();
                stmt->compile(e);
                e.pushGlobalContext();
            }
        }
        e.popGlobalContext();
    }

    std::string targetNamespace = alias.empty() ? finalPath.stem().string() : alias;
    std::string modVirtualVar = "v_" + targetNamespace;
    std::replace(modVirtualVar.begin(), modVirtualVar.end(), '.', '_');

    if (e.getGlobalVars().count(modVirtualVar) == 0) {
        e.registerDeclaration(modVirtualVar);
        e.emitGlobalDecl("VyneValue " + modVirtualVar + ";");
    }

    e.pushMainContext();
    e.emit("// Virtual Module registration for " + targetNamespace);
    e.emitBlockOpen("{");
    std::string tempS = e.newTemp("mod_s");
    e.emit("VyneStruct* " + tempS + " = (VyneStruct*)arena_alloc(sizeof(VyneStruct));");
    e.emit(tempS + "->type_name = \"" + targetNamespace + "\";");
    e.emit(tempS + "->field_count = 0;");
    e.emit(tempS + "->fields = NULL;");
    e.emit(tempS + "->methods = NULL;");
    e.emit(tempS + "->method_count = 0;");
    e.emit(modVirtualVar + ".type = V_STRUCT; " + modVirtualVar + ".as.strct = " + tempS + ";");

    for (const auto& stmt : externalAst->statements) {
        if (stmt && stmt->type() == NodeType::FUNCTION) {
            auto* fn = static_cast<FunctionNode*>(stmt.get());
            std::string fnOrigName = fn->getOriginalName();
            std::string fnRealCName = "fn_" + (alias.empty() ? fnOrigName : (alias + "_" + fnOrigName));
            std::replace(fnRealCName.begin(), fnRealCName.end(), '.', '_');

            std::string wrapperName = "wrap_" + targetNamespace + "_" + fnOrigName;
            std::replace(wrapperName.begin(), wrapperName.end(), '.', '_');

            e.pushFunctionContext();
            e.emitGlobalDecl("VyneValue " + wrapperName + "(int arg_count, VyneValue* args);");
            e.emitBlockOpen("VyneValue " + wrapperName + "(int arg_count, VyneValue* args) {");
            e.emit("if (arg_count > 0) {");
            e.emit("    // args[0] 'self'");
            e.emit("    return " + fnRealCName + "(arg_count - 1, args + 1);");
            e.emit("}");
            e.emit("return " + fnRealCName + "(arg_count, args);");
            e.emitBlockClose();
            e.popFunctionContext();

            e.emit("vyne_register_method(\"" + targetNamespace + "\", \"" +
                   fnOrigName + "\", " + wrapperName + ");");
        }
    }
    e.emitBlockClose();
    e.popMainContext();

    e.setSourceDir(prevDir);
}

std::string ImportNode::getCExpr(C_Emitter& e) const {
    compile(e);
    return "vyne_null()";
}

// ============================================================
// ENUM, DEFER, DISMISS, DEPLOY
// ============================================================

std::string EnumNode::getCExpr(C_Emitter& e) const { return "vyne_null()"; }

void EnumNode::compile(C_Emitter& e) const {
    e.registerGroup(enumName);

    // 1. Declare each member as a global VyneValue
    for (const auto& [name, value] : members) {
        std::string mangled = "v_" + enumName + "_" + name;
        std::replace(mangled.begin(), mangled.end(), '.', '_');
        e.emitGlobalDecl("VyneValue " + mangled + ";");
        e.registerDeclaration(mangled);
    }

    // 2. Initialize in main
    e.pushMainContext();
    for (const auto& [name, value] : members) {
        std::string mangled = "v_" + enumName + "_" + name;
        std::replace(mangled.begin(), mangled.end(), '.', '_');
        e.emit(mangled + " = vyne_int(" + std::to_string(value) + ");");
    }
    e.popMainContext();
}

void DeferNode::compile(C_Emitter& e) const {
    if (isCollected()) {
        return;
    }

    throw std::runtime_error(
        "Compile Error: 'defer' is only supported at the top level of a function body "
        "in the C backend (line " + std::to_string(lineNumber) + "). "
        "Move the 'defer' to the function's top level, or use the interpreter "
        "with --interp.");
}

// ============================================================
// NULL COALESCE ASSIGNMENT
// ============================================================

void NullCoalesceAssignmentNode::compile(C_Emitter& e) const {
    std::string cVar = "v_" + this->varName;
    std::replace(cVar.begin(), cVar.end(), '.', '_');

    if (e.isGlobalContext()) {
        if (e.getGlobalVars().count(cVar) == 0) {
            e.registerDeclaration(cVar);
            e.emitGlobalDecl("VyneValue " + cVar + ";");
        }
        e.pushMainContext();
        std::string val = rhs->getCExpr(e);
        e.emit("if (" + cVar + ".type == V_NULL) {");
        e.emit("    " + cVar + " = " + val + ";");
        e.emit("}");
        e.popMainContext();
    } else {
        if (!e.isLocalDeclared(cVar)) {
            e.registerDeclaration(cVar);
            std::string val = rhs->getCExpr(e);
            e.emit("VyneValue " + cVar + " = vyne_null();");
            e.emit("if (" + cVar + ".type == V_NULL) {");
            e.emit("    " + cVar + " = " + val + ";");
            e.emit("}");
        } else {
            std::string val = rhs->getCExpr(e);
            e.emit("if (" + cVar + ".type == V_NULL) {");
            e.emit("    " + cVar + " = " + val + ";");
            e.emit("}");
        }
    }
}

std::string NullCoalesceAssignmentNode::getCExpr(C_Emitter& e) const {
    std::string cVar = "v_" + this->varName;
    std::replace(cVar.begin(), cVar.end(), '.', '_');
    return cVar;
}

// ============================================================
// NULL COALESCE MEMBER ASSIGNMENT
// ============================================================

void NullCoalesceMemberAssignmentNode::compile(C_Emitter& e) const {
    std::string recv = receiver->getCExpr(e);
    std::string val = rhs->getCExpr(e);
    uint32_t fid = StringPool::intern(memberName);

    e.emit("{");
    e.emit("    VyneValue _field = vyne_struct_get(" + recv + ", " + std::to_string(fid) + ");");
    e.emit("    if (_field.type == V_NULL) {");
    e.emit("        vyne_struct_set(" + recv + ", " + std::to_string(fid) + ", \"" + memberName + "\", " + val + ");");
    e.emit("    }");
    e.emit("}");
}

std::string NullCoalesceMemberAssignmentNode::getCExpr(C_Emitter& e) const {
    compile(e);
    return "vyne_null()";
}

// ============================================================
// DEFER - getCExpr (missing)
// ============================================================

std::string DeferNode::getCExpr(C_Emitter& e) const {
    // Defer is handled at runtime, no expression value
    return "vyne_null()";
}

void DismissNode::compile(C_Emitter& e) const {
    e.emit("vyne_dismiss_module(\"" + originalName + "\");");
}

std::string DismissNode::getCExpr(C_Emitter& e) const { return "vyne_null()"; }

void DeployNode::compile(C_Emitter& e) const {
    e.emit("vyne_deploy_module(\"" + moduleName + "\");");
}

std::string DeployNode::getCExpr(C_Emitter& e) const {
    compile(e);
    return "vyne_null()";
}

// ============================================================
// IN OPERATOR
// ============================================================

std::string InNode::getCExpr(C_Emitter& e) const {
    std::string leftVal = left->getCExpr(e);
    std::string rightVal = right->getCExpr(e);
    std::string temp = e.newTemp("in");

    e.emit("VyneValue " + temp + " = vyne_in_operator(" +
           leftVal + ", " + rightVal + ", " + (isNot ? "1" : "0") + ");");
    return temp;
}

void InNode::compile(C_Emitter& e) const { getCExpr(e); }

// ============================================================
// NULL COALESCE
// ============================================================

std::string NullCoalesceNode::getCExpr(C_Emitter& e) const {
    std::string leftVal = left->getCExpr(e);
    std::string rightVal = right->getCExpr(e);
    std::string temp = e.newTemp("coalesce");

    e.emit("VyneValue " + temp + " = (" + leftVal + ".type == V_NULL) ? " +
           rightVal + " : " + leftVal + ";");
    return temp;
}

void NullCoalesceNode::compile(C_Emitter& e) const { getCExpr(e); }

// ============================================================
// PIPELINE
// ============================================================

void PipelineNode::compile(C_Emitter& e) const {
    if (left) left->compile(e);
    if (right) right->compile(e);
}

std::string PipelineNode::getCExpr(C_Emitter& e) const {
    std::string leftVal = left->getCExpr(e);

    // --- Case 1: right is a FUNCTION_CALL: a |> f(b, c) -> f(a, b, c) ---
    if (right->type() == NodeType::FUNCTION_CALL) {
        auto* fc = static_cast<FunctionCallNode*>(right.get());
        const auto& args = fc->getArguments();
        int totalArgs = 1 + (int)args.size();

        std::string argArr = e.newTemp("pipe_args");
        std::string retTemp = e.newTemp("pipe_ret");

        e.emit("VyneValue* " + argArr + " = (VyneValue*)arena_alloc(sizeof(VyneValue) * " +
               std::to_string(totalArgs) + ");");
        e.emit(argArr + "[0] = " + leftVal + ";");

        for (size_t i = 0; i < args.size(); ++i) {
            std::string v = args[i]->getCExpr(e);
            e.emit(argArr + "[" + std::to_string(i + 1) + "] = " + v + ";");
        }

        std::string mangledName = fc->getOriginalName();
        std::replace(mangledName.begin(), mangledName.end(), '.', '_');

        e.emit("VyneValue " + retTemp + " = fn_" + mangledName +
               "(" + std::to_string(totalArgs) + ", " + argArr + ");");
        return retTemp;
    }

    // --- Case 2: right is a METHOD_CALL: a |> obj.m(b) -> obj.m(a, b) ---
    if (right->type() == NodeType::METHOD_CALL) {
        auto* mc = static_cast<MethodCallNode*>(right.get());
        const auto& args = mc->getArguments();
        int totalArgs = 1 + (int)args.size();

        std::string recv = mc->getReceiver()->getCExpr(e);
        std::string methodName = mc->getMethodName();

        // Route through struct method call if method wasn't one of the
        // built-in array/map ones. Otherwise fall back to a struct call.
        std::string temp = e.newTemp("pipe_mret");
        std::string argArr = e.newTemp("pipe_m_args");

        e.emit("VyneValue " + temp + " = vyne_null();");
        e.emitBlockOpen("if (" + recv + ".type == V_STRUCT) {");
        e.emit("VyneValue* " + argArr + " = (VyneValue*)arena_alloc(sizeof(VyneValue) * " +
               std::to_string(totalArgs + 1) + ");");
        e.emit(argArr + "[0] = " + recv + ";");
        e.emit(argArr + "[1] = " + leftVal + ";");
        for (size_t i = 0; i < args.size(); ++i) {
            e.emit(argArr + "[" + std::to_string(i + 2) + "] = " +
                   args[i]->getCExpr(e) + ";");
        }
        e.emit(temp + " = vyne_struct_call(" + recv + ", \"" + methodName + "\", " +
               std::to_string(totalArgs + 1) + ", " + argArr + ");");
        e.emitBlockClose();
        return temp;
    }

    if (right) right->compile(e);
    return leftVal;
}

// ============================================================
// TRY/CATCH/THROW/FINALLY (stubs for Compile)
// ============================================================

void ThrowNode::compile(C_Emitter& e) const {
    std::string expr = expression ? expression->getCExpr(e) : "vyne_null()";
    e.emit("vyne_throw(" + expr + ");");
}

std::string ThrowNode::getCExpr(C_Emitter& e) const {
    return "vyne_null()";
}

void FinallyNode::compile(C_Emitter& e) const {
    if (body) body->compile(e);
}

std::string FinallyNode::getCExpr(C_Emitter& e) const {
    return "vyne_null()";
}

void TryCatchNode::compile(C_Emitter& e) const {
    std::string mark        = e.newTemp("try_mark");
    std::string cmark       = e.newTemp("catch_mark");
    std::string frameLive   = e.newTemp("frame_live");
    std::string cframeLive  = e.newTemp("cframe_live");
    std::string caught      = e.newTemp("caught");
    std::string errVar      = e.newTemp("try_err");
    std::string cleanup     = e.newTemp("try_cleanup");

    e.emit("VyneValue " + errVar + " = vyne_null();");
    e.emit("int " + caught + " = 0;");
    e.emit("int " + frameLive + " = 1;");
    e.emit("int " + cframeLive + " = 0;");
    e.emit("int " + mark + " = vyne_try_push();");

    // ---- try body ----
    e.emitBlockOpen("if (setjmp(g_exc_stack[" + mark + "].buf) == 0) {");
    e.pushTryCleanup(cleanup);
    if (tryBody) tryBody->compile(e);
    e.popTryCleanup();
    e.emit("goto " + cleanup + ";");
    e.emitBlockClose();

    // ---- landed from a throw in the try body ----
    // vyne_throw already popped the frame before the longjmp.
    e.emit(frameLive + " = 0;");
    e.emit(caught + " = 1;");
    e.emit(errVar + " = g_exc_value;");

    // ---- catch body in its own frame ----
    if (catchBody) {
        e.emit("int " + cmark + " = vyne_try_push();");
        e.emit(cframeLive + " = 1;");
        e.emitBlockOpen("if (setjmp(g_exc_stack[" + cmark + "].buf) == 0) {");

        // Bind the catch variable in the current function scope
        std::string prefix = e.getActiveFunctionPrefix();
        std::string catchSanitized = catchVarName;
        std::replace(catchSanitized.begin(), catchSanitized.end(), '.', '_');
        std::string cVar = prefix.empty()
            ? ("v_" + catchSanitized)
            : ("v_" + prefix + "_" + catchSanitized);
        e.registerDeclaration(cVar);
        e.emit("VyneValue " + cVar + " = " + errVar + ";");

        e.pushTryCleanup(cleanup);
        catchBody->compile(e);
        e.popTryCleanup();

        e.emit("goto " + cleanup + ";");
        e.emitBlockClose();

        // ---- landed from a throw in the catch body ----
        // vyne_throw already popped the catch frame.
        e.emit(cframeLive + " = 0;");
        e.emit(errVar + " = g_exc_value;");
    }

    // ---- cleanup label: run finally, decide what to do next ----
    e.dedent();
    e.emit(cleanup + ":");
    e.indent();

    // Pop whichever frame is still live (skip if throw already popped it).
    e.emit("if (" + frameLive + ") vyne_try_pop();");
    e.emit("if (" + cframeLive + ") { vyne_try_pop(); " + caught + " = 0; }");

    if (finallyBody) finallyBody->compile(e);

    // Pending return takes priority — if the user wrote `return` inside the
    // try or catch, the finally already ran above, so bubble it up now.
    if (e.hasReturnVars()) {
        e.emit("if (" + e.getReturningVar() + ") {");
        if (e.hasTryCleanup()) {
            e.emit("    goto " + e.currentTryCleanup() + ";");
        } else if (e.hasDeferContext()) {
            e.emit("    goto " + e.getDeferCleanupLabel() + ";");
        } else {
            e.emit("    return " + e.getReturnVar() + ";");
        }
        e.emit("}");
    }

    // If the try (or catch) threw and nothing cleared the flag, re-throw.
    e.emit("if (" + caught + ") vyne_throw(" + errVar + ");");
}

std::string TryCatchNode::getCExpr(C_Emitter& e) const {
    compile(e);
    return "vyne_null()";
}

// ============================================================
// MAP LITERAL
// ============================================================

std::string MapNode::getCExpr(C_Emitter& e) const {
    std::string temp = e.newTemp("map");
    e.emit("VyneValue " + temp + " = vyne_map_create();");
    for (const auto& [keyNode, valNode] : pairs) {
        std::string k = keyNode->getCExpr(e);
        std::string v = valNode->getCExpr(e);
        e.emit("vyne_map_set(" + temp + ", " + k + ", " + v + ");");
    }
    return temp;
}

void MapNode::compile(C_Emitter& e) const { getCExpr(e); }

// ============================================================
// INTERPOLATED STRING
// ============================================================

std::string InterpolatedStringNode::getCExpr(C_Emitter& e) const {
    std::string acc = "vyne_string(\"\")";
    size_t exprIdx = 0;

    for (const auto& [part, isExpr] : parts) {
        std::string piece;
        if (isExpr) {
            if (exprIdx >= exprNodes.size()) break;
            std::string v = exprNodes[exprIdx++]->getCExpr(e);
            piece = e.newTemp("is");
            e.emit("VyneValue " + piece + " = vyne_to_string(" + v + ");");
        } else {
            std::string esc;
            esc.reserve(part.size());
            for (char c : part) {
                if      (c == '\\') esc += "\\\\";
                else if (c == '"')  esc += "\\\"";
                else if (c == '\n') esc += "\\n";
                else if (c == '\t') esc += "\\t";
                else                esc += c;
            }
            piece = "vyne_string(\"" + esc + "\")";
        }

        std::string next = e.newTemp("icc");
        e.emit("VyneValue " + next + " = vyne_binop(" + acc + ", " + piece + ", 29);");
        acc = next;
    }
    return acc;
}

void InterpolatedStringNode::compile(C_Emitter& e) const { getCExpr(e); }