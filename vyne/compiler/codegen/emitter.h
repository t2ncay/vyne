#ifndef VYNE_EMITTER_H
#define VYNE_EMITTER_H

#include <sstream>
#include <string>

class C_Emitter {
    std::stringstream bodyStream;
    std::stringstream functionStream;
    int tempVarCount = 0;
    bool isInsideFunction = false;
    std::stringstream includes;

public:
    void setFunctionContext(bool inside) {
        isInsideFunction = inside;
    }

    void emit(const std::string& code) {
        if (isInsideFunction) {
            functionStream << code << "\n";
        } else {
            bodyStream << "  " << code << "\n";
        }
    }

    std::string newTemp() {
        return "t" + std::to_string(tempVarCount++);
    }

    std::string getFunctionCode() { return functionStream.str(); }
    std::string getBodyCode() { return bodyStream.str(); }
    
    void reset() {
        bodyStream.str(""); functionStream.str("");
        bodyStream.clear(); functionStream.clear();
        tempVarCount = 0;
        isInsideFunction = false;
    }

    void addInclude(const std::string& header) {
        includes << "#include \"" << header << "\"\n";
    }

    std::string getIncludes() { return includes.str(); }
};

#endif