#include "../ast/ast.h"
#include "../parser/parser.h"
#include <algorithm>
#include <cctype>

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
    std::string escaped = text;
    // Escape quotes and backslashes for C output
    size_t pos = 0;
    while ((pos = escaped.find('"', pos)) != std::string::npos) {
        escaped.replace(pos, 1, "\\\"");
        pos += 2;
    }
    pos = 0;
    while ((pos = escaped.find('\\', pos)) != std::string::npos) {
        if (pos + 1 < escaped.size() && escaped[pos + 1] != '\\') {
            escaped.replace(pos, 1, "\\\\");
            pos += 2;
        } else {
            pos += 2;
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
    std::string name = "v_" + originalName;
    // Sanitize name for C
    std::replace(name.begin(), name.end(), '.', '_');
    if (e.isReference(name)) {
        return "(*" + name + ")";
    }
    return name;
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
    std::string varName = "v_" + originalName;
    std::replace(varName.begin(), varName.end(), '.', '_');

    if (e.isGlobalContext()) {
        if (e.getGlobalVars().count(varName) == 0) {
            e.registerDeclaration(varName);
            e.emitGlobalDecl("VyneValue " + varName + ";");
        }
        e.pushMainContext();
        std::string val = rhs->getCExpr(e);
        e.emit(varName + " = " + val + ";");
        e.popMainContext();
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
    if (expression) {
        e.emit("return " + expression->getCExpr(e) + ";");
    } else {
        e.emit("return vyne_null();");
    }
}

std::string ReturnNode::getCExpr(C_Emitter& e) const {
    compile(e);
    return "vyne_null()";
}

void BreakNode::compile(C_Emitter& e) const { e.emit("break;"); }
std::string BreakNode::getCExpr(C_Emitter& e) const { return "vyne_null()"; }

void ContinueNode::compile(C_Emitter& e) const { e.emit("continue;"); }
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

    std::string listTemp = e.newTemp("res");
    std::string collection = iterable->getCExpr(e);
    std::string iTemp = e.newTemp("i");
    std::string sizeTemp = e.newTemp("sz");
    std::string elemVar = "v_" + iteratorName;

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

void FunctionNode::compile(C_Emitter& e) const {
    std::string mangledName = originalName;
    std::replace(mangledName.begin(), mangledName.end(), '.', '_');

    e.emitGlobalDecl("VyneValue fn_" + mangledName + "(int arg_count, VyneValue* args);");
    e.pushFunctionContext();

    e.emit("// fn: " + originalName);
    e.emitBlockOpen("VyneValue fn_" + mangledName +
                    "(int arg_count, VyneValue* args) {");

    for (size_t i = 0; i < parameters.size(); ++i) {
        std::string paramName = "v_" + parameters[i].name;
        std::replace(paramName.begin(), paramName.end(), '.', '_');
        e.emit("VyneValue " + paramName +
               " = (arg_count > " + std::to_string(i) +
               ") ? args[" + std::to_string(i) + "] : vyne_null();");
    }

    // Declare frame for local variables (optimized)
    e.emit("// Function frame");
    for (const auto& stmt : body)
        if (stmt) stmt->compile(e);

    e.emit("return vyne_null();");
    e.emitBlockClose();
    e.emit("");

    e.popFunctionContext();
}

void FunctionNode::compileAs(C_Emitter& e, const std::string& mangledName) const {
    std::string name = mangledName;
    std::replace(name.begin(), name.end(), '.', '_');

    e.emitGlobalDecl("VyneValue fn_" + name + "(int arg_count, VyneValue* args);");
    e.pushFunctionContext();
    e.emit("// fn (aliased): " + mangledName);
    e.emitBlockOpen("VyneValue fn_" + name + "(int arg_count, VyneValue* args) {");

    for (size_t i = 0; i < parameters.size(); ++i) {
        std::string paramName = "v_" + parameters[i].name;
        std::replace(paramName.begin(), paramName.end(), '.', '_');
        e.emit("VyneValue " + paramName +
               " = (arg_count > " + std::to_string(i) +
               ") ? args[" + std::to_string(i) + "] : vyne_null();");
    }

    for (const auto& stmt : body)
        if (stmt) stmt->compile(e);

    e.emit("return vyne_null();");
    e.emitBlockClose();
    e.emit("");
    e.popFunctionContext();
}

std::string FunctionNode::getCExpr(C_Emitter& e) const {
    compile(e);
    std::string name = originalName;
    std::replace(name.begin(), name.end(), '.', '_');
    return "fn_" + name;
}

// ============================================================
// FUNCTION CALL
// ============================================================

std::string FunctionCallNode::getCExpr(C_Emitter& e) const {
    int argSize = (int)arguments.size();
    std::string argArr = e.newTemp("args");
    std::string retTemp = e.newTemp("ret");

    std::string mangledName = originalName;
    std::replace(mangledName.begin(), mangledName.end(), '.', '_');

    if (argSize > 0) {
        e.emit("VyneValue* " + argArr + " = (VyneValue*)arena_alloc(sizeof(VyneValue) * " +
               std::to_string(argSize) + ");");
        for (int i = 0; i < argSize; ++i) {
            std::string val = arguments[i]->getCExpr(e);
            e.emit(argArr + "[" + std::to_string(i) + "] = " + val + ";");
        }
    } else {
        e.emit("VyneValue* " + argArr + " = NULL;");
    }

    if (e.isInterface(originalName) || e.isInterface(mangledName)) {
        std::string directArgs;
        for (size_t i = 0; i < arguments.size(); ++i) {
            if (i > 0) directArgs += ", ";
            directArgs += arguments[i]->getCExpr(e);
        }
        e.emit("VyneValue " + retTemp + " = struct_" + mangledName + "(" + directArgs + ");");
        return retTemp;
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
    e.emit("VyneValue " + temp + " = vyne_array_get(" + b + ", " + idx + ");");
    return temp;
}

void IndexAccessNode::compile(C_Emitter& e) const { getCExpr(e); }

void IndexAssignmentNode::compile(C_Emitter& e) const {
    std::string b = base->getCExpr(e);
    std::string i = index->getCExpr(e);
    std::string r = rhs->getCExpr(e);
    e.emit("vyne_array_set(" + b + ", " + i + ", " + r + ");");
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
        std::string end = arguments[1]->getCExpr(e);
        std::string temp = e.newTemp("seq");
        e.emit("VyneValue " + temp + " = vyne_range_create(" + start + ", " + end + ");");
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
    // Add proper includes
    e.addInclude("vyne_runtime.h");
    for (const auto& stmt : statements)
        if (stmt) stmt->compile(e);
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
    compile(e);
    return "vyne_null()";
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
            e.emit("vyne_struct_set(v_self, " + std::to_string(fid) + ", " + val + ");");
            return;
        }
    }

    std::string recv = receiver->getCExpr(e);
    uint32_t fid = StringPool::intern(memberName);
    e.emit("vyne_struct_set(" + recv + ", " + std::to_string(fid) + ", " + val + ");");
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
    e.registerInterface(interfaceName);
    if (!moduleName.empty()) {
        e.registerInterface(moduleName + "." + interfaceName);
        e.registerInterface(moduleName + "_" + interfaceName);
    }
    e.pushFunctionContext();

    e.emit("// interface: " + interfaceName);
    std::string params;
    for (size_t i = 0; i < members.size(); ++i) {
        if (i > 0) params += ", ";
        params += "VyneValue v_" + members[i].name;
    }

    std::string structName = interfaceName;
    std::replace(structName.begin(), structName.end(), '.', '_');

    e.emitBlockOpen("VyneValue struct_" + structName + "(" + params + ") {");

    std::string temp = e.newTemp("s");
    e.emit("VyneStruct* " + temp + " = (VyneStruct*)arena_alloc(sizeof(VyneStruct));");
    e.emit(temp + "->type_name = \"" + interfaceName + "\";");
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
        std::string methodName = interfaceName + "_" + fn->getOriginalName();
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
        e.emit("vyne_register_method(\"" + interfaceName + "\", \"" +
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
            std::string methodName = name + "_" + methodName;
            std::replace(methodName.begin(), methodName.end(), '.', '_');
            e.emit("VyneValue " + resTemp + " = fn_" + methodName +
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
        e.emit("VyneValue " + temp +
               " = vyne_int((int64_t)" + recv + ".as.arr->size);");
        return temp;
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
    e.emit("/* enum — values are inlined as constants */");
}

void DeferNode::compile(C_Emitter& e) const {
    // Defer is handled at runtime
    e.emit("// defer statement");
}

// ============================================================
// NULL COALESCE ASSIGNMENT
// ============================================================

void NullCoalesceAssignmentNode::compile(C_Emitter& e) const {
    std::string varName = "v_" + varName;
    std::replace(varName.begin(), varName.end(), '.', '_');
    
    if (e.isGlobalContext()) {
        if (e.getGlobalVars().count(varName) == 0) {
            e.registerDeclaration(varName);
            e.emitGlobalDecl("VyneValue " + varName + ";");
        }
        e.pushMainContext();
        std::string val = rhs->getCExpr(e);
        e.emit("if (" + varName + ".type == V_NULL) {");
        e.emit("    " + varName + " = " + val + ";");
        e.emit("}");
        e.popMainContext();
    } else {
        if (!e.isLocalDeclared(varName)) {
            e.registerDeclaration(varName);
            std::string val = rhs->getCExpr(e);
            e.emit("VyneValue " + varName + " = vyne_null();");
            e.emit("if (" + varName + ".type == V_NULL) {");
            e.emit("    " + varName + " = " + val + ";");
            e.emit("}");
        } else {
            std::string val = rhs->getCExpr(e);
            e.emit("if (" + varName + ".type == V_NULL) {");
            e.emit("    " + varName + " = " + val + ";");
            e.emit("}");
        }
    }
}

std::string NullCoalesceAssignmentNode::getCExpr(C_Emitter& e) const {
    std::string varName = "v_" + varName;
    std::replace(varName.begin(), varName.end(), '.', '_');
    return varName;
}

// ============================================================
// NULL COALESCE MEMBER ASSIGNMENT
// ============================================================

void NullCoalesceMemberAssignmentNode::compile(C_Emitter& e) const {
    std::string recv = receiver->getCExpr(e);
    std::string val = rhs->getCExpr(e);
    uint32_t fid = StringPool::intern(memberName);
    
    e.emit("{");  // scope
    e.emit("VyneValue _field = vyne_struct_get(" + recv + ", " + std::to_string(fid) + ");");
    e.emit("if (_field.type == V_NULL) {");
    e.emit("    vyne_struct_set(" + recv + ", " + std::to_string(fid) + ", " + val + ");");
    e.emit("}");
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

void DismissNode::compile(C_Emitter& e) const {}

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
    if (right->type() == NodeType::FUNCTION_CALL) {
        return right->getCExpr(e);
    }
    if (right->type() == NodeType::METHOD_CALL) {
        return right->getCExpr(e);
    }
    if (left) left->compile(e);
    if (right) right->compile(e);
    return "vyne_null()";
}

// ============================================================
// TRY/CATCH/THROW/FINALLY (stubs for codegen)
// ============================================================

void ThrowNode::compile(C_Emitter& e) const {
    e.emit("// throw statement - requires runtime support");
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
    if (tryBody) tryBody->compile(e);
    if (catchBody) catchBody->compile(e);
    if (finallyBody) finallyBody->compile(e);
}

std::string TryCatchNode::getCExpr(C_Emitter& e) const {
    compile(e);
    return "vyne_null()";
}