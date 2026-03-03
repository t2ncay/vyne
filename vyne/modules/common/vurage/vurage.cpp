#include "vurage.h"

/**
 * Vurage Native Method Implementations
 */

namespace VurageNative {
    struct urage_db_t;

    enum urage_result_t {
        URAGE_OK = 0,
        URAGE_ERROR = -1,
        URAGE_NOT_FOUND = -2,
        URAGE_FULL = -3,
        URAGE_IO_ERROR = -4,
        URAGE_INVALID_ARG = -5,
        URAGE_CLOSED = -6,
        URAGE_MEMORY_ERROR = -7
    };

    using open_fn = urage_db_t* (*)(const char*, unsigned);
    using close_fn = void (*)(urage_db_t*);
    using put_fn = int (*)(urage_db_t*, uint32_t, const void*, size_t);
    using add_fn = int (*)(urage_db_t*, uint32_t, const void*, size_t);
    using get_fn = int (*)(urage_db_t*, uint32_t, void*, size_t*);
    using delete_fn = int (*)(urage_db_t*, uint32_t);
    using exists_fn = int (*)(urage_db_t*, uint32_t);
    using put_str_fn = int (*)(urage_db_t*, const char*, const void*, size_t);
    using get_str_fn = int (*)(urage_db_t*, const char*, void*, size_t*);
    using del_str_fn = int (*)(urage_db_t*, const char*);
    using exists_str_fn = int (*)(urage_db_t*, const char*);
    using sync_fn = int (*)(urage_db_t*);
    using error_fn = const char* (*)(urage_db_t*);
    using begin_fn = int (*)(urage_db_t*);
    using commit_fn = int (*)(urage_db_t*);
    using rollback_fn = int (*)(urage_db_t*);
    using in_tx_fn = int (*)(urage_db_t*);

    struct UrageApi {
#ifdef _WIN32
        HMODULE handle = nullptr;
#else
        void* handle = nullptr;
#endif
        open_fn open = nullptr;
        close_fn close = nullptr;
        put_fn put = nullptr;
        add_fn add = nullptr;
        get_fn get = nullptr;
        delete_fn del = nullptr;
        exists_fn exists = nullptr;
        put_str_fn put_str = nullptr;
        get_str_fn get_str = nullptr;
        del_str_fn del_str = nullptr;
        exists_str_fn exists_str = nullptr;
        sync_fn sync = nullptr;
        error_fn error = nullptr;
        begin_fn begin = nullptr;
        commit_fn commit = nullptr;
        rollback_fn rollback = nullptr;
        in_tx_fn in_tx = nullptr;
        bool ready = false;
    };

    static UrageApi g_api;
    static std::unordered_map<int64_t, urage_db_t*> g_handles;
    static int64_t g_nextHandle = 1;

    static void* loadSymbol(const char* name) {
#ifdef _WIN32
        return reinterpret_cast<void*>(GetProcAddress(g_api.handle, name));
#else
        return dlsym(g_api.handle, name);
#endif
    }

    static void closeLibrary() {
        if (!g_api.handle) return;
#ifdef _WIN32
        FreeLibrary(g_api.handle);
#else
        dlclose(g_api.handle);
#endif
        g_api = UrageApi{};
    }

    static bool tryLoadLibrary(const char* path) {
        if (!path || path[0] == '\0') return false;
#ifdef _WIN32
        g_api.handle = LoadLibraryA(path);
#else
        g_api.handle = dlopen(path, RTLD_LAZY);
#endif
        return g_api.handle != nullptr;
    }

    static void resolveSymbols() {
        g_api.open = reinterpret_cast<open_fn>(loadSymbol("urage_open"));
        g_api.close = reinterpret_cast<close_fn>(loadSymbol("urage_close"));
        g_api.put = reinterpret_cast<put_fn>(loadSymbol("urage_put"));
        g_api.add = reinterpret_cast<add_fn>(loadSymbol("urage_add"));
        g_api.get = reinterpret_cast<get_fn>(loadSymbol("urage_get"));
        g_api.del = reinterpret_cast<delete_fn>(loadSymbol("urage_delete"));
        g_api.exists = reinterpret_cast<exists_fn>(loadSymbol("urage_exists"));
        g_api.put_str = reinterpret_cast<put_str_fn>(loadSymbol("urage_put_str"));
        g_api.get_str = reinterpret_cast<get_str_fn>(loadSymbol("urage_get_str"));
        g_api.del_str = reinterpret_cast<del_str_fn>(loadSymbol("urage_del_str"));
        g_api.exists_str = reinterpret_cast<exists_str_fn>(loadSymbol("urage_exists_str"));
        g_api.sync = reinterpret_cast<sync_fn>(loadSymbol("urage_sync"));
        g_api.error = reinterpret_cast<error_fn>(loadSymbol("urage_error"));
        g_api.begin = reinterpret_cast<begin_fn>(loadSymbol("urage_begin"));
        g_api.commit = reinterpret_cast<commit_fn>(loadSymbol("urage_commit"));
        g_api.rollback = reinterpret_cast<rollback_fn>(loadSymbol("urage_rollback"));
        g_api.in_tx = reinterpret_cast<in_tx_fn>(loadSymbol("urage_in_transaction"));

        // Some urage builds expose only urage_add (without urage_put).
        if (!g_api.put && g_api.add) {
            g_api.put = reinterpret_cast<put_fn>(g_api.add);
        }

        g_api.ready = g_api.open && g_api.close && g_api.put && g_api.add && g_api.get &&
                      g_api.del && g_api.exists && g_api.put_str && g_api.get_str &&
                      g_api.del_str && g_api.exists_str && g_api.sync && g_api.error;

        if (!g_api.ready) {
            closeLibrary();
            throw std::runtime_error("vurage: urage library loaded, but required symbols are missing");
        }
    }

    static void ensureApiReady() {
        if (g_api.ready) return;

        const char* envPath = std::getenv("VURAGE_LIB");
        if (tryLoadLibrary(envPath)) {
            resolveSymbols();
            return;
        }

#ifdef _WIN32
        const char* candidates[] = {".\\urage.dll", ".\\liburage.dll", "urage.dll", "liburage.dll"};
#elif __APPLE__
        const char* candidates[] = {"./liburage.dylib", "./liburage.so", "liburage.dylib", "liburage.so"};
#else
        const char* candidates[] = {"./liburage.so", "liburage.so", "liburage.so.1"};
#endif

        for (const auto* candidate : candidates) {
            if (tryLoadLibrary(candidate)) {
                resolveSymbols();
                return;
            }
        }

        throw std::runtime_error(
            "vurage: could not load urage shared library. Set VURAGE_LIB to your library path."
        );
    }

    static urage_db_t* dbFromHandle(const Value& v) {
        int64_t handle = v.asInt();
        auto it = g_handles.find(handle);
        if (it == g_handles.end()) {
            throw std::runtime_error("vurage: invalid database handle");
        }
        return it->second;
    }

    static std::string readNumeric(urage_db_t* db, uint32_t key) {
        std::vector<char> buffer(256);
        size_t size = buffer.size();

        for (int tries = 0; tries < 6; ++tries) {
            int rc = g_api.get(db, key, buffer.data(), &size);
            if (rc == URAGE_OK) {
                if (size > 0 && buffer[size - 1] == '\0') {
                    return std::string(buffer.data(), size - 1);
                }
                return std::string(buffer.data(), size);
            }
            if (rc == URAGE_NOT_FOUND) {
                return "";
            }
            if ((rc == URAGE_ERROR || rc == URAGE_INVALID_ARG) && size > buffer.size()) {
                buffer.resize(size);
                continue;
            }
            break;
        }

        const char* err = g_api.error(db);
        throw std::runtime_error(std::string("vurage.get failed: ") + (err ? err : "unknown error"));
    }

    static std::string readString(urage_db_t* db, const std::string& key) {
        std::vector<char> buffer(256);
        size_t size = buffer.size();

        for (int tries = 0; tries < 6; ++tries) {
            int rc = g_api.get_str(db, key.c_str(), buffer.data(), &size);
            if (rc == URAGE_OK) {
                if (size > 0 && buffer[size - 1] == '\0') {
                    return std::string(buffer.data(), size - 1);
                }
                return std::string(buffer.data(), size);
            }
            if (rc == URAGE_NOT_FOUND) {
                return "";
            }
            if ((rc == URAGE_ERROR || rc == URAGE_INVALID_ARG) && size > buffer.size()) {
                buffer.resize(size);
                continue;
            }
            break;
        }

        const char* err = g_api.error(db);
        throw std::runtime_error(std::string("vurage.get_str failed: ") + (err ? err : "unknown error"));
    }

    static Value open(std::vector<Value>& args) {
        if (args.size() < 1 || args.size() > 2) {
            throw std::runtime_error("vurage.open(path [,flags]) expects 1 or 2 arguments");
        }
        if (args[0].getType() != Value::STRING) {
            throw std::runtime_error("vurage.open(path [,flags]) expects String path");
        }
        ensureApiReady();

        const std::string& path = args[0].asString();
        unsigned flags = 0;
        if (args.size() == 2) {
            flags = static_cast<unsigned>(args[1].asInt());
        }

        urage_db_t* db = g_api.open(path.c_str(), flags);
        if (!db) {
            throw std::runtime_error("vurage.open failed for path: " + path);
        }

        int64_t handle = g_nextHandle++;
        g_handles[handle] = db;
        return Value(handle);
    }

    static Value close(std::vector<Value>& args) {
        if (args.size() != 1) {
            throw std::runtime_error("vurage.close(handle) expects 1 argument");
        }
        ensureApiReady();

        int64_t handle = args[0].asInt();
        auto it = g_handles.find(handle);
        if (it == g_handles.end()) {
            return Value(false);
        }

        g_api.close(it->second);
        g_handles.erase(it);
        return Value(true);
    }

    static Value put(std::vector<Value>& args) {
        if (args.size() != 3) {
            throw std::runtime_error("vurage.put(handle, key, value) expects 3 arguments");
        }
        ensureApiReady();

        urage_db_t* db = dbFromHandle(args[0]);
        uint32_t key = static_cast<uint32_t>(args[1].asInt());
        const std::string payload = args[2].toString();

        int rc = g_api.put(db, key, payload.c_str(), payload.size() + 1);
        if (rc != URAGE_OK) {
            const char* err = g_api.error(db);
            throw std::runtime_error(std::string("vurage.put failed: ") + (err ? err : "unknown error"));
        }
        return Value(true);
    }

    static Value add(std::vector<Value>& args) {
        if (args.size() != 3) {
            throw std::runtime_error("vurage.add(handle, key, value) expects 3 arguments");
        }
        ensureApiReady();

        urage_db_t* db = dbFromHandle(args[0]);
        uint32_t key = static_cast<uint32_t>(args[1].asInt());
        const std::string payload = args[2].toString();

        int rc = g_api.add(db, key, payload.c_str(), payload.size() + 1);
        if (rc != URAGE_OK) {
            const char* err = g_api.error(db);
            throw std::runtime_error(std::string("vurage.add failed: ") + (err ? err : "unknown error"));
        }
        return Value(true);
    }

    static Value get(std::vector<Value>& args) {
        if (args.size() != 2) {
            throw std::runtime_error("vurage.get(handle, key) expects 2 arguments");
        }
        ensureApiReady();

        urage_db_t* db = dbFromHandle(args[0]);
        uint32_t key = static_cast<uint32_t>(args[1].asInt());
        const std::string result = readNumeric(db, key);

        if (result.empty()) {
            return Value();
        }
        return Value(result);
    }

    static Value del(std::vector<Value>& args) {
        if (args.size() != 2) {
            throw std::runtime_error("vurage.del(handle, key) expects 2 arguments");
        }
        ensureApiReady();

        urage_db_t* db = dbFromHandle(args[0]);
        uint32_t key = static_cast<uint32_t>(args[1].asInt());

        int rc = g_api.del(db, key);
        if (rc == URAGE_OK) return Value(true);
        if (rc == URAGE_NOT_FOUND) return Value(false);

        const char* err = g_api.error(db);
        throw std::runtime_error(std::string("vurage.del failed: ") + (err ? err : "unknown error"));
    }

    static Value exists(std::vector<Value>& args) {
        if (args.size() != 2) {
            throw std::runtime_error("vurage.exists(handle, key) expects 2 arguments");
        }
        ensureApiReady();

        urage_db_t* db = dbFromHandle(args[0]);
        uint32_t key = static_cast<uint32_t>(args[1].asInt());
        return Value(g_api.exists(db, key) != 0);
    }

    static Value put_str(std::vector<Value>& args) {
        if (args.size() != 3) {
            throw std::runtime_error("vurage.put_str(handle, key, value) expects 3 arguments");
        }
        ensureApiReady();

        urage_db_t* db = dbFromHandle(args[0]);
        if (args[1].getType() != Value::STRING) {
            throw std::runtime_error("vurage.put_str: key must be String");
        }

        const std::string& key = args[1].asString();
        const std::string payload = args[2].toString();

        int rc = g_api.put_str(db, key.c_str(), payload.c_str(), payload.size() + 1);
        if (rc != URAGE_OK) {
            const char* err = g_api.error(db);
            throw std::runtime_error(std::string("vurage.put_str failed: ") + (err ? err : "unknown error"));
        }
        return Value(true);
    }

    static Value get_str(std::vector<Value>& args) {
        if (args.size() != 2) {
            throw std::runtime_error("vurage.get_str(handle, key) expects 2 arguments");
        }
        ensureApiReady();

        urage_db_t* db = dbFromHandle(args[0]);
        if (args[1].getType() != Value::STRING) {
            throw std::runtime_error("vurage.get_str: key must be String");
        }

        const std::string& key = args[1].asString();
        const std::string result = readString(db, key);

        if (result.empty()) {
            return Value();
        }
        return Value(result);
    }

    static Value del_str(std::vector<Value>& args) {
        if (args.size() != 2) {
            throw std::runtime_error("vurage.del_str(handle, key) expects 2 arguments");
        }
        ensureApiReady();

        urage_db_t* db = dbFromHandle(args[0]);
        if (args[1].getType() != Value::STRING) {
            throw std::runtime_error("vurage.del_str: key must be String");
        }

        const std::string& key = args[1].asString();
        int rc = g_api.del_str(db, key.c_str());

        if (rc == URAGE_OK) return Value(true);
        if (rc == URAGE_NOT_FOUND) return Value(false);

        const char* err = g_api.error(db);
        throw std::runtime_error(std::string("vurage.del_str failed: ") + (err ? err : "unknown error"));
    }

    static Value exists_str(std::vector<Value>& args) {
        if (args.size() != 2) {
            throw std::runtime_error("vurage.exists_str(handle, key) expects 2 arguments");
        }
        ensureApiReady();

        urage_db_t* db = dbFromHandle(args[0]);
        if (args[1].getType() != Value::STRING) {
            throw std::runtime_error("vurage.exists_str: key must be String");
        }

        const std::string& key = args[1].asString();
        return Value(g_api.exists_str(db, key.c_str()) != 0);
    }

    static Value sync(std::vector<Value>& args) {
        if (args.size() != 1) {
            throw std::runtime_error("vurage.sync(handle) expects 1 argument");
        }
        ensureApiReady();

        urage_db_t* db = dbFromHandle(args[0]);
        int rc = g_api.sync(db);
        if (rc != URAGE_OK) {
            const char* err = g_api.error(db);
            throw std::runtime_error(std::string("vurage.sync failed: ") + (err ? err : "unknown error"));
        }
        return Value(true);
    }

    static Value error(std::vector<Value>& args) {
        if (args.size() != 1) {
            throw std::runtime_error("vurage.error(handle) expects 1 argument");
        }
        ensureApiReady();

        urage_db_t* db = dbFromHandle(args[0]);
        const char* msg = g_api.error(db);
        return Value(msg ? msg : "");
    }

    static Value begin(std::vector<Value>& args) {
        if (args.size() != 1) {
            throw std::runtime_error("vurage.begin(handle) expects 1 argument");
        }
        ensureApiReady();
        if (!g_api.begin) {
            throw std::runtime_error("vurage.begin is not supported by this urage build");
        }

        urage_db_t* db = dbFromHandle(args[0]);
        int rc = g_api.begin(db);
        return Value(rc == URAGE_OK);
    }

    static Value commit(std::vector<Value>& args) {
        if (args.size() != 1) {
            throw std::runtime_error("vurage.commit(handle) expects 1 argument");
        }
        ensureApiReady();
        if (!g_api.commit) {
            throw std::runtime_error("vurage.commit is not supported by this urage build");
        }

        urage_db_t* db = dbFromHandle(args[0]);
        int rc = g_api.commit(db);
        return Value(rc == URAGE_OK);
    }

    static Value rollback(std::vector<Value>& args) {
        if (args.size() != 1) {
            throw std::runtime_error("vurage.rollback(handle) expects 1 argument");
        }
        ensureApiReady();
        if (!g_api.rollback) {
            throw std::runtime_error("vurage.rollback is not supported by this urage build");
        }

        urage_db_t* db = dbFromHandle(args[0]);
        int rc = g_api.rollback(db);
        return Value(rc == URAGE_OK);
    }

    static Value in_transaction(std::vector<Value>& args) {
        if (args.size() != 1) {
            throw std::runtime_error("vurage.in_transaction(handle) expects 1 argument");
        }
        ensureApiReady();
        if (!g_api.in_tx) {
            throw std::runtime_error("vurage.in_transaction is not supported by this urage build");
        }

        urage_db_t* db = dbFromHandle(args[0]);
        return Value(g_api.in_tx(db) != 0);
    }
}

void setupVurage(SymbolContainer& env, StringPool& pool) {
    const std::string& path = "vurage";
    
    if (env.find(path) == env.end()) {
        env[path] = SymbolTable();
    }

    auto& vurage = env[path];

    // vurage methods
    vurage[pool.intern("open")]            = Value(VurageNative::open);
    vurage[pool.intern("close")]           = Value(VurageNative::close);
    vurage[pool.intern("put")]             = Value(VurageNative::put);
    vurage[pool.intern("add")]             = Value(VurageNative::add);
    vurage[pool.intern("get")]             = Value(VurageNative::get);
    vurage[pool.intern("del")]             = Value(VurageNative::del);
    vurage[pool.intern("exists")]          = Value(VurageNative::exists);
    vurage[pool.intern("put_str")]         = Value(VurageNative::put_str);
    vurage[pool.intern("get_str")]         = Value(VurageNative::get_str);
    vurage[pool.intern("del_str")]         = Value(VurageNative::del_str);
    vurage[pool.intern("exists_str")]      = Value(VurageNative::exists_str);
    vurage[pool.intern("sync")]            = Value(VurageNative::sync);
    vurage[pool.intern("error")]           = Value(VurageNative::error);
    vurage[pool.intern("begin")]           = Value(VurageNative::begin);
    vurage[pool.intern("commit")]          = Value(VurageNative::commit);
    vurage[pool.intern("rollback")]        = Value(VurageNative::rollback);
    vurage[pool.intern("in_transaction")]  = Value(VurageNative::in_transaction);

    // vurage properties
    vurage[pool.intern("version")]         = Value("v0.1.0-alpha").setReadOnly();
    vurage[pool.intern("ok")]              = Value(static_cast<int64_t>(VurageNative::URAGE_OK)).setReadOnly();
    vurage[pool.intern("not_found")]       = Value(static_cast<int64_t>(VurageNative::URAGE_NOT_FOUND)).setReadOnly();
    vurage[pool.intern("invalid_arg")]     = Value(static_cast<int64_t>(VurageNative::URAGE_INVALID_ARG)).setReadOnly();
    vurage[pool.intern("closed")]          = Value(static_cast<int64_t>(VurageNative::URAGE_CLOSED)).setReadOnly();
}
