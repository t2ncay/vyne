#include "codegen.h"

// ============================================================================
// codegen.cpp — AST → C transpiler
//
// Layout (top to bottom):
//
//    1. File-scope helpers       cross-cutting utilities used by many nodes
//    2. Literals                 NumberNode, StringNode, BooleanNode, NullNode
//    3. Variables / assignments  VariableNode, AssignmentNode, nativeInit
//    4. Operators                BinOpNode, UnaryNode, PostFixNode
//    5. Control flow             IfNode, WhileNode, Return/Break/Continue
//    6. For loops                ForNode, int64Bound
//    7. Functions                emitFunctionBody, FunctionNode
//    8. Function calls           FunctionCallNode
//    9. Arrays / index / slice   ArrayNode, Index*, RangeNode, SliceNode
//   10. Built-ins                BuiltInCallNode
//   11. Program / block          ProgramNode, BlockNode
//   12. Ternary                  TernaryNode
//   13. Member access            MemberAccessNode, MemberAssignmentNode
//   14. Group                    GroupNode
//   15. Module                   ModuleNode
//   16. Interface / struct       InterfaceNode // TODO let those mfs initialize with typed unboxed data types
//   17. Method call              MethodCallNode
//   18. Import                   ImportNode
//   19. Enum / defer / dismiss   EnumNode, DeferNode, DismissNode, DeployNode
//   20. Null coalesce            NullCoalesceAssignmentNode, ...Member..., ...Node
//   21. In operator              InNode
//   22. Pipeline                 PipelineNode
//   23. Exceptions               ThrowNode, FinallyNode, TryCatchNode
//   24. Map literal              MapNode
//   25. Interpolated string      InterpolatedStringNode
//
// Two invariants to keep in mind while editing:
//
//   - Every VyneValue crossing a dynamic boundary is boxed. When a static
//     type is provable, M1/M4 unbox it to int64_t / double / VyneArray_*.
//     Box on uncertainty; never guess.
//
//   - Every boxing site in this file goes through boxTypedArray, never raw
//     e.boxIfNative. boxTypedArray is a strict superset that also handles
//     typed arrays. If you add a new site and use boxIfNative, you will
//     emit `VyneArray_i64` into a `VyneValue` slot and the generated C will
//     not compile.
// ============================================================================

// ============================================================
// LITERALS
// ============================================================

static std::string floatLit(double v) {
    char buf[32];
    auto [p, ec] = std::to_chars(buf, buf + sizeof(buf), v,
                                 std::chars_format::general);
    if (ec != std::errc{}) return "0.0";
    return std::string(buf, p);
}

// ============================================================
// M4: typed-array local inference.
//
// resolveType("Array<Int64>") returns VType::Array only — the parser
// discards the element type. So we recover it at emit time from either
// the RHS (ArrayNode) or the base expression (a CType registered when
// the typed local was declared).
//
// Fast path applies when:
//   x: Array<Int64> = [1, 2, 3];     -> VyneArray_i64 local
//   through v :: x -> loop { }        -> flat for over x.data
//   x[i]  /  x[i] = e                  -> x.data[i]
//   x[a:b]                            -> vyne_array_i64_slice
//
// Still boxed (C0 limits):
//   parameters / returns / struct fields declared Array<T>
//   collect / filter / every / unique over a typed array
//   push / pop / sort / delete / place_all on a typed array
// ============================================================

// Find a CType for a C expression string: native temps first (BinOp /
// ArrayNode / SliceNode results), then scoped locals. lookupLocalType
// returns ANY registered kind, unlike C_Emitter::exprNativeType which
// filters to primitives — that's the whole reason this wrapper exists.
static const CType* lookupCType(C_Emitter& e, const std::string& expr) {
    if (const CType* t = e.exprNativeType(expr)) return t;
    return e.lookupAnyType(expr);
}

// Homogeneous element type of an array literal. Returns Unknown for
// empty arrays, non-Array nodes, or any element that isn't Int64/Float64.
// Mixed Int64/Float64 promotes to Float64.
static VType inferArrayElemType(const ASTNode* node) {
    if (!node || node->type() != NodeType::ARRAY) return VType::Unknown;
    auto* arr = static_cast<const ArrayNode*>(node);
    const auto& elems = arr->getElements();
    if (elems.empty()) return VType::Unknown;

    VType common = VType::Unknown;
    for (const auto& el : elems) {
        VType et = el->getStaticType();
        if (et != VType::Int64 && et != VType::Float64) return VType::Unknown;
        if (common == VType::Unknown) {
            common = et;
        } else if (common != et) {
            if ((common == VType::Int64   && et == VType::Float64) ||
                (common == VType::Float64 && et == VType::Int64)) {
                common = VType::Float64;
            } else {
                return VType::Unknown;
            }
        }
    }
    return common;
}

static std::string typedArrayCName(VType elem) {
    switch (elem) {
        case VType::Int64:   return "VyneArray_i64";
        case VType::Float64: return "VyneArray_f64";
        default:             return "VyneValue";
    }
}

// Coerce a C expression string to a native C value of static type `want`.
// Same shape as the `operand` lambda in BinOpNode::getCExpr.
static std::string coerceToNative(C_Emitter& e, const ASTNode* node,
                                  const std::string& expr, VType want) {
    if (node && node->type() == NodeType::NUMBER) {
        auto* num = static_cast<const NumberNode*>(node);
        if ((want == VType::Int64 && num->getStaticType() == VType::Int64) ||
            (want == VType::Float64)) {
            return num->nativeLiteral();
        }
    }
    if (node && node->type() == NodeType::BOOLEAN && want == VType::Bool) {
        return static_cast<const BooleanNode*>(node)->nativeLiteral();
    }
    const CType* ct = e.exprNativeType(expr);
    if (ct && ct->isPrimitive()) {
        if (ct->toVType() == want) return expr;
        if (want == VType::Float64 && ct->kind == CType::Kind::Int64)
            return "(double)(" + expr + ")";
        if (want == VType::Int64 && ct->kind == CType::Kind::Float64)
            return "(int64_t)(" + expr + ")";
        return expr;
    }
    if (want == VType::Float64)
        return "((" + expr + ").type == V_FLOAT64) ? (" + expr +
               ").as.f64 : (double)(" + expr + ").as.i64";
    if (want == VType::Int64)
        return "((" + expr + ").type == V_INT64) ? (" + expr +
               ").as.i64 : (int64_t)(" + expr + ").as.f64";
    return expr;
}

// Box a C expression into a VyneValue expression. If the expression is
// a registered typed array local/temp, this calls vyne_array_*_to_value
// (a copying allocation). Otherwise it delegates to e.boxIfNative, so
// this is a strict superset — safe to use at every boxing site.
//
// Note: this may allocate. Callers that emit the same string twice will
// box twice. In practice every call site materializes the result into a
// temp immediately, so this is one allocation per site.
static std::string boxTypedArray(C_Emitter& e, const std::string& expr) {
    const CType* ct = lookupCType(e, expr);
    if (ct && ct->kind == CType::Kind::Array && !ct->args.empty()) {
        VType elem = ct->args[0].toVType();
        if (elem == VType::Float64)
            return "vyne_array_f64_to_value(&" + expr + ")";
        if (elem == VType::Int64)
            return "vyne_array_i64_to_value(&" + expr + ")";
    }
    return e.boxIfNative(expr);
}

std::string NumberNode::getCExpr(C_Emitter& e) const {
    if (value.getType() == Value::INT64)
        return "vyne_int(" + std::to_string(value.asInt()) + ")";
    return "vyne_float(" + floatLit(value.asFloat()) + ")";
}

std::string NumberNode::nativeLiteral() const {
    if (value.getType() == Value::INT64)
        return std::to_string(value.asInt());
    return floatLit(value.asFloat());
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
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\%03o", c);
                    escaped += buf;
                } else {
                    escaped += static_cast<char>(c);
                }
        }
    }
    return "vyne_string_static(\"" + escaped + "\")";
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

// M1 (issue #79): build the initializer expression for a native-typed local
// declaration or assignment of static type `declared`.
//
//   - NumberNode literal → raw numeric literal (C implicitly widens int → double)
//   - Existing native expression of the same kind → used directly
//   - Anything else (boxed VyneValue) → runtime-checked coercion into the
//     native type, mirroring the ForNode bound conversion and the
//     interpreter's convertIfNeeded semantics.
static std::string nativeInit(C_Emitter& e, const ASTNode* rhs,
                              const std::string& val, const CType& declared) {
    if (rhs->type() == NodeType::NUMBER) {
        auto* num = static_cast<const NumberNode*>(rhs);
        if ((declared.kind == CType::Kind::Int64 &&
             num->getStaticType() == VType::Int64) ||
            (declared.kind == CType::Kind::Float64)) {
            return num->nativeLiteral();
        }
    }
    if (rhs->type() == NodeType::BOOLEAN &&
        declared.kind == CType::Kind::Bool) {
        return static_cast<const BooleanNode*>(rhs)->nativeLiteral();
    }

    const CType* vt = e.exprNativeType(val);
    if (vt && vt->isPrimitive()) {
        // Native value already in hand → same kind directly, other numeric
        // kind via a C cast (mirrors the interpreter's convertIfNeeded).
        if (vt->kind == declared.kind) return val;
        if (declared.kind == CType::Kind::Float64 && vt->kind == CType::Kind::Int64)
            return "(double)(" + val + ")";
        if (declared.kind == CType::Kind::Int64 && vt->kind == CType::Kind::Float64)
            return "(int64_t)(" + val + ")";
        return val;
    }

    switch (declared.kind) {
        case CType::Kind::Int64:
            return "((" + val + ").type == V_INT64) ? (" + val +
                   ").as.i64 : (int64_t)(" + val + ").as.f64";
        case CType::Kind::Float64:
            return "((" + val + ").type == V_FLOAT64) ? (" + val +
                   ").as.f64 : (double)(" + val + ").as.i64";
        case CType::Kind::Bool:
            return "((" + val + ").as.i64 != 0)";
        default:
            return val;
    }
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
        useGlobal = true;
    } else if (!isDeclaration && hasGlobal) {
        useGlobal = true;
    } else {
        useGlobal = false;
    }

    std::string varName = useGlobal ? bareName
                                    : ("v_" + prefix + "_" + sanitized);

    if (useGlobal) {
        CType declared = CType::fromVType(expectedType);

        // M4: top-level `xs: Array<Int64> = [...]` → typed C global.
        // The declaration goes to file scope as `VyneArray_i64 v_xs;`;
        // the assignment happens inside main() because the array ctor
        // touches the arena, which only exists at runtime.
        if (isDeclaration && declared.kind == CType::Kind::Array) {
            VType elem = inferArrayElemType(rhs.get());
            if (elem != VType::Unknown) {
                std::string val = rhs->getCExpr(e);
                const CType* rt = lookupCType(e, val);
                if (rt && rt->kind == CType::Kind::Array && !rt->args.empty()) {
                    if (!hasGlobal) {
                        CType arrType;
                        arrType.kind = CType::Kind::Array;
                        arrType.args.push_back(CType::fromVType(elem));
                        e.declareGlobal(bareName, arrType);
                        e.emitGlobalDecl(typedArrayCName(elem) + " " +
                                         bareName + ";");
                    }
                    e.emit(bareName + " = " + val + ";");
                    return;
                }
                // RHS didn't materialize as a typed array — fall through
                // to the boxed-global path below.
            }
        }

        // Reassignment to an existing typed-array global.
        const CType* existing = e.lookupGlobalType(bareName);
        if (existing && existing->kind == CType::Kind::Array &&
            !existing->args.empty()) {
            VType elem = existing->args[0].toVType();
            std::string val = rhs->getCExpr(e);
            const CType* rt = lookupCType(e, val);
            if (rt && rt->kind == CType::Kind::Array && !rt->args.empty() &&
                rt->args[0].toVType() == elem) {
                e.emit(bareName + " = " + val + ";");
            } else {
                throw std::runtime_error(
                    "Compile Error: cannot reassign typed-array global '" +
                    originalName +
                    "' from a value of a different element type (line " +
                    std::to_string(lineNumber) + ").");
            }
            return;
        }

        // Boxed global (unchanged fallback).
        if (!hasGlobal) {
            e.registerDeclaration(bareName);
            e.emitGlobalDecl("VyneValue " + bareName + ";");
        }
        std::string val = rhs->getCExpr(e);
        e.emit(bareName + " = " + boxTypedArray(e, val) + ";");
        return;
    }

    // M4-C1B: record the declared struct type so downstream member reads
    // can look up field element types. Only when the declaration carried
    // an explicit type path (user wrote `m :: Types.Matrix = ...`).
    if (!declaredTypeName.empty()) {
        if (useGlobal) e.setGlobalStructType(bareName, declaredTypeName);
        else           e.setLocalStructType(varName, declaredTypeName);
    }

    // --- fresh local declaration ---
    if (!e.isLocalDeclared(varName)) {
        CType declared = CType::fromVType(expectedType);

        // M4: Array<T> local whose RHS is a homogeneous numeric literal.
        if (isDeclaration && declared.kind == CType::Kind::Array) {
            VType elem = inferArrayElemType(rhs.get());
            if (elem != VType::Unknown) {
                std::string val = rhs->getCExpr(e);
                const CType* rt = lookupCType(e, val);
                if (rt && rt->kind == CType::Kind::Array && !rt->args.empty()) {
                    CType arrType;
                    arrType.kind = CType::Kind::Array;
                    arrType.args.push_back(CType::fromVType(elem));
                    e.declareLocal(varName, arrType);
                    e.emit(typedArrayCName(elem) + " " + varName +
                           " = " + val + ";");
                    return;
                }
                // RHS didn't materialize as a typed array — box it.
                e.registerDeclaration(varName);
                e.emit("VyneValue " + varName + " = " +
                       boxTypedArray(e, val) + ";");
                return;
            }
        }

        if (isDeclaration && declared.isPrimitive()) {
            std::string val = rhs->getCExpr(e);
            std::string init = nativeInit(e, rhs.get(), val, declared);
            e.declareLocal(varName, declared);
            e.emit(declared.cTypeName() + " " + varName + " = " + init + ";");
        } else {
            e.registerDeclaration(varName);
            std::string val = rhs->getCExpr(e);
            e.emit("VyneValue " + varName + " = " +
                   boxTypedArray(e, val) + ";");
        }
        return;
    }

    // --- reassignment to an existing local ---
    const CType* existing = e.lookupLocalType(varName);

    if (existing && existing->isPrimitive()) {
        std::string val = rhs->getCExpr(e);
        std::string init = nativeInit(e, rhs.get(), val, *existing);
        e.emit(varName + " = " + init + ";");
        return;
    }

    if (existing && existing->kind == CType::Kind::Array &&
        !existing->args.empty()) {
        VType elem = existing->args[0].toVType();
        std::string val = rhs->getCExpr(e);
        const CType* rt = lookupCType(e, val);
        if (rt && rt->kind == CType::Kind::Array && !rt->args.empty() &&
            rt->args[0].toVType() == elem) {
            e.emit(varName + " = " + val + ";");
        } else {
            throw std::runtime_error(
                "Compile Error: cannot reassign typed array '" + originalName +
                "' from a value of a different element type (line " +
                std::to_string(lineNumber) + ").");
        }
        return;
    }

    // --- boxed local ---
    std::string val = rhs->getCExpr(e);
    e.emit(varName + " = " + boxTypedArray(e, val) + ";");
}

// ============================================================
// BINARY / UNARY / POSTFIX
// ============================================================

std::string BinOpNode::getCExpr(C_Emitter& e) const {
    // Materialize both operands first — their getCExpr() may emit statements.
    std::string l = leftNode->getCExpr(e);
    std::string r = rightNode->getCExpr(e);
    std::string temp = e.newTemp("bin");

    // Effective static operand types: node-reported first; when a reference
    // reports Unknown (plain variable reads do), fall back to the emitter's
    // native type table so unboxed locals still take the fast path.
    VType lt = leftNode->getStaticType();
    if (lt == VType::Unknown) {
        if (const CType* ct = e.exprNativeType(l)) lt = ct->toVType();
    }
    VType rt = rightNode->getStaticType();
    if (rt == VType::Unknown) {
        if (const CType* ct = e.exprNativeType(r)) rt = ct->toVType();
    }

    // Read an operand as a native value of static type `st`:
    //  - numeric literal → raw literal
    //  - registered native expression of the same type → used directly
    //  - native of the other numeric kind → cast
    //  - boxed VyneValue → union member read
    auto operand = [&](const ASTNode* node, const std::string& expr,
                       VType st) -> std::string {
        if (node->type() == NodeType::NUMBER && st != VType::Unknown) {
            auto* num = static_cast<const NumberNode*>(node);
            if ((st == VType::Int64 && num->getStaticType() == VType::Int64) ||
                st == VType::Float64) {
                return num->nativeLiteral();
            }
        }
        if (st == VType::Int64 || st == VType::Float64) {
            if (const CType* ct = e.exprNativeType(expr)) {
                if (ct->toVType() == st) return expr;
                if (ct->kind == CType::Kind::Int64 && st == VType::Float64)
                    return "(double)(" + expr + ")";
            }
            return "(" + expr + ")." +
                   (st == VType::Float64 ? "as.f64" : "as.i64");
        }
        return expr;
    };

    // =========================================================
    // FAST PATH: Int64 op Int64 → native int64_t result
    // =========================================================
    if (lt == VType::Int64 && rt == VType::Int64) {
        std::string lv = operand(leftNode.get(), l, VType::Int64);
        std::string rv = operand(rightNode.get(), r, VType::Int64);
        switch (op) {
            case VTokenType::Add:
                e.emit("int64_t " + temp + " = " + lv + " + " + rv + ";");
                e.declareNativeTemp(temp, CType::fromVType(VType::Int64));
                return temp;
            case VTokenType::Substract:
                e.emit("int64_t " + temp + " = " + lv + " - " + rv + ";");
                e.declareNativeTemp(temp, CType::fromVType(VType::Int64));
                return temp;
            case VTokenType::Multiply:
                e.emit("int64_t " + temp + " = " + lv + " * " + rv + ";");
                e.declareNativeTemp(temp, CType::fromVType(VType::Int64));
                return temp;
            case VTokenType::Division:
            case VTokenType::Floor_Divide:
                e.emit("if (" + rv + " == 0) { fprintf(stderr, \"Runtime error: Division by zero!\\n\"); exit(1); }");
                e.emit("int64_t " + temp + " = " + lv + " / " + rv + ";");
                e.declareNativeTemp(temp, CType::fromVType(VType::Int64));
                return temp;
            case VTokenType::Modulo:
                e.emit("if (" + rv + " == 0) { fprintf(stderr, \"Runtime error: Modulo by zero!\\n\"); exit(1); }");
                e.emit("int64_t " + temp + " = " + lv + " % " + rv + ";");
                e.declareNativeTemp(temp, CType::fromVType(VType::Int64));
                return temp;
            case VTokenType::Power:
                e.emit("VyneValue " + temp + " = vyne_float(pow((double)" + lv + ", (double)" + rv + "));");
                return temp;
            case VTokenType::Double_Equals:
                e.emit("VyneValue " + temp + " = vyne_bool(" + lv + " == " + rv + ");");
                return temp;
            case VTokenType::Not_Equal:
                e.emit("VyneValue " + temp + " = vyne_bool(" + lv + " != " + rv + ");");
                return temp;
            case VTokenType::Greater:
                e.emit("VyneValue " + temp + " = vyne_bool(" + lv + " > " + rv + ");");
                return temp;
            case VTokenType::Smaller:
                e.emit("VyneValue " + temp + " = vyne_bool(" + lv + " < " + rv + ");");
                return temp;
            case VTokenType::Greater_Or_Equal:
                e.emit("VyneValue " + temp + " = vyne_bool(" + lv + " >= " + rv + ");");
                return temp;
            case VTokenType::Smaller_Or_Equal:
                e.emit("VyneValue " + temp + " = vyne_bool(" + lv + " <= " + rv + ");");
                return temp;
            case VTokenType::And:
                e.emit("VyneValue " + temp + " = vyne_bool((" + lv + " != 0) && (" + rv + " != 0));");
                return temp;
            case VTokenType::Or:
                e.emit("VyneValue " + temp + " = vyne_bool((" + lv + " != 0) || (" + rv + " != 0));");
                return temp;
            default:
                break; // fall through to slow path
        }
    }

    // =========================================================
    // FAST PATH: Float64 op Float64 → native double result
    // =========================================================
    if (lt == VType::Float64 && rt == VType::Float64) {
        std::string lv = operand(leftNode.get(), l, VType::Float64);
        std::string rv = operand(rightNode.get(), r, VType::Float64);
        switch (op) {
            case VTokenType::Add:
                e.emit("double " + temp + " = " + lv + " + " + rv + ";");
                e.declareNativeTemp(temp, CType::fromVType(VType::Float64));
                return temp;
            case VTokenType::Substract:
                e.emit("double " + temp + " = " + lv + " - " + rv + ";");
                e.declareNativeTemp(temp, CType::fromVType(VType::Float64));
                return temp;
            case VTokenType::Multiply:
                e.emit("double " + temp + " = " + lv + " * " + rv + ";");
                e.declareNativeTemp(temp, CType::fromVType(VType::Float64));
                return temp;
            case VTokenType::Division:
                e.emit("if (" + rv + " == 0.0) { fprintf(stderr, \"Runtime error: Division by zero!\\n\"); exit(1); }");
                e.emit("double " + temp + " = " + lv + " / " + rv + ";");
                e.declareNativeTemp(temp, CType::fromVType(VType::Float64));
                return temp;
            case VTokenType::Modulo:
                e.emit("if (" + rv + " == 0.0) { fprintf(stderr, \"Runtime error: Modulo by zero!\\n\"); exit(1); }");
                e.emit("double " + temp + " = fmod(" + lv + ", " + rv + ");");
                e.declareNativeTemp(temp, CType::fromVType(VType::Float64));
                return temp;
            case VTokenType::Power:
                e.emit("double " + temp + " = pow(" + lv + ", " + rv + ");");
                e.declareNativeTemp(temp, CType::fromVType(VType::Float64));
                return temp;
            case VTokenType::Double_Equals:
                e.emit("VyneValue " + temp + " = vyne_bool(" + lv + " == " + rv + ");");
                return temp;
            case VTokenType::Not_Equal:
                e.emit("VyneValue " + temp + " = vyne_bool(" + lv + " != " + rv + ");");
                return temp;
            case VTokenType::Greater:
                e.emit("VyneValue " + temp + " = vyne_bool(" + lv + " > " + rv + ");");
                return temp;
            case VTokenType::Smaller:
                e.emit("VyneValue " + temp + " = vyne_bool(" + lv + " < " + rv + ");");
                return temp;
            case VTokenType::Greater_Or_Equal:
                e.emit("VyneValue " + temp + " = vyne_bool(" + lv + " >= " + rv + ");");
                return temp;
            case VTokenType::Smaller_Or_Equal:
                e.emit("VyneValue " + temp + " = vyne_bool(" + lv + " <= " + rv + ");");
                return temp;
            default:
                break; // fall through to slow path
        }
    }

    // =========================================================
    // SLOW PATH: dynamic dispatch through vyne_binop()
    // =========================================================
    int opCode = static_cast<int>(op);
    switch (op) {
        case VTokenType::Add:              opCode = 29; break;
        case VTokenType::Substract:        opCode = 30; break;
        case VTokenType::Multiply:         opCode = 31; break;
        case VTokenType::Division:         opCode = 32; break;
        case VTokenType::Modulo:           opCode = 36; break;
        case VTokenType::Power:            opCode = 37; break;
        case VTokenType::Double_Equals:    opCode = 43; break;
        case VTokenType::Not_Equal:        opCode = 44; break;
        case VTokenType::Greater:          opCode = 45; break;
        case VTokenType::Smaller:          opCode = 46; break;
        case VTokenType::Greater_Or_Equal: opCode = 47; break;
        case VTokenType::Smaller_Or_Equal: opCode = 48; break;
        case VTokenType::And:              opCode = 49; break;
        case VTokenType::Or:               opCode = 50; break;
        case VTokenType::Floor_Divide:     opCode = 51; break;
        default: break;
    }

    e.emit("VyneValue " + temp + " = vyne_binop(" + boxTypedArray(e, l) + ", " +
           boxTypedArray(e, r) + ", " + std::to_string(opCode) + ");");
    return temp;
}

void BinOpNode::compile(C_Emitter& e) const { getCExpr(e); }

std::string UnaryNode::getCExpr(C_Emitter& e) const {
    if (op == VTokenType::Addresser) {
        throw std::runtime_error(
            "Compile Error: '&' (address-of) is not supported by the C backend "
            "(line " + std::to_string(lineNumber) + "). Use the interpreter instead.");
    }

    std::string val = boxTypedArray(e, right->getCExpr(e));
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
    std::string temp = e.newTemp("post");

    // x.field++ / x.field-- : struct fields are reached via vyne_struct_get,
    // which returns by value, so we cannot do `....as.i64++` directly.
    // Read, bump, write back.
    if (left->type() == NodeType::MEMBER_ACCESS) {
        auto* memNode = static_cast<MemberAccessNode*>(left.get());
        std::string recv  = memNode->getReceiver()->getCExpr(e);
        uint32_t    fid   = StringPool::intern(memNode->getMemberName());
        std::string fname = memNode->getMemberName();

        e.emit("VyneValue " + temp + " = vyne_struct_get(" + recv +
               ", " + std::to_string(fid) + ");");

        std::string newV = e.newTemp("postn");
        int opc = (op == VTokenType::Double_Increment) ? 29 : 30; // ADD / SUB
        e.emit("VyneValue " + newV + " = vyne_binop(" + temp +
               ", vyne_int(1), " + std::to_string(opc) + ");");

        e.emit("vyne_struct_set(" + recv + ", " + std::to_string(fid) +
               ", \"" + fname + "\", " + newV + ");");
        return temp;
    }

    // Plain variable lvalue: the existing in-place update is fine.
    std::string var = left->getCExpr(e);
    const CType* nativeT = e.exprNativeType(var);
    if (nativeT && nativeT->isPrimitive()) {
        // M1: native int64_t/double local — box the old value, bump in place.
        e.emit("VyneValue " + temp + " = " + nativeT->box(var) + ";");
        if (op == VTokenType::Double_Increment) {
            e.emit(var + "++;");
        } else {
            e.emit(var + "--;");
        }
        return temp;
    }
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
    std::string cond = boxTypedArray(e, condition->getCExpr(e));
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
    std::string temp = e.newTemp("ifres");
    e.emit("VyneValue " + temp + " = vyne_null();");

    std::string cond = boxTypedArray(e, condition->getCExpr(e));
    e.emitBlockOpen("if (vyne_is_truthy(" + cond + ")) {");
    if (body) {
        std::string bv = boxTypedArray(e, body->getCExpr(e));
        e.emit(temp + " = " + bv + ";");
    }
    e.emitBlockClose();

    if (elseBody) {
        e.emitBlockOpen("else {");
        std::string ev = boxTypedArray(e, elseBody->getCExpr(e));
        e.emit(temp + " = " + ev + ";");
        e.emitBlockClose();
    }

    return temp;
}

void WhileNode::compile(C_Emitter& e) const {
    e.emitBlockOpen("while (1) {");
    std::string cond = boxTypedArray(e, condition->getCExpr(e));
    e.emit("if (!vyne_is_truthy(" + cond + ")) break;");
    if (body) body->compile(e);
    e.emitBlockClose();
}

std::string WhileNode::getCExpr(C_Emitter& e) const {
    compile(e);
    return "vyne_null()";
}

void ReturnNode::compile(C_Emitter& e) const {
    // The function ABI is still `(int, VyneValue*) -> VyneValue`, so native
    // values are boxed here at the boundary.
    std::string expr = expression ? boxTypedArray(e, expression->getCExpr(e))
                                  : "vyne_null()";

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

// M1 (issue #79): build an int64_t bound expression for `through x :: lo..hi`.
//   - Int64 literal      → raw literal
//   - native Int64 local → used directly
//   - native Float64     → truncated
//   - boxed VyneValue    → runtime-coerced exactly like the old path
static std::string int64Bound(C_Emitter& e, const ASTNode* node,
                              const std::string& expr) {
    if (node && node->type() == NodeType::NUMBER) {
        auto* num = static_cast<const NumberNode*>(node);
        if (num->getStaticType() == VType::Int64)
            return num->nativeLiteral();
    }
    if (const CType* ct = e.exprNativeType(expr)) {
        if (ct->kind == CType::Kind::Int64) return expr;
        if (ct->kind == CType::Kind::Float64) return "(int64_t)(" + expr + ")";
    }
    return "((" + expr + ".type == V_INT64) ? " + expr +
           ".as.i64 : (int64_t)" + expr + ".as.f64)";
}

void ForNode::compile(C_Emitter& e) const {
    // -----------------------------------------------------------------
    // Range fast path (unchanged, M1).
    // -----------------------------------------------------------------
    if (iterable->type() == NodeType::RANGE) {
        auto* rng = static_cast<RangeNode*>(iterable.get());
        std::string lo  = rng->getLeft()->getCExpr(e);
        std::string hi  = rng->getRight()->getCExpr(e);
        std::string loT = e.newTemp("lo_i");
        std::string hiT = e.newTemp("hi_i");
        std::string iv  = e.newTemp("i");
        std::string elemVar = "v_" + iteratorName;

        e.emit("int64_t " + loT + " = " +
               int64Bound(e, rng->getLeft(), lo) + ";");
        e.emit("int64_t " + hiT + " = " +
               int64Bound(e, rng->getRight(), hi) + ";");
        e.emitBlockOpen("for (int64_t " + iv + " = " + loT + "; " + iv +
                        " <= " + hiT + "; ++" + iv + ") {");
        e.declareLocal(elemVar, CType::fromVType(VType::Int64));
        e.emit("int64_t " + elemVar + " = " + iv + ";");
        if (body) body->compile(e);
        e.emitBlockClose();
        return;
    }

    std::string collection = iterable->getCExpr(e);

    // -----------------------------------------------------------------
    // M4: flat iteration over a typed array.
    // -----------------------------------------------------------------
    {
        const CType* ct = lookupCType(e, collection);
        if (ct && ct->kind == CType::Kind::Array && !ct->args.empty()) {
            VType elem = ct->args[0].toVType();
            CType elemType = CType::fromVType(elem);
            std::string iv = e.newTemp("i");
            std::string elemVar = "v_" + iteratorName;

            e.emitBlockOpen("for (int64_t " + iv + " = 0; " + iv + " < " +
                            collection + ".size; ++" + iv + ") {");
            e.declareLocal(elemVar, elemType);
            e.emit(elemType.cTypeName() + " " + elemVar + " = " +
                   collection + ".data[" + iv + "];");
            if (body) body->compile(e);
            e.emitBlockClose();
            return;
        }
    }

    // -----------------------------------------------------------------
    // Boxed path (unchanged).
    // -----------------------------------------------------------------
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

    // -----------------------------------------------------------------
    // Fast path for `through x :: lo..hi -> collect|filter|every|unique`
    //
    // Same idea as ForNode::compile's fast path, but with the accumulator
    // plumbing that the non-LOOP modes need. The per-mode body emission is
    // duplicated below because the loop header differs (native for-loop vs
    // array indexing); the accumulator semantics are identical.
    // -----------------------------------------------------------------
    if (iterable->type() == NodeType::RANGE) {
        auto* rng = static_cast<RangeNode*>(iterable.get());
        std::string lo  = rng->getLeft()->getCExpr(e);
        std::string hi  = rng->getRight()->getCExpr(e);
        std::string loT = e.newTemp("lo_i");
        std::string hiT = e.newTemp("hi_i");
        std::string iv  = e.newTemp("i");
        std::string elemVar = "v_" + iteratorName;

        e.emit("int64_t " + loT + " = " +
               int64Bound(e, rng->getLeft(), lo) + ";");
        e.emit("int64_t " + hiT + " = " +
               int64Bound(e, rng->getRight(), hi) + ";");

        // --- EVERY mode ---
        if (mode == ForMode::EVERY) {
            std::string everyTemp = e.newTemp("every");
            std::string resTemp   = e.newTemp("every_res");
            e.emit("bool " + everyTemp + " = true;");
            e.emitBlockOpen("for (int64_t " + iv + " = " + loT + "; " + iv +
                            " <= " + hiT + "; ++" + iv + ") {");
            e.declareLocal(elemVar, CType::fromVType(VType::Int64));
            e.emit("int64_t " + elemVar + " = " + iv + ";");
            std::string cond = boxTypedArray(e, body->getCExpr(e));
            e.emitBlockOpen("if (!vyne_is_truthy(" + cond + ")) {");
            e.emit(everyTemp + " = false;");
            e.emit("break;");
            e.emitBlockClose();
            e.emitBlockClose();
            e.emit("VyneValue " + resTemp + " = vyne_bool(" + everyTemp + ");");
            return resTemp;
        }

        // --- COLLECT / FILTER / UNIQUE ---
        std::string listTemp = e.newTemp("res");
        e.emit("VyneValue " + listTemp + " = vyne_array_create(0);");
        e.emitBlockOpen("for (int64_t " + iv + " = " + loT + "; " + iv +
                        " <= " + hiT + "; ++" + iv + ") {");
        e.declareLocal(elemVar, CType::fromVType(VType::Int64));
        e.emit("int64_t " + elemVar + " = " + iv + ";");

        switch (mode) {
            case ForMode::COLLECT: {
                std::string result = boxTypedArray(e, body->getCExpr(e));
                e.emit("vyne_array_push(" + listTemp + ", " + result + ");");
                break;
            }
            case ForMode::FILTER: {
                std::string cond = boxTypedArray(e, body->getCExpr(e));
                e.emitBlockOpen("if (vyne_is_truthy(" + cond + ")) {");
                e.emit("vyne_array_push(" + listTemp + ", " +
                       boxTypedArray(e, elemVar) + ");");
                e.emitBlockClose();
                break;
            }
            case ForMode::UNIQUE: {
                std::string dupCheck = e.newTemp("seen");
                e.emit("bool " + dupCheck + " = vyne_array_contains(" +
                       listTemp + ", " + boxTypedArray(e, elemVar) + ");");
                e.emitBlockOpen("if (!" + dupCheck + ") {");
                e.emit("vyne_array_push(" + listTemp + ", " +
                       boxTypedArray(e, elemVar) + ");");
                e.emitBlockClose();
                break;
            }
            default: break;
        }

        e.emitBlockClose();
        return listTemp;
    }

    // --- M4-C1B: flat collect/every over a typed array ----------------
    {
        std::string rawCollection = iterable->getCExpr(e);
        const CType* ct = lookupCType(e, rawCollection);
        if (ct && ct->kind == CType::Kind::Array && !ct->args.empty()) {
            VType elem = ct->args[0].toVType();
            CType elemType = CType::fromVType(elem);
            VType bodyType = body ? body->getStaticType() : VType::Unknown;

            if (mode == ForMode::COLLECT && bodyType == elem) {
                std::string outName = e.newTemp("cfld");
                std::string ctor    = (elem == VType::Float64)
                    ? "vyne_array_f64_create" : "vyne_array_i64_create";
                std::string pushFn  = (elem == VType::Float64)
                    ? "vyne_array_f64_push"   : "vyne_array_i64_push";

                e.emit(typedArrayCName(elem) + " " + outName + " = " +
                       ctor + "(" + rawCollection + ".size);");
                e.emit(outName + ".size = 0;");

                std::string iv = e.newTemp("i");
                e.emitBlockOpen("for (int64_t " + iv + " = 0; " + iv +
                                " < " + rawCollection + ".size; ++" + iv + ") {");
                std::string elemVar = "v_" + iteratorName;
                e.declareLocal(elemVar, elemType);
                e.emit(elemType.cTypeName() + " " + elemVar + " = " +
                       rawCollection + ".data[" + iv + "];");
                std::string rawExpr = body->getCExpr(e);
                std::string native  = coerceToNative(e, body.get(), rawExpr, elem);
                e.emit(pushFn + "(&" + outName + ", " + native + ");");
                e.emitBlockClose();

                CType outCT;
                outCT.kind = CType::Kind::Array;
                outCT.args.push_back(elemType);
                e.declareNativeTemp(outName, outCT);
                return outName;
            }

            if (mode == ForMode::EVERY) {
                std::string every = e.newTemp("every");
                std::string res   = e.newTemp("every_res");
                e.emit("bool " + every + " = true;");

                std::string iv = e.newTemp("i");
                e.emitBlockOpen("for (int64_t " + iv + " = 0; " + iv +
                                " < " + rawCollection + ".size; ++" + iv + ") {");
                std::string elemVar = "v_" + iteratorName;
                e.declareLocal(elemVar, elemType);
                e.emit(elemType.cTypeName() + " " + elemVar + " = " +
                       rawCollection + ".data[" + iv + "];");
                std::string rawExpr = body->getCExpr(e);
                std::string cond    = boxTypedArray(e, rawExpr);
                e.emitBlockOpen("if (!vyne_is_truthy(" + cond + ")) {");
                e.emit(every + " = false;");
                e.emit("break;");
                e.emitBlockClose();
                e.emitBlockClose();
                e.emit("VyneValue " + res + " = vyne_bool(" + every + ");");
                return res;
            }
        }
    }

    // --- Boxed fallback -----------------------------------------------
    std::string collection = boxTypedArray(e, iterable->getCExpr(e));
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
        std::string cond = boxTypedArray(e, body->getCExpr(e));
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
            std::string result = boxTypedArray(e, body->getCExpr(e));
            e.emit("vyne_array_push(" + listTemp + ", " + result + ");");
            break;
        }
        case ForMode::FILTER: {
            std::string cond = boxTypedArray(e, body->getCExpr(e));
            e.emitBlockOpen("if (vyne_is_truthy(" + cond + ")) {");
            e.emit("vyne_array_push(" + listTemp + ", " +
                   boxTypedArray(e, elemVar) + ");");
            e.emitBlockClose();
            break;
        }
        case ForMode::UNIQUE: {
            std::string dupCheck = e.newTemp("seen");
            e.emit("bool " + dupCheck + " = vyne_array_contains(" +
                   listTemp + ", " + boxTypedArray(e, elemVar) + ");");
            e.emitBlockOpen("if (!" + dupCheck + ") {");
            e.emit("vyne_array_push(" + listTemp + ", " +
                   boxTypedArray(e, elemVar) + ");");
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

        std::string arg = "args[" + std::to_string(i) + "]";
        std::string guard = "(arg_count > " + std::to_string(i) + ")";

        // M1 (issue #79): unbox known-primitive params into native locals at
        // function entry. The ABI stays `(int, VyneValue*)` — callers still
        // box, and any dynamic value (e.g. a Float64 where Int64 is declared)
        // is coerced here exactly like the old ForNode bound conversion.
        // Non-reference params only; reference params go through the
        // interpreter path and stay boxed.
        CType pt = CType::fromVType(parameters[i].type);
        if (pt.isPrimitive() && !parameters[i].isReference) {
            std::string init = "0";
            switch (pt.kind) {
                case CType::Kind::Int64:
                    init = "(" + guard + ") ? ((" + arg + ".type == V_INT64) ? " +
                           arg + ".as.i64 : (int64_t)" + arg + ".as.f64) : 0";
                    break;
                case CType::Kind::Float64:
                    init = "(" + guard + ") ? ((" + arg + ".type == V_FLOAT64) ? " +
                           arg + ".as.f64 : (double)" + arg + ".as.i64) : 0.0";
                    break;
                case CType::Kind::Bool:
                    init = "(" + guard + ") ? (" + arg + ".as.i64 != 0) : false";
                    break;
                default: break;
            }
            e.declareLocal(paramName, pt);
            e.emit(pt.cTypeName() + " " + paramName + " = " + init + ";");
        } else {
            e.registerDeclaration(paramName);
            e.emit("VyneValue " + paramName +
                   " = " + guard + " ? " + arg + " : vyne_null();");
        }
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
    // --- Resolve named args (unchanged) -----------------------------
    std::vector<ASTNode*> orderedArgs;
    if (hasNamedArguments()) {
        const auto* sig = e.getFunctionSignature(originalName);
        if (!sig) {
            throw std::runtime_error(
                "Compile Error: named arguments used for function '" +
                originalName + "' but its signature is unknown (line " +
                std::to_string(lineNumber) + ").");
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

    // --- M2: monomorphization --------------------------------------
    // Only fire when the parser recorded type args. Inference is a
    // follow-up; explicit `foo<Int64>(...)` is the load-bearing case.
    if (!typeArgs.empty()) {
        std::string key = originalName;
        std::replace(key.begin(), key.end(), '.', '_');
        for (const auto& t : typeArgs) key += "__" + t;

        if (const std::string* emitted = e.lookupInstantiation(key)) {
            // Already emitted: call the specialised C function directly.
            int n = (int)orderedArgs.size();
            std::string retTemp = e.newTemp("ret");
            std::string argArr  = e.newTemp("args");
            if (n > 0) {
                e.emit("VyneValue* " + argArr +
                       " = (VyneValue*)arena_alloc(sizeof(VyneValue) * " +
                       std::to_string(n) + ");");
                for (int i = 0; i < n; ++i) {
                    e.emit(argArr + "[" + std::to_string(i) + "] = " +
                           boxTypedArray(e, orderedArgs[i]->getCExpr(e)) + ";");
                }
            } else {
                e.emit("VyneValue* " + argArr + " = NULL;");
            }
            e.emit("VyneValue " + retTemp + " = fn_" + *emitted +
                   "(" + std::to_string(n) + ", " + argArr + ");");
            return retTemp;
        }

        // First sighting: instantiate on demand. The concrete C name
        // is the same key we just built, prefixed with "fn_".
        if (e.beginInstantiation(key)) {
            std::string cName = key;
            e.finishInstantiation(key, cName);
            // NOTE: actual body emission is scheduled by ProgramNode
            // via the emitter's instantiation queue — see
            // `ProgramNode::compile` for the drain loop. For now, fall
            // through to the boxed call so unresolvable cases still
            // produce compilable C.
        }
    }

    int argSize = (int)orderedArgs.size();
    std::string retTemp = e.newTemp("ret");

    std::string mangledName = originalName;
    std::replace(mangledName.begin(), mangledName.end(), '.', '_');

    // ----------------------------------------------------------------
    // Interface constructors take their arguments directly — no
    // `args[]` array, no arena allocation, no deep-copy dance.
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
            argStrs.push_back(boxTypedArray(e, argNode->getCExpr(e)));
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
            // NOTE: no deep copy. Array / map arguments are shared by
            // reference, matching assignment semantics and Python / JS /
            // Lua. If a callee mutates its parameter, the caller sees it.
            std::string val = boxTypedArray(e, orderedArgs[i]->getCExpr(e));
            e.emit(argArr + "[" + std::to_string(i) + "] = " + val + ";");
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
    // M4: homogeneous numeric literal → flat typed array.
    VType elem = inferArrayElemType(this);
    if (elem != VType::Unknown && !elements.empty()) {
        std::string name = e.newTemp("arr");
        std::string ctor = (elem == VType::Float64)
            ? "vyne_array_f64_create"
            : "vyne_array_i64_create";

        e.emit(typedArrayCName(elem) + " " + name + " = " +
               ctor + "(" + std::to_string(elements.size()) + ");");

        for (size_t i = 0; i < elements.size(); ++i) {
            std::string raw = elements[i]->getCExpr(e);
            std::string native = coerceToNative(e, elements[i].get(), raw, elem);
            e.emit(name + ".data[" + std::to_string(i) + "] = " + native + ";");
        }

        CType ct;
        ct.kind = CType::Kind::Array;
        ct.args.push_back(CType::fromVType(elem));
        e.declareNativeTemp(name, ct);
        return name;
    }

    // Boxed fallback — unchanged except the per-element boxing uses
    // boxTypedArray so a nested typed array literal still boxes correctly.
    std::string temp = e.newTemp("arr");
    int size = (int)elements.size();
    e.emit("VyneValue " + temp + " = vyne_array_create(" +
           std::to_string(size) + ");");
    for (int i = 0; i < size; i++) {
        std::string elemExpr = boxTypedArray(e, elements[i]->getCExpr(e));
        e.emit("vyne_array_set(" + temp + ", vyne_int(" +
               std::to_string(i) + "), " + elemExpr + ");");
    }
    return temp;
}

void ArrayNode::compile(C_Emitter& e) const { getCExpr(e); }

std::string IndexAccessNode::getCExpr(C_Emitter& e) const {
    std::string bRaw = base->getCExpr(e);
    const CType* bt = lookupCType(e, bRaw);

    // M4: typed-array base -> direct .data[i], native result.
    if (bt && bt->kind == CType::Kind::Array && !bt->args.empty()) {
        VType elem = bt->args[0].toVType();
        std::string rawIdx = index->getCExpr(e);
        std::string idx = coerceToNative(e, index.get(), rawIdx, VType::Int64);

        std::string name = e.newTemp("idx");
        e.emit((elem == VType::Float64 ? "double " : "int64_t ") + name +
               " = " + bRaw + ".data[" + idx + "];");
        e.declareNativeTemp(name, CType::fromVType(elem));
        return name;
    }

    // Boxed path — unchanged.
    std::string b = boxTypedArray(e, bRaw);
    std::string idx = boxTypedArray(e, index->getCExpr(e));
    std::string temp = e.newTemp("idx");
    e.emit("VyneValue " + temp + " = vyne_index_get(" + b + ", " + idx + ");");
    return temp;
}

void IndexAccessNode::compile(C_Emitter& e) const { getCExpr(e); }

void IndexAssignmentNode::compile(C_Emitter& e) const {
    std::string bRaw = base->getCExpr(e);
    const CType* bt = lookupCType(e, bRaw);

    // M4: typed-array element store — direct .data[i] = v.
    if (bt && bt->kind == CType::Kind::Array && !bt->args.empty()) {
        VType elem = bt->args[0].toVType();
        std::string rawIdx = index->getCExpr(e);
        std::string idx = coerceToNative(e, index.get(), rawIdx, VType::Int64);
        std::string rawVal = rhs->getCExpr(e);
        std::string val = coerceToNative(e, rhs.get(), rawVal, elem);
        e.emit(bRaw + ".data[" + idx + "] = " + val + ";");
        return;
    }

    // Boxed fallback — same as before, routed through boxTypedArray so a
    // typed array literal on the RHS still boxes correctly.
    std::string b = boxTypedArray(e, bRaw);
    std::string i = boxTypedArray(e, index->getCExpr(e));
    std::string r = boxTypedArray(e, rhs->getCExpr(e));
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
    std::string l = boxTypedArray(e, left->getCExpr(e));
    std::string r = boxTypedArray(e, right->getCExpr(e));
    std::string temp = e.newTemp("rng");
    e.emit("VyneValue " + temp + " = vyne_range_create(" + l + ", " + r + ");");
    return temp;
}

void RangeNode::compile(C_Emitter& e) const { getCExpr(e); }

std::string SliceNode::getCExpr(C_Emitter& e) const {
    std::string bRaw = base->getCExpr(e);
    const CType* bt = lookupCType(e, bRaw);

    // M4: typed-array slice -> new typed array. `hi` is exclusive,
    // matching vyne_slice_get's memcpy count (a pre-existing divergence
    // from the interpreter's inclusive hi; fix that separately).
    if (bt && bt->kind == CType::Kind::Array && !bt->args.empty()) {
        VType elem = bt->args[0].toVType();
        std::string rawLo = low  ? low->getCExpr(e)  : "";
        std::string rawHi = high ? high->getCExpr(e) : "";
        std::string lo = low
            ? coerceToNative(e, low.get(),  rawLo, VType::Int64)
            : "0";
        std::string hi = high
            ? coerceToNative(e, high.get(), rawHi, VType::Int64)
            : (bRaw + ".size");

        std::string fn = (elem == VType::Float64)
            ? "vyne_array_f64_slice"
            : "vyne_array_i64_slice";
        std::string name = e.newTemp("slc");
        e.emit(typedArrayCName(elem) + " " + name + " = " +
               fn + "(&" + bRaw + ", " + lo + ", " + hi + ");");

        CType ct;
        ct.kind = CType::Kind::Array;
        ct.args.push_back(CType::fromVType(elem));
        e.declareNativeTemp(name, ct);
        return name;
    }

    // Boxed fallback — same as before, with boxTypedArray on the base.
    std::string b  = boxTypedArray(e, bRaw);
    std::string lo = low  ? boxTypedArray(e, low->getCExpr(e))  : "vyne_null()";
    std::string hi = high ? boxTypedArray(e, high->getCExpr(e)) : "vyne_null()";
    std::string temp = e.newTemp("slc");
    e.emit("VyneValue " + temp + " = vyne_slice_get(" +
           b + ", " + lo + ", " + hi + ");");
    return temp;
}

void SliceNode::compile(C_Emitter& e) const { getCExpr(e); }

// ============================================================
// BUILT-INS
// ============================================================

std::string BuiltInCallNode::getCExpr(C_Emitter& e) const {
    if (funcName == "out") {
        for (const auto& arg : arguments) {
            e.emit("vyne_out(" + boxTypedArray(e, arg->getCExpr(e)) + ");");
        }
        return "vyne_null()";
    }
    if (funcName == "string") {
        if (arguments.empty()) return "vyne_string(\"\")";
        std::string temp = e.newTemp("str");
        e.emit("VyneValue " + temp + " = vyne_to_string(" +
               boxTypedArray(e, arguments[0]->getCExpr(e)) + ");");
        return temp;
    }
    if (funcName == "int64") {
        std::string arg = arguments.empty() ? "vyne_null()"
                                            : boxTypedArray(e, arguments[0]->getCExpr(e));
        return "vyne_to_int(" + arg + ")";
    }
    if (funcName == "float64") {
        if (arguments.empty()) return "vyne_float(0.0)";
        return "vyne_to_float(" + boxTypedArray(e, arguments[0]->getCExpr(e)) + ")";
    }
    if (funcName == "sizeof") {
        if (arguments.empty()) return "vyne_int(0)";
        std::string arg = boxTypedArray(e, arguments[0]->getCExpr(e));
        std::string temp = e.newTemp("sz");
        e.emit("VyneValue " + temp + " = vyne_int(vyne_get_sizeof(" + arg + "));");
        return temp;
    }
    if (funcName == "type") {
        if (arguments.empty()) return "vyne_string(\"null\")";
        std::string temp = e.newTemp("type");
        e.emit("VyneValue " + temp + " = vyne_string(vyne_get_type_name(" +
               boxTypedArray(e, arguments[0]->getCExpr(e)) + "));");
        return temp;
    }
    if (funcName == "free") {
        if (arguments.empty()) return "vyne_null()";
        // Free is a no-op in the C runtime since we use arena allocation
        e.emit("// free() called on: " + boxTypedArray(e, arguments[0]->getCExpr(e)));
        return "vyne_null()";
    }
    if (funcName == "exit") {
        if (!arguments.empty()) {
            e.emit("exit((int)" + boxTypedArray(e, arguments[0]->getCExpr(e)) + ".as.i64);");
        } else {
            e.emit("exit(0);");
        }
        return "vyne_null()";
    }
    if (funcName == "sequence") {
        if (arguments.size() < 2) return "vyne_array_create(0)";
        std::string start = boxTypedArray(e, arguments[0]->getCExpr(e));
        std::string end   = boxTypedArray(e, arguments[1]->getCExpr(e));
        std::string temp  = e.newTemp("seq");
        std::string iv    = e.newTemp("i");
        std::string nTmp  = e.newTemp("seq_n");

        e.emit("int64_t " + nTmp + " = (" + end + ").as.i64 - (" + start + ").as.i64;");
        e.emit("if (" + nTmp + " < 0) " + nTmp + " = 0;");
        e.emit("VyneValue " + temp + " = vyne_array_create((int)" + nTmp + ");");
        e.emitBlockOpen("for (int64_t " + iv + " = 0; " + iv + " < " + nTmp + "; " + iv + "++) {");
        e.emit("vyne_array_set(" + temp + ", vyne_int(" + iv + "), "
               "vyne_int((" + start + ").as.i64 + " + iv + "));");
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
            e.emit(mangled + " = " + boxTypedArray(e, val) + ";");
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
    std::string cond = boxTypedArray(e, condition->getCExpr(e));
    std::string tVal = boxTypedArray(e, trueExpr->getCExpr(e));
    std::string fVal = boxTypedArray(e, falseExpr->getCExpr(e));
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
    // --- Existing native-module / group resolution -------------------
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

    // --- M4-C1B: Array<T> struct field → native VyneArray_* temp -----
    if (receiver->type() == NodeType::VARIABLE) {
        auto* var = static_cast<VariableNode*>(receiver.get());
        std::string recvName = var->getOriginalName();

        std::string typeName;
        if (recvName == "self") {
            typeName = e.getCurrentInterfaceType();
        } else {
            std::string prefix = e.getActiveFunctionPrefix();
            std::string lookupKey = prefix.empty()
                ? ("v_" + recvName)
                : ("v_" + prefix + "_" + recvName);
            if (const std::string* t = e.lookupLocalStructType(lookupKey))
                typeName = *t;
            else if (const std::string* t = e.lookupGlobalStructType("v_" + recvName))
                typeName = *t;
        }

        if (!typeName.empty()) {
            VType elem = e.getInterfaceArrayElem(typeName, memberName);
            if (elem == VType::Int64 || elem == VType::Float64) {
                std::string cacheKey = recvName + "." + memberName;
                if (const auto* cached = e.getFieldCache(cacheKey))
                    return cached->temp;

                std::string recv = boxTypedArray(e, receiver->getCExpr(e));
                uint32_t fid = StringPool::intern(memberName);
                std::string temp = e.newTemp("fld");
                const char* cName = (elem == VType::Float64)
                    ? "VyneArray_f64" : "VyneArray_i64";
                const char* fn = (elem == VType::Float64)
                    ? "vyne_value_to_array_f64" : "vyne_value_to_array_i64";

                e.emit(std::string(cName) + " " + temp + " = " + fn +
                       "(vyne_struct_get(" + recv + ", " +
                       std::to_string(fid) + "));");

                CType ct;
                ct.kind = CType::Kind::Array;
                ct.args.push_back(CType::fromVType(elem));
                e.declareNativeTemp(temp, ct);
                e.setFieldCache(cacheKey, temp, ct);
                return temp;
            }
        }
    }

    // --- Boxed fallback ----------------------------------------------
    std::string recv = boxTypedArray(e, receiver->getCExpr(e));
    uint32_t fid = StringPool::intern(memberName);
    return "vyne_struct_get(" + recv + ", " + std::to_string(fid) + ")";
}

void MemberAccessNode::compile(C_Emitter& e) const {
    // Bare member access as statement — no-op
}

void MemberAssignmentNode::compile(C_Emitter& e) const {
    // Any write invalidates all cached unboxes for this function.
    e.clearFieldCache();

    std::string val = boxTypedArray(e, rhs->getCExpr(e));

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
            e.emit("vyne_struct_set(v_self, " + std::to_string(fid) +
                   ", \"" + memberName + "\", " + val + ");");
            return;
        }
    }

    std::string recv = boxTypedArray(e, receiver->getCExpr(e));
    uint32_t fid = StringPool::intern(memberName);
    e.emit("vyne_struct_set(" + recv + ", " + std::to_string(fid) +
           ", \"" + memberName + "\", " + val + ");");
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
            e.emit(mangled + " = " + boxTypedArray(e, val) + ";");
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
    if (originalName == "vmem")   e.addInclude((moduleBase / "vmem.h").string());
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

    // M4-C1B: register typed-array fields so member reads can unbox.
    for (const auto& m : members) {
        e.registerInterfaceArrayField(fullName, m.name, m.arrayElemType);
        e.registerInterfaceArrayField(interfaceName, m.name, m.arrayElemType);
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
        e.enterFunction(methodName); 
        e.setCurrentInterfaceType(fullName);
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
        e.exitFunction();
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
    // Resolve a dotted path for the receiver, e.g. "vmath", "vlinalg.Types",
    // or a plain variable name. Used to look up interfaces / groups.
    std::string recvPath;
    if (receiver->type() == NodeType::VARIABLE) {
        recvPath = static_cast<VariableNode*>(receiver.get())->getOriginalName();
    } else if (receiver->type() == NodeType::MEMBER_ACCESS) {
        recvPath = static_cast<MemberAccessNode*>(receiver.get())->getFullPath();
    }

    // ----------------------------------------------------------------
    // Native module dispatch (bare module name only, e.g. vmath.sqrt).
    // ----------------------------------------------------------------
    if (receiver->type() == NodeType::VARIABLE) {
        const NativeMapEntry* entry = e.findNative(recvPath, methodName);
        if (entry && !entry->isProperty) {
            std::string resTemp = e.newTemp("n_ret");

            if (entry->usesArgv) {
                // ---- variadic: emit (argc, argv) --------------------
                int n = (int)arguments.size();
                std::string argArr = e.newTemp("n_args");
                e.emit("VyneValue* " + argArr +
                       " = (VyneValue*)arena_alloc(sizeof(VyneValue) * " +
                       std::to_string(n > 0 ? n : 1) + ");");
                for (int i = 0; i < n; ++i) {
                    e.emit(argArr + "[" + std::to_string(i) + "] = " +
                           boxTypedArray(e, arguments[i]->getCExpr(e)) + ";");
                }
                e.emit("VyneValue " + resTemp + " = " + entry->cName +
                       "(" + std::to_string(n) + ", " + argArr + ");");
                return resTemp;
            }

            // M5: native _f64 dispatch when every arg is a provable double.
            if (entry->nativeF64 && !arguments.empty()) {
                bool allF64 = true;
                for (const auto& a : arguments) {
                    std::string raw = a->getCExpr(e);   // may emit
                    const CType* ct = e.exprNativeType(raw);
                    if (!ct || ct->kind != CType::Kind::Float64) { allF64 = false; break; }
                }
                if (allF64) {
                    std::string argStr;
                    for (size_t i = 0; i < arguments.size(); ++i) {
                        if (i > 0) argStr += ", ";
                        std::string raw = arguments[i]->getCExpr(e);
                        argStr += raw;
                    }
                    std::string resTemp = e.newTemp("n_ret");
                    e.emit("double " + resTemp + " = " + entry->nativeF64 +
                           "(" + argStr + ");");
                    e.declareNativeTemp(resTemp, CType::fromKind(CType::Kind::Float64));
                    return resTemp;
                }
            }

            // ---- fixed arity: emit (arg1, arg2, ...) ----------------
            std::string argStr;
            for (size_t i = 0; i < arguments.size(); ++i) {
                if (i > 0) argStr += ", ";
                argStr += boxTypedArray(e, arguments[i]->getCExpr(e));
            }
            e.emit("VyneValue " + resTemp + " = " + entry->cName +
                   "(" + argStr + ");");
            return resTemp;
        }
    }

    // ----------------------------------------------------------------
    // Interface constructor via a dotted path: a.b.Ctor(...)
    //
    // Try the full path first, then progressively shorter suffixes,
    // then the bare method name. `InterfaceNode::compile` registers
    // each interface under its bare name and its immediate module
    // prefix, so "vlinalg.Types.Matrix" resolves via the "Types.Matrix"
    // candidate.
    // ----------------------------------------------------------------
    if (!recvPath.empty()) {
        std::vector<std::string> candidates;
        candidates.push_back(recvPath);
        {
            std::string tmp = recvPath;
            size_t dot;
            while ((dot = tmp.find('.')) != std::string::npos) {
                tmp = tmp.substr(dot + 1);
                candidates.push_back(tmp);
            }
        }
        candidates.push_back("");  // bare method name

        for (const auto& base : candidates) {
            std::string dotted  = base.empty() ? methodName
                                              : (base + "." + methodName);
            std::string mangled = base.empty() ? methodName
                                               : (base + "_" + methodName);
            std::replace(mangled.begin(), mangled.end(), '.', '_');

            if (!e.isInterface(dotted) && !e.isInterface(mangled)) continue;

            const std::vector<std::string>* defaults =
                e.getInterfaceDefaults(dotted);
            if (!defaults) defaults = e.getInterfaceDefaults(mangled);
            if (!defaults) defaults = e.getInterfaceDefaults(methodName);

            std::vector<std::string> argStrs;
            argStrs.reserve(arguments.size());
            for (const auto& a : arguments)
                argStrs.push_back(boxTypedArray(e, a->getCExpr(e)));
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

            std::string resTemp = e.newTemp("g_iface");
            e.emit("VyneValue " + resTemp + " = struct_" + mangled +
                   "(" + directArgs + ");");
            return resTemp;
        }
    }

    // ----------------------------------------------------------------
    // Group call: module.add(...) / module.sub.method(...)
    // ----------------------------------------------------------------
    if (!recvPath.empty() && e.isGroup(recvPath)) {
        int argSize = (int)arguments.size();
        std::string argArr = e.newTemp("g_args");

        if (argSize > 0) {
            e.emit("VyneValue* " + argArr +
                   " = (VyneValue*)arena_alloc(sizeof(VyneValue) * " +
                   std::to_string(argSize) + ");");
            for (int i = 0; i < argSize; ++i) {
                e.emit(argArr + "[" + std::to_string(i) + "] = " +
                       boxTypedArray(e, arguments[i]->getCExpr(e)) + ";");
            }
        } else {
            e.emit("VyneValue* " + argArr + " = NULL;");
        }

        std::string resTemp = e.newTemp("g_ret");
        std::string gMethodName = recvPath + "_" + methodName;
        std::replace(gMethodName.begin(), gMethodName.end(), '.', '_');
        e.emit("VyneValue " + resTemp + " = fn_" + gMethodName +
               "(" + std::to_string(argSize) + ", " + argArr + ");");
        return resTemp;
    }

    // ----------------------------------------------------------------
    // Everything else: struct method call / built-in array / string /
    // map methods on a runtime value.
    // ----------------------------------------------------------------
    std::string recvRaw = receiver->getCExpr(e);
    std::string recv = e.newTemp("m_recv");
    e.emit("VyneValue " + recv + " = " + boxTypedArray(e, recvRaw) + ";");

    // Array methods
    if (methodName == "push") {
        for (const auto& argNode : arguments) {
            e.emit("vyne_array_push(" + recv + ", " +
                   boxTypedArray(e, argNode->getCExpr(e)) + ");");
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

    // If the receiver is a statically-known struct/interface type, its own
    // registered methods win over built-ins that share the name (length,
    // size, has, keys, values, push, pop, ...). Let the struct-method
    // dispatch at the bottom of this function handle it.
    bool receiverIsStruct = false;
    if (receiver->type() == NodeType::VARIABLE) {
        auto* var = static_cast<VariableNode*>(receiver.get());
        std::string recvName = var->getOriginalName();
        std::string prefix   = e.getActiveFunctionPrefix();
        std::string lookupKey = prefix.empty()
            ? ("v_" + recvName)
            : ("v_" + prefix + "_" + recvName);
        if (e.lookupLocalStructType(lookupKey) ||
            e.lookupGlobalStructType("v_" + recvName)) {
            receiverIsStruct = true;
        }
    }

    if (!receiverIsStruct && (methodName == "length" || methodName == "size")) {
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
        std::string idx = boxTypedArray(e, arguments[0]->getCExpr(e));
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
        std::string v = boxTypedArray(e, arguments[0]->getCExpr(e));
        std::string c = boxTypedArray(e, arguments[1]->getCExpr(e));
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
        std::string s = boxTypedArray(e, arguments[0]->getCExpr(e));
        std::string c = (arguments.size() >= 2) ? boxTypedArray(e, arguments[1]->getCExpr(e)) : "vyne_int(-1)";
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
        std::string target = boxTypedArray(e, arguments[0]->getCExpr(e));
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
        std::string o = boxTypedArray(e, arguments[0]->getCExpr(e));
        std::string n = boxTypedArray(e, arguments[1]->getCExpr(e));
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
        std::string arg = boxTypedArray(e, arguments[0]->getCExpr(e));
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
        std::string k = boxTypedArray(e, arguments[0]->getCExpr(e));
        std::string v = boxTypedArray(e, arguments[1]->getCExpr(e));
        e.emit("vyne_map_set(" + recv + ", " + k + ", " + v + ");");
        return v;
    }
    if (methodName == "delete") {
        std::string k = boxTypedArray(e, arguments[0]->getCExpr(e));
        std::string temp = e.newTemp("del");
        e.emit("VyneValue " + temp + " = vyne_delete_any(" + recv + ", " + k + ");");
        return temp;
    }
    if (methodName == "clear") {
        e.emit("vyne_clear_any(" + recv + ");");
        return recv;
    }

    // Struct method call (last resort — user-defined methods on struct
    // values that were not caught by the interface / group branches).
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
                   boxTypedArray(e, arguments[i]->getCExpr(e)) + ";");
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

std::string DeferNode::getCExpr(C_Emitter& e) const { return "vyne_null()"; }

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
        std::string val = boxTypedArray(e, rhs->getCExpr(e));
        e.emit("if (" + cVar + ".type == V_NULL) {");
        e.emit("    " + cVar + " = " + val + ";");
        e.emit("}");
        e.popMainContext();
    } else {
        if (!e.isLocalDeclared(cVar)) {
            e.registerDeclaration(cVar);
            std::string val = boxTypedArray(e, rhs->getCExpr(e));
            e.emit("VyneValue " + cVar + " = vyne_null();");
            e.emit("if (" + cVar + ".type == V_NULL) {");
            e.emit("    " + cVar + " = " + val + ";");
            e.emit("}");
        } else {
            std::string val = boxTypedArray(e, rhs->getCExpr(e));
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
    std::string val = boxTypedArray(e, rhs->getCExpr(e));
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
// IN OPERATOR
// ============================================================

std::string InNode::getCExpr(C_Emitter& e) const {
    std::string leftVal = boxTypedArray(e, left->getCExpr(e));
    std::string rightVal = boxTypedArray(e, right->getCExpr(e));
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
    std::string leftVal = boxTypedArray(e, left->getCExpr(e));
    std::string rightVal = boxTypedArray(e, right->getCExpr(e));
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
        e.emit(argArr + "[0] = " + boxTypedArray(e, leftVal) + ";");

        for (size_t i = 0; i < args.size(); ++i) {
            std::string v = boxTypedArray(e, args[i]->getCExpr(e));
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
        e.emit(argArr + "[1] = " + boxTypedArray(e, leftVal) + ";");
        for (size_t i = 0; i < args.size(); ++i) {
            e.emit(argArr + "[" + std::to_string(i + 2) + "] = " +
                   boxTypedArray(e, args[i]->getCExpr(e)) + ";");
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
    std::string expr = expression ? boxTypedArray(e, expression->getCExpr(e))
                                  : "vyne_null()";
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
// A1 — LEXICAL REGIONS
// ------------------------------------------------------------
// `region name { body }` becomes:
//
//     VyneValue __cp_N = vmem_runtime_checkpoint();
//     { body }                                  // C block: locals scoped
//     vmem_runtime_rewind(__cp_N);
//
// The C block is what keeps locals declared inside the region from
// leaking into the surrounding scope — matching the lifetime
// guarantee at the source level. Nested regions work because
// vmem.h's checkpoint stack is itself a stack; every rewind pops
// its own handle and everything above it.
// ============================================================

void RegionNode::compile(C_Emitter& e) const {
    // The user may not have written `module vmem;`. Region codegen
    // depends on vmem.h's runtime helpers, so pull it in here.
    std::string base = FileUtils::getExeDir();
    std::filesystem::path moduleBase =
        std::filesystem::path(base) / "vyne" / "runtime" / "modules";
    e.addInclude((moduleBase / "vmem.h").string());

    std::string cpHandle = e.newTemp("vmem_cp");

    e.emit("// --- region: " + regionName + " ---");
    e.emit("VyneValue " + cpHandle + " = vmem_runtime_checkpoint();");

    e.emitBlockOpen("{");
    for (const auto& stmt : body) {
        if (stmt) stmt->compile(e);
    }
    e.emitBlockClose();

    e.emit("vmem_runtime_rewind(" + cpHandle + ");");
}

std::string RegionNode::getCExpr(C_Emitter& e) const {
    compile(e);
    return "vyne_null()";
}

// ============================================================
// A3 — REGION COMMIT
// ------------------------------------------------------------
// Not yet implemented. A correct lowering requires hoisting the
// committed value above the checkpoint in the emitted C — which in
// turn requires the escape-analysis pass (A2) to prove the value
// has no aliases inside the region. Without that proof, any lowering
// we pick either doubles the peak (copy) or silently produces a
// dangling reference (transfer).
//
// We fail at compile time so the user sees the constraint instead of
// getting a use-after-free at runtime.
// ============================================================

void RegionCommitNode::compile(C_Emitter& e) const {
    throw std::runtime_error(
        "Compile Error: 'region.commit' is not yet implemented by the C backend "
        "(line " + std::to_string(lineNumber) + "). "
        "Declare the value outside the region and assign to it from inside, "
        "or keep it a primitive (Int64/Float64/Bool), which the region rewind "
        "does not invalidate.");
}

std::string RegionCommitNode::getCExpr(C_Emitter& e) const {
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
        std::string k = boxTypedArray(e, keyNode->getCExpr(e));
        std::string v = boxTypedArray(e, valNode->getCExpr(e));
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
            std::string v = boxTypedArray(e, exprNodes[exprIdx++]->getCExpr(e));
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
