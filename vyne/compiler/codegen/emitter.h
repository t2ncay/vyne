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

#ifndef VYNE_EMITTER_H
#define VYNE_EMITTER_H

#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ctype.h"
#include "native_maps.h"

class C_Emitter {
    // --- Output buffers --------------------------------------------------
    std::stringstream globalsStream;
    std::stringstream functionStream;
    std::stringstream mainStream;
    std::unordered_set<std::string> includeSet;
    std::unordered_set<std::string> systemIncludes;

    int indentLevel = 0;
    enum class EmitContext { GLOBAL, FUNCTION, MAIN };
    std::vector<EmitContext> contextStack;

    std::stringstream& currentStream() {
        if (contextStack.empty())
            return mainStream;
        switch (contextStack.back()) {
        case EmitContext::FUNCTION:
            return functionStream;
        case EmitContext::GLOBAL:
            return globalsStream;
        default:
            return mainStream;
        }
    }

    std::string getIndent() const {
        return std::string(indentLevel * 4, ' ');
    }

    // --- Name declaration tables ----------------------------------------
    std::unordered_set<std::string> declaredVars;
    std::unordered_set<std::string> references;
    std::unordered_set<std::string> globalVars;

    // --- Static type tables (M0/M1) -------------------------------------
    std::unordered_map<std::string, CType> globalTypes;
    std::unordered_map<std::string, CType> nativeTemps;

    // Runtime dimensions for a scratch local, keyed by the mangled C
    // name. Populated by ScratchNode::compile when any dim is non-literal;
    // consulted by scratchFlatIndex to compute strides symbolically.
    // Cleared alongside the other per-function tables.
    std::unordered_map<std::string, std::vector<std::string>>
        scratchRuntimeDims;

    struct LocalScope {
        std::unordered_set<std::string> names;
        std::unordered_map<std::string, CType> types;
        std::unordered_map<std::string, int> regionDepthAtDeclaration;
    };
    std::vector<LocalScope> localScopes = {LocalScope{}};

    // --- Interface / group registry -------------------------------------
    std::unordered_set<std::string> interfaceSet;
    std::unordered_set<std::string> groupSet;
    std::unordered_map<std::string, std::string> interfaceCtorNames;
    std::unordered_map<std::string, std::vector<std::string>>
        functionSignatures;
    std::unordered_map<std::string, std::vector<CType>> functionParamTypes;
    std::unordered_map<std::string, CType> functionReturnTypes;
    std::unordered_map<std::string, std::string> nativeVariants;
    CType nativeReturnType;

    // Out-parameter names for a native variant whose declared return
    // type is a Struct. Empty for every other return kind. Populated by
    // emitNativeFunctionBody; read by ReturnNode::compile to write the
    // boxed result's fields into the out-params at return time.
    std::vector<std::string> structReturnOutParams;

    // Source-level return type of the function currently being emitted.
    // Set by FunctionNode::compile / compileAs before the body is lowered;
    // read by ReturnNode::compile to resolve `return [];` when the
    // declared type is `Array<T>`. Cleared on function exit. Distinct
    // from nativeReturnType_, which is only set for the native variant
    // and carries the ABI-level C representation.
    CType currentFunctionReturnType_;

    std::unordered_map<std::string, std::vector<std::string>> interfaceDefaults;
    std::string groupPrefix;

    // --- M4-C1B: struct field typing ------------------------------------
    std::unordered_map<std::string, std::unordered_map<std::string, VType>>
        interfaceArrayFields;
    std::unordered_map<std::string, std::string> localStructTypes;
    std::unordered_map<std::string, std::string> globalStructTypes;
    std::string currentInterfaceType;

    // M4-C1B (extended): scalar interface fields (Int64/Float64) that can be
    // unboxed at member access. Same key shape as interfaceArrayFields.
    std::unordered_map<std::string, std::unordered_map<std::string, VType>>
        interfacePrimitiveFields;

    // C2: native C struct representation.
    //
    // An interface is C-eligible when every field is Int64/Float64/Bool.
    // (Array-of-primitive and nested-eligible-struct fields are follow-up
    // slices.) Eligible interfaces get a real C typedef emitted at global
    // scope and can flow through Array<T> containers and native-ABI calls
    // as a pointer, not a boxed VyneValue.
    //
    // Keyed by every spelling of the interface name (bare, dotted,
    // underscore-mangled) so call sites can probe with whichever form
    // they hold. Never cleared by push/popFunctionContext — like
    // interfaceStructLayout, it is process-global.
    struct NativeCStruct {
        std::string tag;                     // C typedef name, e.g. "vyne_Atom"
        std::vector<std::string> fieldNames; // declaration order
        std::vector<CType> fieldTypes;       // parallel to fieldNames
    };
    std::unordered_map<std::string, NativeCStruct> nativeCStructs;

    // Slice 3g: fixed-point C-eligibility set. An interface name lands
    // here once every field is a primitive scalar, a String, a Map, or
    // an Array<X> whose element X is itself in this set. Populated by
    // ProgramNode::compile before any statement is lowered.
    //
    // This is deliberately separate from `nativeCStructs`: the latter is
    // populated during InterfaceNode::compile and carries the tag and
    // field layout. Eligibility has to be known earlier, so the fixed
    // point can converge before any typedef is emitted.
    std::unordered_set<std::string> cEligibleInterfaces;

    // Ordered field layout for struct-typed native variants. Key is any
    // spelling under which the interface was registered (bare, dotted,
    // underscore-mangled). Value is the field list in declaration order.
    //
    // This is deliberately NOT cleared by push/popFunctionContext: it is
    // process-global, mirroring functionReturnTypes and functionParamTypes.
    // The callee's native signature is built from this map during
    // emitNativeFunctionBody; a later caller rebuilds the argument list
    // from the same map. If the map were per-function, the two sides
    // could disagree on ordering.
    std::unordered_map<std::string, std::vector<StructFieldDesc>>
        interfaceStructLayout;

    struct FieldCacheEntry {
        std::string temp;
        CType ct;
    };
    std::unordered_map<std::string, FieldCacheEntry> fieldCache;

    // --- M2: monomorphization cache -------------------------------------
    // key = "<mangledFnName>__<type-tuple>" ; value = emitted C name.
    std::unordered_map<std::string, std::string> instantiations;
    std::unordered_set<std::string> instantiationsInProgress;

    // --- Import / context tracking --------------------------------------
    std::unordered_set<std::string> importedFiles;
    std::string sourceDir;
    std::string activeFunctionPrefix;

    // --- Module scoping --------------------------------------------------
    // `activeModule` is set while emitting a function declared with a
    // `module M;` prefix. Call sites consult it to turn the bare name X
    // into the module-qualified C symbol fn_M_X when X is known to be
    // defined in M.
    //
    // `moduleFunctions[M]` and `moduleInterfaces[M]` are the registries
    // that answer "is X defined in M?" — populated as each function or
    // interface is compiled, and read at call sites.
    std::string activeModule;
    std::unordered_map<std::string, std::unordered_set<std::string>>
        moduleFunctions;
    std::unordered_map<std::string, std::unordered_set<std::string>>
        moduleInterfaces;

    // --- Defer / try-cleanup context ------------------------------------
    struct DeferContext {
        std::string cleanupLabel;
        std::string retVar;
        bool active = false;
    };
    DeferContext deferCtx;
    std::vector<std::string> tryCleanupStack;
    std::string currentReturnVar;
    std::string currentReturningVar;

    std::vector<std::string> regionStack;
    std::unordered_set<std::string> committedVars;

    // Active @pool context. The pool's own C variable name lives here
    // so `pool_alloc()` / `pool_free()` — intercepted in
    // FunctionCallNode::getCExpr — can emit `&<handle>` without
    // recomputing the mangling. Only the innermost pool is visible to
    // the body; nested @pool regions shadow cleanly via LIFO.
    struct PoolContext {
        std::string handle;   // C variable name of the VynePool struct
        VType elemType;       // Float64 or Int64
        int64_t elemsPerSlot; // buffer length per allocation
    };
    std::vector<PoolContext> poolStack;

    // Active @speculative context. Tracks the C variable name of the
    // per-region commit flag so `region.commit_if(pred)` — intercepted
    // in RegionCommitIfNode::compile — knows which flag to assign. Only
    // the innermost is visible; nested speculative regions shadow via
    // LIFO, matching every other region-scoped construct.
    struct SpeculativeContext {
        std::string commitFlag; // C variable name of the int flag
    };
    std::vector<SpeculativeContext> speculativeStack;

    bool scratchBoundsEnabled = true;
    bool blasEnabled = false;
    bool poolRuntimeEmitted = false;
    int tempVarCount = 0;

    bool inNativeVariant = false;

  public:
    void setInNativeVariant(bool v) {
        inNativeVariant = v;
    }
    bool isInNativeVariant() const {
        return inNativeVariant;
    }
    // --- Context management ---------------------------------------------
    bool isAlreadyImported(const std::string& path) const {
        return importedFiles.count(path) > 0;
    }
    void markImported(const std::string& path) {
        importedFiles.insert(path);
    }
    std::string getSourceDir() const {
        return sourceDir;
    }
    void setSourceDir(const std::string& dir) {
        sourceDir = dir;
    }

    const std::unordered_set<std::string>& getGlobalVars() const {
        return globalVars;
    }
    void enterFunction(const std::string& prefix) {
        activeFunctionPrefix = prefix;
    }
    void exitFunction() {
        activeFunctionPrefix.clear();
    }
    const std::string& getActiveFunctionPrefix() const {
        return activeFunctionPrefix;
    }

    // --- Module context ------------------------------------------------
    void setActiveModule(const std::string& m) {
        activeModule = m;
    }
    void clearActiveModule() {
        activeModule.clear();
    }
    const std::string& getActiveModule() const {
        return activeModule;
    }

    void
    registerModuleFunction(const std::string& mod, const std::string& name) {
        moduleFunctions[mod].insert(name);
    }
    bool
    isModuleFunction(const std::string& mod, const std::string& name) const {
        auto it = moduleFunctions.find(mod);
        return it != moduleFunctions.end() && it->second.count(name) > 0;
    }

    void
    registerModuleInterface(const std::string& mod, const std::string& name) {
        moduleInterfaces[mod].insert(name);
    }
    bool
    isModuleInterface(const std::string& mod, const std::string& name) const {
        auto it = moduleInterfaces.find(mod);
        return it != moduleInterfaces.end() && it->second.count(name) > 0;
    }

    void pushMainContext() {
        contextStack.emplace_back(EmitContext::MAIN);
        indentLevel = 1;
    }
    void popMainContext() {
        if (!contextStack.empty())
            contextStack.pop_back();
        indentLevel = 0;
    }

    void pushGlobalContext() {
        contextStack.emplace_back(EmitContext::GLOBAL);
        indentLevel = 0;
    }
    void popGlobalContext() {
        if (!contextStack.empty())
            contextStack.pop_back();
        indentLevel = 1;
    }

    bool isGlobalContext() {
        return contextStack.empty() ||
               contextStack.back() == EmitContext::GLOBAL;
    }

    bool isTopLevelOfMain() const {
        bool inMain =
            contextStack.empty() || contextStack.back() == EmitContext::MAIN;
        return inMain && localScopes.size() <= 1;
    }

    bool isGlobalDeclared(const std::string& name) const {
        return globalVars.count(name) > 0;
    }
    bool isLocalDeclared(const std::string& name) const {
        for (auto it = localScopes.rbegin(); it != localScopes.rend(); ++it)
            if (it->names.count(name))
                return true;
        return false;
    }
    bool isLocalVarsEmpty() const {
        return localScopes.empty() || localScopes.back().names.empty();
    }
    bool isAlreadyDeclared(const std::string& name) const {
        return isLocalDeclared(name) || isGlobalDeclared(name);
    }

    void registerDeclaration(const std::string& name) {
        bool atBaseScope = localScopes.size() <= 1;
        if (isGlobalContext() && atBaseScope) {
            globalVars.insert(name);
        } else {
            if (localScopes.empty())
                localScopes.emplace_back();
            localScopes.back().names.insert(name);
            // Record the region depth so the escape check can distinguish
            // a local declared inside a region from one declared outside
            // it. Without this, lookupLocalRegionDepth returns -1 for every
            // boxed local and the check treats it as a depth-0 escape.
            localScopes.back().regionDepthAtDeclaration[name] =
                (int)regionStack.size();
        }
    }

    // regionDepth defaults to the current region stack depth at the
    // moment of declaration. Callers that want to override (e.g. for
    // parameters, which are conceptually at depth 0) can pass an
    // explicit value.
    void declareLocal(
        const std::string& name,
        const CType& ct,
        int regionDepth = -1
    ) {
        if (localScopes.empty())
            localScopes.emplace_back();
        localScopes.back().names.insert(name);
        localScopes.back().types[name] = ct;
        if (regionDepth < 0)
            regionDepth = (int)regionStack.size();
        localScopes.back().regionDepthAtDeclaration[name] = regionDepth;
    }
    void declareGlobal(const std::string& name, const CType& ct) {
        globalVars.insert(name);
        globalTypes[name] = ct;
    }

    const CType* lookupLocalType(const std::string& name) const {
        for (auto it = localScopes.rbegin(); it != localScopes.rend(); ++it) {
            auto t = it->types.find(name);
            if (t != it->types.end())
                return &t->second;
        }
        return nullptr;
    }
    // Effective region depth of a variable: 0 if committed (survives
    // rewind), otherwise the depth at declaration. Returns -1 if the
    // name is not a tracked local.
    int lookupLocalRegionDepth(const std::string& name) const {
        if (committedVars.count(name))
            return 0;
        for (auto it = localScopes.rbegin(); it != localScopes.rend(); ++it) {
            auto r = it->regionDepthAtDeclaration.find(name);
            if (r != it->regionDepthAtDeclaration.end())
                return r->second;
        }
        return -1;
    }

    int currentRegionDepth() const {
        return (int)regionStack.size();
    }

    void markCommitted(const std::string& name) {
        committedVars.insert(name);
    }
    bool isCommitted(const std::string& name) const {
        return committedVars.count(name) > 0;
    }
    const CType* lookupGlobalType(const std::string& name) const {
        auto it = globalTypes.find(name);
        return it == globalTypes.end() ? nullptr : &it->second;
    }

    const CType* lookupType(const std::string& name) const {
        auto it = nativeTemps.find(name);
        if (it != nativeTemps.end())
            return &it->second;
        if (auto lt = lookupLocalType(name))
            return lt;
        return lookupGlobalType(name);
    }

    const CType* lookupAnyType(const std::string& name) const {
        return lookupType(name);
    }

    void declareNativeTemp(const std::string& name, const CType& ct) {
        nativeTemps[name] = ct;
    }

    void registerScratchRuntimeDims(
        const std::string& cName,
        std::vector<std::string> dims
    ) {
        scratchRuntimeDims[cName] = std::move(dims);
    }
    const std::vector<std::string>*
    lookupScratchRuntimeDims(const std::string& cName) const {
        auto it = scratchRuntimeDims.find(cName);
        return it == scratchRuntimeDims.end() ? nullptr : &it->second;
    }

    // Filtered view: non-null only for provably-primitive types.
    // (Behaviorally identical to the old version — it also returned any
    // nativeTemp entry, primitive or not, thanks to the early return.
    // This preserves that.)
    const CType* exprNativeType(const std::string& expr) const {
        const CType* ct = lookupType(expr);
        return (ct && ct->isPrimitive()) ? ct : nullptr;
    }
    std::string boxIfNative(const std::string& expr) const {
        const CType* ct = exprNativeType(expr);
        return ct ? ct->box(expr) : expr;
    }
    std::string boxAny(const std::string& expr) const {
        const CType* ct = lookupType(expr);
        if (ct && ct->kind == CType::Kind::Array && !ct->args.empty()) {
            const CType& el = ct->args[0];

            // Slice 3e: a struct-element array lives in a
            // `vyne_Array_vyne_*` container, not a boxed VyneValue.
            // Box it via the per-interface helper emitted alongside the
            // container typedef. The box is O(N) — one VyneValue per
            // element, built from the same flat fields the constructor
            // already takes — which is the correct cost for a struct
            // array crossing a dynamic boundary. The O(1) pointer-share
            // shortcut only exists for f64/i64 because their payload is
            // a single word; a struct is not.
            if (el.kind == CType::Kind::Struct && el.nativeCStruct) {
                const NativeCStruct* ns = getNativeCStruct(el.mangledName);
                if (ns) {
                    return "vyne_struct_array_box_" + ns->tag + "(" + expr +
                           ")";
                }
                // Registry entry missing despite nativeCStruct=true.
                // Fall through — boxIfNative will pass the struct
                // through unchanged, which produces a C type error at
                // the call site rather than a silent wrong answer.
            }

            VType elem = el.toVType();
            if (elem == VType::Float64)
                return "vyne_array_f64_to_value(&" + expr + ")";
            if (elem == VType::Int64)
                return "vyne_array_i64_to_value(&" + expr + ")";
        }
        // Slice 3a: a native C struct value re-boxes into a VyneValue
        // by calling the interface's boxed constructor with unboxed
        // field reads. The constructor takes VyneValue args, so each
        // field is wrapped with vyne_float / vyne_int / vyne_bool.
        // mangledName may carry dots (module path); the C ctor symbol
        // is underscore-mangled to match InterfaceNode::compile.
        if (ct && ct->kind == CType::Kind::Struct && ct->nativeCStruct &&
            !ct->mangledName.empty()) {
            const NativeCStruct* ns = getNativeCStruct(ct->mangledName);
            if (ns) {
                std::string ctor = "struct_" + ct->mangledName;
                std::replace(ctor.begin(), ctor.end(), '.', '_');
                std::string args;
                for (size_t i = 0; i < ns->fieldNames.size(); ++i) {
                    if (i)
                        args += ", ";
                    const std::string fld = expr + "." + ns->fieldNames[i];
                    CType ft = ns->fieldTypes[i];
                    if (ft.kind == CType::Kind::Float64) {
                        args += "vyne_float(" + fld + ")";
                    } else if (ft.kind == CType::Kind::Int64) {
                        args += "vyne_int(" + fld + ")";
                    } else if (ft.kind == CType::Kind::Bool) {
                        args += "vyne_bool(" + fld + ")";
                    } else if (
                        ft.kind == CType::Kind::Str ||
                        ft.kind == CType::Kind::Map
                    ) {
                        // Already a VyneValue in the native struct; pass
                        // it through unchanged.
                        args += fld;
                    } else if (
                        ft.kind == CType::Kind::Array && !ft.args.empty()
                    ) {
                        const CType& el = ft.args[0];
                        if (el.kind == CType::Kind::Struct &&
                            el.nativeCStruct && !el.nativeName.empty()) {
                            args += "vyne_struct_array_box_" + el.nativeName +
                                    "(" + fld + ")";
                        } else if (el.kind == CType::Kind::Float64) {
                            args += "vyne_array_f64_to_value(&" + fld + ")";
                        } else if (el.kind == CType::Kind::Int64) {
                            args += "vyne_array_i64_to_value(&" + fld + ")";
                        }
                    }
                }
                return ctor + "(" + args + ")";
            }
        }
        return boxIfNative(expr);
    }

    std::string nativeRead(const std::string& expr, VType kind) const {
        const CType* ct = exprNativeType(expr);
        CType want = CType::fromVType(kind);
        if (ct && ct->kind == want.kind)
            return expr;
        return want.unbox(boxIfNative(expr));
    }

    // --- References ------------------------------------------------------
    void registerReference(const std::string name) {
        references.insert(std::move(name));
    }
    bool isReference(const std::string name) const {
        return references.count(std::move(name)) > 0;
    }

    // --- Function-context stack (clears every per-fn table) -------------
    void pushFunctionContext() {
        contextStack.emplace_back(EmitContext::FUNCTION);
        indentLevel = 0;
        localStructTypes.clear();
        fieldCache.clear();
        currentInterfaceType.clear();
        regionStack.clear();
        poolStack.clear(); // paired with regionStack: both are per-function
        speculativeStack.clear();
        committedVars.clear();
        nativeReturnType = CType{};
        currentFunctionReturnType_ = CType{};
        structReturnOutParams.clear();
        localScopes.clear();
        localScopes.emplace_back();
        scratchRuntimeDims.clear();
    }
    void popFunctionContext() {
        if (!contextStack.empty())
            contextStack.pop_back();
        localScopes.clear();
        localScopes.emplace_back();
        scratchRuntimeDims.clear();
        localStructTypes.clear();
        fieldCache.clear();
        currentInterfaceType.clear();
        currentFunctionReturnType_ = CType{};
        indentLevel = 1;
    }
    void setFunctionContext(bool inside) {
        if (inside)
            pushFunctionContext();
        else
            popFunctionContext();
    }

    // --- Indentation / emission -----------------------------------------
    void indent() {
        indentLevel++;
    }
    void dedent() {
        if (indentLevel > 0)
            indentLevel--;
    }

    void emit(const std::string& code) {
        currentStream() << getIndent() << code << "\n";
    }
    void emitGlobalDecl(const std::string& code) {
        globalsStream << code << "\n";
    }

    void emitBlockOpen(const std::string& line) {
        localScopes.emplace_back();
        emit(line);
        indent();
    }
    void emitBlockClose(const std::string& suffix = "") {
        dedent();
        emit("}" + suffix);
        if (localScopes.size() > 1)
            localScopes.pop_back();
        fieldCache.clear();
    }

    std::string newTemp(const std::string& prefix = "t") {
        return prefix + "_" + std::to_string(tempVarCount++);
    }
    void addInclude(const std::string& header) {
        includeSet.insert(header);
    }
    void addSystemInclude(const std::string& header) {
        systemIncludes.insert(header);
    }

    void setBlasEnabled(bool v) {
        blasEnabled = v;
    }
    bool isBlasEnabled() const {
        return blasEnabled;
    }

    // --- M4-C1B: interface field typing ---------------------------------
    void registerInterfaceArrayField(
        const std::string& iface,
        const std::string& field,
        VType elem
    ) {
        if (elem == VType::Int64 || elem == VType::Float64)
            interfaceArrayFields[iface][field] = elem;
    }
    VType getInterfaceArrayElem(
        const std::string& iface,
        const std::string& field
    ) const {
        auto probe = [&](const std::string& k) -> VType {
            auto it = interfaceArrayFields.find(k);
            if (it == interfaceArrayFields.end())
                return VType::Unknown;
            auto f = it->second.find(field);
            return f == it->second.end() ? VType::Unknown : f->second;
        };
        VType v = probe(iface);
        if (v != VType::Unknown)
            return v;
        std::string tmp = iface;
        size_t dot;
        while ((dot = tmp.find('.')) != std::string::npos) {
            tmp = tmp.substr(dot + 1);
            v = probe(tmp);
            if (v != VType::Unknown)
                return v;
        }
        return VType::Unknown;
    }

    void registerInterfacePrimitiveField(
        const std::string& iface,
        const std::string& field,
        VType t
    ) {
        if (t == VType::Int64 || t == VType::Float64)
            interfacePrimitiveFields[iface][field] = t;
    }
    VType getInterfacePrimitiveField(
        const std::string& iface,
        const std::string& field
    ) const {
        auto probe = [&](const std::string& k) -> VType {
            auto it = interfacePrimitiveFields.find(k);
            if (it == interfacePrimitiveFields.end())
                return VType::Unknown;
            auto f = it->second.find(field);
            return f == it->second.end() ? VType::Unknown : f->second;
        };
        VType v = probe(iface);
        if (v != VType::Unknown)
            return v;
        std::string tmp = iface;
        size_t dot;
        while ((dot = tmp.find('.')) != std::string::npos) {
            tmp = tmp.substr(dot + 1);
            v = probe(tmp);
            if (v != VType::Unknown)
                return v;
        }
        return VType::Unknown;
    }

    // --- C2: native C struct representation ---------------------------
    // Register under the interface's fullName; the probe-with-suffix
    // accessors mirror getInterfaceArrayElem so a caller holding
    // "Types.Atom" still finds an interface registered as "Atom".
    void registerNativeCStruct(
        const std::string& ifaceName,
        std::string tag,
        std::vector<std::string> fieldNames,
        std::vector<CType> fieldTypes
    ) {
        NativeCStruct s;
        s.tag = std::move(tag);
        s.fieldNames = std::move(fieldNames);
        s.fieldTypes = std::move(fieldTypes);
        nativeCStructs[ifaceName] = std::move(s);
    }

    const NativeCStruct* getNativeCStruct(const std::string& ifaceName) const {
        auto probe = [&](const std::string& k) -> const NativeCStruct* {
            auto it = nativeCStructs.find(k);
            return it == nativeCStructs.end() ? nullptr : &it->second;
        };
        if (const auto* r = probe(ifaceName))
            return r;
        std::string tmp = ifaceName;
        size_t dot;
        while ((dot = tmp.find('.')) != std::string::npos) {
            tmp = tmp.substr(dot + 1);
            if (const auto* r = probe(tmp))
                return r;
        }
        return nullptr;
    }

    bool hasNativeCStruct(const std::string& ifaceName) const {
        return getNativeCStruct(ifaceName) != nullptr;
    }

    // --- Slice 3g: C-eligibility set ---------------------------------
    void markCEligible(const std::string& ifaceName) {
        cEligibleInterfaces.insert(ifaceName);
    }

    // Probe with the same dot-suffix walk every other accessor uses, so
    // a caller holding "vchem.Molecule" finds an interface registered
    // as either "vchem.Molecule" or "Molecule".
    bool isCEligible(const std::string& ifaceName) const {
        auto probe = [&](const std::string& k) {
            return cEligibleInterfaces.count(k) > 0;
        };
        if (probe(ifaceName))
            return true;
        std::string tmp = ifaceName;
        size_t dot;
        while ((dot = tmp.find('.')) != std::string::npos) {
            tmp = tmp.substr(dot + 1);
            if (probe(tmp))
                return true;
        }
        return false;
    }

    // --- Struct-typed native ABI (Stage 1) ----------------------------
    // Register the ordered field list for an interface under every
    // spelling the caller might query with. Mirrors registerInterfaceArrayField
    // and registerInterfacePrimitiveField's multi-spelling registration.
    void registerInterfaceStructLayout(
        const std::string& iface,
        std::vector<StructFieldDesc> fields
    ) {
        interfaceStructLayout[iface] = std::move(fields);
    }

    // Probe-with-dot-suffix, matching getInterfaceArrayElem. Returns
    // nullptr if nothing matches, so the caller can fall back to the
    // boxed path rather than throw.
    const std::vector<StructFieldDesc>*
    getInterfaceStructLayout(const std::string& iface) const {
        auto probe =
            [&](const std::string& k) -> const std::vector<StructFieldDesc>* {
            auto it = interfaceStructLayout.find(k);
            return it == interfaceStructLayout.end() ? nullptr : &it->second;
        };
        if (const auto* r = probe(iface))
            return r;
        std::string tmp = iface;
        size_t dot;
        while ((dot = tmp.find('.')) != std::string::npos) {
            tmp = tmp.substr(dot + 1);
            if (const auto* r = probe(tmp))
                return r;
        }
        return nullptr;
    }

    bool hasInterfaceStructLayout(const std::string& iface) const {
        return getInterfaceStructLayout(iface) != nullptr;
    }

    void setLocalStructType(const std::string& var, const std::string& t) {
        localStructTypes[var] = t;
    }

    // Resolve a Vyne variable's interface name from its *original* source
    // name (not the mangled C name). Returns "" if the variable is not a
    // tracked struct-typed local or global.
    //
    // Takes a `std::string` deliberately: emitter.h is included from
    // ast.h, so this header cannot reference ASTNode / NodeType / any
    // AST class. Callers that hold an AST node must first extract the
    // variable's original name and pass it in. See
    // `native_dispatch.cpp::nodeInterfaceName` for that unwrapping step.
    std::string
    lookupStructInterfaceName(const std::string& originalName) const {
        if (originalName.empty())
            return "";
        std::string prefix = getActiveFunctionPrefix();
        std::string lookupKey = prefix.empty()
                                    ? ("v_" + originalName)
                                    : ("v_" + prefix + "_" + originalName);
        if (const std::string* t = lookupLocalStructType(lookupKey))
            return *t;
        if (const std::string* t = lookupGlobalStructType("v_" + originalName))
            return *t;
        return "";
    }

    const std::string* lookupLocalStructType(const std::string& var) const {
        auto it = localStructTypes.find(var);
        return it == localStructTypes.end() ? nullptr : &it->second;
    }
    void setGlobalStructType(const std::string& var, const std::string& t) {
        globalStructTypes[var] = t;
    }
    const std::string* lookupGlobalStructType(const std::string& var) const {
        auto it = globalStructTypes.find(var);
        return it == globalStructTypes.end() ? nullptr : &it->second;
    }

    void setCurrentInterfaceType(const std::string& t) {
        currentInterfaceType = t;
    }
    const std::string& getCurrentInterfaceType() const {
        return currentInterfaceType;
    }

    const FieldCacheEntry* getFieldCache(const std::string& key) const {
        auto it = fieldCache.find(key);
        return it == fieldCache.end() ? nullptr : &it->second;
    }
    void setFieldCache(
        const std::string& key,
        const std::string& temp,
        const CType& ct
    ) {
        fieldCache[key] = {temp, ct};
    }
    void clearFieldCache() {
        fieldCache.clear();
    }

    // --- M2: monomorphization cache -------------------------------------
    // Cache-hit lookup. Callers compute the fully-qualified instantiation
    // key (e.g. "fn_max_of__i64"); a non-null return means the C symbol
    // has already been emitted and can be referenced directly.
    const std::string* lookupInstantiation(const std::string& key) const {
        auto it = instantiations.find(key);
        return it == instantiations.end() ? nullptr : &it->second;
    }
    // Mark a name as in-progress so recursive instantiations terminate.
    bool beginInstantiation(const std::string& key) {
        if (instantiations.count(key))
            return false;
        if (instantiationsInProgress.count(key))
            return false;
        instantiationsInProgress.insert(key);
        return true;
    }
    void finishInstantiation(const std::string& key, const std::string& cName) {
        instantiationsInProgress.erase(key);
        instantiations[key] = cName;
    }

    // --- Native module lookup -------------------------------------------
    const NativeMapEntry*
    findNative(const std::string& module, const std::string& member) const {
        if (module == "vcore")
            for (const auto& m : VCORE_MAP)
                if (member == m.vyneName)
                    return &m;
        if (module == "vmath")
            for (const auto& m : VMATH_MAP)
                if (member == m.vyneName)
                    return &m;
        if (module == "vmem")
            for (const auto& m : VMEM_MAP)
                if (member == m.vyneName)
                    return &m;
        if (module == "vfs")
            for (const auto& m : VFS_MAP)
                if (member == m.vyneName)
                    return &m;
        return nullptr;
    }
    std::string getNativeMapping(
        const std::string& module,
        const std::string& member,
        bool asFunctionCall
    ) {
        const NativeMapEntry* e = findNative(module, member);
        if (!e)
            return "v_" + module + "_" + member;
        if (e->isProperty)
            return e->cName;
        return asFunctionCall ? e->cName : std::string(e->cName) + "()";
    }

    // --- Interface / group registry -------------------------------------
    void registerInterface(const std::string& name) {
        interfaceSet.insert(name);
    }
    void
    registerInterfaceCtor(const std::string& name, const std::string& suffix) {
        interfaceCtorNames[name] = suffix;
    }
    const std::string* getInterfaceCtor(const std::string& name) const {
        auto it = interfaceCtorNames.find(name);
        return it == interfaceCtorNames.end() ? nullptr : &it->second;
    }
    void registerGroup(const std::string& name) {
        groupSet.insert(name);
    }
    bool isInterface(const std::string& name) const {
        return interfaceSet.count(name) > 0;
    }
    bool isGroup(const std::string& name) const {
        return groupSet.count(name) > 0;
    }
    void registerFunctionSignature(
        const std::string& name,
        std::vector<std::string> params
    ) {
        functionSignatures[name] = std::move(params);
    }
    void registerFunctionParamTypes(
        const std::string& name,
        std::vector<CType> types
    ) {
        functionParamTypes[name] = std::move(types);
    }
    void registerFunctionReturnType(const std::string& name, const CType& ct) {
        functionReturnTypes[name] = ct;
    }
    const CType* getFunctionReturnType(const std::string& name) const {
        auto it = functionReturnTypes.find(name);
        return it == functionReturnTypes.end() ? nullptr : &it->second;
    }

    // Read-only view of the whole table. Used by the region escape
    // check as a last-resort fuzzy lookup, and by the diagnostic
    // dump when the check fires. Do not mutate through this accessor.
    const std::unordered_map<std::string, CType>&
    getAllFunctionReturnTypes() const {
        return functionReturnTypes;
    }
    const std::vector<CType>*
    getFunctionParamTypes(const std::string& name) const {
        auto it = functionParamTypes.find(name);
        return it == functionParamTypes.end() ? nullptr : &it->second;
    }

    void registerNativeVariant(
        const std::string& boxedMangled,
        const std::string& nativeMangled
    ) {
        nativeVariants[boxedMangled] = nativeMangled;
    }
    const std::string*
    lookupNativeVariant(const std::string& boxedMangled) const {
        auto it = nativeVariants.find(boxedMangled);
        return it == nativeVariants.end() ? nullptr : &it->second;
    }

    void setNativeReturnType(const CType& ct) {
        nativeReturnType = ct;
    }
    void clearNativeReturnType() {
        nativeReturnType = CType{};
    }
    const CType& getNativeReturnType() const {
        return nativeReturnType;
    }

    void setCurrentFunctionReturnType(const CType& ct) {
        currentFunctionReturnType_ = ct;
    }
    void clearCurrentFunctionReturnType() {
        currentFunctionReturnType_ = CType{};
    }
    const CType& getCurrentFunctionReturnType() const {
        return currentFunctionReturnType_;
    }

    void setStructReturnOutParams(std::vector<std::string> names) {
        structReturnOutParams = std::move(names);
    }
    const std::vector<std::string>& getStructReturnOutParams() const {
        return structReturnOutParams;
    }
    const std::vector<std::string>*
    getFunctionSignature(const std::string& name) const {
        auto it = functionSignatures.find(name);
        return it == functionSignatures.end() ? nullptr : &it->second;
    }

    void setGroupPrefix(const std::string& p) {
        groupPrefix = p;
    }
    void clearGroupPrefix() {
        groupPrefix.clear();
    }
    const std::string& getGroupPrefix() const {
        return groupPrefix;
    }

    void setScratchBoundsEnabled(bool v) {
        scratchBoundsEnabled = v;
    }
    bool isScratchBoundsEnabled() const {
        return scratchBoundsEnabled;
    }

    void registerInterfaceDefaults(
        const std::string& name,
        std::vector<std::string> defaults
    ) {
        interfaceDefaults[name] = std::move(defaults);
    }
    const std::vector<std::string>*
    getInterfaceDefaults(const std::string& name) const {
        auto it = interfaceDefaults.find(name);
        return it == interfaceDefaults.end() ? nullptr : &it->second;
    }

    // --- Try / defer context --------------------------------------------
    void pushTryCleanup(const std::string& label) {
        tryCleanupStack.push_back(label);
    }
    void popTryCleanup() {
        if (!tryCleanupStack.empty())
            tryCleanupStack.pop_back();
    }
    bool hasTryCleanup() const {
        return !tryCleanupStack.empty();
    }
    const std::string& currentTryCleanup() const {
        return tryCleanupStack.back();
    }

    // --- Region emitters --------------------------------------------
    void pushRegion(const std::string& cpHandle) {
        regionStack.push_back(cpHandle);
    }
    void popRegion() {
        if (!regionStack.empty())
            regionStack.pop_back();
    }
    bool hasRegion() const {
        return !regionStack.empty();
    }
    const std::vector<std::string>& getRegionStack() const {
        return regionStack;
    }

    // Emit rewind calls for every live region, innermost first.
    // Used by BreakNode / ContinueNode / ReturnNode before the transfer.
    void emitRegionUnwind() {
        for (auto it = regionStack.rbegin(); it != regionStack.rend(); ++it)
            emit("vmem_runtime_rewind(" + *it + ");");
    }

    // --- Pool context ------------------------------------------------
    // The pool is purely a lookup layer over regionStack. A @pool region
    // checkpoints on entry and rewinds on exit exactly like a bump
    // region; the pool struct's arena chunk sits above the checkpoint
    // so the rewind reclaims it. Early returns/breaks/continues rewind
    // through the same emitRegionUnwind() path as bump regions.
    void pushPool(const PoolContext& p) {
        poolStack.push_back(p);
    }
    void popPool() {
        if (!poolStack.empty())
            poolStack.pop_back();
    }
    bool hasPool() const {
        return !poolStack.empty();
    }
    const PoolContext& currentPool() const {
        return poolStack.back();
    }

    // Idempotent include emit for vyne_pool_runtime.h — set on the
    // first @pool region, no-op thereafter.
    bool isPoolRuntimeEmitted() const {
        return poolRuntimeEmitted;
    }
    void markPoolRuntimeEmitted() {
        poolRuntimeEmitted = true;
    }

    // --- Speculative context -----------------------------------------
    // Pushed by compileSpeculativeRegion, read by RegionCommitIfNode.
    // A speculative region still goes on regionStack; the speculative
    // stack only carries the commit flag's C name.
    void pushSpeculative(const SpeculativeContext& s) {
        speculativeStack.push_back(s);
    }
    void popSpeculative() {
        if (!speculativeStack.empty())
            speculativeStack.pop_back();
    }
    bool hasSpeculative() const {
        return !speculativeStack.empty();
    }
    const SpeculativeContext& currentSpeculative() const {
        return speculativeStack.back();
    }

    void setReturnVars(const std::string& rv, const std::string& rf) {
        currentReturnVar = rv;
        currentReturningVar = rf;
    }
    void clearReturnVars() {
        currentReturnVar.clear();
        currentReturningVar.clear();
    }
    bool hasReturnVars() const {
        return !currentReturnVar.empty();
    }
    const std::string& getReturnVar() const {
        return currentReturnVar;
    }
    const std::string& getReturningVar() const {
        return currentReturningVar;
    }

    void pushDeferContext(const std::string& label, const std::string& retVar) {
        deferCtx = {label, retVar, true};
    }
    void popDeferContext() {
        deferCtx = {"", "", false};
    }
    bool hasDeferContext() const {
        return deferCtx.active;
    }
    const std::string& getDeferCleanupLabel() const {
        return deferCtx.cleanupLabel;
    }
    const std::string& getDeferRetVar() const {
        return deferCtx.retVar;
    }

    // --- Output assembly -------------------------------------------------
    std::string finalize(const std::string& runtimeHeader = "vyne_runtime.h") {
        std::stringstream out;
        out << "#include \"" << runtimeHeader << "\"\n";
        for (const auto& inc : includeSet)
            out << "#include \"" << inc << "\"\n";
        for (const auto& inc : systemIncludes)
            out << "#include <" << inc << ">\n";
        out << "\n";

        std::string globals = globalsStream.str();
        if (!globals.empty())
            out << "// --- Globals ---\n" << globals << "\n";

        std::string funcs = functionStream.str();
        if (!funcs.empty())
            out << "// --- Functions ---\n" << funcs << "\n";

        out << "int main(void) {\n";
        out << mainStream.str();
        out << "    arena_free_all();\n";
        out << "    return 0;\n";
        out << "}\n";
        return out.str();
    }

    std::string getFunctionCode() {
        return functionStream.str();
    }
    std::string getBodyCode() {
        return mainStream.str();
    }
    std::string getIncludes() {
        std::string res;
        for (const auto& inc : includeSet)
            res += "#include \"" + inc + "\"\n";
        return res;
    }

    // --- Full reset (between programs) ----------------------------------
    void reset() {
        globalsStream.str("");
        globalsStream.clear();
        functionStream.str("");
        functionStream.clear();
        mainStream.str("");
        mainStream.clear();
        includeSet.clear();
        systemIncludes.clear();
        interfaceDefaults.clear();
        contextStack.clear();
        groupPrefix.clear();
        interfaceSet.clear();
        groupSet.clear();
        declaredVars.clear();
        references.clear();
        importedFiles.clear();
        functionSignatures.clear();
        sourceDir.clear();
        activeFunctionPrefix.clear();
        activeModule.clear();
        moduleFunctions.clear();
        moduleInterfaces.clear();
        tryCleanupStack.clear();
        deferCtx = {};
        regionStack.clear();
        committedVars.clear();
        poolStack.clear();
        speculativeStack.clear();
        poolRuntimeEmitted = false;
        scratchBoundsEnabled = true;
        blasEnabled = false;
        currentReturnVar.clear();
        currentReturningVar.clear();
        localScopes.clear();
        localScopes.emplace_back();
        globalTypes.clear();
        nativeTemps.clear();
        scratchRuntimeDims.clear();

        functionParamTypes.clear();
        functionReturnTypes.clear();
        nativeVariants.clear();
        nativeReturnType = CType{};
        currentFunctionReturnType_ = CType{};
        structReturnOutParams.clear();

        interfaceArrayFields.clear();
        interfacePrimitiveFields.clear();
        nativeCStructs.clear();
        cEligibleInterfaces.clear();
        interfaceStructLayout.clear();
        localStructTypes.clear();
        globalStructTypes.clear();
        currentInterfaceType.clear();
        fieldCache.clear();

        instantiations.clear();
        instantiationsInProgress.clear();

        tempVarCount = 0;
        indentLevel = 1;
    }
};

#endif
