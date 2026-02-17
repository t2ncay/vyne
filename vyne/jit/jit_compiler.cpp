#ifdef VYNE_JIT_ENABLED

#include "jit_compiler.h"
#include "../compiler/ast/ast.h"
#include "../compiler/ast/value.h"

#include <llvm/IR/Constants.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>

#include <stdexcept>

JITCompiler::JITCompiler()
    : context(std::make_unique<llvm::LLVMContext>()),
      module(std::make_unique<llvm::Module>("vyne_jit", *context)),
      builder(std::make_unique<llvm::IRBuilder<>>(*context)) {}

std::unique_ptr<llvm::Module> JITCompiler::compile(const ASTNode* root) {
    // Declare runtime functions
    llvm::FunctionType* outNumTy = llvm::FunctionType::get(
        llvm::Type::getVoidTy(*context),
        {llvm::Type::getDoubleTy(*context)},
        false);
    module->getOrInsertFunction("vyne_rt_out_number", outNumTy);

    // Create __vyne_main: void ()
    llvm::FunctionType* mainTy = llvm::FunctionType::get(
        llvm::Type::getVoidTy(*context), false);
    llvm::Function* mainFn = llvm::Function::Create(
        mainTy, llvm::Function::ExternalLinkage, "__vyne_main", module.get());

    llvm::BasicBlock* entry = llvm::BasicBlock::Create(*context, "entry", mainFn);
    builder->SetInsertPoint(entry);

    pushScope();
    codegen(root);
    popScope();

    // Terminate __vyne_main if the current block isn't already terminated
    if (!builder->GetInsertBlock()->getTerminator()) {
        builder->CreateRetVoid();
    }

    // Verify the module
    std::string err;
    llvm::raw_string_ostream errStream(err);
    if (llvm::verifyModule(*module, &errStream)) {
        throw std::runtime_error("JIT module verification failed:\n" + err);
    }

    return std::move(module);
}

std::unique_ptr<llvm::LLVMContext> JITCompiler::takeContext() {
    return std::move(context);
}

// --- Central dispatch ---

llvm::Value* JITCompiler::codegen(const ASTNode* node) {
    if (!node) return nullptr;

    switch (node->type()) {
        case NodeType::PROGRAM:       return codegenProgram(node);
        case NodeType::NUMBER:        return codegenNumber(node);
        case NodeType::BOOLEAN:       return codegenBoolean(node);
        case NodeType::VARIABLE:      return codegenVariable(node);
        case NodeType::ASSIGNMENT:    return codegenAssignment(node);
        case NodeType::BINARY_OP:     return codegenBinOp(node);
        case NodeType::UNARY:         return codegenUnary(node);
        case NodeType::POSTFIX:       return codegenPostFix(node);
        case NodeType::BLOCK:         return codegenBlock(node);
        case NodeType::IF:            return codegenIf(node);
        case NodeType::WHILE:         return codegenWhile(node);
        case NodeType::BREAK:         return codegenBreak(node);
        case NodeType::CONTINUE:      return codegenContinue(node);
        case NodeType::FUNCTION:      return codegenFunction(node);
        case NodeType::FUNCTION_CALL: return codegenFunctionCall(node);
        case NodeType::RETURN:        return codegenReturn(node);
        case NodeType::BUILTIN_CALL:  return codegenBuiltInCall(node);
        default:
            throw std::runtime_error(
                "JIT: unsupported AST node type " +
                std::to_string(static_cast<int>(node->type())));
    }
}

// --- Leaf nodes ---

llvm::Value* JITCompiler::codegenNumber(const ASTNode* node) {
    auto* n = static_cast<const NumberNode*>(node);
    return llvm::ConstantFP::get(*context, llvm::APFloat(n->value));
}

llvm::Value* JITCompiler::codegenBoolean(const ASTNode* node) {
    auto* b = static_cast<const BooleanNode*>(node);
    return llvm::ConstantFP::get(*context, llvm::APFloat(b->condition ? 1.0 : 0.0));
}

llvm::Value* JITCompiler::codegenVariable(const ASTNode* node) {
    auto* v = static_cast<const VariableNode*>(node);
    llvm::AllocaInst* alloca = lookupVariable(v->nameId);
    if (!alloca) {
        throw std::runtime_error(
            "JIT: undefined variable '" + v->originalName + "'");
    }
    return builder->CreateLoad(llvm::Type::getDoubleTy(*context), alloca, v->originalName);
}

llvm::Value* JITCompiler::codegenAssignment(const ASTNode* node) {
    auto* a = static_cast<const AssignmentNode*>(node);
    llvm::Value* val = codegen(a->rhs.get());

    llvm::AllocaInst* alloca = lookupVariable(a->identifierId);
    if (!alloca) {
        // New variable — create alloca in the current function's entry block
        llvm::Function* fn = builder->GetInsertBlock()->getParent();
        alloca = createEntryBlockAlloca(fn, a->originalName);
        scopeStack.back()[a->identifierId] = alloca;
    }
    builder->CreateStore(val, alloca);
    return val;
}

// --- Expression nodes ---

llvm::Value* JITCompiler::codegenBinOp(const ASTNode* node) {
    auto* b = static_cast<const BinOpNode*>(node);

    // Short-circuit for && and ||
    if (b->op == VTokenType::And) {
        llvm::Function* fn = builder->GetInsertBlock()->getParent();
        llvm::BasicBlock* rhsBB = llvm::BasicBlock::Create(*context, "and.rhs", fn);
        llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(*context, "and.merge", fn);

        llvm::Value* lhs = codegen(b->left.get());
        llvm::Value* lhsBool = toBool(lhs);
        llvm::BasicBlock* lhsBB = builder->GetInsertBlock();
        builder->CreateCondBr(lhsBool, rhsBB, mergeBB);

        builder->SetInsertPoint(rhsBB);
        llvm::Value* rhs = codegen(b->right.get());
        llvm::BasicBlock* rhsEndBB = builder->GetInsertBlock();
        builder->CreateBr(mergeBB);

        builder->SetInsertPoint(mergeBB);
        llvm::PHINode* phi = builder->CreatePHI(llvm::Type::getDoubleTy(*context), 2, "and.result");
        phi->addIncoming(llvm::ConstantFP::get(*context, llvm::APFloat(0.0)), lhsBB);
        phi->addIncoming(rhs, rhsEndBB);
        return phi;
    }

    if (b->op == VTokenType::Or) {
        llvm::Function* fn = builder->GetInsertBlock()->getParent();
        llvm::BasicBlock* rhsBB = llvm::BasicBlock::Create(*context, "or.rhs", fn);
        llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(*context, "or.merge", fn);

        llvm::Value* lhs = codegen(b->left.get());
        llvm::Value* lhsBool = toBool(lhs);
        llvm::BasicBlock* lhsBB = builder->GetInsertBlock();
        builder->CreateCondBr(lhsBool, mergeBB, rhsBB);

        builder->SetInsertPoint(rhsBB);
        llvm::Value* rhs = codegen(b->right.get());
        llvm::BasicBlock* rhsEndBB = builder->GetInsertBlock();
        builder->CreateBr(mergeBB);

        builder->SetInsertPoint(mergeBB);
        llvm::PHINode* phi = builder->CreatePHI(llvm::Type::getDoubleTy(*context), 2, "or.result");
        phi->addIncoming(lhs, lhsBB);
        phi->addIncoming(rhs, rhsEndBB);
        return phi;
    }

    llvm::Value* L = codegen(b->left.get());
    llvm::Value* R = codegen(b->right.get());

    switch (b->op) {
        case VTokenType::Add:
            return builder->CreateFAdd(L, R, "addtmp");
        case VTokenType::Substract:
            return builder->CreateFSub(L, R, "subtmp");
        case VTokenType::Multiply:
            return builder->CreateFMul(L, R, "multmp");
        case VTokenType::Division:
            return builder->CreateFDiv(L, R, "divtmp");
        case VTokenType::Modulo:
            return builder->CreateFRem(L, R, "modtmp");
        case VTokenType::Floor_Divide: {
            llvm::Value* div = builder->CreateFDiv(L, R, "fdivtmp");
            llvm::Function* floorFn = llvm::Intrinsic::getOrInsertDeclaration(
                module.get(), llvm::Intrinsic::floor, {llvm::Type::getDoubleTy(*context)});
            return builder->CreateCall(floorFn, {div}, "floortmp");
        }
        case VTokenType::Power: {
            llvm::Function* powFn = llvm::Intrinsic::getOrInsertDeclaration(
                module.get(), llvm::Intrinsic::pow, {llvm::Type::getDoubleTy(*context)});
            return builder->CreateCall(powFn, {L, R}, "powtmp");
        }

        // Comparisons: fcmp -> uitofp to get 1.0/0.0
        case VTokenType::Double_Equals: {
            llvm::Value* cmp = builder->CreateFCmpOEQ(L, R, "eqtmp");
            return builder->CreateUIToFP(cmp, llvm::Type::getDoubleTy(*context), "eqcast");
        }
        case VTokenType::Not_Equal: {
            llvm::Value* cmp = builder->CreateFCmpONE(L, R, "netmp");
            return builder->CreateUIToFP(cmp, llvm::Type::getDoubleTy(*context), "necast");
        }
        case VTokenType::Greater: {
            llvm::Value* cmp = builder->CreateFCmpOGT(L, R, "gttmp");
            return builder->CreateUIToFP(cmp, llvm::Type::getDoubleTy(*context), "gtcast");
        }
        case VTokenType::Smaller: {
            llvm::Value* cmp = builder->CreateFCmpOLT(L, R, "lttmp");
            return builder->CreateUIToFP(cmp, llvm::Type::getDoubleTy(*context), "ltcast");
        }
        case VTokenType::Greater_Or_Equal: {
            llvm::Value* cmp = builder->CreateFCmpOGE(L, R, "getmp");
            return builder->CreateUIToFP(cmp, llvm::Type::getDoubleTy(*context), "gecast");
        }
        case VTokenType::Smaller_Or_Equal: {
            llvm::Value* cmp = builder->CreateFCmpOLE(L, R, "letmp");
            return builder->CreateUIToFP(cmp, llvm::Type::getDoubleTy(*context), "lecast");
        }

        default:
            throw std::runtime_error(
                "JIT: unsupported binary operator " +
                std::to_string(static_cast<int>(b->op)));
    }
}

llvm::Value* JITCompiler::codegenUnary(const ASTNode* node) {
    auto* u = static_cast<const UnaryNode*>(node);
    llvm::Value* operand = codegen(u->right.get());

    switch (u->op) {
        case VTokenType::Substract:
            return builder->CreateFNeg(operand, "negtmp");
        case VTokenType::Exclamatory: {
            // !x: compare to 0.0, invert
            llvm::Value* isZero = builder->CreateFCmpOEQ(
                operand,
                llvm::ConstantFP::get(*context, llvm::APFloat(0.0)),
                "nottmp");
            return builder->CreateUIToFP(isZero, llvm::Type::getDoubleTy(*context), "notcast");
        }
        default:
            throw std::runtime_error(
                "JIT: unsupported unary operator " +
                std::to_string(static_cast<int>(u->op)));
    }
}

llvm::Value* JITCompiler::codegenPostFix(const ASTNode* node) {
    auto* p = static_cast<const PostFixNode*>(node);

    // The left operand must be a variable
    auto* var = dynamic_cast<const VariableNode*>(p->left.get());
    if (!var) {
        throw std::runtime_error("JIT: postfix operator requires a variable");
    }

    llvm::AllocaInst* alloca = lookupVariable(var->nameId);
    if (!alloca) {
        throw std::runtime_error("JIT: undefined variable '" + var->originalName + "'");
    }

    llvm::Value* oldVal = builder->CreateLoad(
        llvm::Type::getDoubleTy(*context), alloca, "postfix.old");
    llvm::Value* one = llvm::ConstantFP::get(*context, llvm::APFloat(1.0));

    llvm::Value* newVal;
    if (p->op == VTokenType::Double_Increment) {
        newVal = builder->CreateFAdd(oldVal, one, "inc");
    } else {
        newVal = builder->CreateFSub(oldVal, one, "dec");
    }
    builder->CreateStore(newVal, alloca);
    return oldVal; // postfix returns old value
}

// --- Control flow ---

llvm::Value* JITCompiler::codegenBlock(const ASTNode* node) {
    auto* blk = static_cast<const BlockNode*>(node);
    llvm::Value* last = nullptr;

    pushScope();
    for (const auto& stmt : blk->statements) {
        // Stop emitting if the current block is already terminated
        if (builder->GetInsertBlock()->getTerminator()) break;
        last = codegen(stmt.get());
    }
    popScope();
    return last;
}

llvm::Value* JITCompiler::codegenIf(const ASTNode* node) {
    auto* ifn = static_cast<const IfNode*>(node);
    llvm::Function* fn = builder->GetInsertBlock()->getParent();

    llvm::Value* condVal = codegen(ifn->condition.get());
    llvm::Value* condBool = toBool(condVal);

    bool hasElse = (ifn->elseBody != nullptr);

    llvm::BasicBlock* thenBB = llvm::BasicBlock::Create(*context, "if.then", fn);
    llvm::BasicBlock* elseBB = hasElse
        ? llvm::BasicBlock::Create(*context, "if.else", fn)
        : nullptr;
    llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(*context, "if.merge", fn);

    if (hasElse) {
        builder->CreateCondBr(condBool, thenBB, elseBB);
    } else {
        builder->CreateCondBr(condBool, thenBB, mergeBB);
    }

    // Then block
    builder->SetInsertPoint(thenBB);
    codegen(ifn->body.get());
    if (!builder->GetInsertBlock()->getTerminator()) {
        builder->CreateBr(mergeBB);
    }

    // Else block
    if (hasElse) {
        builder->SetInsertPoint(elseBB);
        codegen(ifn->elseBody.get());
        if (!builder->GetInsertBlock()->getTerminator()) {
            builder->CreateBr(mergeBB);
        }
    }

    builder->SetInsertPoint(mergeBB);
    return nullptr;
}

llvm::Value* JITCompiler::codegenWhile(const ASTNode* node) {
    auto* w = static_cast<const WhileNode*>(node);
    llvm::Function* fn = builder->GetInsertBlock()->getParent();

    llvm::BasicBlock* headerBB = llvm::BasicBlock::Create(*context, "while.header", fn);
    llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(*context, "while.body", fn);
    llvm::BasicBlock* exitBB = llvm::BasicBlock::Create(*context, "while.exit", fn);

    builder->CreateBr(headerBB);

    // Header: evaluate condition
    builder->SetInsertPoint(headerBB);
    llvm::Value* condVal = codegen(w->condition.get());
    llvm::Value* condBool = toBool(condVal);
    builder->CreateCondBr(condBool, bodyBB, exitBB);

    // Body
    loopStack.push_back({headerBB, exitBB});
    builder->SetInsertPoint(bodyBB);
    codegen(w->body.get());
    if (!builder->GetInsertBlock()->getTerminator()) {
        builder->CreateBr(headerBB);
    }
    loopStack.pop_back();

    builder->SetInsertPoint(exitBB);
    return nullptr;
}

llvm::Value* JITCompiler::codegenBreak(const ASTNode* /*node*/) {
    if (loopStack.empty()) {
        throw std::runtime_error("JIT: break outside of loop");
    }
    builder->CreateBr(loopStack.back().exitBB);
    return nullptr;
}

llvm::Value* JITCompiler::codegenContinue(const ASTNode* /*node*/) {
    if (loopStack.empty()) {
        throw std::runtime_error("JIT: continue outside of loop");
    }
    builder->CreateBr(loopStack.back().headerBB);
    return nullptr;
}

// --- Functions ---

llvm::Value* JITCompiler::codegenFunction(const ASTNode* node) {
    auto* f = static_cast<const FunctionNode*>(node);

    // Create function type: all params are double, returns double
    size_t numParams = f->parameterIds.size();
    std::vector<llvm::Type*> paramTypes(numParams, llvm::Type::getDoubleTy(*context));
    llvm::FunctionType* funcTy = llvm::FunctionType::get(
        llvm::Type::getDoubleTy(*context), paramTypes, false);

    llvm::Function* func = llvm::Function::Create(
        funcTy, llvm::Function::ExternalLinkage, f->originalName, module.get());

    // Name the parameters
    size_t idx = 0;
    for (auto& arg : func->args()) {
        arg.setName(StringPool::instance().get(f->parameterIds[idx++]));
    }

    // Register in function map
    functionMap[f->funcNameId] = func;

    // Save current insert point
    llvm::BasicBlock* savedBB = builder->GetInsertBlock();

    // Create entry block for the function
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(*context, "entry", func);
    builder->SetInsertPoint(entry);

    pushScope();

    // Create allocas for parameters and store the argument values
    idx = 0;
    for (auto& arg : func->args()) {
        llvm::AllocaInst* alloca = createEntryBlockAlloca(func, arg.getName().str());
        builder->CreateStore(&arg, alloca);
        scopeStack.back()[f->parameterIds[idx++]] = alloca;
    }

    // Codegen function body
    for (const auto& stmt : f->body) {
        if (builder->GetInsertBlock()->getTerminator()) break;
        codegen(stmt.get());
    }

    // If no explicit return, return 0.0
    if (!builder->GetInsertBlock()->getTerminator()) {
        builder->CreateRet(llvm::ConstantFP::get(*context, llvm::APFloat(0.0)));
    }

    popScope();

    // Verify the function
    std::string err;
    llvm::raw_string_ostream errStream(err);
    if (llvm::verifyFunction(*func, &errStream)) {
        throw std::runtime_error("JIT: function '" + f->originalName +
                                 "' verification failed:\n" + err);
    }

    // Restore insert point to __vyne_main
    builder->SetInsertPoint(savedBB);
    return nullptr;
}

llvm::Value* JITCompiler::codegenFunctionCall(const ASTNode* node) {
    auto* fc = static_cast<const FunctionCallNode*>(node);

    auto it = functionMap.find(fc->funcNameId);
    if (it == functionMap.end()) {
        throw std::runtime_error(
            "JIT: undefined function '" + fc->originalName + "'");
    }
    llvm::Function* callee = it->second;

    std::vector<llvm::Value*> args;
    for (const auto& arg : fc->arguments) {
        args.push_back(codegen(arg.get()));
    }

    return builder->CreateCall(callee, args, "calltmp");
}

llvm::Value* JITCompiler::codegenReturn(const ASTNode* node) {
    auto* r = static_cast<const ReturnNode*>(node);
    llvm::Value* retVal;
    if (r->expression) {
        retVal = codegen(r->expression.get());
    } else {
        retVal = llvm::ConstantFP::get(*context, llvm::APFloat(0.0));
    }
    builder->CreateRet(retVal);
    return retVal;
}

llvm::Value* JITCompiler::codegenBuiltInCall(const ASTNode* node) {
    auto* bc = static_cast<const BuiltInCallNode*>(node);

    if (bc->funcName == "out") {
        if (!bc->arguments.empty()) {
            llvm::Value* arg = codegen(bc->arguments[0].get());
            llvm::Function* outFn = module->getFunction("vyne_rt_out_number");
            builder->CreateCall(outFn, {arg});
        }
        return llvm::ConstantFP::get(*context, llvm::APFloat(0.0));
    }

    throw std::runtime_error("JIT: unsupported built-in function '" + bc->funcName + "'");
}

llvm::Value* JITCompiler::codegenProgram(const ASTNode* node) {
    auto* prog = static_cast<const ProgramNode*>(node);
    llvm::Value* last = nullptr;
    for (const auto& stmt : prog->statements) {
        if (builder->GetInsertBlock()->getTerminator()) break;
        last = codegen(stmt.get());
    }
    return last;
}

// --- Helpers ---

llvm::AllocaInst* JITCompiler::createEntryBlockAlloca(
    llvm::Function* fn, const std::string& name) {
    llvm::IRBuilder<> tmpBuilder(&fn->getEntryBlock(), fn->getEntryBlock().begin());
    return tmpBuilder.CreateAlloca(llvm::Type::getDoubleTy(*context), nullptr, name);
}

llvm::AllocaInst* JITCompiler::lookupVariable(uint32_t nameId) {
    // Search from innermost scope outward
    for (auto it = scopeStack.rbegin(); it != scopeStack.rend(); ++it) {
        auto found = it->find(nameId);
        if (found != it->end()) return found->second;
    }
    return nullptr;
}

void JITCompiler::pushScope() {
    scopeStack.emplace_back();
}

void JITCompiler::popScope() {
    if (!scopeStack.empty()) scopeStack.pop_back();
}

llvm::Value* JITCompiler::toBool(llvm::Value* val) {
    return builder->CreateFCmpONE(
        val,
        llvm::ConstantFP::get(*context, llvm::APFloat(0.0)),
        "tobool");
}

#endif
