#include "../ast/ast.h"

// ============================================================
// LITERALS — only return expressions, never emit statements
// ============================================================

std::string NumberNode::getCExpr(C_Emitter& e) const {
    if (value.getType() == Value::INT64)
        return "vyne_int(" + std::to_string(value.asInt()) + ")";
    return "vyne_float(" + std::to_string(value.asFloat()) + ")";
}
void NumberNode::compile(C_Emitter& e) const { /* literals are expressions only */ }

std::string StringNode::getCExpr(C_Emitter& e) const {
    return "vyne_string(\"" + text + "\")";
}
void StringNode::compile(C_Emitter& e) const {}

std::string BooleanNode::getCExpr(C_Emitter& e) const {
    return condition ? "vyne_bool(1)" : "vyne_bool(0)";
}
void BooleanNode::compile(C_Emitter& e) const {}

std::string NullNode::getCExpr(C_Emitter& e) const { return "vyne_null()"; }
void NullNode::compile(C_Emitter& e) const {}

// ============================================================
// VARIABLES AND ASSIGNMENTS
// ============================================================

std::string VariableNode::getCExpr(C_Emitter& e) const {
    return "v_" + originalName;
}
void VariableNode::compile(C_Emitter& e) const {
    // A bare variable reference as a statement is a no-op in C
}

std::string AssignmentNode::getCExpr(C_Emitter& e) const {
    return "v_" + originalName;
}
void AssignmentNode::compile(C_Emitter& e) const {
    std::string val     = rhs->getCExpr(e);
    std::string varName = "v_" + originalName;
    // Emit declaration into current context (main or function body)
    e.emit("VyneValue " + varName + " = " + val + ";");
}

// ============================================================
// BINARY / UNARY / POSTFIX
// op codes sourced directly from VTokenType enum values
// that vyne_binop() switches on in runtime.h
// ============================================================

std::string BinOpNode::getCExpr(C_Emitter& e) const {
    std::string l    = leftNode->getCExpr(e);
    std::string r    = rightNode->getCExpr(e);
    std::string temp = e.newTemp("bin");
    e.emit("VyneValue " + temp + " = vyne_binop(" + l + ", " + r +
           ", " + std::to_string((int)op) + ");");
    return temp;
}
void BinOpNode::compile(C_Emitter& e) const { getCExpr(e); }

std::string UnaryNode::getCExpr(C_Emitter& e) const {
    std::string val  = right->getCExpr(e);
    std::string temp = e.newTemp("un");
    e.emit("VyneValue " + temp + " = vyne_unary(" + val +
           ", " + std::to_string((int)op) + ");");
    return temp;
}
void UnaryNode::compile(C_Emitter& e) const { getCExpr(e); }

std::string PostFixNode::getCExpr(C_Emitter& e) const {
    // left must be an L-value variable
    std::string var  = left->getCExpr(e);
    std::string temp = e.newTemp("post");
    // Save old value to return, then increment/decrement
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
// Use emitBlockOpen / emitBlockClose for proper indentation
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
    // Evaluate condition inside the loop so it re-evaluates each iteration
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
    if (expression)
        e.emit("return " + expression->getCExpr(e) + ";");
    else
        e.emit("return vyne_null();");
}
std::string ReturnNode::getCExpr(C_Emitter& e) const {
    compile(e);
    return "vyne_null()";
}

void BreakNode::compile(C_Emitter& e) const    { e.emit("break;"); }
std::string BreakNode::getCExpr(C_Emitter& e) const { return "vyne_null()"; }

void ContinueNode::compile(C_Emitter& e) const    { e.emit("continue;"); }
std::string ContinueNode::getCExpr(C_Emitter& e) const { return "vyne_null()"; }

// ============================================================
// FOR NODE — no mutable state, no mutual recursion
// compile()   → emits the loop, returns nothing
// getCExpr()  → for collect/filter/unique returns the result array temp
// ============================================================

void ForNode::compile(C_Emitter& e) const {
    // Only LOOP mode uses compile() directly.
    // Other modes go through getCExpr() which sets up the result array first.
    std::string collection = iterable->getCExpr(e);
    std::string iTemp      = e.newTemp("i");
    std::string sizeTemp   = e.newTemp("sz");
    std::string elemVar    = "v_" + iteratorName;

    e.emitBlockOpen("if (" + collection + ".type == V_ARRAY) {");
    e.emit("int64_t " + sizeTemp + " = " + collection + ".as.arr->size;");
    e.emitBlockOpen("for (int64_t " + iTemp + " = 0; " +
                    iTemp + " < " + sizeTemp + "; " + iTemp + "++) {");
    e.emit("VyneValue " + elemVar + " = vyne_array_get(" +
           collection + ", vyne_int(" + iTemp + "));");

    if (body) body->compile(e);

    e.emitBlockClose(); // for
    e.emitBlockClose(); // if
}

std::string ForNode::getCExpr(C_Emitter& e) const {
    if (mode == ForMode::LOOP) {
        compile(e);
        return "vyne_null()";
    }

    // collect / filter / unique / every:
    // Declare result array BEFORE the loop, fill it inside.
    std::string listTemp   = e.newTemp("res");
    std::string collection = iterable->getCExpr(e);
    std::string iTemp      = e.newTemp("i");
    std::string sizeTemp   = e.newTemp("sz");
    std::string elemVar    = "v_" + iteratorName;

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
        case ForMode::EVERY: {
            std::string cond = body->getCExpr(e);
            e.emitBlockOpen("if (!vyne_is_truthy(" + cond + ")) {");
            e.emit(listTemp + " = vyne_bool(0);");
            e.emit("break;");
            e.emitBlockClose();
            break;
        }
        case ForMode::UNIQUE: {
            // Runtime dedup: check if element already in result before pushing
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

    e.emitBlockClose(); // for
    e.emitBlockClose(); // if

    return listTemp;
}

// ============================================================
// FUNCTIONS
// ============================================================

void FunctionNode::compile(C_Emitter& e) const {
    // Functions always go into functionStream regardless of call site
    e.pushFunctionContext();

    e.emit("// fn: " + originalName);
    e.emitBlockOpen("VyneValue fn_" + originalName +
                    "(int arg_count, VyneValue* args) {");

    for (size_t i = 0; i < parameters.size(); ++i) {
        e.emit("VyneValue v_" + parameters[i].name +
               " = (arg_count > " + std::to_string(i) +
               ") ? args[" + std::to_string(i) + "] : vyne_null();");
    }

    for (const auto& stmt : body)
        if (stmt) stmt->compile(e);

    e.emit("return vyne_null();");
    e.emitBlockClose(); // fn body
    e.emit("");

    e.popFunctionContext();
}
std::string FunctionNode::getCExpr(C_Emitter& e) const {
    compile(e);
    return "fn_" + originalName;
}

std::string FunctionCallNode::getCExpr(C_Emitter& e) const {
    int argSize = (int)arguments.size();
    std::string argArr  = e.newTemp("args");
    std::string result  = e.newTemp("ret");

    if (argSize > 0) {
        std::string elems;
        for (int i = 0; i < argSize; ++i) {
            if (i > 0) elems += ", ";
            elems += arguments[i]->getCExpr(e);
        }
        e.emit("VyneValue " + argArr + "[] = {" + elems + "};");
    } else {
        e.emit("VyneValue* " + argArr + " = NULL;");
    }

    e.emit("VyneValue " + result + " = fn_" + originalName +
           "(" + std::to_string(argSize) + ", " + argArr + ");");
    return result;
}
void FunctionCallNode::compile(C_Emitter& e) const { getCExpr(e); }

// ============================================================
// ARRAYS AND INDEX ACCESS
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
    std::string b    = base->getCExpr(e);
    std::string idx  = index->getCExpr(e);
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
// BUILT-INS  (mapped to runtime.h functions)
// ============================================================

std::string BuiltInCallNode::getCExpr(C_Emitter& e) const {
    if (funcName == "out") {
        for (const auto& arg : arguments)
            e.emit("vyne_out(" + arg->getCExpr(e) + ");");
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
        if (arguments.empty()) return "vyne_int(0)";
        std::string temp = e.newTemp("i64");
        std::string arg  = arguments[0]->getCExpr(e);
        // Runtime cast: if float → truncate, if string → atoll
        e.emit("VyneValue " + temp + " = (" + arg + ".type == V_FLOAT64)"
               " ? vyne_int((int64_t)" + arg + ".as.f64)"
               " : (" + arg + ".type == V_STRING)"
               " ? vyne_int(atoll(" + arg + ".as.str))"
               " : " + arg + ";");
        return temp;
    }
    if (funcName == "float64") {
        if (arguments.empty()) return "vyne_float(0.0)";
        std::string temp = e.newTemp("f64");
        std::string arg  = arguments[0]->getCExpr(e);
        e.emit("VyneValue " + temp + " = (" + arg + ".type == V_INT64)"
               " ? vyne_float((double)" + arg + ".as.i64)"
               " : (" + arg + ".type == V_STRING)"
               " ? vyne_float(atof(" + arg + ".as.str))"
               " : " + arg + ";");
        return temp;
    }
    if (funcName == "sizeof") {
        if (arguments.empty()) return "vyne_int(0)";
        std::string arg  = arguments[0]->getCExpr(e);
        std::string temp = e.newTemp("sz");
        // Arrays: return size field. Others: sizeof the C struct.
        e.emit("VyneValue " + temp + " = (" + arg + ".type == V_ARRAY)"
               " ? vyne_int(" + arg + ".as.arr->size)"
               " : vyne_int((int64_t)sizeof(" + arg + "));");
        return temp;
    }
    // Unknown built-in: emit a comment and return null
    e.emit("/* unknown built-in: " + funcName + " */");
    return "vyne_null()";
}
void BuiltInCallNode::compile(C_Emitter& e) const { getCExpr(e); }

// ============================================================
// PROGRAM / BLOCK
// ============================================================

void ProgramNode::compile(C_Emitter& e) const {
    // indentLevel starts at 1 (inside main) by default after reset()
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
    // Evaluate condition first (may emit temps), then inline branch
    std::string cond  = condition->getCExpr(e);
    std::string tVal  = trueExpr->getCExpr(e);
    std::string fVal  = falseExpr->getCExpr(e);
    std::string temp  = e.newTemp("tern");
    e.emit("VyneValue " + temp + " = vyne_is_truthy(" + cond + ") ? " +
           tVal + " : " + fVal + ";");
    return temp;
}
void TernaryNode::compile(C_Emitter& e) const { getCExpr(e); }

// ============================================================
// MEMBER ACCESS / ASSIGNMENT  (group/struct field mangling)
// ============================================================

std::string MemberAccessNode::getCExpr(C_Emitter& e) const {
    if (receiver->type() == NodeType::VARIABLE) {
        auto* var = static_cast<VariableNode*>(receiver.get());
        return "v_" + var->getOriginalName() + "_" + memberName;
    }
    if (receiver->type() == NodeType::MEMBER_ACCESS) {
        return receiver->getCExpr(e) + "_" + memberName;
    }
    return "vyne_null()";
}
void MemberAccessNode::compile(C_Emitter& e) const {
    // bare member access as statement — no-op
}

void MemberAssignmentNode::compile(C_Emitter& e) const {
    std::string val = rhs->getCExpr(e);
    if (receiver->type() == NodeType::VARIABLE) {
        auto* var = static_cast<VariableNode*>(receiver.get());
        e.emit("v_" + var->getOriginalName() + "_" + memberName + " = " + val + ";");
    } else {
        e.emit(receiver->getCExpr(e) + "_" + memberName + " = " + val + ";");
    }
}
std::string MemberAssignmentNode::getCExpr(C_Emitter& e) const {
    compile(e);
    if (receiver->type() == NodeType::VARIABLE) {
        auto* var = static_cast<VariableNode*>(receiver.get());
        return "v_" + var->getOriginalName() + "_" + memberName;
    }
    return receiver->getCExpr(e) + "_" + memberName;
}

// ============================================================
// GROUP  (namespace → mangled globals)
// ============================================================

void GroupNode::compile(C_Emitter& e) const {
    e.pushGlobalContext();
    e.emit("// --- Group: " + groupName + " ---");

    for (const auto& stmt : statements) {
        if (!stmt) continue;
        if (stmt->type() == NodeType::ASSIGNMENT) {
            auto* assign = static_cast<AssignmentNode*>(stmt.get());
            std::string mangled = "v_" + groupName + "_" + assign->getOriginalName();
            // Declaration goes in globals
            e.emit("VyneValue " + mangled + ";");
            // Initialiser runs in main — push/pop context around it
            e.popGlobalContext();
            std::string val = assign->getRHS()->getCExpr(e);
            e.emit(mangled + " = " + val + ";");
            e.pushGlobalContext();
        } else if (stmt->type() == NodeType::FUNCTION) {
            // Functions handle their own context push
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
// MODULE  (maps to runtime header includes)
// ============================================================

void ModuleNode::compile(C_Emitter& e) const {
    if (originalName == "vmath")  e.addInclude("modules/vmath.h");
    if (originalName == "vcore")  e.addInclude("modules/vcore.h");
    if (originalName == "vaudio") e.addInclude("modules/vaudio.h");
    if (originalName == "vglib")  e.addInclude("modules/vglib.h");
}
std::string ModuleNode::getCExpr(C_Emitter& e) const { return "vyne_null()"; }

// ============================================================
// STUBS — not yet compiled to C
// ============================================================

void InterfaceNode::compile(C_Emitter& e) const {
    e.emit("/* interface " + interfaceName + " — runtime only */");
}
std::string InterfaceNode::getCExpr(C_Emitter& e) const { return "vyne_null()"; }

std::string MethodCallNode::getCExpr(C_Emitter& e) const {
    std::string recvRaw = receiver->getCExpr(e);
    std::string recv = e.newTemp("m_recv");
    e.emit("VyneValue " + recv + " = " + recvRaw + ";");

    if (methodName == "push") {
        for (const auto& argNode : arguments) {
            std::string arg = argNode->getCExpr(e);
            e.emit("vyne_array_push(" + recv + ", " + arg + ");");
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

    return "vyne_null()";
}

void MethodCallNode::compile(C_Emitter& e) const { getCExpr(e); }

void ImportNode::compile(C_Emitter& e) const {}
std::string ImportNode::getCExpr(C_Emitter& e) const { return "vyne_null()"; }

void DismissNode::compile(C_Emitter& e) const {}
std::string DismissNode::getCExpr(C_Emitter& e) const { return "vyne_null()"; }

std::string DeployNode::getCExpr(C_Emitter& e) const { return "vyne_null()"; }

void RangeNode::compile(C_Emitter& e) const {
    e.emit("/* range — not yet supported in codegen */");
}
std::string RangeNode::getCExpr(C_Emitter& e) const { return "vyne_null()"; }