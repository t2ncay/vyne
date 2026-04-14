#include "vfs.h"

/**
 * VFS Native Method Implementations
 */

namespace VFsNative {
    namespace fs = std::filesystem;

    std::string ensureStringArg(const std::vector<Value>& args, size_t idx, const char* fnName) {
        if (idx >= args.size()) {
            throw std::runtime_error(
                std::string("Argument Error: vfs.") + fnName + "() missing argument #" + std::to_string(idx + 1) + "."
            );
        }

        if (args[idx].getType() != Value::STRING) {
            throw std::runtime_error(
                std::string("Type Error: vfs.") + fnName + "() expects String at argument #" +
                std::to_string(idx + 1) + ", but got " + args[idx].getTypeName() + "."
            );
        }

        return args[idx].asString();
    }

    bool getOptionalBoolArg(const std::vector<Value>& args, size_t idx, bool fallback, const char* fnName) {
        if (idx >= args.size()) return fallback;
        int type = args[idx].getType();
        if (type != Value::INT64 && type != Value::FLOAT64) {
            throw std::runtime_error(
                std::string("Type Error: vfs.") + fnName + "() expects numeric boolean at argument #" +
                std::to_string(idx + 1) + ", but got " + args[idx].getTypeName() + "."
            );
        }
        return args[idx].asInt() != 0;
    }

    Value pwd(std::vector<Value>& args) {
        if (!args.empty()) {
            throw std::runtime_error("Argument Error: vfs.pwd() takes no arguments.");
        }
        return Value(fs::current_path().string());
    }

    Value setCwd(std::vector<Value>& args) {
        if (args.size() != 1) {
            throw std::runtime_error("Argument Error: vfs.set_cwd() expects 1 argument (path).");
        }

        fs::path p = ensureStringArg(args, 0, "set_cwd");
        std::error_code ec;
        fs::current_path(p, ec);
        if (ec) {
            throw std::runtime_error("Runtime Error: vfs.set_cwd() failed: " + ec.message());
        }
        return Value(true);
    }

    Value exists(std::vector<Value>& args) {
        if (args.size() != 1) {
            throw std::runtime_error("Argument Error: vfs.exists() expects 1 argument (path).");
        }
        fs::path p = ensureStringArg(args, 0, "exists");
        std::error_code ec;
        bool ok = fs::exists(p, ec);
        return Value(ok && !ec);
    }

    Value isFile(std::vector<Value>& args) {
        if (args.size() != 1) {
            throw std::runtime_error("Argument Error: vfs.is_file() expects 1 argument (path).");
        }
        fs::path p = ensureStringArg(args, 0, "is_file");
        std::error_code ec;
        bool ok = fs::is_regular_file(p, ec);
        return Value(ok && !ec);
    }

    Value isDir(std::vector<Value>& args) {
        if (args.size() != 1) {
            throw std::runtime_error("Argument Error: vfs.is_dir() expects 1 argument (path).");
        }
        fs::path p = ensureStringArg(args, 0, "is_dir");
        std::error_code ec;
        bool ok = fs::is_directory(p, ec);
        return Value(ok && !ec);
    }

    Value read(std::vector<Value>& args) {
        if (args.size() != 1) {
            throw std::runtime_error("Argument Error: vfs.read() expects 1 argument (path).");
        }

        const std::string path = ensureStringArg(args, 0, "read");
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            throw std::runtime_error("Runtime Error: vfs.read() cannot open file: " + path);
        }

        std::ostringstream buffer;
        buffer << file.rdbuf();
        return Value(buffer.str());
    }

    Value write(std::vector<Value>& args) {
        if (args.size() < 2 || args.size() > 3) {
            throw std::runtime_error("Argument Error: vfs.write() expects 2 or 3 arguments (path, content, append=0|1).");
        }

        const std::string path = ensureStringArg(args, 0, "write");
        const std::string content = ensureStringArg(args, 1, "write");
        bool append = getOptionalBoolArg(args, 2, false, "write");

        std::ios::openmode mode = std::ios::binary | std::ios::out;
        if (append) mode |= std::ios::app;
        else mode |= std::ios::trunc;

        std::ofstream file(path, mode);
        if (!file) {
            throw std::runtime_error("Runtime Error: vfs.write() cannot open file: " + path);
        }

        file << content;
        if (!file.good()) {
            throw std::runtime_error("Runtime Error: vfs.write() failed while writing file: " + path);
        }

        return Value(true);
    }

    Value append(std::vector<Value>& args) {
        if (args.size() != 2) {
            throw std::runtime_error("Argument Error: vfs.append() expects 2 arguments (path, content).");
        }
        const std::string path = ensureStringArg(args, 0, "append");
        const std::string content = ensureStringArg(args, 1, "append");

        std::ofstream file(path, std::ios::binary | std::ios::out | std::ios::app);
        if (!file) {
            throw std::runtime_error("Runtime Error: vfs.append() cannot open file: " + path);
        }

        file << content;
        if (!file.good()) {
            throw std::runtime_error("Runtime Error: vfs.append() failed while writing file: " + path);
        }

        return Value(true);
    }

    Value remove(std::vector<Value>& args) {
        if (args.empty() || args.size() > 2) {
            throw std::runtime_error("Argument Error: vfs.remove() expects 1 or 2 arguments (path, recursive=0|1).");
        }

        fs::path p = ensureStringArg(args, 0, "remove");
        bool recursive = getOptionalBoolArg(args, 1, false, "remove");

        std::error_code ec;
        bool removed = false;
        if (recursive) {
            removed = fs::remove_all(p, ec) > 0;
        } else {
            removed = fs::remove(p, ec);
        }

        if (ec) {
            throw std::runtime_error("Runtime Error: vfs.remove() failed: " + ec.message());
        }
        return Value(removed);
    }

    Value mkdir(std::vector<Value>& args) {
        if (args.empty() || args.size() > 2) {
            throw std::runtime_error("Argument Error: vfs.mkdir() expects 1 or 2 arguments (path, recursive=0|1).");
        }

        fs::path p = ensureStringArg(args, 0, "mkdir");
        bool recursive = getOptionalBoolArg(args, 1, true, "mkdir");

        std::error_code ec;
        bool created = recursive ? fs::create_directories(p, ec) : fs::create_directory(p, ec);
        if (ec) {
            throw std::runtime_error("Runtime Error: vfs.mkdir() failed: " + ec.message());
        }
        return Value(created);
    }

    Value list(std::vector<Value>& args) {
        if (args.size() > 1) {
            throw std::runtime_error("Argument Error: vfs.list() expects 0 or 1 argument (path).");
        }

        fs::path p = (args.empty() ? fs::current_path() : fs::path(ensureStringArg(args, 0, "list")));
        std::error_code ec;
        if (!fs::exists(p, ec) || ec) {
            throw std::runtime_error("Runtime Error: vfs.list() path does not exist: " + p.string());
        }
        if (!fs::is_directory(p, ec) || ec) {
            throw std::runtime_error("Runtime Error: vfs.list() path is not a directory: " + p.string());
        }

        std::vector<Value> items;
        fs::directory_iterator it(p, ec);
        if (ec) {
            throw std::runtime_error("Runtime Error: vfs.list() failed: " + ec.message());
        }

        for (const auto& entry : it) {
            if (ec) {
                throw std::runtime_error("Runtime Error: vfs.list() failed: " + ec.message());
            }
            items.emplace_back(entry.path().filename().string());
        }
        return Value(std::move(items));
    }

    Value fileSize(std::vector<Value>& args) { // Camel case ofc
        if (args.size() != 1) {
            throw std::runtime_error("Argument Error: vfs.file_size() expects 1 argument (path).");
        }
        fs::path p = ensureStringArg(args, 0, "file_size");
        std::error_code ec;
        auto size = fs::file_size(p, ec);
        if (ec) {
            throw std::runtime_error("Runtime Error: vfs.file_size() failed: " + ec.message());
        }
        return Value(static_cast<int64_t>(size));
    }

    Value rename(std::vector<Value>& args) {
        if (args.size() != 2) {
            throw std::runtime_error("Argument Error: vfs.rename() expects 2 arguments (from, to).");
        }
        fs::path from = ensureStringArg(args, 0, "rename");
        fs::path to = ensureStringArg(args, 1, "rename");

        std::error_code ec;
        fs::rename(from, to, ec);
        if (ec) {
            throw std::runtime_error("Runtime Error: vfs.rename() failed: " + ec.message());
        }
        return Value(true);
    }

    Value copy(std::vector<Value>& args) { // Very halal function
        if (args.size() < 2 || args.size() > 3) { // CLEAN BROTHER
            throw std::runtime_error("Argument Error: vfs.copy() expects 2 or 3 arguments (from, to, overwrite=0|1).");
        }
        fs::path from = ensureStringArg(args, 0, "copy");
        fs::path to = ensureStringArg(args, 1, "copy");
        bool overwrite = getOptionalBoolArg(args, 2, false, "copy");

        std::error_code ec;
        fs::copy_options options = fs::copy_options::recursive;
        if (overwrite) options |= fs::copy_options::overwrite_existing;
        else options |= fs::copy_options::skip_existing;

        fs::copy(from, to, options, ec);
        if (ec) {
            throw std::runtime_error("Runtime Error: vfs.copy() failed: " + ec.message());
        }
        return Value(true);
    }

    Value join(std::vector<Value>& args) {
        if (args.empty()) {
            throw std::runtime_error("Argument Error: vfs.join() expects at least 1 argument.");
        }
        fs::path out = ensureStringArg(args, 0, "join");
        for (size_t i = 1; i < args.size(); ++i) {
            out /= ensureStringArg(args, i, "join");
        } // This is hella useful btw
        return Value(out.string());
    }

    Value absolute(std::vector<Value>& args) {
        if (args.size() != 1) {
            throw std::runtime_error("Argument Error: vfs.absolute() expects 1 argument (path).");
        }
        fs::path p = ensureStringArg(args, 0, "absolute");
        std::error_code ec;
        fs::path out = fs::absolute(p, ec);
        if (ec) {
            throw std::runtime_error("Runtime Error: vfs.absolute() failed: " + ec.message());
        }
        return Value(out.string());
    }

    Value canonical(std::vector<Value>& args) {
        if (args.size() != 1) {
            throw std::runtime_error("Argument Error: vfs.canonical() expects 1 argument (path).");
        }
        fs::path p = ensureStringArg(args, 0, "canonical");
        std::error_code ec;
        fs::path out = fs::weakly_canonical(p, ec);
        if (ec) {
            throw std::runtime_error("Runtime Error: vfs.canonical() failed: " + ec.message());
        }
        return Value(out.string());
    }

    Value filename(std::vector<Value>& args) {
        if (args.size() != 1) {
            throw std::runtime_error("Argument Error: vfs.filename() expects 1 argument (path).");
        }
        fs::path p = ensureStringArg(args, 0, "filename");
        return Value(p.filename().string());
    }

    Value stem(std::vector<Value>& args) {
        if (args.size() != 1) {
            throw std::runtime_error("Argument Error: vfs.stem() expects 1 argument (path).");
        }
        fs::path p = ensureStringArg(args, 0, "stem");
        return Value(p.stem().string());
    }

    Value extension(std::vector<Value>& args) {
        if (args.size() != 1) {
            throw std::runtime_error("Argument Error: vfs.extension() expects 1 argument (path).");
        }
        fs::path p = ensureStringArg(args, 0, "extension");
        return Value(p.extension().string());
    }

    Value parent(std::vector<Value>& args) {
        if (args.size() != 1) {
            throw std::runtime_error("Argument Error: vfs.parent() expects 1 argument (path).");
        }
        fs::path p = ensureStringArg(args, 0, "parent");
        return Value(p.parent_path().string());
    }

    Value parse_csv(const std::vector<Value>& args) {
        std::string path = args[0].asString();
        std::ifstream file(path);
        std::vector<Value> all_rows;

        std::string line;
        std::getline(file, line); // Header-i keçirik

        while (std::getline(file, line)) {
            std::vector<Value> row_values;
            std::stringstream ss(line);
            std::string cell;

            while (std::getline(ss, cell, ',')) {
                row_values.emplace_back(cell);
            }
            all_rows.emplace_back(std::move(row_values));
        }
        return Value(std::move(all_rows));
    }
}

void setupVFs(SymbolContainer& env, StringPool& pool) {
    const std::string& path = "vfs";
    
    if (env.find(path) == env.end()) {
        env[path] = SymbolTable();
    }

    auto& vfs = env[path];

    // vfs methods
    vfs[pool.intern("pwd")]              = Value(VFsNative::pwd);
    vfs[pool.intern("set_cwd")]          = Value(VFsNative::setCwd);
    vfs[pool.intern("exists")]           = Value(VFsNative::exists);
    vfs[pool.intern("is_file")]          = Value(VFsNative::isFile);
    vfs[pool.intern("is_dir")]           = Value(VFsNative::isDir);
    vfs[pool.intern("read")]             = Value(VFsNative::read);
    vfs[pool.intern("write")]            = Value(VFsNative::write);
    vfs[pool.intern("append")]           = Value(VFsNative::append);
    vfs[pool.intern("remove")]           = Value(VFsNative::remove);
    vfs[pool.intern("mkdir")]            = Value(VFsNative::mkdir);
    vfs[pool.intern("list")]             = Value(VFsNative::list);
    vfs[pool.intern("file_size")]        = Value(VFsNative::fileSize);
    vfs[pool.intern("rename")]           = Value(VFsNative::rename);
    vfs[pool.intern("copy")]             = Value(VFsNative::copy);
    vfs[pool.intern("join")]             = Value(VFsNative::join);
    vfs[pool.intern("absolute")]         = Value(VFsNative::absolute);
    vfs[pool.intern("canonical")]        = Value(VFsNative::canonical);
    vfs[pool.intern("filename")]         = Value(VFsNative::filename);
    vfs[pool.intern("stem")]             = Value(VFsNative::stem);
    vfs[pool.intern("extension")]        = Value(VFsNative::extension);
    vfs[pool.intern("parent")]           = Value(VFsNative::parent);
    vfs[pool.intern("parse_csv")]           = Value(VFsNative::parse_csv);

    // vfs properties
    vfs[pool.intern("cwd")]              = Value(std::filesystem::current_path().string()).setReadOnly();
    vfs[pool.intern("sep")]              = Value(std::string(1, std::filesystem::path::preferred_separator)).setReadOnly();
    std::error_code ec;
    std::filesystem::path tempDir = std::filesystem::temp_directory_path(ec);
    if (ec) tempDir = std::filesystem::current_path();
    vfs[pool.intern("temp_dir")]         = Value(tempDir.string()).setReadOnly();
    vfs[pool.intern("version")]          = Value("v0.0.1-alpha").setReadOnly();
}
