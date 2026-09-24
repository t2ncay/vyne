#ifndef VYNE_EMITTER_H
#define VYNE_EMITTER_H

#include <sstream>
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>

#include "native_maps.h"

class C_Emitter {
    std::stringstream globalsStream;
    std::stringstream functionStream;
    std::stringstream mainStream;

    std::unordered_set<std::string> includeSet;

    std::unordered_set<std::string> declaredVars;
    std::unordered_set<std::string> references;

    std::unordered_set<std::string> interfaceSet; 
    std::unordered_set<std::string> groupSet;
    
    std::unordered_set<std::string> localVars;
    std::unordered_set<std::string> globalVars;
    std::vector<std::unordered_set<std::string>> localVarsStack;

    std::unordered_set<std::string> importedFiles;
    std::string sourceDir;
    std::string activeFunctionPrefix;

    std::unordered_map<std::string, std::vector<std::string>> functionSignatures;
    std::unordered_map<std::string, std::vector<std::string>> interfaceDefaults;
    std::string groupPrefix;

    struct DeferContext {
        std::string cleanupLabel;
        std::string retVar;
        bool active = false;
    };
    DeferContext deferCtx;

    std::vector<std::string> tryCleanupStack;
    std::string currentReturnVar;     // __ret_<fn>
    std::string currentReturningVar;  // __returning_<fn>

    int tempVarCount = 0;

    enum class EmitContext { GLOBAL, FUNCTION, MAIN };
    std::vector<EmitContext> contextStack;

    int indentLevel = 0;

    std::string getIndent() const {
        return std::string(indentLevel * 4, ' ');
    }

    std::stringstream& currentStream() {
        if (contextStack.empty()) return mainStream;
        switch (contextStack.back()) {
            case EmitContext::FUNCTION: return functionStream;
            case EmitContext::GLOBAL:   return globalsStream;
            default:                   return mainStream;
        }
    }

public:
    // --- Context Management ---
    
    bool isAlreadyImported(const std::string& path) const {
        return importedFiles.count(path) > 0;
    }
    void markImported(const std::string& path) {
        importedFiles.insert(path);
    }
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

    bool isGlobalContext() {
        return contextStack.empty() || contextStack.back() == EmitContext::GLOBAL;
    }

    bool isGlobalDeclared(const std::string& name) const {
        return globalVars.count(name) > 0;
    }

    bool isLocalVarsEmpty() {
        return localVars.empty();
    }

    bool isLocalDeclared(const std::string& name) const {
        return localVars.count(name) > 0;
    }

    bool isAlreadyDeclared(const std::string name) const {
        return localVars.count(std::move(name)) > 0 || globalVars.count(std::move(name)) > 0;
    }

    void registerDeclaration(std::string name) {
        if (isGlobalContext()) {
            globalVars.insert(std::move(name));
        } else {
            localVars.insert(std::move(name));
        }
    }

    void registerReference(const std::string name) {
        references.insert(std::move(name));
    }

    bool isReference(const std::string name) const {
        return references.count(std::move(name)) > 0;
    }

    void pushFunctionContext() {
        contextStack.emplace_back(EmitContext::FUNCTION);
        indentLevel = 0;
    }

    void popFunctionContext() {
        if (!contextStack.empty()) contextStack.pop_back();
        localVars.clear();
        localVarsStack.clear();   // NEW: drop any leftovers
        indentLevel = 1;
    }

    void pushGlobalContext() {
        contextStack.emplace_back(EmitContext::GLOBAL);
        indentLevel = 0;
    }

    void popGlobalContext() {
        if (!contextStack.empty()) contextStack.pop_back();
        indentLevel = 1;
    }

    void setFunctionContext(bool inside) {
        if (inside) pushFunctionContext();
        else popFunctionContext();
    }

    // --- Indentation ---

    void indent()   { indentLevel++; }
    void dedent()   { if (indentLevel > 0) indentLevel--; }

    // --- Emission ---

    void emit(const std::string& code) {
        currentStream() << getIndent() << code << "\n";
    }
    
    void emitGlobalDecl(const std::string& code) {
        globalsStream << code << "\n";
    }

    void emitBlockOpen(const std::string& line) {
        localVarsStack.push_back(localVars);
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
    }

    // --- Temp Variables ---

    std::string newTemp(const std::string& prefix = "t") {
        return prefix + "_" + std::to_string(tempVarCount++);
    }

    // --- Includes ---

    void addInclude(const std::string& header) {
        includeSet.insert(header);
    }

    // --- Final Output Assembly ---

    std::string finalize(const std::string& runtimeHeader = "vyne_runtime.h") {
        std::stringstream out;
        out << "#include \"" << runtimeHeader << "\"\n";
        for (const auto& inc : includeSet)
            out << "#include \"" << inc << "\"\n";
        out << "\n";

        std::string globals = globalsStream.str();
        if (!globals.empty()) {
            out << "// --- Globals ---\n";
            out << globals << "\n";
        }

        std::string funcs = functionStream.str();
        if (!funcs.empty()) {
            out << "// --- Functions ---\n";
            out << funcs << "\n";
        }

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

    // Return the raw entry so callers can inspect flags (usesArgv, etc.).
    const NativeMapEntry* findNative(const std::string& module,
                                     const std::string& member) const {
        if (module == "vcore") {
            for (const auto& m : VCORE_MAP)
                if (member == m.vyneName) return &m;
        }
        if (module == "vmath") {
            for (const auto& m : VMATH_MAP)
                if (member == m.vyneName) return &m;
        }
        return nullptr;
    }

    // Legacy: still used by MemberAccessNode. Returns cName, or the fallback
    // sentinel if the module/member isn't a native.
    std::string getNativeMapping(const std::string& module,
                                 const std::string& member,
                                 bool asFunctionCall) {
        const NativeMapEntry* e = findNative(module, member);
        if (!e) return "v_" + module + "_" + member;
        if (e->isProperty) return e->cName;
        return asFunctionCall ? e->cName : std::string(e->cName) + "()";
    }

    void registerInterface(const std::string& name) { interfaceSet.insert(name); }
    void registerGroup(const std::string& name)     { groupSet.insert(name); }

    bool isInterface(const std::string& name) const { return interfaceSet.count(name) > 0; }
    bool isGroup(const std::string& name) const     { return groupSet.count(name) > 0; }

    // defaults and signatures
    void registerFunctionSignature(const std::string& name, std::vector<std::string> params) {
        functionSignatures[name] = std::move(params);
    }
    const std::vector<std::string>* getFunctionSignature(const std::string& name) const {
        auto it = functionSignatures.find(name);
        if (it != functionSignatures.end()) return &it->second;
        return nullptr;
    }

    void setGroupPrefix(const std::string& p) { groupPrefix = p; }
    void clearGroupPrefix() { groupPrefix.clear(); }
    const std::string& getGroupPrefix() const { return groupPrefix; }

    void registerInterfaceDefaults(const std::string& name, std::vector<std::string> defaults) {
        interfaceDefaults[name] = std::move(defaults);
    }
    const std::vector<std::string>* getInterfaceDefaults(const std::string& name) const {
        auto it = interfaceDefaults.find(name);
        if (it != interfaceDefaults.end()) return &it->second;
        return nullptr;
    }

    void pushTryCleanup(const std::string& label) { tryCleanupStack.push_back(label); }
    void popTryCleanup() { if (!tryCleanupStack.empty()) tryCleanupStack.pop_back(); }
    bool hasTryCleanup() const { return !tryCleanupStack.empty(); }
    const std::string& currentTryCleanup() const { return tryCleanupStack.back(); }

    void setReturnVars(const std::string& rv, const std::string& rf) {
        currentReturnVar = rv; currentReturningVar = rf;
    }
    void clearReturnVars() { currentReturnVar.clear(); currentReturningVar.clear(); }
    bool hasReturnVars() const { return !currentReturnVar.empty(); }
    const std::string& getReturnVar() const { return currentReturnVar; }
    const std::string& getReturningVar() const { return currentReturningVar; }

    // --- Defer context ---
    void pushDeferContext(const std::string& label, const std::string& retVar) {
        deferCtx = {label, retVar, true};
    }
    void popDeferContext() {
        deferCtx = {"", "", false};
    }
    bool hasDeferContext() const { return deferCtx.active; }
    const std::string& getDeferCleanupLabel() const { return deferCtx.cleanupLabel; }
    const std::string& getDeferRetVar() const { return deferCtx.retVar; }

    void reset() {
        globalsStream.str("");  globalsStream.clear();
        functionStream.str(""); functionStream.clear();
        mainStream.str("");     mainStream.clear();
        includeSet.clear();     interfaceDefaults.clear();
        contextStack.clear();   groupPrefix.clear();
        interfaceSet.clear();
        groupSet.clear();
        declaredVars.clear();
        references.clear();
        importedFiles.clear();
        functionSignatures.clear();
        sourceDir = "";
        activeFunctionPrefix.clear();
        tryCleanupStack.clear();
        currentReturnVar.clear();
        currentReturningVar.clear();
        localVarsStack.clear();
        tempVarCount = 0;
        indentLevel  = 1;
    }
};

#endif