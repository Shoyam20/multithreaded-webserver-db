#pragma once
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include <mysql.h>

// A database row: column name -> value (NULL becomes "").
using Row  = std::map<std::string, std::string>;
using Rows = std::vector<Row>;

// Value of a column, or "" if the row does not have it.
inline std::string col(const Row& row, const std::string& name)
{
    auto it = row.find(name);
    return it == row.end() ? "" : it->second;
}

// What running one SQL statement gives back.
struct Result
{
    bool        ok = false;
    std::string error;                    // MySQL's message when ok is false
    std::vector<std::string> columns;     // SELECT: column names in order
    Rows        rows;                     // SELECT: the data
    long long   insertId = 0;             // INSERT: the new AUTO_INCREMENT id
    long long   affectedRows = 0;         // UPDATE/DELETE: rows changed
};

// One shared MySQL connection; the mutex makes it safe for all worker threads.
class Database
{
public:
    bool connect();
    void disconnect();

    Result run(const std::string& sql);          // any statement
    Rows   rows(const std::string& sql);         // SELECT, ignoring errors
    std::string quote(const std::string& text);  // 'escaped text' (prevents SQL injection)

    std::recursive_mutex& mutex() { return mutex_; }

private:
    MYSQL* conn_ = nullptr;
    std::recursive_mutex mutex_;
};

extern Database db;

// Hold the database exclusively while a block of related queries runs.
class DbLock
{
public:
    DbLock() : guard_(db.mutex()) {}
private:
    std::lock_guard<std::recursive_mutex> guard_;
};

// START TRANSACTION ... COMMIT. If commit() is never called (an early return
// or an error), everything is rolled back automatically.
class Transaction
{
public:
    Transaction();
    ~Transaction();
    void commit();

private:
    DbLock lock_;
    bool   done_ = false;
};

