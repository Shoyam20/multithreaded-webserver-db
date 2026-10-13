#pragma once
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include <mysql.h>

// One database row: column name -> value (a NULL becomes "").
typedef std::map<std::string, std::string> Row;
typedef std::vector<Row> Rows;

// Value of a column, or "" if the row has no such column.
std::string col(const Row& row, const std::string& name);

// What running one SQL statement gives back.
struct Result
{
    bool        ok = false;
    std::string error;           // MySQL's message when ok is false
    Rows        rows;            // SELECT: the data
    long long   insertId = 0;    // INSERT: the new AUTO_INCREMENT id
    long long   affectedRows = 0;// UPDATE / DELETE: how many rows changed
};

// There is ONE MySQL connection shared by all worker threads, so only one thread may use it at a time.
// Every service function starts with:   lock_guard<mutex> lock(dbMutex);
// and keeps the lock until it returns. (The functions below do not lock by themselves.)
extern std::mutex dbMutex;

bool        dbConnect();                         // password comes from the ECOM_DB_PASS environment variable
void        dbClose();
Result      dbRun(const std::string& sql);       // run any statement
Rows        dbRows(const std::string& sql);      // run a SELECT and return just the rows
std::string dbQuote(const std::string& text);    // 'escaped text' - always use this for text typed by users (SQL injection)
