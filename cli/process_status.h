// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Tuncay Gafarli

#pragma once

#ifndef _WIN32
#include <sys/wait.h>
#endif

namespace Vyne {

// system() returns a child exit code on Windows, but an encoded wait status on
// POSIX. Returning the latter from main can turn exit(1) into exit(0).
inline int decodeSystemExitStatus(int status) {
    if (status == -1)
        return 1; // The command processor could not be started or waited for.
#ifdef _WIN32
    return status;
#else
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    if (WIFSIGNALED(status))
        return 128 + WTERMSIG(status);
    return 1;
#endif
}

} // namespace Vyne
