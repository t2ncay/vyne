#pragma once

#ifdef VYNE_JIT_ENABLED

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Verifier.h>

#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <cstdint>

class ASTNode;

class JITCompiler {
public:
    JITCompiler();

    std::unique_ptr<llvm::Module> compile(const ASTNode* root);
    std::unique_ptr<llvm::LLVMContext> takeContext();

private:
    std::unique_ptr<llvm::LLVMContext> context;
    std::unique_ptr<llvm::Module> module;
    std::unique_ptr<llvm::IRBuilder<>> builder;

    // Scope stack: each scope maps variable nameId -> alloca
    std::vector<std::unordered_map<uint32_t, llvm::AllocaInst*>> scopeStack;

    // Function map: funcNameId -> LLVM Function
    std::unordered_map<uint32_t, llvm::Function*> functionMap;

    // Loop stack for break/continue
    struct LoopInfo {
        llvm::BasicBlock* headerBB;
        llvm::BasicBlock* exitBB;
    };
    std::vector<LoopInfo> loopStack;

    // Central dispatch
    llvm::Value* codegen(const ASTNode* node);

    // Node-specific codegen
    llvm::Value* codegenNumber(const ASTNode* node);
    llvm::Value* codegenBoolean(const ASTNode* node);
    llvm::Value* codegenVariable(const ASTNode* node);
    llvm::Value* codegenAssignment(const ASTNode* node);
    llvm::Value* codegenBinOp(const ASTNode* node);
    llvm::Value* codegenUnary(const ASTNode* node);
    llvm::Value* codegenPostFix(const ASTNode* node);
    llvm::Value* codegenBlock(const ASTNode* node);
    llvm::Value* codegenIf(const ASTNode* node);
    llvm::Value* codegenWhile(const ASTNode* node);
    llvm::Value* codegenBreak(const ASTNode* node);
    llvm::Value* codegenContinue(const ASTNode* node);
    llvm::Value* codegenFunction(const ASTNode* node);
    llvm::Value* codegenFunctionCall(const ASTNode* node);
    llvm::Value* codegenReturn(const ASTNode* node);
    llvm::Value* codegenBuiltInCall(const ASTNode* node);
    llvm::Value* codegenProgram(const ASTNode* node);

    // Helpers
    llvm::AllocaInst* createEntryBlockAlloca(llvm::Function* fn, const std::string& name);
    llvm::AllocaInst* lookupVariable(uint32_t nameId);
    void pushScope();
    void popScope();
    llvm::Value* toBool(llvm::Value* val);
};

#endif
