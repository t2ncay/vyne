#ifndef VYNE_EMITTER_H
#define VYNE_EMITTER_H

#include <sstream>
#include <string>
#include <vector>
#include <set>

#include "../../modules/common/vcore/vcore.h"

class C_Emitter {
    std::stringstream globalsStream;
    std::stringstream functionStream;
    std::stringstream mainStream;

    std::set<std::string> includeSet;
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

    void pushFunctionContext() {
        contextStack.emplace_back(EmitContext::FUNCTION);
        indentLevel = 1; // inside function body
    }

    void popFunctionContext() {
        if (!contextStack.empty()) contextStack.pop_back();
        indentLevel = 1; // back to main body indent
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

    void emitBlockOpen(const std::string& line) {
        emit(line);
        indent();
    }

    void emitBlockClose(const std::string& suffix = "") {
        dedent();
        emit("}" + suffix);
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

   std::string getNativeMapping(const std::string& module, const std::string& member, bool asFunctionCall) {
        if (module == "vcore") {
            for (auto& m : VCORE_MAP) {
                if (m.vyneName == member) {
                    if (m.isProperty) return m.cName;
                    return asFunctionCall ? m.cName : m.cName + "()";
                }
            }
        }
        return "v_" + module + "_" + member;
    }

    void reset() {
        globalsStream.str("");   globalsStream.clear();
        functionStream.str(""); functionStream.clear();
        mainStream.str("");     mainStream.clear();
        includeSet.clear();
        contextStack.clear();
        tempVarCount = 0;
        indentLevel  = 1; // default: inside main
    }
};

#endif