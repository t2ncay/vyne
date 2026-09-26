#ifndef VYNE_EMITTER_H
#define VYNE_EMITTER_H

#include <sstream>
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>

#include "ctype.h"
#include "native_maps.h"

class C_Emitter {
    // --- Output buffers --------------------------------------------------
    std::stringstream globalsStream;
    std::stringstream functionStream;
    std::stringstream mainStream;
    std::unordered_set<std::string> includeSet;
    int indentLevel = 0;
    enum class EmitContext { GLOBAL, FUNCTION, MAIN };
    std::vector<EmitContext> contextStack;

    std::stringstream& currentStream() {
        if (contextStack.empty()) return mainStream;
        switch (contextStack.back()) {
            case EmitContext::FUNCTION: return functionStream;
            case EmitContext::GLOBAL:   return globalsStream;
            default:                    return mainStream;
        }
    }

    std::string getIndent() const {
        return std::string(indentLevel * 4, ' ');
    }

    // --- Name declaration tables ----------------------------------------
    std::unordered_set<std::string> declaredVars;
    std::unordered_set<std::string> references;
    std::unordered_set<std::string> localVars;
    std::unordered_set<std::string> globalVars;
    std::vector<std::unordered_set<std::string>> localVarsStack;

    // --- Static type tables (M0/M1) -------------------------------------
    std::unordered_map<std::string, CType> localTypes;
    std::unordered_map<std::string, CType> globalTypes;
    std::vector<std::unordered_map<std::string, CType>> localTypesStack;
    std::unordered_map<std::string, CType> nativeTemps;

    // --- Interface / group registry -------------------------------------
    std::unordered_set<std::string> interfaceSet;
    std::unordered_set<std::string> groupSet;
    std::unordered_map<std::string, std::vector<std::string>> functionSignatures;
    std::unordered_map<std::string, std::vector<std::string>> interfaceDefaults;
    std::string groupPrefix;

    // --- M4-C1B: struct field typing ------------------------------------
    std::unordered_map<std::string,
                       std::unordered_map<std::string, VType>> interfaceArrayFields;
    std::unordered_map<std::string, std::string> localStructTypes;
    std::unordered_map<std::string, std::string> globalStructTypes;
    std::string currentInterfaceType;

    struct FieldCacheEntry { std::string temp; CType ct; };
    std::unordered_map<std::string, FieldCacheEntry> fieldCache;

    // --- M2: monomorphization cache -------------------------------------
    // key = "<mangledFnName>__<type-tuple>" ; value = emitted C name.
    std::unordered_map<std::string, std::string> instantiations;
    std::unordered_set<std::string> instantiationsInProgress;

    // --- Import / context tracking --------------------------------------
    std::unordered_set<std::string> importedFiles;
    std::string sourceDir;
    std::string activeFunctionPrefix;

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

    int tempVarCount = 0;

public:
    // --- Context management ---------------------------------------------
    bool isAlreadyImported(const std::string& path) const {
        return importedFiles.count(path) > 0;
    }
    void markImported(const std::string& path) { importedFiles.insert(path); }
    std::string getSourceDir() const { return sourceDir; }
    void setSourceDir(const std::string& dir) { sourceDir = dir; }

    const std::unordered_set<std::string>& getGlobalVars() const { return globalVars; }
    void enterFunction(const std::string& prefix) { activeFunctionPrefix = prefix; }
    void exitFunction() { activeFunctionPrefix.clear(); }
    const std::string& getActiveFunctionPrefix() const { return activeFunctionPrefix; }

    void pushMainContext() {
        contextStack.emplace_back(EmitContext::MAIN);
        indentLevel = 1;
    }
    void popMainContext() {
        if (!contextStack.empty()) contextStack.pop_back();
        indentLevel = 0;
    }

    void pushGlobalContext() {
        contextStack.emplace_back(EmitContext::GLOBAL);
        indentLevel = 0;
    }
    void popGlobalContext() {
        if (!contextStack.empty()) contextStack.pop_back();
        indentLevel = 1;
    }

    bool isGlobalContext() {
        return contextStack.empty() || contextStack.back() == EmitContext::GLOBAL;
    }

    bool isTopLevelOfMain() const {
        bool inMain = contextStack.empty()
                || contextStack.back() == EmitContext::MAIN;
        return inMain && localVarsStack.empty();
    }

    bool isGlobalDeclared(const std::string& name) const {
        return globalVars.count(name) > 0;
    }
    bool isLocalVarsEmpty() { return localVars.empty(); }
    bool isLocalDeclared(const std::string& name) const {
        return localVars.count(name) > 0;
    }
    bool isAlreadyDeclared(const std::string name) const {
        return localVars.count(std::move(name)) > 0
            || globalVars.count(std::move(name)) > 0;
    }

    void registerDeclaration(std::string name) {
        if (isGlobalContext()) globalVars.insert(std::move(name));
        else                   localVars.insert(std::move(name));
    }

    // --- M0/M1: static type tables --------------------------------------
    void declareLocal(const std::string& name, const CType& ct) {
        localVars.insert(name);
        localTypes[name] = ct;
    }
    void declareGlobal(const std::string& name, const CType& ct) {
        globalVars.insert(name);
        globalTypes[name] = ct;
    }
    const CType* lookupLocalType(const std::string& name) const {
        auto it = localTypes.find(name);
        return it == localTypes.end() ? nullptr : &it->second;
    }
    const CType* lookupGlobalType(const std::string& name) const {
        auto it = globalTypes.find(name);
        return it == globalTypes.end() ? nullptr : &it->second;
    }
    const CType* lookupAnyType(const std::string& name) const {
        auto lt = localTypes.find(name);
        if (lt != localTypes.end()) return &lt->second;
        auto gt = globalTypes.find(name);
        if (gt != globalTypes.end()) return &gt->second;
        return nullptr;
    }

    void declareNativeTemp(const std::string& name, const CType& ct) {
        nativeTemps[name] = ct;
    }
    const CType* exprNativeType(const std::string& expr) const {
        auto it = nativeTemps.find(expr);
        if (it != nativeTemps.end()) return &it->second;
        auto lt = lookupLocalType(expr);
        if (lt && lt->isPrimitive()) return lt;
        auto gt = lookupGlobalType(expr);
        if (gt && gt->isPrimitive()) return gt;
        return nullptr;
    }
    std::string boxIfNative(const std::string& expr) const {
        const CType* ct = exprNativeType(expr);
        return ct ? ct->box(expr) : expr;
    }
    std::string nativeRead(const std::string& expr, VType kind) const {
        const CType* ct = exprNativeType(expr);
        CType want = CType::fromVType(kind);
        if (ct && ct->kind == want.kind) return expr;
        return want.unbox(boxIfNative(expr));
    }

    // --- References ------------------------------------------------------
    void registerReference(const std::string name) { references.insert(std::move(name)); }
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
    }
    void popFunctionContext() {
        if (!contextStack.empty()) contextStack.pop_back();
        localVars.clear();
        localVarsStack.clear();
        localTypes.clear();
        localTypesStack.clear();
        localStructTypes.clear();
        fieldCache.clear();
        currentInterfaceType.clear();
        indentLevel = 1;
    }
    void setFunctionContext(bool inside) {
        if (inside) pushFunctionContext(); else popFunctionContext();
    }

    // --- Indentation / emission -----------------------------------------
    void indent() { indentLevel++; }
    void dedent() { if (indentLevel > 0) indentLevel--; }

    void emit(const std::string& code) {
        currentStream() << getIndent() << code << "\n";
    }
    void emitGlobalDecl(const std::string& code) { globalsStream << code << "\n"; }

    void emitBlockOpen(const std::string& line) {
        localVarsStack.push_back(localVars);
        localTypesStack.push_back(localTypes);
        emit(line);
        indent();
    }
    void emitBlockClose(const std::string& suffix = "") {
        dedent();
        emit("}" + suffix);
        if (!localVarsStack.empty()) {
            localVars = std::move(localVarsStack.back());
            localVarsStack.pop_back();
        }
        if (!localTypesStack.empty()) {
            localTypes = std::move(localTypesStack.back());
            localTypesStack.pop_back();
        }
    }

    std::string newTemp(const std::string& prefix = "t") {
        return prefix + "_" + std::to_string(tempVarCount++);
    }
    void addInclude(const std::string& header) { includeSet.insert(header); }

    // --- M4-C1B: interface field typing ---------------------------------
    void registerInterfaceArrayField(const std::string& iface,
                                     const std::string& field,
                                     VType elem) {
        if (elem == VType::Int64 || elem == VType::Float64)
            interfaceArrayFields[iface][field] = elem;
    }
    VType getInterfaceArrayElem(const std::string& iface,
                                const std::string& field) const {
        auto probe = [&](const std::string& k) -> VType {
            auto it = interfaceArrayFields.find(k);
            if (it == interfaceArrayFields.end()) return VType::Unknown;
            auto f = it->second.find(field);
            return f == it->second.end() ? VType::Unknown : f->second;
        };
        VType v = probe(iface);
        if (v != VType::Unknown) return v;
        std::string tmp = iface;
        size_t dot;
        while ((dot = tmp.find('.')) != std::string::npos) {
            tmp = tmp.substr(dot + 1);
            v = probe(tmp);
            if (v != VType::Unknown) return v;
        }
        return VType::Unknown;
    }

    void setLocalStructType(const std::string& var, const std::string& t) {
        localStructTypes[var] = t;
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

    void setCurrentInterfaceType(const std::string& t) { currentInterfaceType = t; }
    const std::string& getCurrentInterfaceType() const { return currentInterfaceType; }

    const FieldCacheEntry* getFieldCache(const std::string& key) const {
        auto it = fieldCache.find(key);
        return it == fieldCache.end() ? nullptr : &it->second;
    }
    void setFieldCache(const std::string& key, const std::string& temp,
                       const CType& ct) {
        fieldCache[key] = { temp, ct };
    }
    void clearFieldCache() { fieldCache.clear(); }

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
        if (instantiations.count(key)) return false;
        if (instantiationsInProgress.count(key)) return false;
        instantiationsInProgress.insert(key);
        return true;
    }
    void finishInstantiation(const std::string& key, const std::string& cName) {
        instantiationsInProgress.erase(key);
        instantiations[key] = cName;
    }

    // --- Native module lookup -------------------------------------------
    const NativeMapEntry* findNative(const std::string& module,
                                     const std::string& member) const {
        if (module == "vcore")
            for (const auto& m : VCORE_MAP) if (member == m.vyneName) return &m;
        if (module == "vmath")
            for (const auto& m : VMATH_MAP) if (member == m.vyneName) return &m;
        if (module == "vmem")                                       
            for (const auto& m : VMEM_MAP)  if (member == m.vyneName) return &m;
        return nullptr;
    }
    std::string getNativeMapping(const std::string& module,
                                 const std::string& member,
                                 bool asFunctionCall) {
        const NativeMapEntry* e = findNative(module, member);
        if (!e) return "v_" + module + "_" + member;
        if (e->isProperty) return e->cName;
        return asFunctionCall ? e->cName : std::string(e->cName) + "()";
    }

    // --- Interface / group registry -------------------------------------
    void registerInterface(const std::string& name) { interfaceSet.insert(name); }
    void registerGroup(const std::string& name)     { groupSet.insert(name); }
    bool isInterface(const std::string& name) const { return interfaceSet.count(name) > 0; }
    bool isGroup(const std::string& name) const     { return groupSet.count(name) > 0; }

    void registerFunctionSignature(const std::string& name,
                                   std::vector<std::string> params) {
        functionSignatures[name] = std::move(params);
    }
    const std::vector<std::string>* getFunctionSignature(const std::string& name) const {
        auto it = functionSignatures.find(name);
        return it == functionSignatures.end() ? nullptr : &it->second;
    }

    void setGroupPrefix(const std::string& p) { groupPrefix = p; }
    void clearGroupPrefix() { groupPrefix.clear(); }
    const std::string& getGroupPrefix() const { return groupPrefix; }

    void registerInterfaceDefaults(const std::string& name,
                                   std::vector<std::string> defaults) {
        interfaceDefaults[name] = std::move(defaults);
    }
    const std::vector<std::string>* getInterfaceDefaults(const std::string& name) const {
        auto it = interfaceDefaults.find(name);
        return it == interfaceDefaults.end() ? nullptr : &it->second;
    }

    // --- Try / defer context --------------------------------------------
    void pushTryCleanup(const std::string& label) { tryCleanupStack.push_back(label); }
    void popTryCleanup() { if (!tryCleanupStack.empty()) tryCleanupStack.pop_back(); }
    bool hasTryCleanup() const { return !tryCleanupStack.empty(); }
    const std::string& currentTryCleanup() const { return tryCleanupStack.back(); }

    // --- Region emitters --------------------------------------------
    void pushRegion(const std::string& cpHandle) { regionStack.push_back(cpHandle); }
    void popRegion() { if (!regionStack.empty()) regionStack.pop_back(); }
    bool hasRegion() const { return !regionStack.empty(); }
    const std::vector<std::string>& getRegionStack() const { return regionStack; }

    // Emit rewind calls for every live region, innermost first.
    // Used by BreakNode / ContinueNode / ReturnNode before the transfer.
    void emitRegionUnwind() {
        for (auto it = regionStack.rbegin(); it != regionStack.rend(); ++it)
            emit("vmem_runtime_rewind(" + *it + ");");
    }

    void setReturnVars(const std::string& rv, const std::string& rf) {
        currentReturnVar = rv; currentReturningVar = rf;
    }
    void clearReturnVars() { currentReturnVar.clear(); currentReturningVar.clear(); }
    bool hasReturnVars() const { return !currentReturnVar.empty(); }
    const std::string& getReturnVar() const { return currentReturnVar; }
    const std::string& getReturningVar() const { return currentReturningVar; }

    void pushDeferContext(const std::string& label, const std::string& retVar) {
        deferCtx = {label, retVar, true};
    }
    void popDeferContext() { deferCtx = {"", "", false}; }
    bool hasDeferContext() const { return deferCtx.active; }
    const std::string& getDeferCleanupLabel() const { return deferCtx.cleanupLabel; }
    const std::string& getDeferRetVar() const { return deferCtx.retVar; }

    // --- Output assembly -------------------------------------------------
    std::string finalize(const std::string& runtimeHeader = "vyne_runtime.h") {
        std::stringstream out;
        out << "#include \"" << runtimeHeader << "\"\n";
        for (const auto& inc : includeSet)
            out << "#include \"" << inc << "\"\n";
        out << "\n";

        std::string globals = globalsStream.str();
        if (!globals.empty()) out << "// --- Globals ---\n" << globals << "\n";

        std::string funcs = functionStream.str();
        if (!funcs.empty()) out << "// --- Functions ---\n" << funcs << "\n";

        out << "int main(void) {\n";
        out << mainStream.str();
        out << "    arena_free_all();\n";
        out << "    return 0;\n";
        out << "}\n";
        return out.str();
    }

    std::string getFunctionCode() { return functionStream.str(); }
    std::string getBodyCode()     { return mainStream.str(); }
    std::string getIncludes() {
        std::string res;
        for (const auto& inc : includeSet)
            res += "#include \"" + inc + "\"\n";
        return res;
    }

    // --- Full reset (between programs) ----------------------------------
    void reset() {
        globalsStream.str("");  globalsStream.clear();
        functionStream.str(""); functionStream.clear();
        mainStream.str("");     mainStream.clear();
        includeSet.clear();
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
        tryCleanupStack.clear();
        regionStack.clear(); 
        currentReturnVar.clear();
        currentReturningVar.clear();
        localVars.clear();
        globalVars.clear();
        localVarsStack.clear();
        localTypes.clear();
        localTypesStack.clear();
        globalTypes.clear();
        nativeTemps.clear();

        interfaceArrayFields.clear();
        localStructTypes.clear();
        globalStructTypes.clear();
        currentInterfaceType.clear();
        fieldCache.clear();

        instantiations.clear();
        instantiationsInProgress.clear();

        tempVarCount = 0;
        indentLevel  = 1;
    }
};

#endif