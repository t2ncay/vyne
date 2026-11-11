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
#include "autodiff.h"

// Function invocation and native call lowering.
// Keep expressions that emit statements in evaluation order; see README.md.
// ============================================================
// FUNCTION CALL
// ============================================================

std::string FunctionCallNode::getCExpr(C_Emitter& e) const {
    // Phase B (AD) intrinsics. Like the pool/slab intrinsics below, they
    // are intercepted only when the shape matches, so a user-defined
    // `fn grad(...)` or `fn backward(...)` still resolves normally.
    if (originalName == "grad" && arguments.size() == 2) {
        std::string target;
        if (arguments[0]->type() == NodeType::VARIABLE) {
            target = static_cast<const VariableNode*>(arguments[0].get())
                         ->getOriginalName();
        } else if (arguments[0]->type() == NodeType::MEMBER_ACCESS) {
            target = static_cast<const MemberAccessNode*>(arguments[0].get())
                         ->getFullPath();
        }
        if (!target.empty() && e.lookupDifferentiable(target))
            return vyne_ad::emitGradCall(e, *this);
    }
    if (originalName == "backward" && arguments.size() == 1 &&
        arguments[0]->type() == NodeType::VARIABLE) {
        auto* v = static_cast<const VariableNode*>(arguments[0].get());
        if (e.lookupBackwardCall(v->getOriginalName()))
            return vyne_ad::emitBackwardCall(e, *this);
    }

    // `@implicit` intrinsics. `newton_solve` is hijacked only while an
    // implicit region is open, so a user function of the same name still
    // resolves normally elsewhere. `backward(root[, seed])` on a solved
    // root consumes the Implicit Function Theorem record instead of the
    // ordinary @differentiable-call record.
    if (e.inImplicitRegion() && vyne_ad::isImplicitSolveName(originalName))
        return vyne_ad::emitImplicitSolveCall(e, *this);
    if (originalName == "backward" &&
        (arguments.size() == 1 || arguments.size() == 2) &&
        arguments[0]->type() == NodeType::VARIABLE) {
        auto* v = static_cast<const VariableNode*>(arguments[0].get());
        if (e.lookupImplicitSolve(v->getOriginalName()))
            return vyne_ad::emitImplicitBackwardCall(e, *this);
    }

    // ----------------------------------------------------------------
    // Pool intrinsics. `pool_alloc` and `pool_free` are intercepted by
    // name *only when a pool context is active*; outside a @pool region
    // they fall through to normal function resolution, so a user-defined
    // `fn pool_alloc()` at top level still works as written.
    //
    // This must run before named-arg resolution: the intrinsics take
    // positional args only, and there's no signature for them in the
    // emitter's function table anyway.
    // ----------------------------------------------------------------
    if (e.hasPool()) {
        const auto& pool  = e.currentPool();
        const bool  isF64 = (pool.elemType == VType::Float64);

        if (originalName == "pool_alloc") {
            if (!arguments.empty()) {
                throw std::runtime_error(
                    "Compile Error (VNE-083): pool_alloc() takes no "
                    "arguments (line " + std::to_string(lineNumber) + ").");
            }
            std::string temp = e.newTemp("pool_arr");
            std::string fn = isF64 ? "vyne_pool_f64_alloc"
                                   : "vyne_pool_i64_alloc";
            e.emit(std::string(isF64 ? "VyneArray_f64 " : "VyneArray_i64 ") +
                   temp + " = " + fn + "(&" + pool.handle + ");");

            CType arr;
            arr.kind = CType::Kind::Array;
            arr.args.push_back(CType::fromVType(pool.elemType));
            e.declareNativeTemp(temp, arr);
            return temp;
        }

        if (originalName == "pool_free") {
            if (arguments.size() != 1) {
                throw std::runtime_error(
                    "Compile Error (VNE-083): pool_free(x) takes exactly "
                    "one argument (line " + std::to_string(lineNumber) + ").");
            }
            std::string arg = arguments[0]->getCExpr(e);
            std::string fn = isF64 ? "vyne_pool_f64_free"
                                   : "vyne_pool_i64_free";
            e.emit(fn + "(&" + pool.handle + ", &" + arg + ");");
            return "vyne_null()";
        }
    }

    // ----------------------------------------------------------------
    // Slab intrinsics. Like the pool intrinsics above, these are
    // intercepted by name only while a @slab context is active, so a
    // user-defined `fn slab_alloc()` outside a slab region still works.
    // ----------------------------------------------------------------
    if (e.hasSlab()) {
        const auto& slab  = e.currentSlab();
        const bool  isF64 = (slab.elemType == VType::Float64);

        if (originalName == "slab_alloc") {
            if (arguments.size() != 1) {
                throw std::runtime_error(
                    "Compile Error (VNE-083): slab_alloc(n) takes exactly "
                    "one argument (line " + std::to_string(lineNumber) + ").");
            }
            // Accept a literal, a native Int64/Float64, or a boxed value and
            // normalize to int64_t with the least boxing possible.
            std::string nExpr;
            if (arguments[0]->type() == NodeType::NUMBER) {
                nExpr = static_cast<const NumberNode*>(arguments[0].get())
                            ->nativeLiteral();
            } else {
                std::string nRaw = arguments[0]->getCExpr(e);
                if (const CType* ct = e.exprNativeType(nRaw)) {
                    if (ct->kind == CType::Kind::Int64)
                        nExpr = nRaw;
                    else if (ct->kind == CType::Kind::Float64)
                        nExpr = "(int64_t)(" + nRaw + ")";
                    else
                        nExpr = "vyne_to_int(" + e.boxAny(nRaw) + ").as.i64";
                } else {
                    nExpr = "vyne_to_int(" + e.boxAny(nRaw) + ").as.i64";
                }
            }
            std::string temp = e.newTemp("slab_arr");
            std::string fn = isF64 ? "vyne_slab_f64_alloc"
                                   : "vyne_slab_i64_alloc";
            e.emit(std::string(isF64 ? "VyneArray_f64 " : "VyneArray_i64 ") +
                   temp + " = " + fn + "(&" + slab.handle + ", " + nExpr +
                   ");");

            CType arr;
            arr.kind = CType::Kind::Array;
            arr.args.push_back(CType::fromVType(slab.elemType));
            e.declareNativeTemp(temp, arr);
            return temp;
        }

        if (originalName == "slab_free") {
            if (arguments.size() != 1) {
                throw std::runtime_error(
                    "Compile Error (VNE-083): slab_free(x) takes exactly "
                    "one argument (line " + std::to_string(lineNumber) + ").");
            }
            std::string arg = arguments[0]->getCExpr(e);
            std::string fn = isF64 ? "vyne_slab_f64_free"
                                   : "vyne_slab_i64_free";
            e.emit(fn + "(&" + slab.handle + ", &" + arg + ");");
            return "vyne_null()";
        }
    }

    // ----------------------------------------------------------------
    // Ring intrinsic. `ring_alloc(n)` rotates through N fixed-size
    // slots; the oldest is overwritten after N allocations. There is
    // no free intrinsic by design.
    // ----------------------------------------------------------------
    if (e.hasRing()) {
        const auto& ring  = e.currentRing();
        const bool  isF64 = (ring.elemType == VType::Float64);

        if (originalName == "ring_alloc") {
            if (arguments.size() != 1) {
                throw std::runtime_error(
                    "Compile Error (VNE-083): ring_alloc(n) takes exactly "
                    "one argument (line " + std::to_string(lineNumber) + ").");
            }
            std::string nExpr;
            if (arguments[0]->type() == NodeType::NUMBER) {
                nExpr = static_cast<const NumberNode*>(arguments[0].get())
                            ->nativeLiteral();
            } else {
                std::string nRaw = arguments[0]->getCExpr(e);
                if (const CType* ct = e.exprNativeType(nRaw)) {
                    if (ct->kind == CType::Kind::Int64)
                        nExpr = nRaw;
                    else if (ct->kind == CType::Kind::Float64)
                        nExpr = "(int64_t)(" + nRaw + ")";
                    else
                        nExpr = "vyne_to_int(" + e.boxAny(nRaw) + ").as.i64";
                } else {
                    nExpr = "vyne_to_int(" + e.boxAny(nRaw) + ").as.i64";
                }
            }
            std::string temp = e.newTemp("ring_arr");
            std::string fn = isF64 ? "vyne_ring_f64_alloc"
                                   : "vyne_ring_i64_alloc";
            e.emit(std::string(isF64 ? "VyneArray_f64 " : "VyneArray_i64 ") +
                   temp + " = " + fn + "(&" + ring.handle + ", " + nExpr +
                   ");");

            CType arr;
            arr.kind = CType::Kind::Array;
            arr.args.push_back(CType::fromVType(ring.elemType));
            e.declareNativeTemp(temp, arr);
            return temp;
        }
    }

    // ----------------------------------------------------------------
    // Stream intrinsic. `streamed_alloc(n)` draws from the enclosing
    // @streamed region's pinned arena. The region budget is checked by
    // the runtime; there is no free intrinsic.
    // ----------------------------------------------------------------
    if (e.hasStreamed()) {
        const VType elemType = e.currentStreamedElem();
        const bool  isF64    = (elemType == VType::Float64);

        if (originalName == "streamed_alloc") {
            if (arguments.size() != 1) {
                throw std::runtime_error(
                    "Compile Error (VNE-083): streamed_alloc(n) takes exactly "
                    "one argument (line " + std::to_string(lineNumber) + ").");
            }
            std::string nExpr;
            if (arguments[0]->type() == NodeType::NUMBER) {
                nExpr = static_cast<const NumberNode*>(arguments[0].get())
                            ->nativeLiteral();
            } else {
                std::string nRaw = arguments[0]->getCExpr(e);
                if (const CType* ct = e.exprNativeType(nRaw)) {
                    if (ct->kind == CType::Kind::Int64)
                        nExpr = nRaw;
                    else if (ct->kind == CType::Kind::Float64)
                        nExpr = "(int64_t)(" + nRaw + ")";
                    else
                        nExpr = "vyne_to_int(" + e.boxAny(nRaw) + ").as.i64";
                } else {
                    nExpr = "vyne_to_int(" + e.boxAny(nRaw) + ").as.i64";
                }
            }
            std::string temp = e.newTemp("stream_arr");
            std::string fn = isF64 ? "vyne_stream_f64_alloc"
                                   : "vyne_stream_i64_alloc";
            e.emit(std::string(isF64 ? "VyneArray_f64 " : "VyneArray_i64 ") +
                   temp + " = " + fn + "(" + nExpr + ");");

            CType arr;
            arr.kind = CType::Kind::Array;
            arr.args.push_back(CType::fromVType(elemType));
            e.declareNativeTemp(temp, arr);
            return temp;
        }
    }

    // ----------------------------------------------------------------
    // Device intrinsics. These are intercepted only inside an active
    // @device region. Handles are Int64 pointer values; host buffers
    // must be typed Array<Float64> values so `.data` is a double*.
    // ----------------------------------------------------------------
    if (e.hasDevice()) {
        auto normInt = [&](const std::unique_ptr<ASTNode>& arg) -> std::string {
            if (arg->type() == NodeType::NUMBER) {
                return static_cast<const NumberNode*>(arg.get())
                    ->nativeLiteral();
            }
            std::string raw = arg->getCExpr(e);
            if (const CType* ct = e.exprNativeType(raw)) {
                if (ct->kind == CType::Kind::Int64)
                    return raw;
                if (ct->kind == CType::Kind::Float64)
                    return "(int64_t)(" + raw + ")";
            }
            return "vyne_to_int(" + e.boxAny(raw) + ").as.i64";
        };

        auto normFloat = [&](const std::unique_ptr<ASTNode>& arg) -> std::string {
            if (arg->type() == NodeType::NUMBER) {
                return static_cast<const NumberNode*>(arg.get())
                    ->nativeLiteral();
            }
            std::string raw = arg->getCExpr(e);
            if (const CType* ct = e.exprNativeType(raw)) {
                if (ct->kind == CType::Kind::Float64)
                    return raw;
                if (ct->kind == CType::Kind::Int64)
                    return "(double)(" + raw + ")";
            }
            return "(" + e.boxAny(raw) + ").as.f64";
        };

        auto hostData = [&](const std::unique_ptr<ASTNode>& arg,
                            const char* what, bool& isF32) -> std::string {
            std::string raw = arg->getCExpr(e);
            const CType* ct = e.lookupAnyType(raw);
            if (!ct || ct->kind != CType::Kind::Array || ct->args.empty()) {
                throw std::runtime_error(
                    "Compile Error (VNE-083): " + std::string(what) +
                    " expects a typed Array<Float32> or Array<Float64> "
                    "buffer (line " + std::to_string(lineNumber) + ").");
            }
            VType et = ct->args[0].toVType();
            if (et == VType::Float32) {
                isF32 = true;
                return "(" + raw + ").data";
            }
            if (et == VType::Float64) {
                isF32 = false;
                return "(" + raw + ").data";
            }
            throw std::runtime_error(
                "Compile Error (VNE-083): " + std::string(what) +
                " expects a typed Array<Float32> or Array<Float64> buffer, "
                "got " + VTypeToString(et) + " (line " +
                std::to_string(lineNumber) + ").");
        };

        auto expectArgs = [&](size_t want, const char* what) {
            if (arguments.size() != want) {
                throw std::runtime_error(
                    "Compile Error (VNE-083): " + std::string(what) +
                    " expects " + std::to_string(want) +
                    " argument(s), got " + std::to_string(arguments.size()) +
                    " (line " + std::to_string(lineNumber) + ").");
            }
        };

        if (originalName == "device_alloc" || originalName == "device_alloc_f64") {
            const bool wantF64 = (originalName == "device_alloc_f64");
            expectArgs(1, wantF64 ? "device_alloc_f64(n)" : "device_alloc(n)");
            std::string temp = e.newTemp("dev_ptr");
            e.emit("int64_t " + temp + " = " +
                   (wantF64 ? "vyne_device_alloc_f64("
                            : "vyne_device_alloc(") +
                   normInt(arguments[0]) + ");");
            e.declareNativeTemp(temp, CType::fromVType(VType::Int64));
            return temp;
        }

        if (originalName == "device_upload") {
            expectArgs(3, "device_upload(handle, host, n)");
            bool f32 = false;
            std::string data = hostData(arguments[1], "device_upload", f32);
            e.emit(std::string(f32 ? "vyne_device_upload_f32("
                                   : "vyne_device_upload_f64(") +
                   normInt(arguments[0]) + ", " + data + ", " +
                   normInt(arguments[2]) + ");");
            return "vyne_null()";
        }

        if (originalName == "device_download") {
            expectArgs(3, "device_download(handle, host, n)");
            bool f32 = false;
            std::string data = hostData(arguments[1], "device_download", f32);
            e.emit(std::string(f32 ? "vyne_device_download_f32("
                                   : "vyne_device_download_f64(") +
                   normInt(arguments[0]) + ", " + data + ", " +
                   normInt(arguments[2]) + ");");
            return "vyne_null()";
        }

        if (originalName == "device_add") {
            expectArgs(4, "device_add(out, a, b, n)");
            e.emit("vyne_device_add(" + normInt(arguments[0]) + ", " +
                   normInt(arguments[1]) + ", " + normInt(arguments[2]) + ", " +
                   normInt(arguments[3]) + ");");
            return "vyne_null()";
        }

        if (originalName == "device_sub") {
            expectArgs(4, "device_sub(out, a, b, n)");
            e.emit("vyne_device_sub(" + normInt(arguments[0]) + ", " +
                   normInt(arguments[1]) + ", " + normInt(arguments[2]) + ", " +
                   normInt(arguments[3]) + ");");
            return "vyne_null()";
        }

        if (originalName == "device_mul") {
            expectArgs(4, "device_mul(out, a, b, n)");
            e.emit("vyne_device_mul(" + normInt(arguments[0]) + ", " +
                   normInt(arguments[1]) + ", " + normInt(arguments[2]) + ", " +
                   normInt(arguments[3]) + ");");
            return "vyne_null()";
        }

        if (originalName == "device_scale") {
            expectArgs(4, "device_scale(out, a, s, n)");
            e.emit("vyne_device_scale(" + normInt(arguments[0]) + ", " +
                   normInt(arguments[1]) + ", " + normFloat(arguments[2]) +
                   ", " + normInt(arguments[3]) + ");");
            return "vyne_null()";
        }

        if (originalName == "device_relu") {
            expectArgs(3, "device_relu(out, a, n)");
            e.emit("vyne_device_relu(" + normInt(arguments[0]) + ", " +
                   normInt(arguments[1]) + ", " + normInt(arguments[2]) + ");");
            return "vyne_null()";
        }

        if (originalName == "device_fill") {
            expectArgs(3, "device_fill(handle, value, n)");
            e.emit("vyne_device_fill(" + normInt(arguments[0]) + ", " +
                   normFloat(arguments[1]) + ", " + normInt(arguments[2]) +
                   ");");
            return "vyne_null()";
        }

        if (originalName == "device_mse_grad") {
            expectArgs(5, "device_mse_grad(grad, partial, pred, target, n)");
            e.emit("vyne_device_mse_grad(" + normInt(arguments[0]) + ", " +
                   normInt(arguments[1]) + ", " + normInt(arguments[2]) + ", " +
                   normInt(arguments[3]) + ", " + normInt(arguments[4]) + ");");
            return "vyne_null()";
        }

        if (originalName == "device_sgd_step") {
            expectArgs(4, "device_sgd_step(param, grad, lr, n)");
            e.emit("vyne_device_sgd_step(" + normInt(arguments[0]) + ", " +
                   normInt(arguments[1]) + ", " + normFloat(arguments[2]) + ", " +
                   normInt(arguments[3]) + ");");
            return "vyne_null()";
        }

        if (originalName == "device_adamw_step") {
            expectArgs(11, "device_adamw_step(param, m, v, grad, lr, beta1, beta2, eps, weight_decay, step, n)");
            e.emit("vyne_device_adamw_step(" + normInt(arguments[0]) + ", " +
                   normInt(arguments[1]) + ", " + normInt(arguments[2]) + ", " +
                   normInt(arguments[3]) + ", " + normFloat(arguments[4]) + ", " +
                   normFloat(arguments[5]) + ", " + normFloat(arguments[6]) + ", " +
                   normFloat(arguments[7]) + ", " + normFloat(arguments[8]) + ", " +
                   normInt(arguments[9]) + ", " + normInt(arguments[10]) + ");");
            return "vyne_null()";
        }

        if (originalName == "device_matmul") {
            expectArgs(6, "device_matmul(out, a, b, m, n, k)");
            e.emit("vyne_device_matmul(" + normInt(arguments[0]) + ", " +
                   normInt(arguments[1]) + ", " + normInt(arguments[2]) + ", " +
                   normInt(arguments[3]) + ", " + normInt(arguments[4]) + ", " +
                   normInt(arguments[5]) + ");");
            return "vyne_null()";
        }

        if (originalName == "device_conv2d") {
            expectArgs(
                14,
                "device_conv2d(out, input, weight, bias, in_h, in_w, in_c, "
                "out_h, out_w, out_c, k, stride, pad, has_bias)"
            );
            e.emit("vyne_device_conv2d(" + normInt(arguments[0]) + ", " +
                   normInt(arguments[1]) + ", " + normInt(arguments[2]) + ", " +
                   normInt(arguments[3]) + ", " + normInt(arguments[4]) + ", " +
                   normInt(arguments[5]) + ", " + normInt(arguments[6]) + ", " +
                   normInt(arguments[7]) + ", " + normInt(arguments[8]) + ", " +
                   normInt(arguments[9]) + ", " + normInt(arguments[10]) + ", " +
                   normInt(arguments[11]) + ", " + normInt(arguments[12]) + ", " +
                   normInt(arguments[13]) + ");");
            return "vyne_null()";
        }

        if (originalName == "device_sync") {
            expectArgs(0, "device_sync()");
            e.emit("vyne_device_sync();");
            return "vyne_null()";
        }

        if (originalName == "device_bytes") {
            expectArgs(0, "device_bytes()");
            std::string temp = e.newTemp("dev_bytes");
            e.emit("int64_t " + temp + " = vyne_device_bytes().as.i64;");
            e.declareNativeTemp(temp, CType::fromVType(VType::Int64));
            return temp;
        }

        if (originalName == "device_peak_bytes") {
            expectArgs(0, "device_peak_bytes()");
            std::string temp = e.newTemp("dev_peak");
            e.emit("int64_t " + temp + " = vyne_device_peak_bytes().as.i64;");
            e.declareNativeTemp(temp, CType::fromVType(VType::Int64));
            return temp;
        }

        if (originalName == "device_backend") {
            expectArgs(0, "device_backend()");
            std::string temp = e.newTemp("dev_backend");
            e.emit("VyneValue " + temp + " = vyne_device_backend();");
            return temp;
        }

        if (originalName == "device_available") {
            expectArgs(0, "device_available()");
            std::string temp = e.newTemp("dev_avail");
            e.emit("VyneValue " + temp + " = vyne_device_available();");
            return temp;
        }

        // --- device-native vconv primitives -------------------------
        // These mirror the C `vconv_*_device` functions: Int64 handles
        // in, a fresh Int64 device handle out. They are only reachable
        // inside a @device region and require the vconv runtime header,
        // which the call site's module include provides.
        auto emitHandleCall =
            [&](const char* cfname, const char* what,
                const std::vector<std::unique_ptr<ASTNode>>& args) -> std::string {
            // The device vconv primitives live in the vconv runtime
            // header, which itself pulls in vdevice.h. Include it on
            // first use (idempotent); the device region already emits
            // vdevice.h, so ordering never matters.
            {
                std::filesystem::path moduleBase =
                    std::filesystem::path(FileUtils::getExeDir()) / "vyne" /
                    "runtime" / "modules";
                e.addInclude((moduleBase / "vconv.h").string());
            }
            std::string temp = e.newTemp("dev_out");
            std::string call = std::string(cfname) + "(";
            for (size_t i = 0; i < args.size(); ++i) {
                if (i) call += ", ";
                call += normInt(args[i]);
            }
            call += ")";
            e.emit("int64_t " + temp + " = " + call + ";");
            e.declareNativeTemp(temp, CType::fromVType(VType::Int64));
            (void)what;
            return temp;
        };

        if (originalName == "vconv_conv2d_fwd_device") {
            expectArgs(11, "vconv_conv2d_fwd_device(x, w, b, in_h, in_w, in_c, "
                           "out_c, kh, kw, stride, pad)");
            return emitHandleCall("vconv_conv2d_fwd_device", originalName.c_str(),
                                  arguments);
        }
        if (originalName == "vconv_conv2d_backward_input_device") {
            expectArgs(12, "vconv_conv2d_backward_input_device("
                           "grad, w, in_h, in_w, in_c, out_h, out_w, out_c, "
                           "kh, kw, stride, pad)");
            return emitHandleCall("vconv_conv2d_backward_input_device",
                                  originalName.c_str(), arguments);
        }
        if (originalName == "vconv_conv2d_backward_weight_device") {
            expectArgs(12, "vconv_conv2d_backward_weight_device("
                           "x, grad, in_h, in_w, in_c, out_h, out_w, out_c, "
                           "kh, kw, stride, pad)");
            return emitHandleCall("vconv_conv2d_backward_weight_device",
                                  originalName.c_str(), arguments);
        }
        if (originalName == "vconv_conv2d_backward_bias_device") {
            expectArgs(4, "vconv_conv2d_backward_bias_device(grad, out_h, out_w, out_c)");
            return emitHandleCall("vconv_conv2d_backward_bias_device",
                                  originalName.c_str(), arguments);
        }
        if (originalName == "vconv_relu_fwd_device") {
            expectArgs(2, "vconv_relu_fwd_device(x, n)");
            return emitHandleCall("vconv_relu_fwd_device", originalName.c_str(),
                                  arguments);
        }
        if (originalName == "vconv_relu_backward_device") {
            expectArgs(3, "vconv_relu_backward_device(grad, pre, n)");
            return emitHandleCall("vconv_relu_backward_device", originalName.c_str(),
                                  arguments);
        }
        if (originalName == "vconv_pixel_shuffle_fwd_device") {
            expectArgs(5, "vconv_pixel_shuffle_fwd_device(x, h, w, c, r)");
            return emitHandleCall("vconv_pixel_shuffle_fwd_device",
                                  originalName.c_str(), arguments);
        }
        if (originalName == "vconv_pixel_shuffle_backward_device") {
            expectArgs(5, "vconv_pixel_shuffle_backward_device(grad, h, w, c, r)");
            return emitHandleCall("vconv_pixel_shuffle_backward_device",
                                  originalName.c_str(), arguments);
        }
    }

    // --- Resolve named args (unchanged) -----------------------------
    std::vector<ASTNode*> orderedArgs;
    if (hasNamedArguments()) {
        const auto* sig = e.getFunctionSignature(originalName);
        if (!sig) {
            throw std::runtime_error(
                "Compile Error: named arguments used for function '" +
                originalName + "' but its signature is unknown (line " +
                std::to_string(lineNumber) + ").");
        }
        std::unordered_map<std::string, ASTNode*> nameToArg;
        for (const auto& [name, arg] : namedArguments) {
            if (nameToArg.count(name)) {
                throw std::runtime_error(
                    "Compile Error: duplicate named argument '" + name +
                    "' in call to '" + originalName + "' (line " +
                    std::to_string(lineNumber) + ").");
            }
            nameToArg[name] = arg.get();
        }
        for (const auto& paramName : *sig) {
            auto it = nameToArg.find(paramName);
            if (it == nameToArg.end()) {
                throw std::runtime_error(
                    "Compile Error: missing argument '" + paramName +
                    "' in call to '" + originalName + "' (line " +
                    std::to_string(lineNumber) + ").");
            }
            orderedArgs.push_back(it->second);
        }
        for (const auto& [name, _] : nameToArg) {
            bool found = false;
            for (const auto& pn : *sig) if (pn == name) { found = true; break; }
            if (!found) {
                throw std::runtime_error(
                    "Compile Error: unknown argument '" + name +
                    "' in call to '" + originalName + "' (line " +
                    std::to_string(lineNumber) + ").");
            }
        }
    } else {
        for (const auto& a : arguments) orderedArgs.push_back(a.get());
    }

    if (!e.isInterface(originalName)) {
        if (const auto* sig = e.getFunctionSignature(originalName)) {
            if (orderedArgs.size() != sig->size()) {
                throw std::runtime_error(
                    "Compile Error: function '" + originalName +
                    "' expects " + std::to_string(sig->size()) +
                    " argument(s), got " + std::to_string(orderedArgs.size()) +
                    " (line " + std::to_string(lineNumber) + ").");
            }
        }
    }

    // --- M2: monomorphization --------------------------------------
    // Only fire when the parser recorded type args. Inference is a
    // follow-up; explicit `foo<Int64>(...)` is the load-bearing case.
    if (!typeArgs.empty()) {
        std::string key = originalName;
        std::replace(key.begin(), key.end(), '.', '_');
        for (const auto& t : typeArgs) key += "__" + t;

        if (const std::string* emitted = e.lookupInstantiation(key)) {
            // Already emitted: call the specialised C function directly.
            int n = (int)orderedArgs.size();
            std::string retTemp = e.newTemp("ret");
            std::string argArr  = e.newTemp("args");
            if (n > 0) {
                e.emit("VyneValue* " + argArr +
                       " = (VyneValue*)arena_alloc(sizeof(VyneValue) * " +
                       std::to_string(n) + ");");
                for (int i = 0; i < n; ++i) {
                    e.emit(argArr + "[" + std::to_string(i) + "] = " +
                           e.boxAny(orderedArgs[i]->getCExpr(e)) + ";");
                }
            } else {
                e.emit("VyneValue* " + argArr + " = NULL;");
            }
            e.emit("VyneValue " + retTemp + " = fn_" + *emitted +
                   "(" + std::to_string(n) + ", " + argArr + ");");
            return retTemp;
        }

        // First sighting: instantiate on demand. The concrete C name
        // is the same key we just built, prefixed with "fn_".
        if (e.beginInstantiation(key)) {
            std::string cName = key;
            e.finishInstantiation(key, cName);
            // NOTE: actual body emission is scheduled by ProgramNode
            // via the emitter's instantiation queue — see
            // `ProgramNode::compile` for the drain loop. For now, fall
            // through to the boxed call so unresolvable cases still
            // produce compilable C.
        }
    }

    std::vector<std::string> argCExprs;
    std::vector<VType>       argVTypes;
    std::vector<const CType*> argCTypes;
    argCExprs.reserve(orderedArgs.size());
    argVTypes.reserve(orderedArgs.size());
    argCTypes.reserve(orderedArgs.size());

    const std::vector<CType>* callParamTypes =
        e.getFunctionParamTypes(originalName);
    if (!callParamTypes) {
        std::string bare = originalName;
        std::replace(bare.begin(), bare.end(), '.', '_');
        callParamTypes = e.getFunctionParamTypes(bare);
    }

    bool hasSliceParam = false;
    for (size_t ai = 0; ai < orderedArgs.size(); ++ai) {
        ASTNode* a = orderedArgs[ai];
        const CType* want =
            (callParamTypes && ai < callParamTypes->size())
                ? &(*callParamTypes)[ai]
                : nullptr;

        if (want && want->kind == CType::Kind::Slice && !want->args.empty()) {
            hasSliceParam = true;
            std::string s;
            if (a && a->type() == NodeType::SLICE) {
                s = emitSliceView(e, a, *want, lineNumber);
            } else {
                std::string raw = a->getCExpr(e);
                const CType* have = e.lookupType(raw);
                if (have && have->kind == CType::Kind::Slice) {
                    s = std::move(raw);
                } else if (
                    have && have->kind == CType::Kind::Array &&
                    !have->args.empty() &&
                    have->args[0].toVType() == want->args[0].toVType()
                ) {
                    s = emitSliceView(e, a, *want, lineNumber);
                } else {
                    throw std::runtime_error(
                        "Compile Error: cannot pass '" +
                        VTypeToString(a->getStaticType()) +
                        "' to a Slice<" +
                        VTypeToString(want->args[0].toVType()) +
                        "> parameter (line " + std::to_string(lineNumber) +
                        ")."
                    );
                }
            }
            const CType* fullCt = e.lookupType(s);
            argCExprs.push_back(std::move(s));
            argVTypes.push_back(VType::Slice);
            argCTypes.push_back(fullCt);
            continue;
        }

        std::string s = a->getCExpr(e);
        const CType* fullCt = e.lookupType(s);
        const CType* primCt = e.exprNativeType(s);
        VType t = primCt ? primCt->toVType() : a->getStaticType();
        argCExprs.push_back(std::move(s));
        argVTypes.push_back(t);
        argCTypes.push_back(fullCt);
    }

    int argSize = (int)orderedArgs.size();
    std::string retTemp = e.newTemp("ret");

    std::string mangledName = originalName;
    std::replace(mangledName.begin(), mangledName.end(), '.', '_');

    // If the callee's bare name is defined in the module we are currently
    // emitting, the C symbol is fn_<module>_<name>. Without this, a call
    // like `_skip_ws(x)` inside vjson emits `fn__skip_ws`, which does not
    // exist — the definition is `fn_vjson__skip_ws`.
    const std::string& mod = e.getActiveModule();
    bool sameModuleFn = !mod.empty() && e.isModuleFunction(mod, originalName);
    if (sameModuleFn) {
        mangledName = mod + "_" + mangledName;
    }

    // ----------------------------------------------------------------
    // Native-variant dispatch. Same helper the group-call path uses;
    // see codegen/native_dispatch.cpp for the full doc.
    // ----------------------------------------------------------------
    if (auto nativeResult = tryEmitNativeCall(
            e, mangledName, originalName,
            orderedArgs, argCExprs, argVTypes, argCTypes)) {
        return *nativeResult;
    }

    // Slice is a native-only view type: it cannot be boxed into a single
    // VyneValue without losing the borrowed-buffer contract. A callee that
    // takes Slice<T> is always registered as a native variant; reaching
    // here means its return type or body disqualified that variant.
    if (hasSliceParam) {
        throw std::runtime_error(
            "Compile Error: function '" + originalName +
            "' takes a Slice<T> parameter but has no native variant (line " +
            std::to_string(lineNumber) +
            "). Slice parameters require a native-callable signature "
            "(primitive, Array<T>, Slice<T>, or a flat struct return)."
        );
    }

    // ----------------------------------------------------------------
    // Interface constructors take their arguments directly — no
    // `args[]` array, no arena allocation, no deep-copy dance.
    // ----------------------------------------------------------------
    if (e.isInterface(originalName) || e.isInterface(mangledName)) {
        if (hasNamedArguments()) {
            throw std::runtime_error(
                "Compile Error: named arguments are not supported for interface constructors "
                "(line " + std::to_string(lineNumber) + ").");
        }

        // Same-module interface constructor: `Parser(...)` inside vjson
        // must emit struct_vjson_Parser, not struct_Parser.
        std::string ctorSuffix = mangledName;
        std::string dottedName = originalName;
        if (!mod.empty() && e.isModuleInterface(mod, originalName)) {
            ctorSuffix = mod + "_" + mangledName;
            dottedName = mod + "." + originalName;
        }

        // Slice 3a: if this interface has a registered native C struct,
        // emit a compound literal of that C type instead of the boxed
        // constructor call. The value flows through the emitter as a
        // native temp with CType{Struct, mangledName=dottedName}; the
        // boxAny path re-boxes it at any dynamic boundary.
        //
        // Lookup key is the dotted form so a module-scoped interface
        // resolves to its own struct even if another module has an
        // interface with the same bare name.
        if (const auto* ns = e.getNativeCStruct(dottedName)) {
            std::string init = "((" + ns->tag + "){";
            for (size_t i = 0; i < ns->fieldNames.size(); ++i) {
                if (i) init += ", ";
                init += "." + ns->fieldNames[i] + " = ";
                if (i < orderedArgs.size()) {
                    CType ft = ns->fieldTypes[i];

                    // Struct-element array field receiving an empty
                    // literal: `ArrayNode::getCExpr` has no type
                    // context, so it lowers `[]` to a boxed
                    // `vyne_array_create(0)` — a VyneValue. The native
                    // struct field is `vyne_Array_vyne_<tag>`, so we
                    // substitute the native empty-container constructor.
                    //
                    // Non-empty struct-array literals already resolve
                    // correctly: the array-literal fast path in
                    // collections.cpp detects all-native-struct
                    // elements and builds the container directly, and
                    // coerceToNative passes it through unchanged.
                    // Only the empty-literal shape needs this repair.
                    if (ft.kind == CType::Kind::Array && !ft.args.empty()
                        && ft.args[0].kind == CType::Kind::Struct
                        && ft.args[0].nativeCStruct
                        && orderedArgs[i]
                        && orderedArgs[i]->type() == NodeType::ARRAY) {
                        auto* arrNode = static_cast<const ArrayNode*>(
                            orderedArgs[i]);
                        if (arrNode->getElements().empty()) {
                            const auto* elNs = e.getNativeCStruct(
                                ft.args[0].mangledName);
                            if (elNs) {
                                init += "vyne_Array_" + elNs->tag +
                                        "_create(0)";
                                continue;
                            }
                        }
                    }

                    VType want = ft.toVType();
                    init += coerceToNative(
                        e, orderedArgs[i], argCExprs[i], want);
                } else {
                    CType ft = ns->fieldTypes[i];
                    if      (ft.kind == CType::Kind::Int64)   init += "0";
                    else if (ft.kind == CType::Kind::Float64) init += "0.0";
                    else if (ft.kind == CType::Kind::Bool)    init += "false";
                    // Missing argument at a struct-element array
                    // position: same repair as above, for the case
                    // where the constructor call supplies fewer
                    // arguments than the interface declares fields.
                    else if (ft.kind == CType::Kind::Array
                             && !ft.args.empty()
                             && ft.args[0].kind == CType::Kind::Struct
                             && ft.args[0].nativeCStruct) {
                        const auto* elNs = e.getNativeCStruct(
                            ft.args[0].mangledName);
                        if (elNs) {
                            init += "vyne_Array_" + elNs->tag +
                                    "_create(0)";
                        }
                    }
                }
            }
            init += "})";

            std::string nativeTemp = e.newTemp("struct");
            e.emit(ns->tag + " " + nativeTemp + " = " + init + ";");

            CType ct;
            ct.kind          = CType::Kind::Struct;
            ct.mangledName   = dottedName;
            ct.nativeCStruct = true;   // Slice 3: C-level value is `vyne_*`
            e.declareNativeTemp(nativeTemp, ct);
            return nativeTemp;
        }

        // Boxed path (unchanged) — used when the interface has no
        // native C struct representation, or its fields are not
        // C-eligible. Same logic as before, now reached only as a
        // fallback.
        const std::vector<std::string>* defaults =
            e.getInterfaceDefaults(originalName);
        if (!defaults) defaults = e.getInterfaceDefaults(mangledName);

        std::vector<std::string> argStrs;
        argStrs.reserve(orderedArgs.size());
        for (size_t i = 0; i < orderedArgs.size(); ++i) {
            argStrs.push_back(e.boxAny(argCExprs[i]));
        }

        if (defaults) {
            for (size_t i = argStrs.size(); i < defaults->size(); ++i) {
                argStrs.push_back((*defaults)[i]);
            }
        }

        std::string directArgs;
        for (size_t i = 0; i < argStrs.size(); ++i) {
            if (i > 0) directArgs += ", ";
            directArgs += argStrs[i];
        }

        e.emit("VyneValue " + retTemp + " = struct_" + ctorSuffix +
               "(" + directArgs + ");");
        return retTemp;
    }

    // ----------------------------------------------------------------
    // Normal function call: build args[] on the arena, deep-copy
    // arrays/maps so callee mutations don't leak back to the caller.
    // ----------------------------------------------------------------
    std::string argArr = e.newTemp("args");

    if (argSize > 0) {
        e.emit("VyneValue* " + argArr + " = (VyneValue*)arena_alloc(sizeof(VyneValue) * " +
               std::to_string(argSize) + ");");
        for (int i = 0; i < argSize; ++i) {
            // NOTE: no deep copy. Array / map arguments are shared by
            // reference, matching assignment semantics and Python / JS /
            // Lua. If a callee mutates its parameter, the caller sees it.
            std::string val = e.boxAny(argCExprs[i]);
            e.emit(argArr + "[" + std::to_string(i) + "] = " + val + ";");
        }
    } else {
        e.emit("VyneValue* " + argArr + " = NULL;");
    }

    e.emit("VyneValue " + retTemp + " = fn_" + mangledName +
           "(" + std::to_string(argSize) + ", " + argArr + ");");
    return retTemp;
}

void FunctionCallNode::compile(C_Emitter& e) const { getCExpr(e); }
