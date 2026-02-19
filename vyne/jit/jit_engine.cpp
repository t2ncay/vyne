#ifdef VYNE_JIT_ENABLED

#include "jit_engine.h"
#include "jit_runtime.h"

#include <llvm/Support/TargetSelect.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/ExecutionEngine/Orc/AbsoluteSymbols.h>
#include <llvm/ExecutionEngine/JITSymbol.h>

#include <stdexcept>

JITEngine::JITEngine() = default;

void JITEngine::initialize() {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    auto jitExpected = llvm::orc::LLJITBuilder().create();
    if (!jitExpected) {
        std::string err;
        llvm::raw_string_ostream os(err);
        os << jitExpected.takeError();
        throw std::runtime_error("JIT: failed to create LLJIT: " + err);
    }
    jit = std::move(*jitExpected);
}

void JITEngine::registerRuntimeSymbols() {
    auto& mainDylib = jit->getMainJITDylib();

    llvm::orc::SymbolMap symbols;
    symbols[jit->mangleAndIntern("vyne_rt_out_number")] = {
        llvm::orc::ExecutorAddr::fromPtr(&vyne_rt_out_number),
        llvm::JITSymbolFlags::Exported | llvm::JITSymbolFlags::Callable};

    if (auto err = mainDylib.define(llvm::orc::absoluteSymbols(std::move(symbols)))) {
        std::string errStr;
        llvm::raw_string_ostream os(errStr);
        os << err;
        throw std::runtime_error("JIT: failed to register runtime symbols: " + errStr);
    }
}

void JITEngine::addModule(std::unique_ptr<llvm::Module> module,
                          std::unique_ptr<llvm::LLVMContext> context) {
    auto tsm = llvm::orc::ThreadSafeModule(std::move(module),
                                            std::move(context));
    if (auto err = jit->addIRModule(std::move(tsm))) {
        std::string errStr;
        llvm::raw_string_ostream os(errStr);
        os << err;
        throw std::runtime_error("JIT: failed to add module: " + errStr);
    }
}

void JITEngine::run(const std::string& entryPoint) {
    auto sym = jit->lookup(entryPoint);
    if (!sym) {
        std::string err;
        llvm::raw_string_ostream os(err);
        os << sym.takeError();
        throw std::runtime_error("JIT: failed to find '" + entryPoint + "': " + err);
    }

    auto* fn = sym->toPtr<void(*)()>();
    fn();
}

#endif
