// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Tuncay Gafarli
//
// This file is part of the Vyne compiler.
//
// Vyne is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License as published by the
// Free Software Foundation, version 3.
//
// Vyne is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public
// License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with Vyne. If not, see <https://www.gnu.org/licenses/>.

#include "detail/codegen_helpers.h"

// ============================================================
// C2: C-eligibility predicate
// ------------------------------------------------------------
// An interface is C-eligible iff every field is a primitive scalar.
// Array-of-primitive and nested-eligible-struct fields are follow-up
// slices: they need container typedefs, unbox helpers, and an ABI
// extension, all of which want this first slice to be landable on its
// own. Keeping the rule strict here means no existing interface's
// emission changes — only new typedefs appear.
// ============================================================
static std::string arrayElemTypeName(const InterfaceMember& m) {
    const std::string& tp = m.typePath;
    size_t lt = tp.find('<');
    if (lt == std::string::npos)
        return "";
    if (tp.empty() || tp.back() != '>')
        return "";
    if (tp.substr(0, lt) != "Array")
        return "";

    std::string inner = tp.substr(lt + 1, tp.size() - lt - 2);
    size_t s = inner.find_first_not_of(" \t");
    size_t e = inner.find_last_not_of(" \t");
    if (s == std::string::npos)
        return "";
    return inner.substr(s, e - s + 1);
}

static bool memberIsCEligible(C_Emitter& e, const InterfaceMember& m) {
    // Primitives fit the flat representation directly.
    if (m.type == VType::Int64 || m.type == VType::Float64 ||
        m.type == VType::Bool)
        return true;

    // String and Map store as a single VyneValue inside the C typedef.
    // Not unboxed, but a valid struct field — enough to let a struct
    // containing them stay native at the outer level.
    if (m.type == VType::String || m.type == VType::Map)
        return true;

    // Array<X> is eligible iff X is a C-eligible struct. Array<Int64>
    // and Array<Float64> are deliberately not accepted here — that
    // would flip `vlin.Types.Matrix` to native, which is out of scope
    // for this slice.
    if (m.type == VType::Array) {
        std::string elemName = arrayElemTypeName(m);
        if (elemName.empty())
            return false;
        return e.isCEligible(elemName);
    }

    return false;
}

bool interfaceIsCEligible(C_Emitter& e, const InterfaceNode& iface) {
    const auto& members = iface.getMembers();
    if (members.empty())
        return false;
    for (const auto& m : members) {
        if (!memberIsCEligible(e, m))
            return false;
    }
    return true;
}

static CType cFieldTypeFor(C_Emitter& e, const InterfaceMember& m) {
    switch (m.type) {
    case VType::Int64:
        return CType::fromKind(CType::Kind::Int64);
    case VType::Float64:
        return CType::fromKind(CType::Kind::Float64);
    case VType::Bool:
        return CType::fromKind(CType::Kind::Bool);
    case VType::String:
        return CType::fromKind(CType::Kind::Str);
    case VType::Map:
        return CType::fromKind(CType::Kind::Map);
    case VType::Array: {
        std::string elemName = arrayElemTypeName(m);
        if (elemName.empty())
            return CType::fromKind(CType::Kind::Unknown);

        CType el;
        el.kind = CType::Kind::Struct;
        el.mangledName = elemName;
        el.nativeCStruct = true;
        // Tag is optional at construction time. If the element
        // interface hasn't been compiled yet, `nativeName` stays
        // empty and the caller emits a source-order diagnostic.
        if (const auto* ns = e.getNativeCStruct(elemName)) {
            el.nativeName = ns->tag;
        }

        CType ct;
        ct.kind = CType::Kind::Array;
        ct.args.push_back(el);
        return ct;
    }
    default:
        return CType::fromKind(CType::Kind::Unknown);
    }
}

// Interface declarations, field typing, and method registration.
// Keep expressions that emit statements in evaluation order; see README.md.
// ============================================================
// INTERFACE / STRUCT
// ============================================================

void InterfaceNode::compile(C_Emitter& e) const {
    // Determine the effective module: explicit moduleName wins, else use
    // the currently active group prefix (set by GroupNode::compile).
    std::string effectiveModule = moduleName;
    if (effectiveModule.empty())
        effectiveModule = e.getGroupPrefix();

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
    // Register under every spelling the parser accepted.
    // parseInterfaceDefinition inserts into `declaredTypes`:
    //   <interfaceName>                      ("Graph")
    //   <namespacePart>.<interfaceName>      ("Types.Graph")
    //   <currentModuleName>.<interfaceName>  ("vchem.Graph")
    //   <module>.<group>.<interfaceName>     ("vchem.Types.Graph")
    // Any one of those is a legal reference site, so codegen has to
    // recognise all of them too. The actual emitted C symbol is
    // `struct_<cStructName>` regardless of which spelling the caller
    // used — record that once and look it up at each call site.
    e.registerInterface(interfaceName);
    e.registerInterfaceCtor(interfaceName, cStructName);

    if (!effectiveModule.empty()) {
        // Full path and underscore-mangled form of the same.
        e.registerInterface(effectiveModule + "." + interfaceName);
        e.registerInterfaceCtor(
            effectiveModule + "." + interfaceName,
            cStructName
        );
        e.registerInterface(effectiveModule + "_" + interfaceName);
        e.registerInterfaceCtor(
            effectiveModule + "_" + interfaceName,
            cStructName
        );
        e.registerModuleInterface(effectiveModule, interfaceName);

        // Group-relative form: "vchem.Types" -> "Types.Graph".
        std::string suffix = effectiveModule;
        size_t dot;
        while ((dot = suffix.find('.')) != std::string::npos) {
            suffix = suffix.substr(dot + 1);
            std::string grpRel = suffix + "." + interfaceName;
            e.registerInterface(grpRel);
            e.registerInterfaceCtor(grpRel, cStructName);
        }

        // Module-relative form: "vchem.Types" -> "vchem.Graph".
        if ((dot = effectiveModule.find('.')) != std::string::npos) {
            std::string firstSeg = effectiveModule.substr(0, dot);
            std::string modRel = firstSeg + "." + interfaceName;
            e.registerInterface(modRel);
            e.registerInterfaceCtor(modRel, cStructName);
        }
    }
    // --- C2: native C struct emission ---------------------------------
    // Eligibility is a property of the field list only. Methods do NOT
    // affect it: an interface with methods can still have a C struct
    // representation for its fields, because method dispatch always
    // goes through the boxed VyneStruct (see vyne_struct_call).
    if (e.isCEligible(fullName) || e.isCEligible(interfaceName)) {
        std::string tag = "vyne_" + cStructName;

        std::vector<std::string> fieldNames;
        std::vector<CType> fieldTypes;
        fieldNames.reserve(members.size());
        fieldTypes.reserve(members.size());
        for (const auto& m : members) {
            fieldNames.push_back(m.name);
            CType ft = cFieldTypeFor(e, m);

            if (ft.kind == CType::Kind::Unknown) {
                throw std::runtime_error(
                    "internal: interface '" + fullName +
                    "' passed "
                    "C-eligibility but field '" +
                    m.name +
                    "' has no "
                    "C representation."
                );
            }

            // Array<Struct> fields reference the element interface's
            // container typedef, which must already exist. Enforce
            // source order with a readable diagnostic rather than
            // letting the C compiler complain about an unknown type.
            if (ft.kind == CType::Kind::Array && !ft.args.empty() &&
                ft.args[0].kind == CType::Kind::Struct &&
                ft.args[0].nativeName.empty()) {
                const std::string& elemName = ft.args[0].mangledName;
                throw std::runtime_error(
                    "Compile Error: interface '" + fullName + "' has field '" +
                    m.name + "' of type '" + m.typePath + "', but interface '" +
                    elemName + "' is defined after '" + fullName +
                    "'.\n"
                    "  Define '" +
                    elemName + "' above '" + fullName + "'."
                );
            }

            fieldTypes.push_back(ft);
        }

        // Register under every spelling a caller might probe with.
        // fullName is the canonical key; interfaceName covers bare-name
        // lookups from a native variant signature parsed without a
        // module prefix; the mangled form covers member_access paths
        // that already dot-normalised.
        e.registerNativeCStruct(fullName, tag, fieldNames, fieldTypes);
        if (fullName != interfaceName) {
            e.registerNativeCStruct(interfaceName, tag, fieldNames, fieldTypes);
        }

        // Emit the typedef into the globals stream. emitGlobalDecl
        // writes to globalsStream regardless of current emit context,
        // so we do not need to push/pop a GLOBAL context here — the
        // typedef lands at the top of the generated file, before any
        // function body that might reference it.
        std::string typedefSrc = "typedef struct {\n";
        for (size_t i = 0; i < fieldNames.size(); ++i) {
            const CType& ft = fieldTypes[i];
            typedefSrc += "    " + ft.cTypeName() + " " + fieldNames[i] + ";\n";
        }
        typedefSrc += "} " + tag + ";";
        e.emitGlobalDecl(typedefSrc);

        e.emitGlobalDecl(
            "VYNE_DEFINE_STRUCT_ARRAY(vyne_Array_" + tag + ", " + tag + ");"
        );

        // Slice 3e-box: emit a whole-array box helper. `boxAny` on an
        // Array<X> where X is a C-eligible struct calls this. It cannot
        // be an O(1) pointer-wrap like the f64/i64 case, because each
        // element is a full struct — the array-of-VyneValue must be
        // materialised element by element. That is the correct cost at
        // a boundary; what was wrong was refusing to pay it.
        //
        // The constructor `struct_<cStructName>` is defined later in
        // the function stream, so it needs a prototype here. The helper
        // itself is `static inline` and lives in the globals stream,
        // where the typedef and container are already visible.
        {
            std::string helperName = "vyne_struct_array_box_" + tag;
            std::string ctorName = "struct_" + cStructName;
            std::string container = "vyne_Array_" + tag;

            // Prototype for the boxed constructor. Cheap, idempotent,
            // and lets the helper be emitted before the definition.
            std::string ctorParams;
            for (size_t i = 0; i < fieldNames.size(); ++i) {
                if (i)
                    ctorParams += ", ";
                ctorParams += "VyneValue v_" + fieldNames[i];
            }
            e.emitGlobalDecl("VyneValue " + ctorName + "(" + ctorParams + ");");

            // Build the per-element box call argument list from the
            // same field descriptor list the typedef used above.
            std::string ctorArgs;
            for (size_t i = 0; i < fieldNames.size(); ++i) {
                if (i)
                    ctorArgs += ", ";
                const CType& ft = fieldTypes[i];
                std::string fld = "arr.data[i]." + fieldNames[i];
                if (ft.kind == CType::Kind::Float64) {
                    ctorArgs += "vyne_float(" + fld + ")";
                } else if (ft.kind == CType::Kind::Int64) {
                    ctorArgs += "vyne_int(" + fld + ")";
                } else if (ft.kind == CType::Kind::Bool) {
                    ctorArgs += "vyne_bool(" + fld + ")";
                } else if (
                    ft.kind == CType::Kind::Str || ft.kind == CType::Kind::Map
                ) {
                    ctorArgs += fld;
                } else if (ft.kind == CType::Kind::Array && !ft.args.empty()) {
                    const CType& el = ft.args[0];
                    if (el.kind == CType::Kind::Struct && el.nativeCStruct &&
                        !el.nativeName.empty()) {
                        ctorArgs += "vyne_struct_array_box_" + el.nativeName +
                                    "(" + fld + ")";
                    } else if (el.kind == CType::Kind::Float64) {
                        ctorArgs += "vyne_array_f64_to_value(&" + fld + ")";
                    } else if (el.kind == CType::Kind::Int64) {
                        ctorArgs += "vyne_array_i64_to_value(&" + fld + ")";
                    }
                }
            }

            std::string body =
                "static inline VyneValue " + helperName + "(" + container +
                " arr) {\n"
                "    VyneValue out = vyne_array_create((int)arr.size);\n"
                "    for (int64_t i = 0; i < arr.size; ++i) {\n"
                "        vyne_array_set(out, vyne_int(i), " +
                ctorName + "(" + ctorArgs +
                "));\n"
                "    }\n"
                "    return out;\n"
                "}";
            e.emitGlobalDecl(body);
        }
    }
    // --- end C2 --------------------------------------------------------

    for (const auto& m : members) {
        e.registerInterfaceArrayField(fullName, m.name, m.arrayElemType);
        e.registerInterfaceArrayField(interfaceName, m.name, m.arrayElemType);
    }

    for (const auto& m : members) {
        e.registerInterfaceArrayField(fullName, m.name, m.arrayElemType);
        e.registerInterfaceArrayField(interfaceName, m.name, m.arrayElemType);
        e.registerInterfacePrimitiveField(fullName, m.name, m.type);
        e.registerInterfacePrimitiveField(interfaceName, m.name, m.type);
    }

    // Build the ordered field descriptor list once, from the same `members`
    // vector that drives the constructor emission below. This is the single
    // source of truth for struct-typed native-variant parameter expansion:
    // the caller and the callee both read it back via getInterfaceStructLayout.
    //
    // Field types are constrained to what the flat ABI can carry:
    //   - Int64, Float64                       → scalar param
    //   - Array with element Int64 or Float64  → pointer-to-container param
    // Anything else (String, Map, nested Struct, Unknown) is left out of
    // the descriptor. A struct that contains such a field simply won't be
    // eligible for native dispatch — see the gate in ProgramNode::compile.
    registerInterfaceLayoutsFromMembers(e, interfaceName, moduleName, members);

    // Per-field defaults (used to pad short constructor calls).
    {
        std::vector<std::string> defaults;
        defaults.reserve(members.size());
        for (const auto& m : members) {
            switch (m.type) {
            case VType::String:
                defaults.push_back("vyne_string(\"\")");
                break;
            case VType::Int64:
                defaults.push_back("vyne_int(0)");
                break;
            case VType::Float64:
                defaults.push_back("vyne_float(0.0)");
                break;
            case VType::Array:
                defaults.push_back("vyne_array_create(0)");
                break;
            case VType::Map:
                defaults.push_back("vyne_map_create()");
                break;
            default:
                defaults.push_back("vyne_null()");
                break;
            }
        }
        e.registerInterfaceDefaults(interfaceName, defaults);
        if (!effectiveModule.empty()) {
            e.registerInterfaceDefaults(
                effectiveModule + "." + interfaceName,
                defaults
            );
            e.registerInterfaceDefaults(
                effectiveModule + "_" + interfaceName,
                defaults
            );
        }
    }

    e.pushFunctionContext();

    e.emit("// interface: " + fullName);
    std::string params;
    for (size_t i = 0; i < members.size(); ++i) {
        if (i > 0)
            params += ", ";
        params += "VyneValue v_" + members[i].name;
    }

    e.emitBlockOpen("VyneValue struct_" + cStructName + "(" + params + ") {");

    std::string temp = e.newTemp("s");
    e.emit(
        "VyneStruct* " + temp +
        " = (VyneStruct*)arena_alloc(sizeof(VyneStruct));"
    );
    e.emit(temp + "->type_name = \"" + fullName + "\";");
    e.emit(temp + "->field_count = " + std::to_string(members.size()) + ";");
    e.emit(
        temp + "->fields = (VyneField*)arena_alloc(sizeof(VyneField) * " +
        std::to_string(members.size()) + ");"
    );
    e.emit(temp + "->methods = NULL;");
    e.emit(temp + "->method_count = 0;");
    e.emit(temp + "->field_cache[0] = -1;");
    e.emit(temp + "->field_cache[1] = -1;");
    e.emit(temp + "->field_cache[2] = -1;");
    e.emit(temp + "->field_cache[3] = -1;");

    for (size_t i = 0; i < members.size(); ++i) {
        uint32_t fid = StringPool::intern(members[i].name);
        e.emit(
            temp + "->fields[" + std::to_string(i) +
            "].id = " + std::to_string(fid) + ";"
        );
        e.emit(
            temp + "->fields[" + std::to_string(i) + "].name = \"" +
            members[i].name + "\";"
        );
        e.emit(
            temp + "->fields[" + std::to_string(i) + "].value = v_" +
            members[i].name + ";"
        );
    }

    e.emit("VyneValue res; res.type = V_STRUCT; res.as.strct = " + temp + ";");
    e.emit("return res;");
    e.emitBlockClose();
    e.emit("");

    for (const auto& method : methods) {
        if (!method)
            continue;
        auto* fn = static_cast<FunctionNode*>(method.get());
        std::string methodName = cStructName + "_" + fn->getOriginalName();
        std::replace(methodName.begin(), methodName.end(), '.', '_');

        // Register this interface method's return type under every
        // spelling the region escape check (assignments.cpp:
        // checkRegionEscape) might query with. Interface methods are
        // emitted inline by this function and never routed through
        // FunctionNode::compile, so without this block the emitter's
        // functionReturnTypes table has no entry for them — every
        // `x = iface.method(...)` inside a region then looked like an
        // untyped, non-primitive RHS and tripped VNE-070. The
        // `vlin.cross_entropy(...)` case in ml_seq.vy is exactly this:
        // cross_entropy is declared with a `-> Float64` return type,
        // but nothing ever wrote that type into the emitter's table.
        {
            CType retCt = CType::fromVType(fn->getReturnType());
            if (fn->getReturnType() == VType::Array &&
                fn->getReturnArrayElemType() != VType::Unknown) {
                retCt.args.push_back(
                    CType::fromVType(fn->getReturnArrayElemType())
                );
            }

            std::string orig = fn->getOriginalName();
            std::string mangled = orig;
            std::replace(mangled.begin(), mangled.end(), '.', '_');

            // Bare name and dotted/mangled forms.
            e.registerFunctionReturnType(orig, retCt);
            e.registerFunctionReturnType(mangled, retCt);
            e.registerFunctionReturnType(methodName, retCt);

            // Receiver-qualified forms. `vlin.cross_entropy(...)` has
            // recvPath = "vlin"; checkRegionEscape will query
            // "vlin_cross_entropy" and "vlin.cross_entropy".
            if (!effectiveModule.empty()) {
                e.registerFunctionReturnType(
                    effectiveModule + "_" + mangled,
                    retCt
                );
                e.registerFunctionReturnType(
                    effectiveModule + "." + orig,
                    retCt
                );
            }
            // Also under the interface's own name, so
            // `Matrix.multiply(...)` style calls resolve too.
            if (interfaceName != effectiveModule) {
                e.registerFunctionReturnType(
                    interfaceName + "_" + mangled,
                    retCt
                );
                e.registerFunctionReturnType(interfaceName + "." + orig, retCt);
            }
        }

        e.emitGlobalDecl(
            "VyneValue fn_" + methodName + "(int arg_count, VyneValue* args);"
        );
        e.pushFunctionContext();
        e.enterFunction(methodName);
        e.setCurrentInterfaceType(fullName);
        e.emitBlockOpen(
            "VyneValue fn_" + methodName + "(int arg_count, VyneValue* args) {"
        );

        e.emit("VyneValue v_self = (arg_count > 0) ? args[0] : vyne_null();");

        // Parameter binding. Mirrors emitFunctionBody's primitive
        // unboxing so interface methods take the same native fast
        // paths as free functions. `self` is always boxed; it is a
        // VyneStruct at every call site and has no native
        // representation. Parameters with a declared primitive type
        // (Int64 / Float64 / Bool) unbox into native locals at entry;
        // everything else stays boxed, with the struct interface path
        // registered for member-access fast paths.
        //
        // Without this, every `Matrix.add_scalar(s)` and `Matrix.get(r, c)`
        // lowered to three vyne_binop calls per access — the machine-
        // generated fld_N / fp_N chains in the old emitted C. See the
        // dispatch chain note in method_calls.cpp: the receiver bind
        // is inside tryEmitInterfaceConstructor, but argument
        // evaluation is inside the boxed-arg loop of the ordinary
        // MethodCallNode path.
        const auto& params2 = fn->getParameters();
        for (size_t i = 0; i < params2.size(); ++i) {
            std::string paramSanitized = params2[i].name;
            std::replace(
                paramSanitized.begin(),
                paramSanitized.end(),
                '.',
                '_'
            );
            std::string paramName = "v_" + paramSanitized;
            std::string arg = "args[" + std::to_string(i + 1) + "]";
            std::string guard = "(arg_count > " + std::to_string(i + 1) + ")";

            CType pt = CType::fromVType(params2[i].type);
            if (pt.isPrimitive() && !params2[i].isReference) {
                std::string init = "0";
                switch (pt.kind) {
                case CType::Kind::Int64:
                    init = "(" + guard + ") ? ((" + arg +
                           ".type == V_INT64) ? " + arg +
                           ".as.i64 : (int64_t)" + arg + ".as.f64) : 0";
                    break;
                case CType::Kind::Float64:
                    init = "(" + guard + ") ? ((" + arg +
                           ".type == V_FLOAT64) ? " + arg +
                           ".as.f64 : (double)" + arg + ".as.i64) : 0.0";
                    break;
                case CType::Kind::Bool:
                    init =
                        "(" + guard + ") ? (" + arg + ".as.i64 != 0) : false";
                    break;
                default:
                    break;
                }
                e.declareLocal(paramName, pt);
                e.emit(pt.cTypeName() + " " + paramName + " = " + init + ";");
            } else {
                e.emit(
                    "VyneValue " + paramName + " = " + guard + " ? " + arg +
                    " : vyne_null();"
                );

                // Register the struct-typed parameter's interface so
                // subsequent `p.field` member reads hit the
                // registerInterfaceArrayField / PrimitiveField fast
                // paths in members.cpp instead of the boxed fallback.
                if (params2[i].type == VType::Struct &&
                    !params2[i].typePath.empty()) {
                    e.setLocalStructType(paramName, params2[i].typePath);
                }
            }
        }

        for (const auto& stmt : fn->getBody())
            if (stmt)
                stmt->compile(e);

        e.emit("return vyne_null();");
        e.emitBlockClose();
        e.emit("");
        e.exitFunction();
        e.popFunctionContext();

        e.pushMainContext();
        e.emit(
            "vyne_register_method(\"" + fullName + "\", \"" +
            fn->getOriginalName() + "\", fn_" + methodName + ");"
        );
        e.popMainContext();
    }

    e.popFunctionContext();
}

void registerInterfaceLayoutsFromMembers(
    C_Emitter& e,
    const std::string& interfaceName,
    const std::string& moduleName,
    const std::vector<InterfaceMember>& members
) {
    std::string effectiveModule = moduleName;
    if (effectiveModule.empty())
        effectiveModule = e.getGroupPrefix();

    std::string fullName = effectiveModule.empty()
                               ? interfaceName
                               : (effectiveModule + "." + interfaceName);

    std::vector<StructFieldDesc> layout;
    layout.reserve(members.size());
    for (const auto& m : members) {
        StructFieldDesc fd;
        fd.id = StringPool::intern(m.name);
        fd.name = m.name;

        if (m.type == VType::Int64) {
            fd.type = CType::fromKind(CType::Kind::Int64);
        } else if (m.type == VType::Float64) {
            fd.type = CType::fromKind(CType::Kind::Float64);
        } else if (
            m.type == VType::Array && (m.arrayElemType == VType::Int64 ||
                                       m.arrayElemType == VType::Float64)
        ) {
            fd.type.kind = CType::Kind::Array;
            fd.type.args.push_back(CType::fromVType(m.arrayElemType));
        } else {
            // Unsupported field type (String, Map, nested Struct, Unknown).
            // Do not register a partial layout: a partial layout would
            // let the native dispatcher emit a call whose signature does
            // not match the callee. Leaving the map entry absent forces
            // both sides onto the boxed ABI, which is always available.
            return;
        }
        layout.push_back(std::move(fd));
    }

    e.registerInterfaceStructLayout(interfaceName, layout);
    if (!effectiveModule.empty()) {
        e.registerInterfaceStructLayout(
            effectiveModule + "." + interfaceName,
            layout
        );
        e.registerInterfaceStructLayout(
            effectiveModule + "_" + interfaceName,
            layout
        );
    }
    if (fullName != interfaceName &&
        fullName != effectiveModule + "." + interfaceName) {
        e.registerInterfaceStructLayout(fullName, layout);
    }
}

std::string InterfaceNode::getCExpr(C_Emitter& e) const {
    return "vyne_null()";
}
