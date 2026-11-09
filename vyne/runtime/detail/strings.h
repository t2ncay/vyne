// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tuncay Gafarli
//
// This file is part of the Vyne runtime library, distributed under the
// MIT License. See LICENSE-MIT at the repository root for the full text.

#pragma once
#include "maps.h"
// ============================================================================
// STRING OPERATIONS
// ============================================================================

static inline VyneValue vyne_string_substr(VyneValue str, int64_t start, int64_t count) {
    if (str.type != V_STRING) return vyne_null();
    const char* s = str.as.str;
    int64_t len = (int64_t)strlen(s);
    if (start < 0) start = 0;
    if (start > len) start = len;
    if (count < 0) {
        // till end
        return vyne_string(s + start);
    }
    if (start + count > len) count = len - start;
    char* buf = (char*)arena_alloc(count + 1);
    memcpy(buf, s + start, count);
    buf[count] = '\0';
    return vyne_string_own(buf);
}

static inline VyneValue vyne_string_find(VyneValue str, VyneValue target) {
    if (str.type != V_STRING || target.type != V_STRING) return vyne_int(-1);
    const char* found = strstr(str.as.str, target.as.str);
    if (!found) return vyne_int(-1);
    return vyne_int((int64_t)(found - str.as.str));
}

static inline VyneValue vyne_string_uppercase(VyneValue str) {
    if (str.type != V_STRING) return vyne_null();
    size_t len = strlen(str.as.str);
    char* buf = (char*)arena_alloc(len + 1);
    for (size_t i = 0; i < len; i++) buf[i] = (char)toupper((unsigned char)str.as.str[i]);
    buf[len] = '\0';
    return vyne_string_own(buf);
}

static inline VyneValue vyne_string_lowercase(VyneValue str) {
    if (str.type != V_STRING) return vyne_null();
    size_t len = strlen(str.as.str);
    char* buf = (char*)arena_alloc(len + 1);
    for (size_t i = 0; i < len; i++) buf[i] = (char)tolower((unsigned char)str.as.str[i]);
    buf[len] = '\0';
    return vyne_string_own(buf);
}

static inline VyneValue vyne_string_trim(VyneValue str) {
    if (str.type != V_STRING) return vyne_null();
    const char* s = str.as.str;
    size_t len = strlen(s);
    size_t start = 0;
    while (start < len && isspace((unsigned char)s[start])) start++;
    size_t end = len;
    while (end > start && isspace((unsigned char)s[end-1])) end--;
    size_t new_len = end - start;
    char* buf = (char*)arena_alloc(new_len + 1);
    memcpy(buf, s + start, new_len);
    buf[new_len] = '\0';
    return vyne_string_own(buf);
}

static inline VyneValue vyne_string_replace(VyneValue str, VyneValue old_s, VyneValue new_s) {
    if (str.type != V_STRING || old_s.type != V_STRING || new_s.type != V_STRING) return vyne_null();
    const char* src = str.as.str;
    const char* o = old_s.as.str;
    const char* n = new_s.as.str;
    size_t olen = strlen(o);
    if (olen == 0) return str;
    size_t nlen = strlen(n);

    // Fast path: if the target and replacement are the same length,
    // we can do a single-pass overwrite into a freshly allocated buffer
    // without a counting pass.
    size_t src_len = strlen(src);

    if (nlen == olen) {
        // No size change possible — but we still must not mutate `src`
        // because it might be a static literal. So copy into the arena
        // first, then replace in place.
        char* buf = (char*)arena_alloc(src_len + 1);
        memcpy(buf, src, src_len + 1);
        char* p = buf;
        while ((p = strstr(p, o)) != NULL) {
            memcpy(p, n, nlen);
            p += nlen;
        }
        return vyne_string_own(buf);
    }

    // General case: count first (unavoidable to size the output), then copy.
    size_t count = 0;
    const char* p = src;
    while ((p = strstr(p, o)) != NULL) { count++; p += olen; }

    size_t result_len = (nlen > olen)
        ? src_len + count * (nlen - olen)
        : src_len - count * (olen - nlen);

    char* buf = (char*)arena_alloc(result_len + 1);
    size_t pos = 0;
    p = src;
    while (1) {
        const char* found = strstr(p, o);
        if (!found) {
            size_t tail = strlen(p);
            memcpy(buf + pos, p, tail);
            pos += tail;
            break;
        }
        size_t head = found - p;
        memcpy(buf + pos, p, head); pos += head;
        memcpy(buf + pos, n, nlen); pos += nlen;
        p = found + olen;
    }
    buf[pos] = '\0';
    return vyne_string_own(buf);
}

