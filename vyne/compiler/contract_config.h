// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Tuncay Gafarli
//
// This file is part of the Vyne compiler.
//
// Compile-time contract policy. `ruleset { contracts = ... }` sets the
// mode at parse time; codegen reads it when emitting `requires`,
// `ensures`, and `@invariant` checks. The mode is process-global
// because a ruleset applies to the translation unit, matching the
// existing `dynamic_casting` / `type_check` ruleset behaviour.

#pragma once

enum class ContractMode {
    Off,      // emit no checks (release)
    Check,    // emit vyne_contract_assert, no evidence log (default)
    Evidence, // emit asserts + machine-readable pass/fail log
};

inline ContractMode& contractModeRef() {
    static ContractMode mode = ContractMode::Check;
    return mode;
}

inline ContractMode contractMode() {
    return contractModeRef();
}

inline void setContractMode(ContractMode mode) {
    contractModeRef() = mode;
}

inline void resetContractMode() {
    contractModeRef() = ContractMode::Check;
}
