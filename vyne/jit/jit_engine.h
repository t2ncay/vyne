#pragma once

#ifdef VYNE_JIT_ENABLED

#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>

#include <memory>
#include <string>

class JITEngine {
public:
    JITEngine();

    void initialize();
    void registerRuntimeSymbols();
    void addModule(std::unique_ptr<llvm::Module> module,
                   std::unique_ptr<llvm::LLVMContext> context);
    void run(const std::string& entryPoint);

private:
    std::unique_ptr<llvm::orc::LLJIT> jit;
};

#endif
