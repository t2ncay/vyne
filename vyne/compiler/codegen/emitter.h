#ifndef VYNE_EMITTER_H
#define VYNE_EMITTER_H

#include "chunk.h"

struct Local {
    std::string name;
    int depth;
};

class C_Emitter {
    std::stringstream body;
    int tempVarCount = 0;
    int indentLevel = 1;

public:
    void emit(const std::string& code) {
        for(int i = 0; i < indentLevel; i++) body << "  ";
        body << code << "\n";
    }

    std::string newTemp() {
        return "t" + std::to_string(tempVarCount++);
    }

    std::string getBodyCode() {
        return body.str();
    }
    
    void reset() {
        body.str("");
        body.clear();
        tempVarCount = 0;
    }
};

#endif