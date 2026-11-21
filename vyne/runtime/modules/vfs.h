// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tuncay Gafarli
//
// This file is part of the Vyne runtime library, distributed under the
// MIT License. See LICENSE-MIT at the repository root for the full text.

/* vyne/runtime/modules/vfs.h
 * -------------------------------------------------------------------
 * Vyne runtime vfs module — transpiler target.
 *
 * File-system primitives: read, write, list, walk, glob, path
 * manipulation. Paths are treated as UTF-8 byte strings. On Windows
 * the narrow-char APIs are used (_stat, _mkdir, FindFirstFileA),
 * which is what the rest of the runtime already assumes; non-ASCII
 * paths on Windows will not round-trip through the ANSI code page.
 * If that matters, a wide-char path is a follow-up.
 *
 * Naming convention: vfs_<name>
 *
 *   Read      : read, read_lines, read_bytes
 *   Write     : write, write_atomic, write_lines, write_bytes, append
 *   Metadata  : exists, is_file, is_dir, size, mtime, atime
 *   Directory : list_dir, walk, glob, mkdir, mkdir_p,
 *               remove, remove_dir
 *   Transfer  : copy, move
 *   Path      : basename, dirname, stem, extension, join
 *
 * Errors the caller can reasonably recover from (missing file,
 * permission denied) return `null` or `false`. Argument-type
 * violations abort with a diagnostic, matching vcore.
 * ------------------------------------------------------------------- */

#ifndef VYNE_VFS_RT_H
#define VYNE_VFS_RT_H

#include "../vyne_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>

/* ---------- platform shims ---------------------------------------- */
#ifdef _WIN32
    #ifndef _WINDOWS_
        #define WIN32_LEAN_AND_MEAN
        #define NOMINMAX
        #define NOGDI
        #define NOUSER
        #include <windows.h>
    #endif
    #include <direct.h>
    #include <io.h>

    #define VFS_STAT(p, s)    _stat((p), (s))
    #define VFS_STAT_T        struct _stat
    #define VFS_MKDIR(p)      _mkdir(p)
    #define VFS_UNLINK(p)     _unlink(p)
    #define VFS_RMDIR(p)      _rmdir(p)
    #define VFS_ISDIR(m)      (((m) & _S_IFDIR) != 0)
    #define VFS_ISREG(m)      (((m) & _S_IFREG) != 0)
    #define VFS_PATH_SEP      '\\'
    #define VFS_IS_SEP(c)     ((c) == '/' || (c) == '\\')
#else
    #include <unistd.h>
    #include <dirent.h>
    #include <errno.h>

    #define VFS_STAT(p, s)    stat((p), (s))
    #define VFS_STAT_T        struct stat
    #define VFS_MKDIR(p)      mkdir((p), 0755)
    #define VFS_UNLINK(p)     unlink(p)
    #define VFS_RMDIR(p)      rmdir(p)
    #define VFS_ISDIR(m)      S_ISDIR(m)
    #define VFS_ISREG(m)      S_ISREG(m)
    #define VFS_PATH_SEP      '/'
    #define VFS_IS_SEP(c)     ((c) == '/')
#endif

/* ---------------------------------------------------------------- */
/* Metadata                                                          */
/* ---------------------------------------------------------------- */

static inline VyneValue vfs_exists(VyneValue path_val) {
    if (path_val.type != V_STRING || !path_val.as.str) return vyne_bool(false);
    VFS_STAT_T st;
    return vyne_bool(VFS_STAT(path_val.as.str, &st) == 0);
}

static inline VyneValue vfs_is_file(VyneValue path_val) {
    if (path_val.type != V_STRING || !path_val.as.str) return vyne_bool(false);
    VFS_STAT_T st;
    if (VFS_STAT(path_val.as.str, &st) != 0) return vyne_bool(false);
    return vyne_bool(VFS_ISREG(st.st_mode));
}

static inline VyneValue vfs_is_dir(VyneValue path_val) {
    if (path_val.type != V_STRING || !path_val.as.str) return vyne_bool(false);
    VFS_STAT_T st;
    if (VFS_STAT(path_val.as.str, &st) != 0) return vyne_bool(false);
    return vyne_bool(VFS_ISDIR(st.st_mode));
}

static inline VyneValue vfs_size(VyneValue path_val) {
    if (path_val.type != V_STRING || !path_val.as.str) return vyne_int(0);
    VFS_STAT_T st;
    if (VFS_STAT(path_val.as.str, &st) != 0) return vyne_int(0);
    if (!VFS_ISREG(st.st_mode)) return vyne_int(0);
    return vyne_int((int64_t)st.st_size);
}

static inline VyneValue vfs_mtime(VyneValue path_val) {
    if (path_val.type != V_STRING || !path_val.as.str) return vyne_int(0);
    VFS_STAT_T st;
    if (VFS_STAT(path_val.as.str, &st) != 0) return vyne_int(0);
    return vyne_int((int64_t)st.st_mtime);
}

static inline VyneValue vfs_atime(VyneValue path_val) {
    if (path_val.type != V_STRING || !path_val.as.str) return vyne_int(0);
    VFS_STAT_T st;
    if (VFS_STAT(path_val.as.str, &st) != 0) return vyne_int(0);
    return vyne_int((int64_t)st.st_atime);
}

/* ---------------------------------------------------------------- */
/* Reading                                                           */
/* ---------------------------------------------------------------- */

static inline VyneValue vfs_read(VyneValue path_val) {
    if (path_val.type != V_STRING || !path_val.as.str) {
        fprintf(stderr, "Runtime error: vfs.read() expects a String path.\n");
        exit(1);
    }
    FILE* f = fopen(path_val.as.str, "rb");
    if (!f) return vyne_null();

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return vyne_null(); }
    long n = ftell(f);
    if (n < 0) { fclose(f); return vyne_null(); }
    rewind(f);

    char* buf = (char*)arena_alloc((size_t)n + 1);
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[got] = '\0';
    return vyne_string_own(buf);
}

static inline VyneValue vfs_read_lines(VyneValue path_val) {
    VyneValue content = vfs_read(path_val);
    VyneValue arr = vyne_array_create(0);
    if (content.type != V_STRING || !content.as.str) return arr;

    const char* s = content.as.str;
    const char* start = s;
    while (*s) {
        if (*s == '\n') {
            size_t len = (size_t)(s - start);
            if (len > 0 && start[len - 1] == '\r') len--;
            char* buf = (char*)arena_alloc(len + 1);
            memcpy(buf, start, len);
            buf[len] = '\0';
            vyne_array_push(arr, vyne_string_own(buf));
            start = s + 1;
        }
        s++;
    }
    /* Trailing line without a final newline. */
    if (s > start) {
        size_t len = (size_t)(s - start);
        if (len > 0 && start[len - 1] == '\r') len--;
        char* buf = (char*)arena_alloc(len + 1);
        memcpy(buf, start, len);
        buf[len] = '\0';
        vyne_array_push(arr, vyne_string_own(buf));
    }
    return arr;
}

/* Boxed Array<Int64>, one element per byte, values 0..255.
 * Not a typed array: the boxed iteration path in the emitter only
 * handles V_ARRAY, so returning V_I64_ARRAY here would silently fail
 * to iterate in `through`. */
static inline VyneValue vfs_read_bytes(VyneValue path_val) {
    if (path_val.type != V_STRING || !path_val.as.str) {
        fprintf(stderr, "Runtime error: vfs.read_bytes() expects a String path.\n");
        exit(1);
    }
    VyneValue arr = vyne_array_create(0);
    FILE* f = fopen(path_val.as.str, "rb");
    if (!f) return arr;

    int c;
    while ((c = fgetc(f)) != EOF) {
        vyne_array_push(arr, vyne_int((int64_t)(unsigned char)c));
    }
    fclose(f);
    return arr;
}

/* ---------------------------------------------------------------- */
/* Writing                                                           */
/* ---------------------------------------------------------------- */

static inline VyneValue vfs_write(VyneValue path_val, VyneValue content_val) {
    if (path_val.type != V_STRING || !path_val.as.str) {
        fprintf(stderr, "Runtime error: vfs.write() expects a String path.\n");
        exit(1);
    }
    const char* content = "";
    size_t len = 0;
    if (content_val.type == V_STRING && content_val.as.str) {
        content = content_val.as.str;
        len = strlen(content);
    }
    FILE* f = fopen(path_val.as.str, "wb");
    if (!f) return vyne_bool(false);
    size_t wrote = fwrite(content, 1, len, f);
    int closed = fclose(f);
    return vyne_bool(wrote == len && closed == 0);
}

/* Write a private sibling first, then atomically replace the destination.
 * A write/close/replace failure never opens or truncates the destination. */
static inline VyneValue vfs_write_atomic(VyneValue path_val, VyneValue content_val) {
    if (path_val.type != V_STRING || !path_val.as.str) {
        fprintf(stderr, "Runtime error: vfs.write_atomic() expects a String path.\n");
        exit(1);
    }
    const char* content = content_val.type == V_STRING && content_val.as.str ? content_val.as.str : "";
    size_t len = strlen(content), path_len = strlen(path_val.as.str);
    if (path_len > SIZE_MAX - 80) return vyne_bool(false);
    char* temporary = (char*)malloc(path_len + 80);
    if (!temporary) return vyne_bool(false);
    int fd = -1;
#ifdef _WIN32
    static volatile LONG sequence;
    for (int attempt = 0; attempt < 64; ++attempt) {
        unsigned long serial = (unsigned long)InterlockedIncrement(&sequence);
        snprintf(temporary, path_len + 80, "%s.vyne-tmp-%lu-%lu", path_val.as.str,
                 (unsigned long)GetCurrentProcessId(), serial);
        fd = _open(temporary, _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY, _S_IREAD | _S_IWRITE);
        if (fd >= 0 || errno != EEXIST) break;
    }
#else
    memcpy(temporary, path_val.as.str, path_len);
    memcpy(temporary + path_len, ".vyne-tmp-XXXXXX", sizeof(".vyne-tmp-XXXXXX"));
    fd = mkstemp(temporary);
#endif
    if (fd < 0) { free(temporary); return vyne_bool(false); }
#ifdef _WIN32
    FILE* f = _fdopen(fd, "wb");
#else
    FILE* f = fdopen(fd, "wb");
#endif
    if (!f) {
#ifdef _WIN32
        _close(fd);
#else
        close(fd);
#endif
        VFS_UNLINK(temporary); free(temporary); return vyne_bool(false);
    }
    size_t wrote = fwrite(content, 1, len, f);
    int failed = wrote != len || ferror(f);
    int closed = fclose(f);
    if (failed || closed != 0) {
        VFS_UNLINK(temporary); free(temporary); return vyne_bool(false);
    }
#ifdef _WIN32
    int replaced = MoveFileExA(temporary, path_val.as.str, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    int replaced = rename(temporary, path_val.as.str) == 0;
#endif
    if (!replaced) VFS_UNLINK(temporary);
    free(temporary);
    return vyne_bool(replaced);
}

static inline VyneValue vfs_append(VyneValue path_val, VyneValue content_val) {
    if (path_val.type != V_STRING || !path_val.as.str) {
        fprintf(stderr, "Runtime error: vfs.append() expects a String path.\n");
        exit(1);
    }
    const char* content = "";
    size_t len = 0;
    if (content_val.type == V_STRING && content_val.as.str) {
        content = content_val.as.str;
        len = strlen(content);
    }
    FILE* f = fopen(path_val.as.str, "ab");
    if (!f) return vyne_bool(false);
    size_t wrote = fwrite(content, 1, len, f);
    fclose(f);
    return vyne_bool(wrote == len);
}

static inline VyneValue vfs_write_lines(VyneValue path_val, VyneValue lines_val) {
    if (path_val.type != V_STRING || !path_val.as.str) {
        fprintf(stderr, "Runtime error: vfs.write_lines() expects a String path.\n");
        exit(1);
    }
    if (lines_val.type != V_ARRAY) {
        fprintf(stderr, "Runtime error: vfs.write_lines() expects an Array.\n");
        exit(1);
    }
    FILE* f = fopen(path_val.as.str, "wb");
    if (!f) return vyne_bool(false);

    VyneArray* arr = lines_val.as.arr;
    for (int i = 0; i < arr->size; i++) {
        VyneValue e = arr->elements[i];
        if (e.type == V_STRING && e.as.str) {
            fwrite(e.as.str, 1, strlen(e.as.str), f);
        }
        fputc('\n', f);
    }
    fclose(f);
    return vyne_bool(true);
}

static inline VyneValue vfs_write_bytes(VyneValue path_val, VyneValue arr_val) {
    if (path_val.type != V_STRING || !path_val.as.str) {
        fprintf(stderr, "Runtime error: vfs.write_bytes() expects a String path.\n");
        exit(1);
    }
    if (arr_val.type != V_ARRAY) {
        fprintf(stderr, "Runtime error: vfs.write_bytes() expects an Array.\n");
        exit(1);
    }
    FILE* f = fopen(path_val.as.str, "wb");
    if (!f) return vyne_bool(false);

    VyneArray* arr = arr_val.as.arr;
    for (int i = 0; i < arr->size; i++) {
        VyneValue e = arr->elements[i];
        int byte = 0;
        if      (e.type == V_INT64)   byte = (int)(e.as.i64 & 0xFF);
        else if (e.type == V_FLOAT64) byte = (int)((int64_t)e.as.f64 & 0xFF);
        fputc(byte, f);
    }
    fclose(f);
    return vyne_bool(true);
}

/* ---------------------------------------------------------------- */
/* Directory listing                                                 */
/* ---------------------------------------------------------------- */

static inline VyneValue vfs_list_dir(VyneValue path_val) {
    VyneValue arr = vyne_array_create(0);
    if (path_val.type != V_STRING || !path_val.as.str) return arr;

#ifdef _WIN32
    char pattern[1024];
    size_t plen = strlen(path_val.as.str);
    if (plen + 3 >= sizeof(pattern)) return arr;
    memcpy(pattern, path_val.as.str, plen);
    if (plen > 0 && !VFS_IS_SEP(pattern[plen - 1])) pattern[plen++] = '\\';
    pattern[plen++] = '*';
    pattern[plen]   = '\0';

    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return arr;
    do {
        if (fd.cFileName[0] == '.' &&
            (fd.cFileName[1] == '\0' ||
             (fd.cFileName[1] == '.' && fd.cFileName[2] == '\0'))) continue;
        vyne_array_push(arr, vyne_string(fd.cFileName));
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR* d = opendir(path_val.as.str);
    if (!d) return arr;
    struct dirent* ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.' &&
            (ent->d_name[1] == '\0' ||
             (ent->d_name[1] == '.' && ent->d_name[2] == '\0'))) continue;
        vyne_array_push(arr, vyne_string(ent->d_name));
    }
    closedir(d);
#endif
    return arr;
}

/* ---------------------------------------------------------------- */
/* Directory management                                              */
/* ---------------------------------------------------------------- */

static inline VyneValue vfs_mkdir(VyneValue path_val) {
    if (path_val.type != V_STRING || !path_val.as.str) return vyne_bool(false);
    if (VFS_MKDIR(path_val.as.str) == 0) return vyne_bool(true);
    VFS_STAT_T st;
    if (VFS_STAT(path_val.as.str, &st) == 0 && VFS_ISDIR(st.st_mode))
        return vyne_bool(true);
    return vyne_bool(false);
}

/* Equivalent to `mkdir -p`: creates every intermediate directory.
 * Intermediate failures are ignored — the final stat decides success. */
static inline VyneValue vfs_mkdir_p(VyneValue path_val) {
    if (path_val.type != V_STRING || !path_val.as.str) return vyne_bool(false);
    const char* path = path_val.as.str;
    size_t len = strlen(path);
    if (len == 0) return vyne_bool(false);

    char* buf = (char*)arena_alloc(len + 1);
    memcpy(buf, path, len + 1);

    for (size_t i = 1; i < len; i++) {
        if (VFS_IS_SEP(buf[i])) {
            char save = buf[i];
            buf[i] = '\0';
            VFS_MKDIR(buf);
            buf[i] = save;
        }
    }
    VFS_MKDIR(buf);

    VFS_STAT_T st;
    if (VFS_STAT(buf, &st) == 0 && VFS_ISDIR(st.st_mode))
        return vyne_bool(true);
    return vyne_bool(false);
}

static inline VyneValue vfs_remove(VyneValue path_val) {
    if (path_val.type != V_STRING || !path_val.as.str) return vyne_bool(false);
    return vyne_bool(VFS_UNLINK(path_val.as.str) == 0);
}

static inline void _vfs_remove_dir_recursive(const char* dir);

/* Recursive delete. Removes everything under `path`, then the directory
 * itself. Symlinks are not followed. */
static inline VyneValue vfs_remove_dir(VyneValue path_val) {
    if (path_val.type != V_STRING || !path_val.as.str) return vyne_bool(false);
    _vfs_remove_dir_recursive(path_val.as.str);
    VFS_STAT_T st;
    return vyne_bool(VFS_STAT(path_val.as.str, &st) != 0);
}

/* ---------------------------------------------------------------- */
/* Recursive traversal                                               */
/* ---------------------------------------------------------------- */

static inline void _vfs_remove_dir_recursive(const char* dir) {
    VyneValue entries = vfs_list_dir(vyne_string(dir));
    if (entries.type == V_ARRAY) {
        VyneArray* arr = entries.as.arr;
        for (int i = 0; i < arr->size; i++) {
            VyneValue e = arr->elements[i];
            if (e.type != V_STRING || !e.as.str) continue;
            size_t blen = strlen(dir);
            size_t elen = strlen(e.as.str);
            char* full = (char*)arena_alloc(blen + 1 + elen + 1);
            memcpy(full, dir, blen);
            full[blen] = VFS_PATH_SEP;
            memcpy(full + blen + 1, e.as.str, elen);
            full[blen + 1 + elen] = '\0';

            VFS_STAT_T st;
            if (VFS_STAT(full, &st) == 0 && VFS_ISDIR(st.st_mode)) {
                _vfs_remove_dir_recursive(full);
            } else {
                VFS_UNLINK(full);
            }
        }
    }
    VFS_RMDIR(dir);
}

static inline void _vfs_walk_recursive(const char* base, VyneValue out) {
    VyneValue entries = vfs_list_dir(vyne_string(base));
    if (entries.type != V_ARRAY) return;
    VyneArray* arr = entries.as.arr;
    for (int i = 0; i < arr->size; i++) {
        VyneValue e = arr->elements[i];
        if (e.type != V_STRING || !e.as.str) continue;
        size_t blen = strlen(base);
        size_t elen = strlen(e.as.str);
        char* full = (char*)arena_alloc(blen + 1 + elen + 1);
        memcpy(full, base, blen);
        full[blen] = VFS_PATH_SEP;
        memcpy(full + blen + 1, e.as.str, elen);
        full[blen + 1 + elen] = '\0';

        vyne_array_push(out, vyne_string_own(full));

        VFS_STAT_T st;
        if (VFS_STAT(full, &st) == 0 && VFS_ISDIR(st.st_mode)) {
            _vfs_walk_recursive(full, out);
        }
    }
}

/* Depth-first, pre-order. Returns full paths, not just names. */
static inline VyneValue vfs_walk(VyneValue path_val) {
    VyneValue out = vyne_array_create(0);
    if (path_val.type != V_STRING || !path_val.as.str) return out;
    _vfs_walk_recursive(path_val.as.str, out);
    return out;
}

/* ---------------------------------------------------------------- */
/* Glob                                                              */
/* ---------------------------------------------------------------- */

/* Recursive wildcard match. `*` matches any run, `?` matches one byte.
 * Both are byte-level, not char-level; multi-byte UTF-8 filenames match
 * by component. No character classes ([a-z]) — they are easy to add
 * but rarely needed and complicate the parser. */
static inline int _vfs_glob_match(const char* pat, const char* str) {
    while (*pat) {
        if (*pat == '*') {
            pat++;
            if (!*pat) return 1;
            while (*str) {
                if (_vfs_glob_match(pat, str)) return 1;
                str++;
            }
            return 0;
        }
        if (*pat == '?') {
            if (!*str) return 0;
            pat++; str++;
            continue;
        }
        if (*pat != *str) return 0;
        pat++; str++;
    }
    return *str == '\0';
}

/* Non-recursive: matches against the immediate entries of `dir`.
 * For recursive globbing, walk() first, then filter the result. */
static inline VyneValue vfs_glob(VyneValue dir_val, VyneValue pat_val) {
    if (dir_val.type != V_STRING || !dir_val.as.str) return vyne_array_create(0);
    if (pat_val.type != V_STRING || !pat_val.as.str) return vyne_array_create(0);

    VyneValue entries = vfs_list_dir(dir_val);
    VyneValue out = vyne_array_create(0);
    if (entries.type != V_ARRAY) return out;

    VyneArray* arr = entries.as.arr;
    for (int i = 0; i < arr->size; i++) {
        VyneValue e = arr->elements[i];
        if (e.type != V_STRING || !e.as.str) continue;
        if (_vfs_glob_match(pat_val.as.str, e.as.str)) {
            vyne_array_push(out, e);
        }
    }
    return out;
}

/* ---------------------------------------------------------------- */
/* Copy / move                                                       */
/* ---------------------------------------------------------------- */

static inline VyneValue vfs_copy(VyneValue src_val, VyneValue dst_val) {
    VyneValue content = vfs_read(src_val);
    if (content.type != V_STRING) return vyne_bool(false);
    return vfs_write(dst_val, content);
}

/* Try rename() first (atomic, same-filesystem). Fall back to copy +
 * unlink on cross-device moves, matching `mv`'s behavior. */
static inline VyneValue vfs_move(VyneValue src_val, VyneValue dst_val) {
    if (src_val.type != V_STRING || !src_val.as.str) return vyne_bool(false);
    if (dst_val.type != V_STRING || !dst_val.as.str) return vyne_bool(false);
    if (rename(src_val.as.str, dst_val.as.str) == 0) return vyne_bool(true);

    VyneValue ok = vfs_copy(src_val, dst_val);
    if (ok.type == V_BOOL && ok.as.i64) {
        VFS_UNLINK(src_val.as.str);
        return vyne_bool(true);
    }
    return vyne_bool(false);
}

/* ---------------------------------------------------------------- */
/* Path manipulation                                                 */
/* ---------------------------------------------------------------- */

static inline VyneValue vfs_basename(VyneValue path_val) {
    if (path_val.type != V_STRING || !path_val.as.str) return vyne_string("");
    const char* s = path_val.as.str;
    size_t len = strlen(s);
    size_t end = len;
    while (end > 0 && VFS_IS_SEP(s[end - 1])) end--;
    size_t start = end;
    while (start > 0 && !VFS_IS_SEP(s[start - 1])) start--;
    size_t n = end - start;
    char* buf = (char*)arena_alloc(n + 1);
    memcpy(buf, s + start, n);
    buf[n] = '\0';
    return vyne_string_own(buf);
}

static inline VyneValue vfs_dirname(VyneValue path_val) {
    if (path_val.type != V_STRING || !path_val.as.str) return vyne_string(".");
    const char* s = path_val.as.str;
    size_t len = strlen(s);
    size_t end = len;
    while (end > 0 && VFS_IS_SEP(s[end - 1])) end--;
    size_t i = end;
    while (i > 0 && !VFS_IS_SEP(s[i - 1])) i--;
    if (i == 0) return vyne_string(".");
    size_t last = i - 1;
    if (last == 0) {
        char root[2] = { s[0], '\0' };
        return vyne_string(root);
    }
    char* buf = (char*)arena_alloc(last + 1);
    memcpy(buf, s, last);
    buf[last] = '\0';
    return vyne_string_own(buf);
}

static inline VyneValue vfs_stem(VyneValue path_val) {
    VyneValue base = vfs_basename(path_val);
    if (base.type != V_STRING || !base.as.str) return vyne_string("");
    const char* s = base.as.str;
    const char* dot = strrchr(s, '.');
    if (!dot || dot == s) return base;
    size_t n = (size_t)(dot - s);
    char* buf = (char*)arena_alloc(n + 1);
    memcpy(buf, s, n);
    buf[n] = '\0';
    return vyne_string_own(buf);
}

/* Includes the leading dot: "foo.txt" -> ".txt", "Makefile" -> "". */
static inline VyneValue vfs_extension(VyneValue path_val) {
    VyneValue base = vfs_basename(path_val);
    if (base.type != V_STRING || !base.as.str) return vyne_string("");
    const char* dot = strrchr(base.as.str, '.');
    if (!dot || dot == base.as.str) return vyne_string("");
    return vyne_string(dot);
}

/* If `b` is absolute, returns `b`. Otherwise joins with the platform
 * separator, avoiding a doubled separator if `a` already ends in one. */
static inline VyneValue vfs_join(VyneValue a_val, VyneValue b_val) {
    const char* sa = (a_val.type == V_STRING && a_val.as.str) ? a_val.as.str : "";
    const char* sb = (b_val.type == V_STRING && b_val.as.str) ? b_val.as.str : "";

    if (VFS_IS_SEP(sb[0])) return vyne_string(sb);
#ifdef _WIN32
    if (sb[0] != '\0' && sb[1] == ':') return vyne_string(sb);
#endif

    size_t la = strlen(sa);
    size_t lb = strlen(sb);
    if (la == 0) return vyne_string(sb);

    int need_sep = !VFS_IS_SEP(sa[la - 1]);

    char* buf = (char*)arena_alloc(la + (need_sep ? 1 : 0) + lb + 1);
    memcpy(buf, sa, la);
    size_t pos = la;
    if (need_sep) buf[pos++] = VFS_PATH_SEP;
    memcpy(buf + pos, sb, lb);
    buf[pos + lb] = '\0';
    return vyne_string_own(buf);
}

#endif /* VYNE_VFS_RT_H */
