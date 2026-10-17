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

// Member access and assignment.
// Keep expressions that emit statements in evaluation order; see README.md.
// ============================================================
// MEMBER ACCESS / ASSIGNMENT
// ============================================================

std::string MemberAccessNode::getCExpr(C_Emitter& e) const {
    // --- Slice 3c (+ extension): direct field access on a native C struct ---
    // The receiver's C-level value is the `vyne_*` typedef, not a
    // VyneValue, so vyne_struct_get is a type error. Emit `recv.member`
    // directly and declare the temp with the field's own CType so
    // downstream native dispatch sees the right primitive kind.
    //
    // Fires for any receiver whose CType carries Struct + nativeCStruct:
    //   - a bare variable (Slice 3c)
    //   - an index read from a `vyne_Array_vyne_*` container (Slice 3f)
    //   - a field read on another native struct (chained access)
    //   - a struct constructor used directly as a receiver
    //
    // The receiver is materialized exactly once here and reused by every
    // subsequent branch, so no path below can double-emit its statements.
    std::string recvC = receiver->getCExpr(e);
    const CType* recvCt = e.lookupType(recvC);

    if (recvCt && recvCt->kind == CType::Kind::Struct &&
        recvCt->nativeCStruct) {
        const auto* ns = e.getNativeCStruct(recvCt->mangledName);
        if (ns) {
            for (size_t i = 0; i < ns->fieldNames.size(); ++i) {
                if (ns->fieldNames[i] == memberName) {
                    CType fct = ns->fieldTypes[i];
                    std::string temp = e.newTemp("fld");
                    e.emit(
                        fct.cTypeName() + " " + temp + " = " + recvC + "." +
                        memberName + ";"
                    );
                    e.declareNativeTemp(temp, fct);
                    return temp;
                }
            }
        }
    }

    // --- Existing native-module / group resolution -------------------
    if (receiver->type() == NodeType::VARIABLE) {
        auto* var = static_cast<VariableNode*>(receiver.get());
        std::string modName = var->getOriginalName();

        std::string native = e.getNativeMapping(modName, memberName, false);
        if (native.find("v_" + modName) == std::string::npos) {
            return native;
        }

        if (e.isGroup(modName)) {
            std::string name = "v_" + modName + "_" + memberName;
            std::replace(name.begin(), name.end(), '.', '_');
            return name;
        }
    }

    if (receiver->type() == NodeType::VARIABLE) {
        auto* var = static_cast<VariableNode*>(receiver.get());
        std::string recvName = var->getOriginalName();

        std::string typeName;
        if (recvName == "self") {
            typeName = e.getCurrentInterfaceType();
        } else {
            std::string prefix = e.getActiveFunctionPrefix();
            std::string lookupKey = prefix.empty()
                                        ? ("v_" + recvName)
                                        : ("v_" + prefix + "_" + recvName);
            if (const std::string* t = e.lookupLocalStructType(lookupKey))
                typeName = *t;
            else if (
                const std::string* t = e.lookupGlobalStructType("v_" + recvName)
            )
                typeName = *t;
        }

        if (!typeName.empty()) {
            VType elem = e.getInterfaceArrayElem(typeName, memberName);
            if (elem == VType::Int64 || elem == VType::Float64) {
                std::string cacheKey = recvName + "." + memberName;
                if (const auto* cached = e.getFieldCache(cacheKey))
                    return cached->temp;

                std::string recv = e.boxAny(recvC);
                uint32_t fid = StringPool::intern(memberName);
                std::string temp = e.newTemp("fld");
                const char* cName = (elem == VType::Float64) ? "VyneArray_f64"
                                                             : "VyneArray_i64";
                const char* fn = (elem == VType::Float64)
                                     ? "vyne_value_to_array_f64"
                                     : "vyne_value_to_array_i64";

                e.emit(
                    std::string(cName) + " " + temp + " = " + fn +
                    "(vyne_struct_get(" + recv + ", " + std::to_string(fid) +
                    "));"
                );

                CType ct;
                ct.kind = CType::Kind::Array;
                ct.args.push_back(CType::fromVType(elem));
                e.declareNativeTemp(temp, ct);
                e.setFieldCache(cacheKey, temp, ct);
                return temp;
            }

            // M4-C1B: scalar (Int64/Float64) interface fields. Struct
            // storage is always boxed VyneValue; unbox once into a native
            // temp so downstream native dispatch (tryEmitNativeCall) sees
            // a provably-typed operand.
            VType ptype = e.getInterfacePrimitiveField(typeName, memberName);
            if (ptype == VType::Int64 || ptype == VType::Float64) {
                std::string cacheKey = recvName + "." + memberName + ":prim";
                if (const auto* cached = e.getFieldCache(cacheKey))
                    return cached->temp;

                std::string recv = e.boxAny(recvC);
                uint32_t fid = StringPool::intern(memberName);
                std::string boxed = e.newTemp("fb");
                e.emit(
                    "VyneValue " + boxed + " = vyne_struct_get(" + recv + ", " +
                    std::to_string(fid) + ");"
                );

                std::string temp = e.newTemp("fp");
                if (ptype == VType::Float64) {
                    e.emit(
                        "double " + temp + " = (" + boxed +
                        ".type == V_FLOAT64) ? " + boxed +
                        ".as.f64 : (double)" + boxed + ".as.i64;"
                    );
                    e.declareNativeTemp(temp, CType::fromVType(VType::Float64));
                } else {
                    e.emit(
                        "int64_t " + temp + " = (" + boxed +
                        ".type == V_INT64) ? " + boxed + ".as.i64 : (int64_t)" +
                        boxed + ".as.f64;"
                    );
                    e.declareNativeTemp(temp, CType::fromVType(VType::Int64));
                }
                e.setFieldCache(cacheKey, temp, CType::fromVType(ptype));
                return temp;
            }
        }
    }

    // --- Boxed fallback ----------------------------------------------
    std::string recv = e.boxAny(recvC);
    uint32_t fid = StringPool::intern(memberName);
    return "vyne_struct_get(" + recv + ", " + std::to_string(fid) + ")";
}

void MemberAccessNode::compile(C_Emitter& e) const {
    // Bare member access as statement — no-op
}

void MemberAssignmentNode::compile(C_Emitter& e) const {
    // Any write invalidates all cached unboxes for this function.
    e.clearFieldCache();

    // --- FAST PATH: index-into-native-struct-array as lvalue -----------
    // `arr[i].member = value` where `arr` is an `Array<Struct>` cannot
    // go through the general receiver path below. `IndexAccessNode::
    // getCExpr` always materializes the element into a temp copy, which
    // is correct for reads but loses writes: `idx_N.member = v` updates
    // the local copy and the array element is untouched.
    //
    // Emit `base.data[idx].member = value` directly. The container's
    // `data` is a pointer into the shared backing store, so writing
    // through it reaches the real element. This mirrors the read path
    // in collections.cpp:IndexAccessNode::getCExpr (Slice 3f) — but for
    // writes we deliberately do NOT materialize the element first.
    if (receiver->type() == NodeType::INDEX_ACCESS) {
        auto* ia = static_cast<IndexAccessNode*>(receiver.get());
        std::string baseRaw = ia->getBase()->getCExpr(e);
        const CType* bt = e.lookupType(baseRaw);
        if (bt && bt->kind == CType::Kind::Array && !bt->args.empty() &&
            bt->args[0].kind == CType::Kind::Struct &&
            bt->args[0].nativeCStruct) {
            const auto* elNs = e.getNativeCStruct(bt->args[0].mangledName);
            if (elNs) {
                for (size_t i = 0; i < elNs->fieldNames.size(); ++i) {
                    if (elNs->fieldNames[i] != memberName)
                        continue;
                    CType fct = elNs->fieldTypes[i];
                    std::string rawIdx = ia->getIndex()->getCExpr(e);
                    std::string idx =
                        coerceToNative(e, ia->getIndex(), rawIdx, VType::Int64);
                    std::string rawRhs = rhs->getCExpr(e);
                    std::string native =
                        coerceToNative(e, rhs.get(), rawRhs, fct.toVType());
                    e.emit(
                        baseRaw + ".data[" + idx + "]." + memberName + " = " +
                        native + ";"
                    );
                    return;
                }
                throw std::runtime_error(
                    "Compile Error: interface '" + bt->args[0].mangledName +
                    "' has no field '" + memberName + "' (line " +
                    std::to_string(lineNumber) + ")."
                );
            }
        }
    }
    // --- end index-lvalue fast path -----------------------------------

    // Materialize the receiver exactly once. Every branch below either
    // returns (native struct write) or reuses recvC (boxed fallback),
    // so compound receivers — index reads, chained field access,
    // constructor expressions — are never evaluated twice. This also
    // pins the receiver's evaluation ahead of the RHS, matching source
    // order for `recv.member = rhs`.
    std::string recvC = receiver->getCExpr(e);
    const CType* recvCt = e.lookupType(recvC);

    // --- Slice 3d (extended): field write on a native C struct --------
    // The receiver's C-level value is the `vyne_*` typedef, not a
    // VyneValue. The generic box-and-set path below would rebuild a
    // throwaway boxed copy via boxAny, mutate the copy, and discard it,
    // silently losing the write. Emit `recv.member = value;` directly
    // and coerce the RHS to the field's primitive kind.
    //
    // Fires for any receiver whose CType carries Struct + nativeCStruct:
    //   - a bare variable (Slice 3d)
    //   - an index read from a vyne_Array_vyne_* container (`mol.atoms[i].x =
    //   1`)
    //   - a field read on another native struct (chained access)
    //   - a struct constructor used directly as a receiver
    if (recvCt && recvCt->kind == CType::Kind::Struct &&
        recvCt->nativeCStruct) {
        const auto* ns = e.getNativeCStruct(recvCt->mangledName);
        if (!ns) {
            throw std::runtime_error(
                "internal: nativeCStruct CType without registry entry "
                "for '" +
                recvCt->mangledName + "' (line " + std::to_string(lineNumber) +
                ")."
            );
        }
        bool found = false;
        for (size_t i = 0; i < ns->fieldNames.size(); ++i) {
            if (ns->fieldNames[i] != memberName)
                continue;
            found = true;
            CType fct = ns->fieldTypes[i];
            std::string rawRhs = rhs->getCExpr(e);
            std::string native =
                coerceToNative(e, rhs.get(), rawRhs, fct.toVType());
            e.emit(recvC + "." + memberName + " = " + native + ";");
            break;
        }
        if (!found) {
            throw std::runtime_error(
                "Compile Error: interface '" + recvCt->mangledName +
                "' has no field '" + memberName + "' (line " +
                std::to_string(lineNumber) + ")."
            );
        }
        return;
    }
    // --- end Slice 3d -------------------------------------------------

    // --- Region escape check ------------------------------------------
    // If the receiver is a variable at a shallower region depth than
    // the current one, and RHS is a non-primitive from a deeper region,
    // we're writing a region-local pointer into outer memory.
    if (e.hasRegion() && receiver->type() == NodeType::VARIABLE) {
        auto* var = static_cast<VariableNode*>(receiver.get());
        std::string rs = var->getOriginalName();
        std::replace(rs.begin(), rs.end(), '.', '_');
        std::string prefix = e.getActiveFunctionPrefix();
        std::string rname =
            prefix.empty() ? ("v_" + rs) : ("v_" + prefix + "_" + rs);
        int recvDepth = e.lookupLocalRegionDepth(rname);
        if (recvDepth < 0)
            recvDepth = 0;
        if (recvDepth < e.currentRegionDepth()) {
            bool safe = (rhs->type() == NodeType::STRING);
            if (!safe) {
                VType st = rhs->getStaticType();
                safe =
                    (st == VType::Int64 || st == VType::Float64 ||
                     st == VType::Bool || st == VType::Null);
            }
            // NEW: same reasoning as IndexAssignmentNode — a read whose
            // root variable lives outside the region cannot produce a
            // region-local pointer.
            if (!safe && rhsIsRootedOutsideRegion(e, rhs.get())) {
                safe = true;
            }
            if (!safe) {
                throw std::runtime_error(
                    "Escape Error (VNE-070): member assignment writes a "
                    "region-local value into '" +
                    rname + "." + memberName +
                    "' (declared outside the region) at line " +
                    std::to_string(lineNumber) + "."
                );
            }
        }
    }
    // --- End escape check --------------------------------------------

    std::string val = e.boxAny(rhs->getCExpr(e));

    if (receiver->type() == NodeType::VARIABLE) {
        auto* var = static_cast<VariableNode*>(receiver.get());
        std::string modName = var->getOriginalName();

        if (e.isGroup(modName)) {
            std::string name = "v_" + modName + "_" + memberName;
            std::replace(name.begin(), name.end(), '.', '_');
            e.emit(name + " = " + val + ";");
            return;
        }

        if (modName == "self") {
            uint32_t fid = StringPool::intern(memberName);
            e.emit(
                "vyne_struct_set(v_self, " + std::to_string(fid) + ", \"" +
                memberName + "\", " + val + ");"
            );
            return;
        }
    }

    std::string recv = e.boxAny(recvC);
    uint32_t fid = StringPool::intern(memberName);
    e.emit(
        "vyne_struct_set(" + recv + ", " + std::to_string(fid) + ", \"" +
        memberName + "\", " + val + ");"
    );
}

std::string MemberAssignmentNode::getCExpr(C_Emitter& e) const {
    compile(e);
    if (receiver->type() == NodeType::VARIABLE) {
        auto* var = static_cast<VariableNode*>(receiver.get());
        std::string name = "v_" + var->getOriginalName() + "_" + memberName;
        std::replace(name.begin(), name.end(), '.', '_');
        return name;
    }
    return receiver->getCExpr(e) + "_" + memberName;
}
