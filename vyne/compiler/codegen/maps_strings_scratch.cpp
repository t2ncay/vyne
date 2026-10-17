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

// Map and interpolated-string expressions; region-bound typed scratch storage.
// Keep expressions that emit statements in evaluation order; see README.md.
// ============================================================
// MAP LITERAL
// ============================================================

std::string MapNode::getCExpr(C_Emitter& e) const {
    std::string temp = e.newTemp("map");
    e.emit("VyneValue " + temp + " = vyne_map_create();");
    for (const auto& [keyNode, valNode] : pairs) {
        std::string k = e.boxAny(keyNode->getCExpr(e));
        std::string v = e.boxAny(valNode->getCExpr(e));
        e.emit("vyne_map_set(" + temp + ", " + k + ", " + v + ");");
    }
    return temp;
}

void MapNode::compile(C_Emitter& e) const {
    getCExpr(e);
}

// ============================================================
// INTERPOLATED STRING
// ============================================================

std::string InterpolatedStringNode::getCExpr(C_Emitter& e) const {
    std::string acc = "vyne_string(\"\")";
    size_t exprIdx = 0;

    for (const auto& [part, isExpr] : parts) {
        std::string piece;
        if (isExpr) {
            if (exprIdx >= exprNodes.size())
                break;
            std::string v = e.boxAny(exprNodes[exprIdx++]->getCExpr(e));
            piece = e.newTemp("is");
            e.emit("VyneValue " + piece + " = vyne_to_string(" + v + ");");
        } else {
            std::string esc;
            esc.reserve(part.size());
            for (char c : part) {
                if (c == '\\')
                    esc += "\\\\";
                else if (c == '"')
                    esc += "\\\"";
                else if (c == '\n')
                    esc += "\\n";
                else if (c == '\t')
                    esc += "\\t";
                else
                    esc += c;
            }
            piece = "vyne_string(\"" + esc + "\")";
        }

        std::string next = e.newTemp("icc");
        e.emit(
            "VyneValue " + next + " = vyne_binop(" + acc + ", " + piece +
            ", 29);"
        );
        acc = next;
    }
    return acc;
}

void InterpolatedStringNode::compile(C_Emitter& e) const {
    getCExpr(e);
}

// ============================================================
// SHAPE-TYPED SCRATCH
// ============================================================

void ScratchNode::compile(C_Emitter& e) const {
    if (!e.hasRegion()) {
        throw std::runtime_error(
            "Compile Error: 'scratch' is only valid inside a 'region' block "
            "(line " +
            std::to_string(lineNumber) + ")."
        );
    }

    std::string sanitized = varName;
    std::replace(sanitized.begin(), sanitized.end(), '.', '_');

    std::string prefix = e.getActiveFunctionPrefix();
    std::string cName =
        prefix.empty() ? ("v_" + sanitized) : ("v_" + prefix + "_" + sanitized);

    const std::string elemCName = CType::elemCName(elemType);

    // -------- runtime-dimension path --------------------------------
    // Any dim that isn't a compile-time literal lands here. `shape`
    // is empty (parser invariant) and `dimExprs` carries every dim.
    if (shape.empty() && !dimExprs.empty()) {
        std::vector<std::string> dimTemps;
        dimTemps.reserve(dimExprs.size());

        for (size_t i = 0; i < dimExprs.size(); ++i) {
            std::string raw = dimExprs[i]->getCExpr(e);
            std::string v =
                coerceToNative(e, dimExprs[i].get(), raw, VType::Int64);
            std::string t = e.newTemp("dim");
            e.emit("int64_t " + t + " = " + v + ";");
            dimTemps.push_back(t);
        }

        // Total element count = d0 * d1 * ... * d_{N-1}.
        std::string sizeExpr;
        for (size_t i = 0; i < dimTemps.size(); ++i) {
            if (i)
                sizeExpr += " * ";
            sizeExpr += dimTemps[i];
        }

        // VLA. GCC and Clang accept this in C and C++ (as an extension
        // for the latter). See the MSVC note below if you ever target
        // that toolchain.
        e.emit(elemCName + " " + cName + "[" + sizeExpr + "];");
        CType arrType;
        arrType.kind = CType::Kind::Array;
        arrType.args.push_back(CType::fromVType(elemType));
        arrType.nativeName = elemCName;
        // NOTE: shape deliberately left empty. The runtime registry
        // is the source of truth for this local.
        e.declareLocal(cName, arrType);
        e.registerScratchRuntimeDims(cName, std::move(dimTemps));

        if (initializer) {
            throw std::runtime_error(
                "Compile Error: scratch initializers are not yet supported "
                "(line " +
                std::to_string(lineNumber) + ")."
            );
        }
        return;
    }
    // -------- end runtime path --------------------------------------

    // -------- static path (unchanged) -------------------------------
    CType arrType;
    arrType.kind = CType::Kind::Array;
    arrType.args.push_back(CType::fromVType(elemType));
    arrType.shape = shape;
    arrType.nativeName = CType::elemCName(elemType);

    // C declaration: double v_grad_w1[64*16];
    int64_t n = arrType.numElements();
    e.emit(elemCName + " " + cName + "[" + std::to_string(n) + "];");

    // Register as a scoped local with shape, so later accesses see it.
    e.declareLocal(cName, arrType);

    if (initializer) {
        throw std::runtime_error(
            "Compile Error: scratch initializers are not yet supported "
            "(line " +
            std::to_string(lineNumber) + ")."
        );
    }
}
std::string ScratchNode::getCExpr(C_Emitter&) const {
    throw std::runtime_error("scratch is a declaration, not an expression");
}

static std::string e_shapeStr(const CType& ct) {
    std::string s;
    for (size_t i = 0; i < ct.shape.size(); ++i) {
        if (i)
            s += ", ";
        s += std::to_string(ct.shape[i]);
    }
    return s;
}

static std::string scratchFlatIndex(
    C_Emitter& e,
    const CType& arrType,
    const std::vector<std::unique_ptr<ASTNode>>& idxNodes,
    const std::string& baseCName
) {

    // -------- runtime-dim path --------------------------------------
    // Registry hit wins. `arrType.shape` is empty for these locals;
    // strides are computed symbolically from the dim temps.
    if (const auto* runtimeDims = e.lookupScratchRuntimeDims(baseCName)) {
        if (idxNodes.size() != runtimeDims->size()) {
            throw std::runtime_error(
                "Shape Error (VNE-071): expected " +
                std::to_string(runtimeDims->size()) + " index(es), got " +
                std::to_string(idxNodes.size()) + " [ line " +
                std::to_string(idxNodes.empty() ? 0 : idxNodes[0]->lineNumber) +
                " ]"
            );
        }

        std::string flat;
        for (size_t k = 0; k < idxNodes.size(); ++k) {
            std::string raw = idxNodes[k]->getCExpr(e);
            std::string idxExpr =
                coerceToNative(e, idxNodes[k].get(), raw, VType::Int64);

            // Materialize the index once so the bounds check and the
            // load read the same value, matching the static path.
            std::string iv;
            if (e.isScratchBoundsEnabled()) {
                iv = e.newTemp("si");
                e.emit("int64_t " + iv + " = " + idxExpr + ";");
                e.emit(
                    "if (VYNE_UNLIKELY(" + iv + " < 0 || " + iv +
                    " >= " + (*runtimeDims)[k] + ")) {"
                );
                e.emit(
                    "    fprintf(stderr, \"Runtime error (VNE-072): "
                    "scratch index out of bounds: dim " +
                    std::to_string(k) + "\\n\"); exit(1);"
                );
                e.emit("}");
            } else {
                iv = idxExpr;
            }

            // Stride_k = d_{k+1} * d_{k+2} * ... * d_{n-1}.
            std::string stride = "1";
            for (size_t s = k + 1; s < runtimeDims->size(); ++s) {
                if (stride == "1")
                    stride = (*runtimeDims)[s];
                else
                    stride += " * " + (*runtimeDims)[s];
            }

            if (k)
                flat += " + ";
            if (stride == "1")
                flat += "(" + iv + ")";
            else
                flat += "(" + iv + ") * (" + stride + ")";
        }
        return flat.empty() ? "0" : flat;
    }
    // -------- end runtime path --------------------------------------

    // -------- static path (unchanged) -------------------------------
    if ((int)idxNodes.size() != (int)arrType.shape.size()) {
        throw std::runtime_error(
            "Shape Error (VNE-071): expected " +
            std::to_string(arrType.shape.size()) + " index(es) for shape [" +
            [&] {
                std::string s;
                for (size_t i = 0; i < arrType.shape.size(); ++i) {
                    if (i)
                        s += ", ";
                    s += std::to_string(arrType.shape[i]);
                }
                return s;
            }() +
            "], got " + std::to_string(idxNodes.size()) + " [ line " +
            std::to_string(idxNodes.empty() ? 0 : idxNodes[0]->lineNumber) +
            " ]"
        );
    }

    auto strides = arrType.strides();
    std::string flat;
    for (size_t k = 0; k < idxNodes.size(); ++k) {
        std::string raw = idxNodes[k]->getCExpr(e);
        std::string idxExpr =
            coerceToNative(e, idxNodes[k].get(), raw, VType::Int64);

        // Materialize into a temp, then bounds-check per dimension.
        // The check is gated so a benchmark build can turn it off;
        // see C_Emitter::isScratchBoundsEnabled.
        std::string iv;
        if (e.isScratchBoundsEnabled()) {
            iv = e.newTemp("si");
            e.emit("int64_t " + iv + " = " + idxExpr + ";");
            e.emit(
                "if (VYNE_UNLIKELY(" + iv + " < 0 || " + iv +
                " >= " + std::to_string(arrType.shape[k]) + ")) {"
            );
            e.emit(
                "    fprintf(stderr, \"Runtime error (VNE-072): scratch index "
                "out of bounds: dim " +
                std::to_string(k) + " of shape [" + e_shapeStr(arrType) +
                "] got %lld at line " +
                std::to_string(idxNodes[k]->lineNumber) + "\\n\", (long long)" +
                iv + ");"
            );
            e.emit("    exit(1);");
            e.emit("}");
        } else {
            iv = idxExpr;
        }

        if (k)
            flat += " + ";
        if (strides[k] == 1)
            flat += "(" + iv + ")";
        else
            flat += "(" + iv + ") * " + std::to_string(strides[k]);
    }
    if (flat.empty())
        flat = "0";
    return flat;
}

std::string ScratchIndexNode::getCExpr(C_Emitter& e) const {
    std::string baseExpr = base->getCExpr(e);
    const CType* bt = e.lookupAnyType(baseExpr);

    // A runtime-dim scratch has no static shape; the registry is the
    // shape. Only fall through when neither source can resolve.
    bool hasRuntimeDims = (e.lookupScratchRuntimeDims(baseExpr) != nullptr);

    // Fall back to plain single-index path if we somehow lost the shape.
    if (!bt || (!bt->hasShape() && !hasRuntimeDims)) {
        // Reconstruct a classic IndexAccessNode and let it run.
        return "(/*scratch fallback*/ 0)";
    }

    std::string flat = scratchFlatIndex(e, *bt, indices, baseExpr);
    VType elem = bt->args.empty() ? VType::Float64 : bt->args[0].toVType();

    std::string tmp = e.newTemp("sid");
    e.emit(
        std::string(CType::elemCName(elem)) + " " + tmp + " = " + baseExpr +
        "[" + flat + "];"
    );
    e.declareNativeTemp(tmp, CType::fromVType(elem));
    return tmp;
}

void ScratchIndexNode::compile(C_Emitter& e) const {
    getCExpr(e);
}

void ScratchStoreNode::compile(C_Emitter& e) const {
    std::string baseExpr = base->getCExpr(e);
    const CType* bt = e.lookupAnyType(baseExpr);
    bool hasRuntimeDims = (e.lookupScratchRuntimeDims(baseExpr) != nullptr);
    if (!bt || (!bt->hasShape() && !hasRuntimeDims)) {
        throw std::runtime_error(
            "Compile Error: scratch store requires a shaped-array base "
            "(line " +
            std::to_string(lineNumber) + ")."
        );
    }

    std::string flat = scratchFlatIndex(e, *bt, indices, baseExpr);
    VType elem = bt->args.empty() ? VType::Float64 : bt->args[0].toVType();

    std::string rawRhs = rhs->getCExpr(e);
    std::string val = coerceToNative(e, rhs.get(), rawRhs, elem);

    e.emit(baseExpr + "[" + flat + "] = " + val + ";");
}

std::string ScratchStoreNode::getCExpr(C_Emitter& e) const {
    compile(e);
    return "vyne_null()";
}
