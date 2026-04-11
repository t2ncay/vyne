#ifndef URAGE_CPP_HPP
#define URAGE_CPP_HPP

#include <string>
#include <vector>
#include <stdexcept>
#include <cstring>
#include <iostream>

extern "C" {
    #include <urage.h>
}

namespace urage {

// ==================== Exception Classes ====================
class Error : public std::runtime_error {
public:
    explicit Error(const std::string& msg) : std::runtime_error(msg) {}
};

class NotFound : public Error {
public:
    NotFound() : Error("Key not found") {}
    explicit NotFound(const std::string& key) : Error("Key not found: " + key) {}
};

// ==================== Statistics Structure ====================
struct Stats {
    uint64_t keys;
    uint32_t height;
    uint64_t data_size;
    uint32_t pages;
    
    Stats() : keys(0), height(0), data_size(0), pages(0) {}
    
    explicit Stats(const urage_stats_t& s) 
        : keys(s.keys_count), height(s.btree_height), 
          data_size(s.data_size), pages(s.page_count) {}
};

// ==================== Field Definition ====================
struct Field {
    std::string name;
    int type;
    int size;
    
    Field(const std::string& n, int t, int s = 4) 
        : name(n), type(t), size(s) {}
    
    FieldDef to_c() const {
        FieldDef fd;
        std::strncpy(fd.name, name.c_str(), sizeof(fd.name) - 1);
        fd.name[sizeof(fd.name) - 1] = '\0';
        fd.type = type;
        fd.size = size;
        fd.offset = 0;
        return fd;
    }
};

// ==================== Main Database Class ====================
class Database {
private:
    urage_db_t* db;
    std::string last_error_msg;

    void check_db() const {
        if (!db) throw Error("Database not open");
    }

public:
    // ========== Constructor / Destructor ==========
    
    explicit Database(const std::string& path) {
        db = urage_open(path.c_str(), 0);
        if (!db) {
            throw Error("Failed to open database: " + path);
        }
    }
    
    ~Database() {
        if (db) {
            try {
                sync();
            } catch (...) {}
            urage_close(db);
            db = nullptr;
        }
    }
    
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    
    Database(Database&& other) noexcept : db(other.db) {
        other.db = nullptr;
    }
    
    Database& operator=(Database&& other) noexcept {
        if (this != &other) {
            if (db) urage_close(db);
            db = other.db;
            other.db = nullptr;
        }
        return *this;
    }
    
    // ========== Numeric Key Operations ==========
    // Uses: urage_add, urage_get, urage_delete, urage_exists
    
    void add(uint32_t key, const std::string& value) {
        check_db();
        urage_result_t r = urage_add(db, key, value.c_str(), value.size() + 1);
        if (r != URAGE_OK) {
            throw Error("Failed to add key " + std::to_string(key));
        }
    }
    
    std::string get(uint32_t key) {
        check_db();
        char buffer[4096];
        size_t size = sizeof(buffer);
        urage_result_t r = urage_get(db, key, buffer, &size);
        
        if (r == URAGE_OK) {
            return std::string(buffer, size);
        } else if (r == URAGE_NOT_FOUND) {
            return "";
        } else {
            throw Error("Failed to get key " + std::to_string(key));
        }
    }
    
    std::string get(uint32_t key, const std::string& default_value) {
        std::string result = get(key);
        return result.empty() ? default_value : result;
    }
    
    bool remove(uint32_t key) {
        check_db();
        urage_result_t r = urage_delete(db, key);
        if (r == URAGE_OK) return true;
        if (r == URAGE_NOT_FOUND) return false;
        throw Error("Failed to remove key " + std::to_string(key));
    }
    
    bool exists(uint32_t key) {
        check_db();
        return urage_exists(db, key) != 0;
    }
    
    // ========== String Key Operations ==========
    // Uses: urage_put_str, urage_get_str, urage_del_str, urage_exists_str
    
    void put_str(const std::string& key, const std::string& value) {
        check_db();
        urage_result_t r = urage_put_str(db, key.c_str(), value.c_str(), value.size() + 1);
        if (r != URAGE_OK) {
            throw Error("Failed to put string key: " + key);
        }
    }
    
    std::string get_str(const std::string& key) {
        check_db();
        char buffer[4096];
        size_t size = sizeof(buffer);
        urage_result_t r = urage_get_str(db, key.c_str(), buffer, &size);
        
        if (r == URAGE_OK) {
            return std::string(buffer, size);
        } else if (r == URAGE_NOT_FOUND) {
            return "";
        } else {
            throw Error("Failed to get string key: " + key);
        }
    }
    
    bool remove_str(const std::string& key) {
        check_db();
        urage_result_t r = urage_del_str(db, key.c_str());
        if (r == URAGE_OK) return true;
        if (r == URAGE_NOT_FOUND) return false;
        throw Error("Failed to delete string key: " + key);
    }
    
    bool exists_str(const std::string& key) {
        check_db();
        return urage_exists_str(db, key.c_str()) != 0;
    }
    
    // ========== Type System ==========
    // Uses: urage_define_type, urage_undefine_type, urage_get_type,
    //       urage_add_typed, urage_get_typed
    
    void define_type(const std::string& name, const std::vector<Field>& fields) {
        check_db();
        
        std::vector<FieldDef> c_fields;
        for (const auto& f : fields) {
            c_fields.push_back(f.to_c());
        }
        
        urage_result_t r = urage_define_type(db, name.c_str(), 
                                             c_fields.data(), c_fields.size());
        if (r != URAGE_OK) {
            if (r == URAGE_ERROR) {
                throw Error("Type already exists: " + name);
            } else {
                throw Error("Failed to define type: " + name);
            }
        }
    }
    
    void add_typed(const std::string& type, uint32_t key, const std::string& fields) {
        check_db();
        urage_result_t r = urage_add_typed(db, type.c_str(), key, fields.c_str());
        if (r == URAGE_NOT_FOUND) {
            throw NotFound("Type not found: " + type);
        } else if (r != URAGE_OK) {
            throw Error("Failed to add typed data");
        }
    }
    
    std::string get_typed(const std::string& type, uint32_t key) {
        check_db();
        char buffer[8192];
        urage_result_t r = urage_get_typed(db, type.c_str(), key, buffer, sizeof(buffer));
        
        if (r == URAGE_OK) {
            return std::string(buffer);
        } else if (r == URAGE_NOT_FOUND) {
            return "";
        } else {
            throw Error("Failed to get typed data");
        }
    }
    
    std::vector<Field> describe_type(const std::string& name) {
        check_db();
        TypeDef* type = urage_get_type(db, name.c_str());
        if (!type) {
            throw NotFound("Type not found: " + name);
        }
        
        std::vector<Field> fields;
        for (uint32_t i = 0; i < type->field_count; i++) {
            fields.emplace_back(
                type->fields[i].name,
                type->fields[i].type,
                type->fields[i].size
            );
        }
        
        free(type);
        return fields;
    }
    
    bool undefine_type(const std::string& name) {
        check_db();
        urage_result_t r = urage_undefine_type(db, name.c_str());
        if (r == URAGE_OK) return true;
        if (r == URAGE_NOT_FOUND) return false;
        throw Error("Failed to undefine type: " + name);
    }
    
    // ========== Transactions ==========
    // Uses: urage_begin, urage_commit, urage_rollback, urage_in_transaction
    
    void begin() {
        check_db();
        urage_result_t r = urage_begin(db);
        if (r != URAGE_OK) {
            throw Error("Failed to begin transaction");
        }
    }
    
    void commit() {
        check_db();
        urage_result_t r = urage_commit(db);
        if (r != URAGE_OK) {
            throw Error("Failed to commit transaction");
        }
    }
    
    void rollback() {
        check_db();
        urage_result_t r = urage_rollback(db);
        if (r != URAGE_OK) {
            throw Error("Failed to rollback transaction");
        }
    }
    
    bool in_transaction() {
        check_db();
        return urage_in_transaction(db) != 0;
    }
    
    // ========== Utilities ==========
    // Uses: urage_sync, urage_stats, urage_error
    
    void sync() {
        check_db();
        urage_result_t r = urage_sync(db);
        if (r != URAGE_OK) {
            throw Error("Failed to sync database");
        }
    }
    
    Stats stats() {
        check_db();
        urage_stats_t c_stats;
        urage_result_t r = urage_stats(db, &c_stats);
        if (r != URAGE_OK) {
            throw Error("Failed to get stats");
        }
        return Stats(c_stats);
    }
    
    std::string get_last_error() {
        if (!db) return "Database not open";
        const char* err = urage_error(db);
        return err ? std::string(err) : "";
    }
};

} // namespace urage

#endif // URAGE_CPP_HPP