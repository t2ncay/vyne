#include "../ast/ast.h"

// --- Literals

std::string NumberNode::getCExpr(C_Emitter& e) const {
    if (value.getType() == Value::INT64)
        return "vyne_int(" + std::to_string(value.asInt()) + ")";
    return "vyne_float(" + std::to_string(value.asFloat()) + ")";
}

void NumberNode::compile(C_Emitter& e) const { getCExpr(e); }

std::string StringNode::getCExpr(C_Emitter& e) const {
    return "vyne_string(\"" + text + "\")";
}
void StringNode::compile(C_Emitter& e) const { getCExpr(e); }

std::string BooleanNode::getCExpr(C_Emitter& e) const {
    return condition ? "vyne_bool(1)" : "vyne_bool(0)";
}
void BooleanNode::compile(C_Emitter& e) const { getCExpr(e); }

std::string NullNode::getCExpr(C_Emitter& e) const {
    return "vyne_null()";
}
void NullNode::compile(C_Emitter& e) const { getCExpr(e); }

// --- Variables and Assignments ---

std::string VariableNode::getCExpr(C_Emitter& e) const {
    return "v_" + originalName;
}
void VariableNode::compile(C_Emitter& e) const { e.emit(getCExpr(e) + ";"); }

std::string AssignmentNode::getCExpr(C_Emitter& e) const {
    std::string val = rhs->getCExpr(e);
    std::string varName = "v_" + originalName;
    e.emit("VyneValue " + varName + " = " + val + ";");
    return varName;
}
void AssignmentNode::compile(C_Emitter& e) const { getCExpr(e); }

// --- Mathematical and Logical Operations ---

std::string BinOpNode::getCExpr(C_Emitter& e) const {
    // Diqqət: 'left' və 'right'ə 'this->' ilə müraciət edirik
    std::string l = this->leftNode->getCExpr(e);
    std::string r = this->rightNode->getCExpr(e);
    std::string temp = e.newTemp();
    
    // Op-u int-ə cast edirik ki, enum xətası verməsin
    e.emit("VyneValue " + temp + " = vyne_binop(" + l + ", " + r + ", " + std::to_string((int)this->op) + ");");
    return temp;
}
void BinOpNode::compile(C_Emitter& e) const { getCExpr(e); }

std::string UnaryNode::getCExpr(C_Emitter& e) const {
    std::string rightVal = right->getCExpr(e);
    std::string temp = e.newTemp();
    e.emit("VyneValue " + temp + " = vyne_unary(" + rightVal + ", " + std::to_string((int)op) + ");");
    return temp;
}
void UnaryNode::compile(C_Emitter& e) const { getCExpr(e); }

std::string PostFixNode::getCExpr(C_Emitter& e) const {
    std::string var = left->getCExpr(e);
    e.emit(var + ".as.f64++;"); // Sadəlik üçün float fərz edirik
    return var;
}
void PostFixNode::compile(C_Emitter& e) const { getCExpr(e); }

// --- Control Flow ---

void IfNode::compile(C_Emitter& e) const {
    std::string cond = condition->getCExpr(e);
    e.emit("if (vyne_is_truthy(" + cond + ")) {");
    if (body) body->compile(e);
    e.emit("}");
    if (elseBody) {
        e.emit("else {");
        elseBody->compile(e);
        e.emit("}");
    }
}
std::string IfNode::getCExpr(C_Emitter& e) const { compile(e); return "vyne_null()"; }

void WhileNode::compile(C_Emitter& e) const {
    e.emit("while (1) {");
    std::string cond = condition->getCExpr(e);
    e.emit("  if (!vyne_is_truthy(" + cond + ")) break;");
    if (body) body->compile(e);
    e.emit("}");
}
std::string WhileNode::getCExpr(C_Emitter& e) const { compile(e); return "vyne_null()"; }

void ReturnNode::compile(C_Emitter& e) const {
    if (expression) {
        e.emit("return " + expression->getCExpr(e) + ";");
    } else {
        e.emit("return vyne_null();");
    }
}
std::string ReturnNode::getCExpr(C_Emitter& e) const { compile(e); return "vyne_null()"; }

// --- Functions and Calls ---

void FunctionNode::compile(C_Emitter& e) const {
    e.setFunctionContext(true);
    
    e.emit("");
    e.emit("// Vyne Function: " + originalName);
    e.emit("VyneValue fn_" + originalName + "(int arg_count, VyneValue* args) {");
    
    for (size_t i = 0; i < parameters.size(); ++i) {
        std::string paramName = "v_" + parameters[i].name;
        e.emit("    VyneValue " + paramName + " = (arg_count > " + std::to_string(i) + ") ? args[" + std::to_string(i) + "] : vyne_null();");
    }

    for (const auto& stmt : body) {
        if (stmt) stmt->compile(e);
    }

    e.emit("    return vyne_null();");
    e.emit("}");
    
    e.setFunctionContext(false);
}
std::string FunctionNode::getCExpr(C_Emitter& e) const { compile(e); return "fn_" + originalName; }

std::string FunctionCallNode::getCExpr(C_Emitter& e) const {
    int argSize = arguments.size();
    std::string argArrayName = e.newTemp() + "_args";
    
    if (argSize > 0) {
        e.emit("VyneValue " + argArrayName + "[] = {");
        for (int i = 0; i < argSize; ++i) {
            e.emit("    " + arguments[i]->getCExpr(e) + (i == argSize - 1 ? "" : ","));
        }
        e.emit("};");
    } else {
        e.emit("VyneValue* " + argArrayName + " = NULL;");
    }

    std::string resultTemp = e.newTemp();
    e.emit("VyneValue " + resultTemp + " = fn_" + originalName + "(" + std::to_string(argSize) + ", " + argArrayName + ");");
    
    return resultTemp;
}
void FunctionCallNode::compile(C_Emitter& e) const { 
    std::string temp = getCExpr(e); 
    e.emit("// Call finished"); 
}

// --- Program structure ---

void ProgramNode::compile(C_Emitter& e) const {
    e.emit("// --- Vyne Program Generated C Code ---");
    for (const auto& stmt : statements) if (stmt) stmt->compile(e);
}
std::string ProgramNode::getCExpr(C_Emitter& e) const { compile(e); return "vyne_null()"; }

void BlockNode::compile(C_Emitter& e) const {
    e.emit("{");
    for (const auto& stmt : statements) if (stmt) stmt->compile(e);
    e.emit("}");
}
std::string BlockNode::getCExpr(C_Emitter& e) const { compile(e); return "vyne_null()"; }

// --- Others ---

std::string ArrayNode::getCExpr(C_Emitter& e) const {
    std::string temp = e.newTemp();
    int size = elements.size();
    
    e.emit("VyneValue " + temp + " = vyne_array_create(" + std::to_string(size) + ");");
    
    for (int i = 0; i < size; i++) {
        std::string elemVal = elements[i]->getCExpr(e);
        e.emit("vyne_array_set(" + temp + ", vyne_int(" + std::to_string(i) + "), " + elemVal + ");");
    }
    
    return temp;
}
void ArrayNode::compile(C_Emitter& e) const { getCExpr(e); }

std::string IndexAccessNode::getCExpr(C_Emitter& e) const {
    std::string b = base->getCExpr(e);
    std::string i = index->getCExpr(e);
    std::string temp = e.newTemp();
    
    e.emit("VyneValue " + temp + " = vyne_array_get(" + b + ", " + i + ");");
    return temp;
}
void IndexAccessNode::compile(C_Emitter& e) const { getCExpr(e); }

std::string BuiltInCallNode::getCExpr(C_Emitter& e) const {
    if (funcName == "out") {
        for (const auto& arg : arguments) e.emit("vyne_out(" + arg->getCExpr(e) + ");");
    }
    return "vyne_null()";
}
void BuiltInCallNode::compile(C_Emitter& e) const { getCExpr(e); }

// --- (Interface, Module, Group) ---

void InterfaceNode::compile(C_Emitter& e) const { e.emit("// Interface " + interfaceName + " logic here"); }
std::string InterfaceNode::getCExpr(C_Emitter& e) const { return "vyne_null()"; }

void GroupNode::compile(C_Emitter& e) const { /* C-də prefix-lərlə həll olunacaq */ }
std::string GroupNode::getCExpr(C_Emitter& e) const { return "vyne_null()"; }

void ModuleNode::compile(C_Emitter& e) const { e.emit("// Module " + originalName); }
std::string ModuleNode::getCExpr(C_Emitter& e) const { return "vyne_null()"; }

void ForNode::compile(C_Emitter& e) const { e.emit("// For loop logic here"); }
std::string ForNode::getCExpr(C_Emitter& e) const { return "vyne_null()"; }

void TernaryNode::compile(C_Emitter& e) const {
    std::string cond = condition->getCExpr(e);
    std::string temp = e.newTemp();
    e.emit("VyneValue " + temp + " = vyne_is_truthy(" + cond + ") ? " + trueExpr->getCExpr(e) + " : " + falseExpr->getCExpr(e) + ";");
}
std::string TernaryNode::getCExpr(C_Emitter& e) const { compile(e); return "vyne_null()"; }

// Break, Continue, Dismiss, Import, Dismiss
void BreakNode::compile(C_Emitter& e) const { e.emit("break;"); }
std::string BreakNode::getCExpr(C_Emitter& e) const { return "vyne_null()"; }

void ContinueNode::compile(C_Emitter& e) const { e.emit("continue;"); }
std::string ContinueNode::getCExpr(C_Emitter& e) const { return "vyne_null()"; }

void ImportNode::compile(C_Emitter& e) const {}
std::string ImportNode::getCExpr(C_Emitter& e) const { return "vyne_null()"; }

void DismissNode::compile(C_Emitter& e) const {}
std::string DismissNode::getCExpr(C_Emitter& e) const { return "vyne_null()"; }

std::string DeployNode::getCExpr(C_Emitter& e) const { return "vyne_null()"; }

void RangeNode::compile(C_Emitter& e) const {}
std::string RangeNode::getCExpr(C_Emitter& e) const { return "vyne_null()"; }

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

void MethodCallNode::compile(C_Emitter& e) const {}
std::string MethodCallNode::getCExpr(C_Emitter& e) const { return "vyne_null()"; }

void MemberAccessNode::compile(C_Emitter& e) const {}
std::string MemberAccessNode::getCExpr(C_Emitter& e) const { return "vyne_null()"; }

void MemberAssignmentNode::compile(C_Emitter& e) const {}
std::string MemberAssignmentNode::getCExpr(C_Emitter& e) const { return "vyne_null()"; }