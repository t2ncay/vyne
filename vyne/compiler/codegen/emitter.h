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

#ifndef VYNE_EMITTER_H
#define VYNE_EMITTER_H

#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <functional>

#include "ctype.h"
#include "native_maps.h"

// emitter.h is included from ast.h, so it cannot include ast.h back.
// The AD tape record only stores opaque AST pointers.
class ASTNode;

// ---------------------------------------------------------------------
// Target selection.
//
// Desktop emits a `main()` and links the full hosted runtime. AVR and
// ESP32 are embedded targets: they emit `vyne_setup`/`vyne_loop`, use a
// target-specific runtime header, and gate optional peripherals behind
// VYNE_USE_<FEATURE> defines.
// ---------------------------------------------------------------------
enum class Target { Desktop, AVR, ESP32 };

// Uppercase helper for feature-define emission. Kept file-scope and
// inline so including emitter.h from several translation units is safe.
static inline std::string vyne_upper(std::string s) {
    for (char& c : s)
        if (c >= 'a' && c <= 'z')
            c = (char)(c - 'a' + 'A');
    return s;
}

class C_Emitter {
    // --- Output buffers --------------------------------------------------
    // Plain strings, not stringstreams: emit() is the hottest codegen
    // path, and operator<< on a stringstream costs a locale/vtable call
    // per write while append/push_back is an inline memcpy.
    std::string globalsStream;
    std::string functionStream;
    std::string mainStream;
    std::unordered_set<std::string> includeSet;
    std::unordered_set<std::string> systemIncludes;

    int indentLevel = 0;
    enum class EmitContext { GLOBAL, FUNCTION, MAIN };
    std::vector<EmitContext> contextStack;

    std::string& currentStream() {
        if (contextStack.empty())
            return mainStream;
        switch (contextStack.back()) {
        case EmitContext::FUNCTION:
            return functionStream;
        case EmitContext::GLOBAL:
            return globalsStream;
        default:
            return mainStream;
        }
    }

    // Write indentation straight into the output buffer. The old
    // getIndent() allocated a fresh std::string for every emitted line.
    void writeIndent() {
        static const char spaces[] =
            "                                                                ";
        constexpr size_t chunkMax = sizeof(spaces) - 1;
        size_t n = static_cast<size_t>(indentLevel * 4);
        while (n > 0) {
            size_t chunk = n < chunkMax ? n : chunkMax;
            currentStream().append(spaces, chunk);
            n -= chunk;
        }
    }

    // --- Name declaration tables ----------------------------------------
    std::unordered_set<std::string> declaredVars;
    std::unordered_set<std::string> references;
    std::unordered_set<std::string> globalVars;

    // --- Static type tables (M0/M1) -------------------------------------
    std::unordered_map<std::string, CType> globalTypes;
    std::unordered_map<std::string, CType> nativeTemps;
    std::unordered_map<std::string, std::string> nativeModuleAliases;

    // Runtime dimensions for a scratch local, keyed by the mangled C
    // name. Populated by ScratchNode::compile when any dim is non-literal;
    // consulted by scratchFlatIndex to compute strides symbolically.
    // Cleared alongside the other per-function tables.
    std::unordered_map<std::string, std::vector<std::string>>
        scratchRuntimeDims;

    struct LocalScope {
        std::unordered_set<std::string> names;
        std::unordered_map<std::string, CType> types;
        std::unordered_map<std::string, int> regionDepthAtDeclaration;
    };
    std::vector<LocalScope> localScopes = {LocalScope{}};

    // --- Interface / group registry -------------------------------------
    std::unordered_set<std::string> interfaceSet;
    std::unordered_set<std::string> groupSet;
    std::unordered_map<std::string, std::string> interfaceCtorNames;
    std::unordered_map<std::string, std::vector<std::string>>
        functionSignatures;
    std::unordered_map<std::string, std::vector<CType>> functionParamTypes;
    std::unordered_map<std::string, CType> functionReturnTypes;
    std::unordered_map<std::string, std::string> nativeVariants;
    CType nativeReturnType;

    // Out-parameter names for a native variant whose declared return
    // type is a Struct. Empty for every other return kind. Populated by
    // emitNativeFunctionBody; read by ReturnNode::compile to write the
    // boxed result's fields into the out-params at return time.
    std::vector<std::string> structReturnOutParams;

    // Source-level return type of the function currently being emitted.
    // Set by FunctionNode::compile / compileAs before the body is lowered;
    // read by ReturnNode::compile to resolve `return [];` when the
    // declared type is `Array<T>`. Cleared on function exit. Distinct
    // from nativeReturnType_, which is only set for the native variant
    // and carries the ABI-level C representation.
    CType currentFunctionReturnType_;

    std::unordered_map<std::string, std::vector<std::string>> interfaceDefaults;
    std::string groupPrefix;

    // --- M4-C1B: struct field typing ------------------------------------
    std::unordered_map<std::string, std::unordered_map<std::string, VType>>
        interfaceArrayFields;
    std::unordered_map<std::string, std::string> localStructTypes;
    std::unordered_map<std::string, std::string> globalStructTypes;
    std::string currentInterfaceType;

    // M4-C1B (extended): scalar interface fields (Int64/Float64) that can be
    // unboxed at member access. Same key shape as interfaceArrayFields.
    std::unordered_map<std::string, std::unordered_map<std::string, VType>>
        interfacePrimitiveFields;

    // C2: native C struct representation.
    //
    // An interface is C-eligible when every field is Int64/Float64/Bool.
    // (Array-of-primitive and nested-eligible-struct fields are follow-up
    // slices.) Eligible interfaces get a real C typedef emitted at global
    // scope and can flow through Array<T> containers and native-ABI calls
    // as a pointer, not a boxed VyneValue.
    //
    // Keyed by every spelling of the interface name (bare, dotted,
    // underscore-mangled) so call sites can probe with whichever form
    // they hold. Never cleared by push/popFunctionContext — like
    // interfaceStructLayout, it is process-global.
    struct NativeCStruct {
        std::string tag;                     // C typedef name, e.g. "vyne_Atom"
        std::vector<std::string> fieldNames; // declaration order
        std::vector<CType> fieldTypes;       // parallel to fieldNames
    };
    std::unordered_map<std::string, NativeCStruct> nativeCStructs;

  public:
    // --- Layout-aware scratch (AoS / SoA) --------------------------------
    // A `scratch particles :: Particle[N];` declaration inside a
    // `@layout<soa>` region is lowered to one C array per field instead
    // of one array of structs. This registry is how member access later
    // finds the field arrays without re-resolving the interface.
    struct SoaField {
        std::string name;       // source field name
        std::string arrayName;  // emitted C array, e.g. v_particles_x
        CType type;             // field type (primitive only)
    };
    struct SoaScratch {
        std::string elemTypeName;         // source interface name
        std::string tag;                  // C typedef tag, e.g. vyne_Particle
        std::vector<int64_t> shape;       // static shape (empty if runtime)
        std::vector<SoaField> fields;     // declaration order
    };
    std::unordered_map<std::string, SoaScratch> soaScratch;

    void registerSoaScratch(const std::string& cName, SoaScratch s) {
        soaScratch[cName] = std::move(s);
    }
    const SoaScratch* getSoaScratch(const std::string& cName) const {
        auto it = soaScratch.find(cName);
        return it == soaScratch.end() ? nullptr : &it->second;
    }

  private:
    // Slice 3g: fixed-point C-eligibility set. An interface name lands
    // here once every field is a primitive scalar, a String, a Map, or
    // an Array<X> whose element X is itself in this set. Populated by
    // ProgramNode::compile before any statement is lowered.
    //
    // This is deliberately separate from `nativeCStructs`: the latter is
    // populated during InterfaceNode::compile and carries the tag and
    // field layout. Eligibility has to be known earlier, so the fixed
    // point can converge before any typedef is emitted.
    std::unordered_set<std::string> cEligibleInterfaces;

    // Ordered field layout for struct-typed native variants. Key is any
    // spelling under which the interface was registered (bare, dotted,
    // underscore-mangled). Value is the field list in declaration order.
    //
    // This is deliberately NOT cleared by push/popFunctionContext: it is
    // process-global, mirroring functionReturnTypes and functionParamTypes.
    // The callee's native signature is built from this map during
    // emitNativeFunctionBody; a later caller rebuilds the argument list
    // from the same map. If the map were per-function, the two sides
    // could disagree on ordering.
    std::unordered_map<std::string, std::vector<StructFieldDesc>>
        interfaceStructLayout;

    struct FieldCacheEntry {
        std::string temp;
        CType ct;
    };
    std::unordered_map<std::string, FieldCacheEntry> fieldCache;

    // --- M2: monomorphization cache -------------------------------------
    // key = "<mangledFnName>__<type-tuple>" ; value = emitted C name.
    std::unordered_map<std::string, std::string> instantiations;
    std::unordered_set<std::string> instantiationsInProgress;

    // --- Import / context tracking --------------------------------------
    std::unordered_set<std::string> importedFiles;
    std::string sourceDir;
    std::string activeFunctionPrefix;

    // --- Module scoping --------------------------------------------------
    // `activeModule` is set while emitting a function declared with a
    // `module M;` prefix. Call sites consult it to turn the bare name X
    // into the module-qualified C symbol fn_M_X when X is known to be
    // defined in M.
    //
    // `moduleFunctions[M]` and `moduleInterfaces[M]` are the registries
    // that answer "is X defined in M?" — populated as each function or
    // interface is compiled, and read at call sites.
    std::string activeModule;
    std::unordered_map<std::string, std::unordered_set<std::string>>
        moduleFunctions;
    std::unordered_map<std::string, std::unordered_set<std::string>>
        moduleInterfaces;

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
    // Layout mode of each open region ("aos" by default, "soa" for
    // `@layout<soa>`). Unlike regionStack this is compile-time-only: it is
    // pushed/popped by RegionNode::compile and read by ScratchNode.
    std::vector<std::string> layoutStack;
    // Parallel to regionStack: true for @streamed regions, which need a
    // stream_leave() in addition to the main-arena rewind on early exit.
    std::vector<bool> streamedRegionStack;
    // Innermost @streamed element types, used by the streamed_alloc intrinsic.
    std::vector<VType> streamedElemStack;
    // Parallel to regionStack: true for @persistent regions, which do not
    // rewind on exit and do not add to the escape-analysis region depth.
    std::vector<bool> persistentRegionStack;
    // Parallel to regionStack: true for @device regions, which close their
    // device frame before the host rewind.
    std::vector<bool> deviceRegionStack;
    int deviceRegionCount = 0;
    int deviceFunctionDepth = 0;
    std::unordered_set<std::string> deviceFunctionSet;
    std::unordered_set<std::string> committedVars;

    // Active @pool context. The pool's own C variable name lives here
    // so `pool_alloc()` / `pool_free()` — intercepted in
    // FunctionCallNode::getCExpr — can emit `&<handle>` without
    // recomputing the mangling. Only the innermost pool is visible to
    // the body; nested @pool regions shadow cleanly via LIFO.
    struct PoolContext {
        std::string handle;   // C variable name of the VynePool struct
        VType elemType;       // Float64 or Int64
        int64_t elemsPerSlot; // buffer length per allocation
    };
    std::vector<PoolContext> poolStack;

    // Active @slab context. Mirrors PoolContext: the C variable name of the
    // VyneSlab_f64/i64 struct so `slab_alloc(n)` / `slab_free(x)` — intercepted
    // in FunctionCallNode::getCExpr — can address it. Innermost slab wins; nested
    // @slab regions shadow cleanly via LIFO.
    struct SlabContext {
        std::string handle;  // C variable name of the VyneSlab struct
        VType elemType;      // Float64 or Int64
        int64_t maxElems;    // largest single allocation, in elements
        int64_t capacity;    // slots per size class
    };
    std::vector<SlabContext> slabStack;

    // Active @ring context. A ring is N fixed-size slots rotating in order;
    // `ring_alloc(n)` — intercepted in FunctionCallNode::getCExpr — addresses
    // the innermost ring. There is no free intrinsic: the slot is overwritten
    // after N allocations.
    struct RingContext {
        std::string handle;   // C variable name of the VyneRing struct
        VType elemType;       // Float64 or Int64
        int64_t slots;        // number of rotating slots
        int64_t slotElems;    // elements per slot
    };
    std::vector<RingContext> ringStack;

    // Active @speculative context. Tracks the C variable name of the
    // per-region commit flag so `region.commit_if(pred)` — intercepted
    // in RegionCommitIfNode::compile — knows which flag to assign. Only
    // the innermost is visible; nested speculative regions shadow via
    // LIFO, matching every other region-scoped construct.
    struct SpeculativeContext {
        std::string commitFlag; // C variable name of the int flag
    };
    std::vector<SpeculativeContext> speculativeStack;

    bool scratchBoundsEnabled = true;
    bool blasEnabled = false;

    // Project-level build policy from project.toml. Emitted as
    // VYNE_DETERMINISTIC_BUILD / VYNE_CUDA_SM_VERSION defines so the
    // runtime and generated code can specialize without the C emitter
    // knowing about the TOML schema.
    bool deterministicBuild_ = false;
    int cudaSmVersion_ = 0;

    bool poolRuntimeEmitted = false;
    bool slabRuntimeEmitted = false;
    bool ringRuntimeEmitted = false;
    int tempVarCount = 0;

    // Nesting depth of @zeroed / @deterministic regions currently open
    // in the function being emitted. Read by ScratchNode::compile to
    // decide between an uninitialized C array and a zeroed one. Cleared
    // per function; incremented / decremented by compileZeroedRegion.
    int zeroModeDepth = 0;

    // Per-open-@bounded-region context. A stack because nested @bounded
    // regions each carry their own bound. Every scratch declaration
    // inside a region is inside every enclosing @bounded, so bytes are
    // attributed to every context on the stack, and each context's
    // static accumulator is compared against its own bound independently.
    //
    //   bound       : N from @bounded<N>
    //   staticUsed  : compiler-side sum of static scratch bytes emitted
    //                 so far (for VNE-101 enforcement)
    //   budgetVar   : C variable name holding the runtime budget, e.g.
    //                 `__budget_bounded_0` — initialised to N at region
    //                 entry, decremented by static scratch during
    //                 emission. The exit check compares `arena_used >
    //                 budgetVar`, i.e. `arena_used + static_used > N`.
    //   startVar    : C variable holding `g_arena.total_allocated` at
    //                 region entry — read by the VLA runtime check to
    //                 compute arena usage at the declaration point.
    struct BoundedContext {
        std::string regionName;
        int64_t bound;
        int64_t staticUsed;
        std::string budgetVar;
        std::string startVar;
    };
    std::vector<BoundedContext> boundedStack;

    // Per-open-@realtime-region context. The region records `startVar`
    // on entry; the exit check and every early-return path call
    // `vyne_realtime_check(name, line, deadline, startVar)`.
    struct RealtimeContext {
        std::string regionName;
        int64_t deadlineMs;
        std::string startVar;
        int line;
    };
    std::vector<RealtimeContext> realtimeStack;

    // Postcondition emitter installed by the function body emitter while
    // that function's body is lowered. ReturnNode calls it before every
    // return, so ensures clauses run on early exits too. Stored as a
    // std::function so emitter.h does not need to know ASTNode.
    std::function<void(const std::string&)> ensuresEmitter;

    bool inNativeVariant = false;

    // Target selection. Only the entry-point emission and the runtime
    // header name depend on this; every AST lowering path stays identical.
    Target target_ = Target::Desktop;

    // Optional runtime features requested by the program. finalize()
    // turns each entry into a VYNE_USE_<UPPER> define so the embedded
    // entry header pulls only the peripheral headers the sketch actually
    // uses. Without this, every sketch would drag in the WiFi/display
    // stacks and their flash + static-heap cost.
    std::unordered_set<std::string> embeddedFeatures_;

  public:
    void setTarget(Target t) {
        target_ = t;
    }
    Target getTarget() const {
        return target_;
    }
    bool isAVRTarget() const {
        return target_ == Target::AVR;
    }
    bool isESP32Target() const {
        return target_ == Target::ESP32;
    }
    bool isEmbeddedTarget() const {
        return target_ != Target::Desktop;
    }

    // Deprecated compat shim: kept for one release to ease migration.
    // New code should call setTarget(Target::AVR) directly.
    void setArduinoTarget(bool v) {
        setTarget(v ? Target::AVR : Target::Desktop);
    }
    bool isArduinoTarget() const {
        return target_ == Target::AVR;
    }

    void noteEmbeddedFeature(const std::string& name) {
        embeddedFeatures_.insert(name);
    }
    bool hasEmbeddedFeature(const std::string& name) const {
        return embeddedFeatures_.count(name) > 0;
    }

    // Compat shims for the pre-generic call sites.
    void noteArduinoI2cUse() {
        noteEmbeddedFeature("i2c");
    }
    void noteArduinoSpiUse() {
        noteEmbeddedFeature("spi");
    }
    bool arduinoUsesI2c() const {
        return hasEmbeddedFeature("i2c");
    }
    bool arduinoUsesSpi() const {
        return hasEmbeddedFeature("spi");
    }
    void noteArduinoRealtimeUse() {
        noteEmbeddedFeature("realtime");
    }
    void noteArduinoContractsUse() {
        noteEmbeddedFeature("contracts");
    }
    bool arduinoUsesRealtime() const {
        return hasEmbeddedFeature("realtime");
    }
    bool arduinoUsesContracts() const {
        return hasEmbeddedFeature("contracts");
    }

    void setInNativeVariant(bool v) {
        inNativeVariant = v;
    }
    bool isInNativeVariant() const {
        return inNativeVariant;
    }

    // --- Phase B (AD): differentiable-function registry ----------------
    // Populated by the ProgramNode pre-pass so a `grad(...)` or
    // `backward(...)` call can resolve the companion even when the
    // definition appears later in the source file.
    struct ADInfo {
        std::string adjointName; // C name of `fn_<mangled>_grad`
        size_t arity = 0;
        std::vector<VType> paramTypes;
        bool custom = false; // registered by the `<fn>_vjp` convention
    };
    void registerDifferentiable(
        const std::string& name,
        std::string adjointName,
        size_t arity,
        std::vector<VType> paramTypes = {},
        bool custom = false
    ) {
        differentiable_[name] = ADInfo{
            std::move(adjointName),
            arity,
            std::move(paramTypes),
            custom
        };
    }
    const ADInfo* lookupDifferentiable(const std::string& name) const {
        auto it = differentiable_.find(name);
        return it == differentiable_.end() ? nullptr : &it->second;
    }

    // A function named `<fn>_vjp` registers itself as the custom adjoint
    // for `<fn>` by convention. This lets a function with tensor/array
    // locals opt out of the scalar AD subset and supply its own reverse
    // pass (for example a conv-net loss using vconv backward kernels).
    void registerCustomVJP(const std::string& base, std::string cName) {
        customVJPs_[base] = std::move(cName);
    }
    const std::string* lookupCustomVJP(const std::string& base) const {
        auto it = customVJPs_.find(base);
        return it == customVJPs_.end() ? nullptr : &it->second;
    }

    // --- Phase B (AD): `@backward` region scope ------------------------
    // Each open `@backward` region records the differentiable calls made
    // in its body, keyed by the variable that receives the primal result.
    // `backward(loss)` consumes that record to emit the adjoint call.
    struct BackwardCall {
        std::string callee;
        std::vector<const ASTNode*> args;
    };
    struct BackwardRegion {
        std::unordered_map<std::string, BackwardCall> calls;
    };
    void pushBackwardRegion() {
        backwardRegions_.emplace_back();
    }
    void popBackwardRegion() {
        if (!backwardRegions_.empty())
            backwardRegions_.pop_back();
    }
    bool inBackwardRegion() const {
        return !backwardRegions_.empty();
    }
    void recordBackwardCall(
        const std::string& binding,
        std::string callee,
        std::vector<const ASTNode*> args
    ) {
        if (backwardRegions_.empty())
            return;
        backwardRegions_.back().calls[binding] =
            BackwardCall{std::move(callee), std::move(args)};
    }
    const BackwardCall* lookupBackwardCall(const std::string& binding) const {
        if (backwardRegions_.empty())
            return nullptr;
        auto it = backwardRegions_.back().calls.find(binding);
        return it == backwardRegions_.back().calls.end() ? nullptr
                                                          : &it->second;
    }

    // --- Phase B (AD): `@implicit` region scope -------------------------
    // A `newton_solve(F, x..., z0)` call inside an `@implicit` region is
    // recorded here. `backward(z)` reads the record and emits the
    // Implicit Function Theorem adjoint instead of unrolling the solver.
    // The C temps hold the parameter values and the converged root, so the
    // reverse pass differentiates at the fixed point even if the source
    // variables changed after the solve.
    struct ImplicitSolve {
        const ASTNode* node = nullptr;
        std::string binding;
        std::string residualName;
        std::string adjointName;
        std::string nativeName;
        std::string stateCName;
        std::vector<std::string> paramVars;
        std::vector<std::string> paramCNames;
        int line = 0;
        bool filled = false;
    };
    struct ImplicitRegion {
        std::string name;
        int maxIters = 64;
        std::unordered_map<std::string, ImplicitSolve> solves;
    };
    void pushImplicitRegion(std::string name, int maxIters = 64) {
        ImplicitRegion r;
        r.name = std::move(name);
        r.maxIters = maxIters;
        implicitRegions_.push_back(std::move(r));
    }
    void popImplicitRegion() {
        if (!implicitRegions_.empty())
            implicitRegions_.pop_back();
    }
    bool inImplicitRegion() const {
        return !implicitRegions_.empty();
    }
    int currentImplicitMaxIters() const {
        return implicitRegions_.empty() ? 64
                                        : implicitRegions_.back().maxIters;
    }
    const std::string& currentImplicitRegionName() const {
        static const std::string kEmpty;
        return implicitRegions_.empty() ? kEmpty
                                        : implicitRegions_.back().name;
    }
    void beginImplicitSolve(
        const std::string& binding,
        const ASTNode* node
    ) {
        if (implicitRegions_.empty())
            return;
        auto& rec = implicitRegions_.back().solves[binding];
        rec = ImplicitSolve{};
        rec.node = node;
        rec.binding = binding;
    }
    ImplicitSolve* findImplicitSolveByNode(const ASTNode* node) {
        if (implicitRegions_.empty())
            return nullptr;
        for (auto& [key, rec] : implicitRegions_.back().solves) {
            (void)key;
            if (rec.node == node)
                return &rec;
        }
        return nullptr;
    }
    const ImplicitSolve* lookupImplicitSolve(const std::string& binding) const {
        if (implicitRegions_.empty())
            return nullptr;
        auto it = implicitRegions_.back().solves.find(binding);
        return it == implicitRegions_.back().solves.end() ? nullptr
                                                          : &it->second;
    }

    // `w.grad` reads the gradient slot that `backward(...)` populated.
    // Keyed by the source-level variable name, scoped per function
    // context (cleared by push/popFunctionContext).
    void registerGradientAccess(
        const std::string& varName,
        const std::string& cName
    ) {
        gradientAccess_[varName] = cName;
    }
    const std::string* lookupGradientAccess(const std::string& varName) const {
        auto it = gradientAccess_.find(varName);
        return it == gradientAccess_.end() ? nullptr : &it->second;
    }

    // Phase B (AD) state. `differentiable_` is program-global (survives
    // function boundaries); the other two are per-function.
    std::unordered_map<std::string, ADInfo> differentiable_;
    std::unordered_map<std::string, std::string> customVJPs_;
    std::vector<BackwardRegion> backwardRegions_;
    std::unordered_map<std::string, std::string> gradientAccess_;
    std::vector<ImplicitRegion> implicitRegions_;

    // --- Context management ---------------------------------------------
    bool isAlreadyImported(const std::string& path) const {
        return importedFiles.count(path) > 0;
    }
    void markImported(const std::string& path) {
        importedFiles.insert(path);
    }
    std::string getSourceDir() const {
        return sourceDir;
    }
    void setSourceDir(const std::string& dir) {
        sourceDir = dir;
    }

    const std::unordered_set<std::string>& getGlobalVars() const {
        return globalVars;
    }
    void enterFunction(const std::string& prefix) {
        activeFunctionPrefix = prefix;
    }
    void exitFunction() {
        activeFunctionPrefix.clear();
    }
    const std::string& getActiveFunctionPrefix() const {
        return activeFunctionPrefix;
    }

    // --- Module context ------------------------------------------------
    void setActiveModule(const std::string& m) {
        activeModule = m;
    }
    void clearActiveModule() {
        activeModule.clear();
    }
    const std::string& getActiveModule() const {
        return activeModule;
    }

    void
    registerModuleFunction(const std::string& mod, const std::string& name) {
        moduleFunctions[mod].insert(name);
    }
    bool
    isModuleFunction(const std::string& mod, const std::string& name) const {
        auto it = moduleFunctions.find(mod);
        return it != moduleFunctions.end() && it->second.count(name) > 0;
    }

    void
    registerModuleInterface(const std::string& mod, const std::string& name) {
        moduleInterfaces[mod].insert(name);
    }
    bool
    isModuleInterface(const std::string& mod, const std::string& name) const {
        auto it = moduleInterfaces.find(mod);
        return it != moduleInterfaces.end() && it->second.count(name) > 0;
    }

    void pushMainContext() {
        contextStack.emplace_back(EmitContext::MAIN);
        indentLevel = 1;
    }
    void popMainContext() {
        if (!contextStack.empty())
            contextStack.pop_back();
        indentLevel = 0;
    }

    void pushGlobalContext() {
        contextStack.emplace_back(EmitContext::GLOBAL);
        indentLevel = 0;
    }
    void popGlobalContext() {
        if (!contextStack.empty())
            contextStack.pop_back();
        indentLevel = 1;
    }

    bool isGlobalContext() {
        return contextStack.empty() ||
               contextStack.back() == EmitContext::GLOBAL;
    }

    bool isTopLevelOfMain() const {
        bool inMain =
            contextStack.empty() || contextStack.back() == EmitContext::MAIN;
        return inMain && localScopes.size() <= 1;
    }

    bool isGlobalDeclared(const std::string& name) const {
        return globalVars.count(name) > 0;
    }
    bool isLocalDeclared(const std::string& name) const {
        for (auto it = localScopes.rbegin(); it != localScopes.rend(); ++it)
            if (it->names.count(name))
                return true;
        return false;
    }
    bool isLocalVarsEmpty() const {
        return localScopes.empty() || localScopes.back().names.empty();
    }
    bool isAlreadyDeclared(const std::string& name) const {
        return isLocalDeclared(name) || isGlobalDeclared(name);
    }

    void registerDeclaration(const std::string& name) {
        bool atBaseScope = localScopes.size() <= 1;
        if (isGlobalContext() && atBaseScope) {
            globalVars.insert(name);
        } else {
            if (localScopes.empty())
                localScopes.emplace_back();
            localScopes.back().names.insert(name);
            // Record the region depth so the escape check can distinguish
            // a local declared inside a region from one declared outside
            // it. Without this, lookupLocalRegionDepth returns -1 for every
            // boxed local and the check treats it as a depth-0 escape.
            localScopes.back().regionDepthAtDeclaration[name] =
                regionEscapeDepth();
        }
    }

    // regionDepth defaults to the current region stack depth at the
    // moment of declaration. Callers that want to override (e.g. for
    // parameters, which are conceptually at depth 0) can pass an
    // explicit value.
    void declareLocal(
        const std::string& name,
        const CType& ct,
        int regionDepth = -1
    ) {
        if (localScopes.empty())
            localScopes.emplace_back();
        localScopes.back().names.insert(name);
        localScopes.back().types[name] = ct;
        if (regionDepth < 0)
            regionDepth = regionEscapeDepth();
        localScopes.back().regionDepthAtDeclaration[name] = regionDepth;
    }
    void declareGlobal(const std::string& name, const CType& ct) {
        globalVars.insert(name);
        globalTypes[name] = ct;
    }

    const CType* lookupLocalType(const std::string& name) const {
        for (auto it = localScopes.rbegin(); it != localScopes.rend(); ++it) {
            auto t = it->types.find(name);
            if (t != it->types.end())
                return &t->second;
        }
        return nullptr;
    }
    // Effective region depth of a variable: 0 if committed (survives
    // rewind), otherwise the depth at declaration. Returns -1 if the
    // name is not a tracked local.
    int lookupLocalRegionDepth(const std::string& name) const {
        if (committedVars.count(name))
            return 0;
        for (auto it = localScopes.rbegin(); it != localScopes.rend(); ++it) {
            auto r = it->regionDepthAtDeclaration.find(name);
            if (r != it->regionDepthAtDeclaration.end())
                return r->second;
        }
        return -1;
    }

    // Region depth used by the escape checker. @persistent regions do not
    // increase it: their allocations survive any enclosing rewind, so an
    // assignment out of a persistent region is not an escape.
    int regionEscapeDepth() const {
        int n = 0;
        for (size_t i = 0; i < regionStack.size(); ++i) {
            if (i >= persistentRegionStack.size() ||
                !persistentRegionStack[i])
                ++n;
        }
        return n;
    }

    int currentRegionDepth() const {
        return regionEscapeDepth();
    }

    void markCommitted(const std::string& name) {
        committedVars.insert(name);
    }
    bool isCommitted(const std::string& name) const {
        return committedVars.count(name) > 0;
    }
    const CType* lookupGlobalType(const std::string& name) const {
        auto it = globalTypes.find(name);
        return it == globalTypes.end() ? nullptr : &it->second;
    }

    const CType* lookupType(const std::string& name) const {
        auto it = nativeTemps.find(name);
        if (it != nativeTemps.end())
            return &it->second;
        if (auto lt = lookupLocalType(name))
            return lt;
        return lookupGlobalType(name);
    }

    const CType* lookupAnyType(const std::string& name) const {
        return lookupType(name);
    }

    void declareNativeTemp(const std::string& name, const CType& ct) {
        nativeTemps[name] = ct;
    }

    void registerScratchRuntimeDims(
        const std::string& cName,
        std::vector<std::string> dims
    ) {
        scratchRuntimeDims[cName] = std::move(dims);
    }
    const std::vector<std::string>*
    lookupScratchRuntimeDims(const std::string& cName) const {
        auto it = scratchRuntimeDims.find(cName);
        return it == scratchRuntimeDims.end() ? nullptr : &it->second;
    }

    // Filtered view: non-null only for provably-primitive types.
    // (Behaviorally identical to the old version — it also returned any
    // nativeTemp entry, primitive or not, thanks to the early return.
    // This preserves that.)
    const CType* exprNativeType(const std::string& expr) const {
        const CType* ct = lookupType(expr);
        return (ct && ct->isPrimitive()) ? ct : nullptr;
    }
    std::string boxIfNative(const std::string& expr) const {
        const CType* ct = exprNativeType(expr);
        return ct ? ct->box(expr) : expr;
    }
    std::string boxAny(const std::string& expr) const {
        const CType* ct = lookupType(expr);
        if (ct && ct->kind == CType::Kind::Slice) {
            throw std::runtime_error(
                "Compile Error: Slice<T> is a native-only borrowed view and "
                "cannot cross a boxed boundary. Pass it to a function whose "
                "parameter is Slice<T>, or copy into an Array<T> first "
                "(while lowering '" + expr + "')."
            );
        }
        if (ct && ct->kind == CType::Kind::Array && !ct->args.empty()) {
            const CType& el = ct->args[0];

            // Slice 3e: a struct-element array lives in a
            // `vyne_Array_vyne_*` container, not a boxed VyneValue.
            // Box it via the per-interface helper emitted alongside the
            // container typedef. The box is O(N) — one VyneValue per
            // element, built from the same flat fields the constructor
            // already takes — which is the correct cost for a struct
            // array crossing a dynamic boundary. The O(1) pointer-share
            // shortcut only exists for f64/i64 because their payload is
            // a single word; a struct is not.
            if (el.kind == CType::Kind::Struct && el.nativeCStruct) {
                const NativeCStruct* ns = getNativeCStruct(el.mangledName);
                if (ns) {
                    return "vyne_struct_array_box_" + ns->tag + "(" + expr +
                           ")";
                }
                // Registry entry missing despite nativeCStruct=true.
                // Fall through — boxIfNative will pass the struct
                // through unchanged, which produces a C type error at
                // the call site rather than a silent wrong answer.
            }

            VType elem = el.toVType();
            if (elem == VType::Float64)
                return "vyne_array_f64_to_value(&" + expr + ")";
            if (elem == VType::Float32)
                return "vyne_array_f32_to_value(&" + expr + ")";
            if (elem == VType::Int64)
                return "vyne_array_i64_to_value(&" + expr + ")";
        }
        // Slice 3a: a native C struct value re-boxes into a VyneValue
        // by calling the interface's boxed constructor with unboxed
        // field reads. The constructor takes VyneValue args, so each
        // field is wrapped with vyne_float / vyne_int / vyne_bool.
        // mangledName may carry dots (module path); the C ctor symbol
        // is underscore-mangled to match InterfaceNode::compile.
        if (ct && ct->kind == CType::Kind::Struct && ct->nativeCStruct &&
            !ct->mangledName.empty()) {
            const NativeCStruct* ns = getNativeCStruct(ct->mangledName);
            if (ns) {
                std::string ctor = "struct_" + ct->mangledName;
                std::replace(ctor.begin(), ctor.end(), '.', '_');
                std::string args;
                for (size_t i = 0; i < ns->fieldNames.size(); ++i) {
                    if (i)
                        args += ", ";
                    const std::string fld = expr + "." + ns->fieldNames[i];
                    CType ft = ns->fieldTypes[i];
                    if (ft.kind == CType::Kind::Float64) {
                        args += "vyne_float(" + fld + ")";
                    } else if (ft.kind == CType::Kind::Int64) {
                        args += "vyne_int(" + fld + ")";
                    } else if (ft.kind == CType::Kind::Bool) {
                        args += "vyne_bool(" + fld + ")";
                    } else if (
                        ft.kind == CType::Kind::Str ||
                        ft.kind == CType::Kind::Map
                    ) {
                        // Already a VyneValue in the native struct; pass
                        // it through unchanged.
                        args += fld;
                    } else if (
                        ft.kind == CType::Kind::Array && !ft.args.empty()
                    ) {
                        const CType& el = ft.args[0];
                        if (el.kind == CType::Kind::Struct &&
                            el.nativeCStruct && !el.nativeName.empty()) {
                            args += "vyne_struct_array_box_" + el.nativeName +
                                    "(" + fld + ")";
                        } else if (el.kind == CType::Kind::Float64) {
                            args += "vyne_array_f64_to_value(&" + fld + ")";
                        } else if (el.kind == CType::Kind::Int64) {
                            args += "vyne_array_i64_to_value(&" + fld + ")";
                        }
                    }
                }
                return ctor + "(" + args + ")";
            }
        }
        return boxIfNative(expr);
    }

    std::string nativeRead(const std::string& expr, VType kind) const {
        const CType* ct = exprNativeType(expr);
        CType want = CType::fromVType(kind);
        if (ct && ct->kind == want.kind)
            return expr;
        return want.unbox(boxIfNative(expr));
    }

    // --- References ------------------------------------------------------
    void registerReference(const std::string name) {
        references.insert(std::move(name));
    }
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
        layoutStack.clear();
        streamedRegionStack.clear();
        streamedElemStack.clear();
        persistentRegionStack.clear();
        deviceRegionStack.clear();
        deviceRegionCount = 0;
        deviceFunctionDepth = 0;
        poolStack.clear(); // paired with regionStack: both are per-function
        slabStack.clear(); // paired with regionStack: both are per-function
        ringStack.clear(); // paired with regionStack: both are per-function
        speculativeStack.clear();
        committedVars.clear();
        zeroModeDepth = 0;
        boundedStack.clear();
        nativeReturnType = CType{};
        currentFunctionReturnType_ = CType{};
        structReturnOutParams.clear();
        localScopes.clear();
        localScopes.emplace_back();
        scratchRuntimeDims.clear();
        backwardRegions_.clear();
        gradientAccess_.clear();
        implicitRegions_.clear();
    }
    void popFunctionContext() {
        if (!contextStack.empty())
            contextStack.pop_back();
        localScopes.clear();
        localScopes.emplace_back();
        scratchRuntimeDims.clear();
        localStructTypes.clear();
        fieldCache.clear();
        currentInterfaceType.clear();
        currentFunctionReturnType_ = CType{};
        zeroModeDepth = 0;
        boundedStack.clear();
        backwardRegions_.clear();
        gradientAccess_.clear();
        implicitRegions_.clear();
        indentLevel = 1;
    }
    void setFunctionContext(bool inside) {
        if (inside)
            pushFunctionContext();
        else
            popFunctionContext();
    }

    // --- Indentation / emission -----------------------------------------
    void indent() {
        indentLevel++;
    }
    void dedent() {
        if (indentLevel > 0)
            indentLevel--;
    }

    void emit(const std::string& code) {
        writeIndent();
        currentStream().append(code);
        currentStream().push_back('\n');
    }
    void emitGlobalDecl(const std::string& code) {
        globalsStream.append(code);
        globalsStream.push_back('\n');
    }

    void emitBlockOpen(const std::string& line) {
        localScopes.emplace_back();
        emit(line);
        indent();
    }
    void emitBlockClose(const std::string& suffix = "") {
        dedent();
        emit("}" + suffix);
        if (localScopes.size() > 1)
            localScopes.pop_back();
        fieldCache.clear();
    }

    std::string newTemp(const std::string& prefix = "t") {
        return prefix + "_" + std::to_string(tempVarCount++);
    }
    void addInclude(const std::string& header) {
        includeSet.insert(header);
    }
    void addSystemInclude(const std::string& header) {
        systemIncludes.insert(header);
    }

    void setBlasEnabled(bool v) {
        blasEnabled = v;
    }
    bool isBlasEnabled() const {
        return blasEnabled;
    }

    void setDeterministicBuild(bool v) {
        deterministicBuild_ = v;
    }
    void setCudaSmVersion(int v) {
        cudaSmVersion_ = v;
    }

    // --- M4-C1B: interface field typing ---------------------------------
    void registerInterfaceArrayField(
        const std::string& iface,
        const std::string& field,
        VType elem
    ) {
        if (elem == VType::Int64 || elem == VType::Float64)
            interfaceArrayFields[iface][field] = elem;
    }
    VType getInterfaceArrayElem(
        const std::string& iface,
        const std::string& field
    ) const {
        auto probe = [&](const std::string& k) -> VType {
            auto it = interfaceArrayFields.find(k);
            if (it == interfaceArrayFields.end())
                return VType::Unknown;
            auto f = it->second.find(field);
            return f == it->second.end() ? VType::Unknown : f->second;
        };
        VType v = probe(iface);
        if (v != VType::Unknown)
            return v;
        std::string tmp = iface;
        size_t dot;
        while ((dot = tmp.find('.')) != std::string::npos) {
            tmp = tmp.substr(dot + 1);
            v = probe(tmp);
            if (v != VType::Unknown)
                return v;
        }
        return VType::Unknown;
    }

    void registerInterfacePrimitiveField(
        const std::string& iface,
        const std::string& field,
        VType t
    ) {
        if (t == VType::Int64 || t == VType::Float64)
            interfacePrimitiveFields[iface][field] = t;
    }
    VType getInterfacePrimitiveField(
        const std::string& iface,
        const std::string& field
    ) const {
        auto probe = [&](const std::string& k) -> VType {
            auto it = interfacePrimitiveFields.find(k);
            if (it == interfacePrimitiveFields.end())
                return VType::Unknown;
            auto f = it->second.find(field);
            return f == it->second.end() ? VType::Unknown : f->second;
        };
        VType v = probe(iface);
        if (v != VType::Unknown)
            return v;
        std::string tmp = iface;
        size_t dot;
        while ((dot = tmp.find('.')) != std::string::npos) {
            tmp = tmp.substr(dot + 1);
            v = probe(tmp);
            if (v != VType::Unknown)
                return v;
        }
        return VType::Unknown;
    }

    // --- C2: native C struct representation ---------------------------
    // Register under the interface's fullName; the probe-with-suffix
    // accessors mirror getInterfaceArrayElem so a caller holding
    // "Types.Atom" still finds an interface registered as "Atom".
    void registerNativeCStruct(
        const std::string& ifaceName,
        std::string tag,
        std::vector<std::string> fieldNames,
        std::vector<CType> fieldTypes
    ) {
        NativeCStruct s;
        s.tag = std::move(tag);
        s.fieldNames = std::move(fieldNames);
        s.fieldTypes = std::move(fieldTypes);
        nativeCStructs[ifaceName] = std::move(s);
    }

    const NativeCStruct* getNativeCStruct(const std::string& ifaceName) const {
        auto probe = [&](const std::string& k) -> const NativeCStruct* {
            auto it = nativeCStructs.find(k);
            return it == nativeCStructs.end() ? nullptr : &it->second;
        };
        if (const auto* r = probe(ifaceName))
            return r;
        std::string tmp = ifaceName;
        size_t dot;
        while ((dot = tmp.find('.')) != std::string::npos) {
            tmp = tmp.substr(dot + 1);
            if (const auto* r = probe(tmp))
                return r;
        }
        return nullptr;
    }

    bool hasNativeCStruct(const std::string& ifaceName) const {
        return getNativeCStruct(ifaceName) != nullptr;
    }

    // --- Slice 3g: C-eligibility set ---------------------------------
    void markCEligible(const std::string& ifaceName) {
        cEligibleInterfaces.insert(ifaceName);
    }

    // Probe with the same dot-suffix walk every other accessor uses, so
    // a caller holding "vchem.Molecule" finds an interface registered
    // as either "vchem.Molecule" or "Molecule".
    bool isCEligible(const std::string& ifaceName) const {
        auto probe = [&](const std::string& k) {
            return cEligibleInterfaces.count(k) > 0;
        };
        if (probe(ifaceName))
            return true;
        std::string tmp = ifaceName;
        size_t dot;
        while ((dot = tmp.find('.')) != std::string::npos) {
            tmp = tmp.substr(dot + 1);
            if (probe(tmp))
                return true;
        }
        return false;
    }

    // --- Struct-typed native ABI (Stage 1) ----------------------------
    // Register the ordered field list for an interface under every
    // spelling the caller might query with. Mirrors registerInterfaceArrayField
    // and registerInterfacePrimitiveField's multi-spelling registration.
    void registerInterfaceStructLayout(
        const std::string& iface,
        std::vector<StructFieldDesc> fields
    ) {
        interfaceStructLayout[iface] = std::move(fields);
    }

    // Probe-with-dot-suffix, matching getInterfaceArrayElem. Returns
    // nullptr if nothing matches, so the caller can fall back to the
    // boxed path rather than throw.
    const std::vector<StructFieldDesc>*
    getInterfaceStructLayout(const std::string& iface) const {
        auto probe =
            [&](const std::string& k) -> const std::vector<StructFieldDesc>* {
            auto it = interfaceStructLayout.find(k);
            return it == interfaceStructLayout.end() ? nullptr : &it->second;
        };
        if (const auto* r = probe(iface))
            return r;
        std::string tmp = iface;
        size_t dot;
        while ((dot = tmp.find('.')) != std::string::npos) {
            tmp = tmp.substr(dot + 1);
            if (const auto* r = probe(tmp))
                return r;
        }
        return nullptr;
    }

    bool hasInterfaceStructLayout(const std::string& iface) const {
        return getInterfaceStructLayout(iface) != nullptr;
    }

    void setLocalStructType(const std::string& var, const std::string& t) {
        localStructTypes[var] = t;
    }

    // Resolve a Vyne variable's interface name from its *original* source
    // name (not the mangled C name). Returns "" if the variable is not a
    // tracked struct-typed local or global.
    //
    // Takes a `std::string` deliberately: emitter.h is included from
    // ast.h, so this header cannot reference ASTNode / NodeType / any
    // AST class. Callers that hold an AST node must first extract the
    // variable's original name and pass it in. See
    // `native_dispatch.cpp::nodeInterfaceName` for that unwrapping step.
    std::string
    lookupStructInterfaceName(const std::string& originalName) const {
        if (originalName.empty())
            return "";
        std::string prefix = getActiveFunctionPrefix();
        std::string lookupKey = prefix.empty()
                                    ? ("v_" + originalName)
                                    : ("v_" + prefix + "_" + originalName);
        if (const std::string* t = lookupLocalStructType(lookupKey))
            return *t;
        if (const std::string* t = lookupGlobalStructType("v_" + originalName))
            return *t;
        return "";
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

    void setCurrentInterfaceType(const std::string& t) {
        currentInterfaceType = t;
    }
    const std::string& getCurrentInterfaceType() const {
        return currentInterfaceType;
    }

    const FieldCacheEntry* getFieldCache(const std::string& key) const {
        auto it = fieldCache.find(key);
        return it == fieldCache.end() ? nullptr : &it->second;
    }
    void setFieldCache(
        const std::string& key,
        const std::string& temp,
        const CType& ct
    ) {
        fieldCache[key] = {temp, ct};
    }
    void clearFieldCache() {
        fieldCache.clear();
    }

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
        if (instantiations.count(key))
            return false;
        if (instantiationsInProgress.count(key))
            return false;
        instantiationsInProgress.insert(key);
        return true;
    }
    void finishInstantiation(const std::string& key, const std::string& cName) {
        instantiationsInProgress.erase(key);
        instantiations[key] = cName;
    }

    // --- Native module lookup -------------------------------------------
    std::string resolveNativeModule(const std::string& name) const {
        auto found = nativeModuleAliases.find(name);
        return found == nativeModuleAliases.end() ? name : found->second;
    }
    void registerNativeModuleAlias(const std::string& alias, const std::string& module) {
        if (!alias.empty() && alias != module) nativeModuleAliases[alias] = resolveNativeModule(module);
    }
    const NativeMapEntry*
    findNative(const std::string& name, const std::string& member) const {
        const std::string module = resolveNativeModule(name);
        const NativeMapEntry* table = nullptr;
        size_t count = 0;

        if (module == "vcore") {
            table = VCORE_MAP;
            count = sizeof(VCORE_MAP) / sizeof(VCORE_MAP[0]);
        } else if (module == "vmath") {
            table = VMATH_MAP;
            count = sizeof(VMATH_MAP) / sizeof(VMATH_MAP[0]);
        } else if (module == "vmem") {
            table = VMEM_MAP;
            count = sizeof(VMEM_MAP) / sizeof(VMEM_MAP[0]);
        } else if (module == "vfs") {
            table = VFS_MAP;
            count = sizeof(VFS_MAP) / sizeof(VFS_MAP[0]);
        } else if (module == "vimage") {
            table = VIMAGE_MAP;
            count = sizeof(VIMAGE_MAP) / sizeof(VIMAGE_MAP[0]);
        } else if (module == "vraylib") {
            if (isEmbeddedTarget()) return nullptr;
            table = VRAYLIB_MAP;
            count = sizeof(VRAYLIB_MAP) / sizeof(VRAYLIB_MAP[0]);
        } else if (module == "vplot") {
            if (isAVRTarget()) return nullptr;
            if (isESP32Target() && (member.rfind("image_", 0) == 0 || member.rfind("save",0)==0 || member == "pixel_rgb")) return nullptr;
            if (!isESP32Target() && (member == "display_surface" || member.rfind("show_",0)==0)) return nullptr;
            table = VPLOT_MAP;
            count = sizeof(VPLOT_MAP) / sizeof(VPLOT_MAP[0]);
        } else if (module == "vconv") {
            table = VCONV_MAP;
            count = sizeof(VCONV_MAP) / sizeof(VCONV_MAP[0]);
        } else if (module == "arduino") {
            if (isAVRTarget()) {
                table = ARDUINO_MAP;
                count = sizeof(ARDUINO_MAP) / sizeof(ARDUINO_MAP[0]);
            } else if (isESP32Target()) {
                table = ESP32_ARDUINO_MAP;
                count =
                    sizeof(ESP32_ARDUINO_MAP) / sizeof(ESP32_ARDUINO_MAP[0]);
            } else {
                return nullptr;
            }
        } else if (module == "esp32") {
            if (!isESP32Target())
                return nullptr;
            // `esp32.*` exposes the shared surface first, then the
            // ESP32-only names. A name that exists on both (e.g. `delay`)
            // resolves to the shared `arduino_*` C symbol.
            for (size_t i = 0;
                 i < sizeof(ESP32_ARDUINO_MAP) / sizeof(ESP32_ARDUINO_MAP[0]);
                 ++i)
                if (member == ESP32_ARDUINO_MAP[i].vyneName)
                    return &ESP32_ARDUINO_MAP[i];
            table = ESP32_SPECIFIC_MAP;
            count =
                sizeof(ESP32_SPECIFIC_MAP) / sizeof(ESP32_SPECIFIC_MAP[0]);
        } else
            return nullptr;

        for (size_t i = 0; i < count; ++i)
            if (member == table[i].vyneName)
                return &table[i];
        return nullptr;
    }
    std::string getNativeMapping(
        const std::string& module,
        const std::string& member,
        bool asFunctionCall
    ) {
        const NativeMapEntry* e = findNative(module, member);
        if (!e)
            return "v_" + module + "_" + member;
        if (e->isProperty)
            return e->cName;
        return asFunctionCall ? e->cName : std::string(e->cName) + "()";
    }

    // --- Interface / group registry -------------------------------------
    void registerInterface(const std::string& name) {
        interfaceSet.insert(name);
    }
    void
    registerInterfaceCtor(const std::string& name, const std::string& suffix) {
        interfaceCtorNames[name] = suffix;
    }
    const std::string* getInterfaceCtor(const std::string& name) const {
        auto it = interfaceCtorNames.find(name);
        return it == interfaceCtorNames.end() ? nullptr : &it->second;
    }
    void registerGroup(const std::string& name) {
        groupSet.insert(name);
    }
    bool isInterface(const std::string& name) const {
        return interfaceSet.count(name) > 0;
    }
    bool isGroup(const std::string& name) const {
        return groupSet.count(name) > 0;
    }
    void registerFunctionSignature(
        const std::string& name,
        std::vector<std::string> params
    ) {
        functionSignatures[name] = std::move(params);
    }
    void registerFunctionParamTypes(
        const std::string& name,
        std::vector<CType> types
    ) {
        functionParamTypes[name] = std::move(types);
    }
    void registerFunctionReturnType(const std::string& name, const CType& ct) {
        functionReturnTypes[name] = ct;
    }
    const CType* getFunctionReturnType(const std::string& name) const {
        auto it = functionReturnTypes.find(name);
        return it == functionReturnTypes.end() ? nullptr : &it->second;
    }

    // Read-only view of the whole table. Used by the region escape
    // check as a last-resort fuzzy lookup, and by the diagnostic
    // dump when the check fires. Do not mutate through this accessor.
    const std::unordered_map<std::string, CType>&
    getAllFunctionReturnTypes() const {
        return functionReturnTypes;
    }
    const std::vector<CType>*
    getFunctionParamTypes(const std::string& name) const {
        auto it = functionParamTypes.find(name);
        return it == functionParamTypes.end() ? nullptr : &it->second;
    }

    void registerNativeVariant(
        const std::string& boxedMangled,
        const std::string& nativeMangled
    ) {
        nativeVariants[boxedMangled] = nativeMangled;
    }
    const std::string*
    lookupNativeVariant(const std::string& boxedMangled) const {
        auto it = nativeVariants.find(boxedMangled);
        return it == nativeVariants.end() ? nullptr : &it->second;
    }

    void setNativeReturnType(const CType& ct) {
        nativeReturnType = ct;
    }
    void clearNativeReturnType() {
        nativeReturnType = CType{};
    }
    const CType& getNativeReturnType() const {
        return nativeReturnType;
    }

    void setCurrentFunctionReturnType(const CType& ct) {
        currentFunctionReturnType_ = ct;
    }
    void clearCurrentFunctionReturnType() {
        currentFunctionReturnType_ = CType{};
    }
    const CType& getCurrentFunctionReturnType() const {
        return currentFunctionReturnType_;
    }

    void setStructReturnOutParams(std::vector<std::string> names) {
        structReturnOutParams = std::move(names);
    }
    const std::vector<std::string>& getStructReturnOutParams() const {
        return structReturnOutParams;
    }
    const std::vector<std::string>*
    getFunctionSignature(const std::string& name) const {
        auto it = functionSignatures.find(name);
        return it == functionSignatures.end() ? nullptr : &it->second;
    }

    void setGroupPrefix(const std::string& p) {
        groupPrefix = p;
    }
    void clearGroupPrefix() {
        groupPrefix.clear();
    }
    const std::string& getGroupPrefix() const {
        return groupPrefix;
    }

    void setScratchBoundsEnabled(bool v) {
        scratchBoundsEnabled = v;
    }
    bool isScratchBoundsEnabled() const {
        return scratchBoundsEnabled;
    }

    void registerInterfaceDefaults(
        const std::string& name,
        std::vector<std::string> defaults
    ) {
        interfaceDefaults[name] = std::move(defaults);
    }
    const std::vector<std::string>*
    getInterfaceDefaults(const std::string& name) const {
        auto it = interfaceDefaults.find(name);
        return it == interfaceDefaults.end() ? nullptr : &it->second;
    }

    // --- Try / defer context --------------------------------------------
    void pushTryCleanup(const std::string& label) {
        tryCleanupStack.push_back(label);
    }
    void popTryCleanup() {
        if (!tryCleanupStack.empty())
            tryCleanupStack.pop_back();
    }
    bool hasTryCleanup() const {
        return !tryCleanupStack.empty();
    }
    const std::string& currentTryCleanup() const {
        return tryCleanupStack.back();
    }

    // --- Region emitters --------------------------------------------
    void pushRegion(const std::string& cpHandle) {
        regionStack.push_back(cpHandle);
        streamedRegionStack.push_back(false);
        persistentRegionStack.push_back(false);
        deviceRegionStack.push_back(false);
    }
    // A @streamed region has both a main-arena checkpoint (for ordinary
    // allocations and the region lifetime) and a stream-arena frame (for
    // pinned streamed_alloc buffers). The checkpoint handle is stored so the
    // existing rewind path still works; the streamed flag drives the extra
    // stream_leave() on close and on early exit.
    void pushStreamedRegion(const std::string& cpHandle, VType elem) {
        regionStack.push_back(cpHandle);
        streamedRegionStack.push_back(true);
        persistentRegionStack.push_back(false);
        deviceRegionStack.push_back(false);
        streamedElemStack.push_back(elem);
    }
    // A @persistent region swaps the allocator to the commit arena and never
    // rewinds it. The checkpoint handle is used only to keep the vmem stack
    // balanced; the persistent flag drives persistent_leave() and a checkpoint
    // pop instead of a rewind.
    void pushPersistentRegion(const std::string& cpHandle) {
        regionStack.push_back(cpHandle);
        streamedRegionStack.push_back(false);
        persistentRegionStack.push_back(true);
        deviceRegionStack.push_back(false);
    }
    // A @device region is also a normal host region; the device frame is
    // closed before the host rewind on every exit path.
    void pushDeviceRegion(const std::string& cpHandle) {
        regionStack.push_back(cpHandle);
        streamedRegionStack.push_back(false);
        persistentRegionStack.push_back(false);
        deviceRegionStack.push_back(true);
        deviceRegionCount++;
    }
    void popRegion() {
        if (!regionStack.empty()) {
            if (streamedRegionStack.back())
                streamedElemStack.pop_back();
            if (deviceRegionStack.back())
                deviceRegionCount--;
            deviceRegionStack.pop_back();
            persistentRegionStack.pop_back();
            streamedRegionStack.pop_back();
            regionStack.pop_back();
        }
    }

    // --- Layout context ----------------------------------------------
    // Compile-time stack parallel to the lexical region nesting. Scratch
    // declarations consult the innermost open layout. @layout<soa> is the
    // only non-default mode today; everything else is "aos".
    void pushLayout(const std::string& mode) {
        layoutStack.push_back(mode);
    }
    void popLayout() {
        if (!layoutStack.empty())
            layoutStack.pop_back();
    }
    const std::string& currentLayout() const {
        static const std::string aos = "aos";
        return layoutStack.empty() ? aos : layoutStack.back();
    }

    bool hasRegion() const {
        return !regionStack.empty();
    }
    bool hasStreamed() const {
        return !streamedElemStack.empty();
    }
    VType currentStreamedElem() const {
        return streamedElemStack.back();
    }
    bool hasDevice() const {
        return deviceRegionCount > 0;
    }
    void pushDeviceFunction() {
        deviceFunctionDepth++;
    }
    void popDeviceFunction() {
        if (deviceFunctionDepth > 0)
            deviceFunctionDepth--;
    }
    bool inDeviceFunction() const {
        return deviceFunctionDepth > 0;
    }
    bool hasDeviceContext() const {
        return hasDevice() || inDeviceFunction();
    }
    void registerDeviceFunction(const std::string& name) {
        deviceFunctionSet.insert(name);
    }
    bool isDeviceFunction(const std::string& name) const {
        return deviceFunctionSet.count(name) > 0;
    }
    const std::vector<std::string>& getRegionStack() const {
        return regionStack;
    }

    // @zeroed / @deterministic nesting depth. Incremented on entry,
    // decremented on exit. ScratchNode::compile consults hasZeroMode()
    // to decide between an uninitialised C array and a zeroed one.
    void pushZeroMode() {
        zeroModeDepth++;
    }
    void popZeroMode() {
        if (zeroModeDepth > 0)
            zeroModeDepth--;
    }
    bool hasZeroMode() const {
        return zeroModeDepth > 0;
    }

    void pushBounded(
        const std::string& regionName,
        int64_t bound,
        const std::string& budgetVar,
        const std::string& startVar
    ) {
        boundedStack.push_back({regionName, bound, 0, budgetVar, startVar});
    }
    void popBounded() {
        if (!boundedStack.empty())
            boundedStack.pop_back();
    }
    bool hasBounded() const {
        return !boundedStack.empty();
    }
    const std::vector<BoundedContext>& getBoundedStack() const {
        return boundedStack;
    }

    // --- @realtime context -------------------------------------------
    void pushRealtime(
        const std::string& regionName,
        int64_t deadlineMs,
        const std::string& startVar,
        int line
    ) {
        realtimeStack.push_back({regionName, deadlineMs, startVar, line});
    }
    void popRealtime() {
        if (!realtimeStack.empty())
            realtimeStack.pop_back();
    }
    bool hasRealtime() const {
        return !realtimeStack.empty();
    }
    // Emit the deadline check for every open @realtime region, innermost
    // first. Called by ReturnNode before an early exit and by the region
    // itself at normal close.
    void emitRealtimeChecks() {
        for (size_t i = realtimeStack.size(); i-- > 0;) {
            const auto& ctx = realtimeStack[i];
            emit(
                "vyne_realtime_check(\"" + ctx.regionName + "\", " +
                std::to_string(ctx.line) + ", " +
                std::to_string(ctx.deadlineMs) + ", " + ctx.startVar + ");"
            );
        }
    }

    // --- ensures/postcondition emitter --------------------------------
    void setEnsuresEmitter(std::function<void(const std::string&)> fn) {
        ensuresEmitter = std::move(fn);
    }
    void clearEnsuresEmitter() {
        ensuresEmitter = nullptr;
    }
    bool hasEnsuresEmitter() const {
        return static_cast<bool>(ensuresEmitter);
    }
    void emitEnsuresForResult(const std::string& resultExpr) {
        if (ensuresEmitter)
            ensuresEmitter(resultExpr);
    }

    // Record `bytes` of static-shape scratch allocated inside every
    // currently open @bounded region. Fires VNE-101 as soon as any
    // context's cumulative static-used exceeds its bound, and emits
    // a `__budget_<ctx> -= bytes;` decrement per context so the runtime
    // exit check sees the reduced budget.
    void applyBoundedStaticScratch(int64_t bytes, int line) {
        for (auto& ctx : boundedStack) {
            ctx.staticUsed += bytes;
            if (ctx.staticUsed > ctx.bound) {
                throw std::runtime_error(
                    "Compile Error (VNE-101): @bounded<" +
                    std::to_string(ctx.bound) + "> on region '" +
                    ctx.regionName + "' exceeded by static scratch: " +
                    std::to_string(ctx.staticUsed) + " bytes used by line " +
                    std::to_string(line) +
                    ".\n"
                    "  @bounded counts scratch (stack) plus arena (heap). "
                    "Reduce the scratch size, split the region, or raise "
                    "the bound."
                );
            }
            emit(ctx.budgetVar + " -= " + std::to_string(bytes) + ";");
        }
    }

    // Emit rewind calls for every live region, innermost first.
    // Used by BreakNode / ContinueNode / ReturnNode before the transfer.
    void emitRegionUnwind() {
        for (size_t i = regionStack.size(); i-- > 0;) {
            if (i < streamedRegionStack.size() && streamedRegionStack[i]) {
                emit("vyne_stream_leave();");
                emit("vmem_runtime_rewind(" + regionStack[i] + ");");
            } else if (
                i < persistentRegionStack.size() && persistentRegionStack[i]
            ) {
                emit("vyne_persistent_leave();");
                emit("vmem_runtime_pop_checkpoints(1);");
            } else if (i < deviceRegionStack.size() && deviceRegionStack[i]) {
                emit("vyne_device_leave();");
                emit("vmem_runtime_rewind(" + regionStack[i] + ");");
            } else {
                emit("vmem_runtime_rewind(" + regionStack[i] + ");");
            }
        }
    }

    // Close only the @streamed frames, innermost first, without touching the
    // main arena. Used by early-return paths that pop checkpoints instead of
    // rewinding (native primitive/struct returns, non-primitive returns that
    // must keep arena values alive).
    void emitStreamedUnwind() {
        for (size_t i = regionStack.size(); i-- > 0;) {
            if (i < streamedRegionStack.size() && streamedRegionStack[i])
                emit("vyne_stream_leave();");
        }
    }

    // Close only the @persistent frames, innermost first. The outermost leave
    // restores the enclosing arena; nested leaves only decrement the depth.
    void emitPersistentUnwind() {
        for (size_t i = regionStack.size(); i-- > 0;) {
            if (i < persistentRegionStack.size() && persistentRegionStack[i])
                emit("vyne_persistent_leave();");
        }
    }

    // Close only the @device frames, innermost first. Used by early-return
    // paths that pop checkpoints instead of rewinding the host arena.
    void emitDeviceUnwind() {
        for (size_t i = regionStack.size(); i-- > 0;) {
            if (i < deviceRegionStack.size() && deviceRegionStack[i])
                emit("vyne_device_leave();");
        }
    }

    // --- Pool context ------------------------------------------------
    // The pool is purely a lookup layer over regionStack. A @pool region
    // checkpoints on entry and rewinds on exit exactly like a bump
    // region; the pool struct's arena chunk sits above the checkpoint
    // so the rewind reclaims it. Early returns/breaks/continues rewind
    // through the same emitRegionUnwind() path as bump regions.
    void pushPool(const PoolContext& p) {
        poolStack.push_back(p);
    }
    void popPool() {
        if (!poolStack.empty())
            poolStack.pop_back();
    }
    bool hasPool() const {
        return !poolStack.empty();
    }
    const PoolContext& currentPool() const {
        return poolStack.back();
    }

    // Idempotent include emit for vyne_pool_runtime.h — set on the
    // first @pool region, no-op thereafter.
    bool isPoolRuntimeEmitted() const {
        return poolRuntimeEmitted;
    }
    void markPoolRuntimeEmitted() {
        poolRuntimeEmitted = true;
    }

    // --- Slab context ------------------------------------------------
    // Like @pool, @slab is a lookup layer over regionStack: the slab
    // checkpoints on entry and rewinds on exit, reclaiming the slab struct's
    // backing store along with the body's allocations.
    void pushSlab(const SlabContext& s) {
        slabStack.push_back(s);
    }
    void popSlab() {
        if (!slabStack.empty())
            slabStack.pop_back();
    }
    bool hasSlab() const {
        return !slabStack.empty();
    }
    const SlabContext& currentSlab() const {
        return slabStack.back();
    }

    // Idempotent include emit for vyne_slab_runtime.h.
    bool isSlabRuntimeEmitted() const {
        return slabRuntimeEmitted;
    }
    void markSlabRuntimeEmitted() {
        slabRuntimeEmitted = true;
    }

    // --- Ring context ------------------------------------------------
    // Like @slab, @ring is a lookup layer over regionStack: the ring is
    // created after the checkpoint and its backing store is reclaimed by the
    // region rewind.
    void pushRing(const RingContext& r) {
        ringStack.push_back(r);
    }
    void popRing() {
        if (!ringStack.empty())
            ringStack.pop_back();
    }
    bool hasRing() const {
        return !ringStack.empty();
    }
    const RingContext& currentRing() const {
        return ringStack.back();
    }

    // Idempotent include emit for vyne_ring_runtime.h.
    bool isRingRuntimeEmitted() const {
        return ringRuntimeEmitted;
    }
    void markRingRuntimeEmitted() {
        ringRuntimeEmitted = true;
    }

    // --- Speculative context -----------------------------------------
    // Pushed by compileSpeculativeRegion, read by RegionCommitIfNode.
    // A speculative region still goes on regionStack; the speculative
    // stack only carries the commit flag's C name.
    void pushSpeculative(const SpeculativeContext& s) {
        speculativeStack.push_back(s);
    }
    void popSpeculative() {
        if (!speculativeStack.empty())
            speculativeStack.pop_back();
    }
    bool hasSpeculative() const {
        return !speculativeStack.empty();
    }
    const SpeculativeContext& currentSpeculative() const {
        return speculativeStack.back();
    }

    void setReturnVars(const std::string& rv, const std::string& rf) {
        currentReturnVar = rv;
        currentReturningVar = rf;
    }
    void clearReturnVars() {
        currentReturnVar.clear();
        currentReturningVar.clear();
    }
    bool hasReturnVars() const {
        return !currentReturnVar.empty();
    }
    const std::string& getReturnVar() const {
        return currentReturnVar;
    }
    const std::string& getReturningVar() const {
        return currentReturningVar;
    }

    void pushDeferContext(const std::string& label, const std::string& retVar) {
        deferCtx = {label, retVar, true};
    }
    void popDeferContext() {
        deferCtx = {"", "", false};
    }
    bool hasDeferContext() const {
        return deferCtx.active;
    }
    const std::string& getDeferCleanupLabel() const {
        return deferCtx.cleanupLabel;
    }
    const std::string& getDeferRetVar() const {
        return deferCtx.retVar;
    }

    // --- Output assembly -------------------------------------------------
    std::string finalize(const std::string& runtimeHeader = "vyne_runtime.h") {
        std::stringstream out;

        // Project build policy (empty for plain flag-based invocations).
        if (deterministicBuild_)
            out << "#define VYNE_DETERMINISTIC_BUILD 1\n";
        if (cudaSmVersion_ > 0)
            out << "#define VYNE_CUDA_SM_VERSION " << cudaSmVersion_ << "\n";
        if (deterministicBuild_ || cudaSmVersion_ > 0)
            out << "\n";

        if (isEmbeddedTarget()) {
            out << "#include <Arduino.h>\n";
            // Peripheral headers are opt-in: a sketch that never calls
            // arduino.i2c_* / esp32.wifi_* must not link the Wire, SPI,
            // WiFi, or display stacks. The defines below are set by the
            // native-call dispatcher (method_calls.cpp) while lowering
            // the body.
            for (const auto& f : embeddedFeatures_)
                out << "#define VYNE_USE_" << vyne_upper(f) << " 1\n";
            if (!embeddedFeatures_.empty())
                out << "\n";
            // Embedded targets use their own MCU runtime, not the desktop
            // one. The caller (file_handler) is responsible for placing
            // the runtime directory on the include path.
            if (isAVRTarget())
                out << "#include \"vyne_arduino.h\"\n";
            else
                out << "#include \"vyne_esp32.h\"\n";
        } else {
            out << "#include \"" << runtimeHeader << "\"\n";
            for (const auto& inc : includeSet)
                out << "#include \"" << inc << "\"\n";
            for (const auto& inc : systemIncludes)
                out << "#include <" << inc << ">\n";
        }
        out << "\n";

        const std::string& globals = globalsStream;
        if (!globals.empty())
            out << "// --- Globals ---\n" << globals << "\n";

        const std::string& funcs = functionStream;
        if (!funcs.empty())
            out << "// --- Functions ---\n" << funcs << "\n";

        if (isEmbeddedTarget()) {
            // User code that runs on the board lives in `setup()` and
            // `loop()`. The emitter keeps mainStream single-stream, so
            // we split it at the first top-level `region` / `through`
            // — user-visible convention: everything before the loop is
            // setup, everything from the loop onward is loop body.
            //
            // Today the emitter cannot make that split without AST
            // inspection, so this emits the whole main stream as
            // `vyne_setup()` and an empty `vyne_loop()`. The names are
            // deliberately NOT `setup()`/`loop()`: `arduino/vyne_arduino.h`
            // already defines those and calls into `vyne_setup`/`vyne_loop`
            // via the extern declarations at the top of that header.
            // Emitting `setup()` here would be a hard redefinition at link
            // time. The next commit adds a `splitArduinoMain()` pass that
            // walks the AST once at finalize-time; this stub is honest
            // about the limitation.
            out << "void vyne_setup() {\n";
            out << mainStream;
            out << "    arena_free_all();\n";
            out << "}\n\n";

            out << "void vyne_loop() {\n";
            out << "    // TODO: top-level `through ... -> loop` moves here\n";
            out << "}\n";
        } else {
            out << "int main(void) {\n";
            out << mainStream;
            out << "    arena_free_all();\n";
            out << "    return 0;\n";
            out << "}\n";
        }
        return out.str();
    }

    std::string getFunctionCode() {
        return functionStream;
    }
    std::string getBodyCode() {
        return mainStream;
    }
    std::string getIncludes() {
        std::string res;
        for (const auto& inc : includeSet)
            res += "#include \"" + inc + "\"\n";
        return res;
    }

    // --- Full reset (between programs) ----------------------------------
    void reset() {
        globalsStream.clear();
        functionStream.clear();
        mainStream.clear();
        includeSet.clear();
        systemIncludes.clear();
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
        activeModule.clear();
        moduleFunctions.clear();
        moduleInterfaces.clear();
        tryCleanupStack.clear();
        deferCtx = {};
        regionStack.clear();
        streamedRegionStack.clear();
        streamedElemStack.clear();
        persistentRegionStack.clear();
        deviceRegionStack.clear();
        deviceRegionCount = 0;
        deviceFunctionDepth = 0;
        deviceFunctionSet.clear();
        committedVars.clear();
        poolStack.clear();
        slabStack.clear();
        ringStack.clear();
        speculativeStack.clear();
        poolRuntimeEmitted = false;
        slabRuntimeEmitted = false;
        ringRuntimeEmitted = false;
        scratchBoundsEnabled = true;
        blasEnabled = false;
        deterministicBuild_ = false;
        cudaSmVersion_ = 0;
        zeroModeDepth = 0;
        boundedStack.clear();
        currentReturnVar.clear();
        currentReturningVar.clear();
        localScopes.clear();
        localScopes.emplace_back();
        globalTypes.clear();
        nativeTemps.clear();
        nativeModuleAliases.clear();
        scratchRuntimeDims.clear();

        functionParamTypes.clear();
        functionReturnTypes.clear();
        nativeVariants.clear();
        differentiable_.clear();
        customVJPs_.clear();
        backwardRegions_.clear();
        gradientAccess_.clear();
        implicitRegions_.clear();
        nativeReturnType = CType{};
        currentFunctionReturnType_ = CType{};
        structReturnOutParams.clear();

        interfaceArrayFields.clear();
        interfacePrimitiveFields.clear();
        nativeCStructs.clear();
        soaScratch.clear();
        cEligibleInterfaces.clear();
        interfaceStructLayout.clear();
        localStructTypes.clear();
        globalStructTypes.clear();
        currentInterfaceType.clear();
        fieldCache.clear();

        instantiations.clear();
        instantiationsInProgress.clear();

        inNativeVariant = false;
        target_ = Target::Desktop;
        embeddedFeatures_.clear();
        realtimeStack.clear();
        ensuresEmitter = nullptr;
        tempVarCount = 0;
        indentLevel = 1;
    }
};

#endif
