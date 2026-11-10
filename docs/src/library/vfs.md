---
id: vfs
title: VFS
group: Standard Library
icon: fa-folder-open
order: 5
description: Filesystem reads, writes, directory walking, globbing, and path manipulation.
---

# VFS

`vfs` is the desktop filesystem module. Import it with `use native vfs;`. It has no AVR implementation; the Uno has no filesystem.

## Reading

| Function | Result | Notes |
| :--- | :--- | :--- |
| `vfs.read(path)` | `String` / `null` | Whole file in binary mode; `null` on failure. |
| `vfs.read_lines(path)` | `Array` | Lines without `\n` (and without a trailing `\r`). |
| `vfs.read_bytes(path)` | `Array` | Boxed `Int64` bytes, `0..255`. |

```vyne
use native vfs;

text := vfs.read("data/input.csv");
lines := vfs.read_lines("data/input.txt");
bytes := vfs.read_bytes("data/model.bin");
```

## Writing

| Function | Result | Notes |
| :--- | :--- | :--- |
| `vfs.write(path, text)` | `Bool` | Truncate and write a string. |
| `vfs.append(path, text)` | `Bool` | Append a string. |
| `vfs.write_lines(path, lines)` | `Bool` | Write each line followed by `\n`. |
| `vfs.write_bytes(path, bytes)` | `Bool` | Write `Int64` bytes masked to `0..255`. |

```vyne
vfs.write("out.txt", "hello\n");
vfs.append("out.txt", "world\n");
vfs.write_lines("out.txt", ["a", "b", "c"]);
```

## Metadata

| Function | Result |
| :--- | :--- |
| `vfs.exists(path)` | `Bool` |
| `vfs.is_file(path)` | `Bool` |
| `vfs.is_dir(path)` | `Bool` |
| `vfs.size(path)` | `Int64` bytes; `0` when unavailable |
| `vfs.mtime(path)` / `vfs.atime(path)` | `Int64` epoch seconds |

## Directories

| Function | Result | Notes |
| :--- | :--- | :--- |
| `vfs.list_dir(path)` | `Array` | Entry names, `.` and `..` omitted. |
| `vfs.walk(path)` | `Array` | Recursive file list. |
| `vfs.glob(dir, pattern)` | `Array` | Glob match against entry names. |
| `vfs.mkdir(path)` | `Bool` | Create one directory. |
| `vfs.mkdir_p(path)` | `Bool` | Create a directory tree. |
| `vfs.remove(path)` | `Bool` | Remove a file. |
| `vfs.remove_dir(path)` | `Bool` | Remove a directory recursively. |
| `vfs.copy(src, dst)` | `Bool` | Copy a file. |
| `vfs.move(src, dst)` | `Bool` | Move or rename. `rename` is an alias. |

```vyne
vfs.mkdir_p("build");
for_name := vfs.glob("data", "*.csv");
vfs.copy("data/input.csv", "build/input.csv");
```

## Path operations

| Function | Result |
| :--- | :--- |
| `vfs.basename(path)` | Last path component |
| `vfs.dirname(path)` | Parent directory |
| `vfs.stem(path)` | Filename without extension |
| `vfs.extension(path)` | Extension including the dot |
| `vfs.join(a, b)` | Joined path |

```vyne
out(vfs.join("data", "train.csv"));   # data/train.csv
out(vfs.stem("data/train.csv"));      # train
out(vfs.extension("data/train.csv")); # .csv
```

## Memory and errors

- `vfs.read` allocates the file contents in the arena. Inside a region, the string is reclaimed at the closing brace.
- File-open failures return `null` (reads) or `false` (writes), not exceptions. Check the result when the file is required.
- Paths are UTF-8. Windows separators are handled internally; use `/` or `\` as appropriate.

## See also

- [VCsv](page:vcsv)
- [VImage](page:vimage)
- [VCore](page:vcore)
