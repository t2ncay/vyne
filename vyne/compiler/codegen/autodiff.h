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

#pragma once

#include <string>

class C_Emitter;
class FunctionNode;
class FunctionCallNode;

// ---------------------------------------------------------------------------
// Phase B (AD): source-to-source reverse mode.
//
// A `@differentiable` function whose body is straight-line over Float64
// scalars gets a compiler-generated companion:
//
//     void fn_<name>_grad(double p0, ..., double d_out,
//                         double* d_p0, ...);
//
// The companion recomputes the primal intermediates (the "tape" is a set of
// ordinary C locals inside the caller's region scope) and then walks the
// assignments in reverse, accumulating `d_<var>` slots. There is no runtime
// tape interpreter and no per-operation dispatch.
//
// The supported subset is deliberately small and explicit:
//   * every parameter and the return type are Float64;
//   * the body is a sequence of fresh Float64 declarations followed by one
//     `return <expr>;`;
//   * expressions are +, -, *, /, ** and the vmath unary functions
//     sin/cos/exp/log/sqrt/tanh/abs/relu, plus pow(a, b).
//
// Anything outside the subset is rejected with VNE-120..VNE-126 so a user
// never silently gets a wrong gradient, unless the function registers a
// custom `<fn>_vjp` adjoint. The custom adjoint receives the original boxed
// arguments and owns its gradient storage; the compiler only routes
// `backward(loss)` to it.
// ---------------------------------------------------------------------------
namespace vyne_ad {

// Validate the differentiable subset. Returns false and fills `reason`
// when the function cannot be lowered yet.
bool validate(const FunctionNode& fn, std::string* reason);

// C name of the generated adjoint, e.g. `fn_loss_grad`.
std::string adjointCName(const std::string& mangledName);

// Full C prototype (no trailing semicolon) for the companion.
std::string
adjointPrototype(const FunctionNode& fn, const std::string& mangledName);

// Emit the companion into the emitter's function stream.
void emitAdjoint(
    C_Emitter& e,
    const FunctionNode& fn,
    const std::string& mangledName
);

// Lower `grad(f, x)` and `backward(loss)`. These are intercepted by
// FunctionCallNode before normal name resolution.
std::string emitGradCall(C_Emitter& e, const FunctionCallNode& call);
std::string emitBackwardCall(C_Emitter& e, const FunctionCallNode& call);

// `@implicit` region lowering.
//
// `newton_solve(F, x..., z0)` is intercepted only while an `@implicit`
// region is active. It emits a Newton solver over the scalar residual
// `F(x..., z) -> Float64` and records the fixed point plus the captured
// parameter values in the emitter's implicit scope. `backward(z)` then
// differentiates the residual at that fixed point and applies the
// Implicit Function Theorem:
//
//     dz/dx_i = -(dF/dx_i) / (dF/dz)
//
// so no solver iterations are stored or replayed.
bool isImplicitSolveName(const std::string& name);
std::string emitImplicitSolveCall(C_Emitter& e, const FunctionCallNode& call);
std::string
emitImplicitBackwardCall(C_Emitter& e, const FunctionCallNode& call);

} // namespace vyne_ad
