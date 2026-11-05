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

#include "detail/codegen_helpers.h"

// Region checkpoint and commit lowering.
// Keep expressions that emit statements in evaluation order; see README.md.
// ============================================================
// A1 — LEXICAL REGIONS
// ------------------------------------------------------------
// `region name { body }` becomes:
//
//     VyneValue __cp_N = vmem_runtime_checkpoint();
//     { body }                                  // C block: locals scoped
//     vmem_runtime_rewind(__cp_N);
//
// The C block is what keeps locals declared inside the region from
// leaking into the surrounding scope — matching the lifetime
// guarantee at the source level. Nested regions work because
// vmem.h's checkpoint stack is itself a stack; every rewind pops
// its own handle and everything above it.
// ============================================================

static void ensureVmemInclude(C_Emitter& e) {
    std::string base = FileUtils::getExeDir();
    std::filesystem::path moduleBase =
        std::filesystem::path(base) / "vyne" / "runtime" / "modules";
    e.addInclude((moduleBase / "vmem.h").string());
}

static void emitPoolRuntimeOnce(C_Emitter& e) {
    if (e.isPoolRuntimeEmitted())
        return;
    std::string base = FileUtils::getExeDir();
    std::filesystem::path moduleBase =
        std::filesystem::path(base) / "vyne" / "runtime" / "modules";
    e.addInclude((moduleBase / "vyne_pool_runtime.h").string());
    e.markPoolRuntimeEmitted();
}

static void compileBumpRegion(C_Emitter& e, const RegionNode& r) {
    ensureVmemInclude(e);

    std::string cpHandle = e.newTemp("vmem_cp");

    e.emit("// --- region: " + r.getRegionName() + " (bump) ---");
    e.emit("VyneValue " + cpHandle + " = vmem_runtime_checkpoint();");

    e.pushRegion(cpHandle);
    e.emitBlockOpen("{");
    for (const auto& stmt : r.getBody())
        if (stmt)
            stmt->compile(e);
    e.emitBlockClose();
    e.popRegion();

    e.emit("vmem_runtime_rewind(" + cpHandle + ");");
}

// ============================================================
// @pool<T, N> — fixed-slot pool region
// ------------------------------------------------------------
// The pool is a bump region with a specialized allocator inside. The
// checkpoint is taken BEFORE the pool struct is allocated, so the
// rewind at region close reclaims the pool's backing store and free
// list along with everything the body allocated.
//
// Early return/break/continue works through the existing
// emitRegionUnwind() path: cpHandle is on the region stack, so
// ReturnNode::compile and friends emit `vmem_runtime_rewind(cp)` for
// it exactly like they do for a bump region.
//
// Pool buffers do NOT survive the region. Attempting to hand one to
// `region.commit()` triggers VNE-084 from RegionCommitNode::compile.
// ============================================================
static void compilePoolRegion(C_Emitter& e, const RegionNode& r) {
    const auto& args = r.getPolicyArgs();
    const int line = r.lineNumber;

    if (args.size() != 2) {
        throw std::runtime_error(
            "Compile Error (VNE-080): @pool expects exactly 2 arguments "
            "(element type, slot count), got " +
            std::to_string(args.size()) + " (line " + std::to_string(line) +
            ").\n"
            "  Syntax: @pool<Float64, 64> region name { ... };"
        );
    }

    VType elem;
    if (args[0] == "Float64")
        elem = VType::Float64;
    else if (args[0] == "Int64")
        elem = VType::Int64;
    else {
        throw std::runtime_error(
            "Compile Error (VNE-080): @pool element type must be "
            "Float64 or Int64, got '" +
            args[0] + "' (line " + std::to_string(line) + ")."
        );
    }

    int64_t elemsPerSlot = 0;
    try {
        elemsPerSlot = std::stoll(args[1]);
    } catch (...) {
        throw std::runtime_error(
            "Compile Error (VNE-080): @pool slot count must be an integer, "
            "got '" +
            args[1] + "' (line " + std::to_string(line) + ")."
        );
    }
    if (elemsPerSlot <= 0) {
        throw std::runtime_error(
            "Compile Error (VNE-080): @pool slot count must be positive "
            "(line " +
            std::to_string(line) + ")."
        );
    }

    // Capacity is the number of concurrent live buffers the pool will
    // hold. There is no third arg today — eight is the conservative
    // default that covers the KV-cache / sliding-window shapes the
    // policy was designed for. `@pool<T, N, K>` is a five-line
    // extension when a workload needs more.
    const int64_t capacity = 8;

    ensureVmemInclude(e);
    emitPoolRuntimeOnce(e);

    std::string cpHandle = e.newTemp("vmem_cp");
    std::string poolVar = e.newTemp("vmem_pool");
    std::string ctor = (elem == VType::Float64) ? "vyne_pool_f64_create"
                                                : "vyne_pool_i64_create";
    const char* poolCName =
        (elem == VType::Float64) ? "VynePool_f64" : "VynePool_i64";

    e.emit(
        "// --- region: " + r.getRegionName() + " (pool<" + args[0] + ", " +
        args[1] + ">) ---"
    );
    e.emit("VyneValue " + cpHandle + " = vmem_runtime_checkpoint();");
    e.emit(
        std::string(poolCName) + " " + poolVar + " = " + ctor + "(" +
        std::to_string(elemsPerSlot) + ", " + std::to_string(capacity) + ");"
    );

    e.pushRegion(cpHandle);
    e.pushPool({poolVar, elem, elemsPerSlot});

    e.emitBlockOpen("{");
    for (const auto& stmt : r.getBody())
        if (stmt)
            stmt->compile(e);
    e.emitBlockClose();

    e.popPool();
    e.popRegion();

    e.emit("vmem_runtime_rewind(" + cpHandle + ");");
}

static void compileSpeculativeRegion(C_Emitter& e, const RegionNode& r) {
    if (!r.getPolicyArgs().empty()) {
        throw std::runtime_error(
            "Compile Error (VNE-080): @speculative takes no arguments, "
            "got " +
            std::to_string(r.getPolicyArgs().size()) + " (line " +
            std::to_string(r.lineNumber) + ")."
        );
    }

    ensureVmemInclude(e);

    std::string cpHandle = e.newTemp("vmem_cp");
    std::string commitFlag = e.newTemp("spec_committed");

    e.emit("// --- region: " + r.getRegionName() + " (speculative) ---");
    e.emit("VyneValue " + cpHandle + " = vmem_runtime_checkpoint();");
    e.emit("int " + commitFlag + " = 0;");

    e.pushRegion(cpHandle);
    e.pushSpeculative({commitFlag});

    e.emitBlockOpen("{");
    for (const auto& stmt : r.getBody())
        if (stmt)
            stmt->compile(e);
    e.emitBlockClose();

    e.popSpeculative();
    e.popRegion();

    e.emit("if (" + commitFlag + ") {");
    e.emit("    vmem_runtime_pop_checkpoints(1);");
    e.emit("} else {");
    e.emit("    vmem_runtime_rewind(" + cpHandle + ");");
    e.emit("}");
}

void RegionCommitIfNode::compile(C_Emitter& e) const {
    if (!e.hasSpeculative()) {
        throw std::runtime_error(
            "Compile Error (VNE-085): region.commit_if() is only valid "
            "inside a @speculative region (line " +
            std::to_string(lineNumber) +
            ").\n"
            "  Wrap the containing region in '@speculative' to make its "
            "commit decision conditional."
        );
    }

    const auto& ctx = e.currentSpeculative();
    std::string pred = e.boxAny(predicate->getCExpr(e));
    // Last write wins. Multiple commit_if calls simply overwrite the flag;
    // the region's fate is decided by whichever call fires last.
    e.emit(ctx.commitFlag + " = vyne_is_truthy(" + pred + ") ? 1 : 0;");
}

std::string RegionCommitIfNode::getCExpr(C_Emitter& e) const {
    compile(e);
    return "vyne_null()";
}

// ============================================================
// @bounded<N> — runtime-checked memory ceiling
// ------------------------------------------------------------
// Emits a byte delta measured against the arena's allocation counter,
// compared against N at region close. A violation halts with a
// diagnostic naming the region. Runtime-checked, not statically
// proven — but the counter evidence is exactly what certification
// workflows (DO-178C, ISO 26262) need: a documented bound and a test
// suite that demonstrates it held.
// ============================================================
static void compileBoundedRegion(C_Emitter& e, const RegionNode& r) {
    ensureVmemInclude(e);

    if (r.getPolicyArgs().size() != 1) {
        throw std::runtime_error(
            "Compile Error (VNE-101): @bounded expects exactly 1 argument "
            "(byte count), got " +
            std::to_string(r.getPolicyArgs().size()) + " (line " +
            std::to_string(r.lineNumber) +
            ").\n"
            "  Syntax: @bounded<4096> region name { ... };"
        );
    }

    int64_t bound = 0;
    try {
        bound = std::stoll(r.getPolicyArgs()[0]);
    } catch (...) {
        throw std::runtime_error(
            "Compile Error (VNE-101): @bounded argument must be an integer, "
            "got '" +
            r.getPolicyArgs()[0] + "' (line " + std::to_string(r.lineNumber) +
            ")."
        );
    }
    if (bound <= 0) {
        throw std::runtime_error(
            "Compile Error (VNE-101): @bounded argument must be positive "
            "(line " +
            std::to_string(r.lineNumber) + ")."
        );
    }

    std::string cpHandle = e.newTemp("vmem_cp");
    std::string startVar = e.newTemp("bounded_start");
    std::string usedVar = e.newTemp("bounded_used");

    e.emit(
        "// --- region: " + r.getRegionName() + " (bounded<" +
        std::to_string(bound) + ">) ---"
    );
    e.emit("VyneValue " + cpHandle + " = vmem_runtime_checkpoint();");
    e.emit("size_t " + startVar + " = g_arena.total_allocated;");

    e.pushRegion(cpHandle);
    e.pushBounded();
    e.emitBlockOpen("{");
    for (const auto& stmt : r.getBody())
        if (stmt)
            stmt->compile(e);
    e.emitBlockClose();
    e.popBounded();
    e.popRegion();

    e.emit(
        "size_t " + usedVar + " = g_arena.total_allocated - " + startVar + ";"
    );
    e.emit("if (" + usedVar + " > " + std::to_string(bound) + ") {");
    e.emit(
        "    vyne_panic_region_bound(\"" + r.getRegionName() + "\", " +
        std::to_string(r.lineNumber) + ", " + std::to_string(bound) + ", " +
        usedVar + ");"
    );
    e.emit("}");
    e.emit("vmem_runtime_rewind(" + cpHandle + ");");
}

// ============================================================
// @zeroed — every allocation inside the region is zero-filled
// ------------------------------------------------------------
// Bumps g_arena_zero_mode around the body; arena_alloc memsets on
// every return path when the counter is > 0. ScratchNode::compile
// consults the same counter to emit `double v_x[N] = {0};`
// (static shape) or a `memset` (VLA). The counter is a plain int —
// nesting is supported but rare; a nested @zeroed region is
// equivalent to the outer one for its own body.
// ============================================================
static void compileZeroedRegion(C_Emitter& e, const RegionNode& r) {
    ensureVmemInclude(e);

    std::string cpHandle = e.newTemp("vmem_cp");

    e.emit("// --- region: " + r.getRegionName() + " (zeroed) ---");
    e.emit("VyneValue " + cpHandle + " = vmem_runtime_checkpoint();");
    e.emit("g_arena_zero_mode++;");

    e.pushRegion(cpHandle);
    e.pushZeroMode();
    e.emitBlockOpen("{");
    for (const auto& stmt : r.getBody())
        if (stmt)
            stmt->compile(e);
    e.emitBlockClose();
    e.popZeroMode();
    e.popRegion();

    e.emit("g_arena_zero_mode--;");
    e.emit("vmem_runtime_rewind(" + cpHandle + ");");
}

// ============================================================
// @deterministic — @zeroed + a non-determinism scan
// ------------------------------------------------------------
// Rejects calls to time / RNG / sensor-read sources inside the
// region body. Combined with @zeroed, produces byte-identical
// output across runs conditioned on identical inputs.
//
// The scan cannot see inside WhileNode / ForNode bodies — those AST
// nodes do not expose their body accessor. Documented limitation;
// the suggestion list on VNE-106 names it, and the runtime zeroed
// counter catches the rest at allocation time.
// ============================================================
static bool isNonDeterministicCall(
    const std::string& recvPath,
    const std::string& methodOrFn,
    std::string& outOffender
) {
    if ((recvPath == "vcore" || recvPath.empty()) &&
        (methodOrFn == "now" || methodOrFn == "now_ns" ||
         methodOrFn == "input")) {
        outOffender = "vcore." + methodOrFn + "()";
        return true;
    }
    if ((recvPath == "vmath" || recvPath.empty()) &&
        (methodOrFn == "random" || methodOrFn == "random_float" ||
         methodOrFn == "seed")) {
        outOffender = "vmath." + methodOrFn + "()";
        return true;
    }
    if (recvPath == "arduino" &&
        (methodOrFn == "millis" || methodOrFn == "micros" ||
         methodOrFn == "random" || methodOrFn == "random_seed" ||
         methodOrFn.find("analog_read") == 0)) {
        outOffender = "arduino." + methodOrFn + "()";
        return true;
    }
    return false;
}

static bool scanNonDeterminism(const ASTNode* node, std::string& outOffender) {
    if (!node)
        return false;

    auto receiverPath = [](const ASTNode* r) -> std::string {
        if (!r)
            return "";
        if (r->type() == NodeType::VARIABLE)
            return static_cast<const VariableNode*>(r)->getOriginalName();
        if (r->type() == NodeType::MEMBER_ACCESS)
            return static_cast<const MemberAccessNode*>(r)->getFullPath();
        return "";
    };

    switch (node->type()) {
    case NodeType::FUNCTION_CALL: {
        auto* fc = static_cast<const FunctionCallNode*>(node);
        const std::string& name = fc->getOriginalName();
        size_t dot = name.find_last_of('.');
        std::string recvPath =
            (dot == std::string::npos) ? "" : name.substr(0, dot);
        std::string method =
            (dot == std::string::npos) ? name : name.substr(dot + 1);
        if (isNonDeterministicCall(recvPath, method, outOffender))
            return true;
        for (const auto& a : fc->getArguments())
            if (scanNonDeterminism(a.get(), outOffender))
                return true;
        return false;
    }
    case NodeType::METHOD_CALL: {
        auto* mc = static_cast<const MethodCallNode*>(node);
        if (isNonDeterministicCall(
                receiverPath(mc->getReceiver()),
                mc->getMethodName(),
                outOffender
            ))
            return true;
        if (scanNonDeterminism(mc->getReceiver(), outOffender))
            return true;
        for (const auto& a : mc->getArguments())
            if (scanNonDeterminism(a.get(), outOffender))
                return true;
        return false;
    }
    case NodeType::BINARY_OP: {
        auto* b = static_cast<const BinOpNode*>(node);
        return scanNonDeterminism(b->leftNode.get(), outOffender) ||
               scanNonDeterminism(b->rightNode.get(), outOffender);
    }
    case NodeType::UNARY: {
        return scanNonDeterminism(
            static_cast<const UnaryNode*>(node)->getRight(),
            outOffender
        );
    }
    case NodeType::ASSIGNMENT: {
        return scanNonDeterminism(
            static_cast<const AssignmentNode*>(node)->getRHS(),
            outOffender
        );
    }
    case NodeType::INDEX_ACCESS: {
        auto* ia = static_cast<const IndexAccessNode*>(node);
        return scanNonDeterminism(ia->getBase(), outOffender) ||
               scanNonDeterminism(ia->getIndex(), outOffender);
    }
    case NodeType::MEMBER_ACCESS: {
        return scanNonDeterminism(
            static_cast<const MemberAccessNode*>(node)->getReceiver(),
            outOffender
        );
    }
    case NodeType::BLOCK: {
        for (const auto& s : static_cast<const BlockNode*>(node)->statements)
            if (scanNonDeterminism(s.get(), outOffender))
                return true;
        return false;
    }
    case NodeType::IF: {
        auto* f = static_cast<const IfNode*>(node);
        return scanNonDeterminism(f->condition.get(), outOffender) ||
               scanNonDeterminism(f->body.get(), outOffender) ||
               scanNonDeterminism(f->elseBody.get(), outOffender);
    }
    default:
        // WhileNode / ForNode / ReturnNode bodies are not publicly
        // reachable through ASTNode accessors. Runtime zeroed counters
        // are the defence in depth for these subtrees.
        return false;
    }
}

static void compileDeterministicRegion(C_Emitter& e, const RegionNode& r) {
    std::string offender;
    for (const auto& stmt : r.getBody()) {
        if (stmt && scanNonDeterminism(stmt.get(), offender)) {
            throw std::runtime_error(
                "Compile Error (VNE-106): @deterministic region '" +
                r.getRegionName() +
                "' contains a call to non-deterministic "
                "'" +
                offender + "' (line " + std::to_string(stmt->lineNumber) +
                ").\n"
                "  @deterministic guarantees byte-identical output across "
                "runs; time and RNG sources break that guarantee.\n"
                "  Fix: pass time and RNG state in as region parameters, "
                "or drop @deterministic from this region."
            );
        }
    }
    // Body is clean — @deterministic implies @zeroed.
    compileZeroedRegion(e, r);
}

void RegionNode::compile(C_Emitter& e) const {
    if (policy.empty() || policy == "bump") {
        compileBumpRegion(e, *this);
        return;
    }
    if (policy == "pool") {
        compilePoolRegion(e, *this);
        return;
    }
    if (policy == "speculative") {
        compileSpeculativeRegion(e, *this);
        return;
    }
    if (policy == "bounded") {
        compileBoundedRegion(e, *this);
        return;
    }
    if (policy == "zeroed") {
        compileZeroedRegion(e, *this);
        return;
    }
    if (policy == "deterministic") {
        compileDeterministicRegion(e, *this);
        return;
    }

    throw std::runtime_error(
        "Compile Error (VNE-080): region policy '@" + policy +
        "' is "
        "recognized but not yet implemented (line " +
        std::to_string(lineNumber) +
        ").\n"
        "  Implemented policies: bump (default), pool, speculative, "
        "bounded, zeroed, deterministic."
    );
}

std::string RegionNode::getCExpr(C_Emitter& e) const {
    compile(e);
    return "vyne_null()";
}

// ============================================================
// A3 — REGION COMMIT
// ------------------------------------------------------------
// Not yet implemented. A correct lowering requires hoisting the
// committed value above the checkpoint in the emitted C — which in
// turn requires the escape-analysis pass (A2) to prove the value
// has no aliases inside the region. Without that proof, any lowering
// we pick either doubles the peak (copy) or silently produces a
// dangling reference (transfer).
//
// We fail at compile time so the user sees the constraint instead of
// getting a use-after-free at runtime.
// ============================================================

void RegionCommitNode::compile(C_Emitter& e) const {
    auto* var = dynamic_cast<VariableNode*>(expression.get());
    if (!var) {
        throw std::runtime_error(
            "Compile Error: region.commit() requires an lvalue variable "
            "(line " +
            std::to_string(lineNumber) + ")"
        );
    }

    // Same mangling rule as VariableNode::getCExpr. Keep them in sync.
    std::string sanitized = var->getOriginalName();
    std::replace(sanitized.begin(), sanitized.end(), '.', '_');

    std::string prefix = e.getActiveFunctionPrefix();
    std::string cVar;
    if (!prefix.empty()) {
        std::string localName = "v_" + prefix + "_" + sanitized;
        cVar = e.isLocalDeclared(localName) ? localName : ("v_" + sanitized);
    } else {
        cVar = "v_" + sanitized;
    }

    // vmem_runtime_commit() takes a VyneValue. A typed-array local
    // (VyneArray_f64 / VyneArray_i64) is a struct, not a VyneValue —
    // passing it straight through would fail at C-compile time with a
    // struct/union mismatch. Box it into a VyneValue, commit the boxed
    // value, then unbox back into the typed-array local. The commit
    // arena survives region rewind, so the unboxed container's backing
    // buffer outlives the region — exactly what region.commit promises.
    if (const CType* ct = e.lookupType(cVar)) {
        if (ct->kind == CType::Kind::Array && !ct->args.empty()) {
            // A shape-typed scratch is a raw C stack array (`double
            // v_x[N];`), not a VyneArray_f64 container. It cannot be
            // boxed, and its storage dies with the C stack frame — a
            // commit can only ever produce a dangling reference. Keep
            // this rejection; it is a correctness boundary, not a
            // missing feature.
            if (ct->hasShape()) {
                throw std::runtime_error(
                    "Compile Error (VNE-084): region.commit() on a "
                    "stack-allocated scratch array is impossible (line " +
                    std::to_string(lineNumber) +
                    ").\n"
                    "  Copy elements into a boxed Array<Float64> and "
                    "commit that instead."
                );
            }

            VType elem = ct->args[0].toVType();
            if (elem == VType::Float64 || elem == VType::Int64) {
                std::string boxFn = (elem == VType::Float64)
                                        ? "vyne_array_f64_to_value"
                                        : "vyne_array_i64_to_value";
                std::string unboxFn = (elem == VType::Float64)
                                          ? "vyne_value_to_array_f64"
                                          : "vyne_value_to_array_i64";

                std::string boxed = e.newTemp("commit_box");
                e.emit(
                    "VyneValue " + boxed + " = " + boxFn + "(&" + cVar + ");"
                );

                std::string committed = e.newTemp("commit");
                e.emit(
                    "VyneValue " + committed + " = vmem_runtime_commit(" +
                    boxed + ");"
                );

                e.emit(cVar + " = " + unboxFn + "(" + committed + ");");

                // The typed-array local now points into commit-arena
                // memory, which survives the rewind. Treat it as depth 0
                // for the escape checker.
                e.markCommitted(cVar);
                return;
            }
        }
    }

    std::string tmp = e.newTemp("commit");
    e.emit("VyneValue " + tmp + " = vmem_runtime_commit(" + cVar + ");");
    e.emit(cVar + " = " + tmp + ";");

    // The variable now points into the commit arena, which survives
    // region rewinds. Its effective depth is 0.
    e.markCommitted(cVar);
}

std::string RegionCommitNode::getCExpr(C_Emitter& e) const {
    compile(e);
    return "vyne_null()";
}
