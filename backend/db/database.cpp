#include "database.h"
#include "../config.h"
#include "../core/log.h"
#include <chrono>
#include <iostream>
#include <vector>

using namespace std;

Database db; 

// Establish a connection between the C++ server and MySQL.
bool Database::connect()
{
    conn_ = mysql_init(nullptr);

    if (!conn_)
    {
        cout << "[DB] mysql_init failed!" << endl;
        return false;
    }

    // Connect using the database credentials and settings from config.h.
    if (!mysql_real_connect(conn_, config::dbHost.c_str(), config::dbUser.c_str(),
                            config::dbPass.c_str(), config::dbName.c_str(),
                            config::dbPort, nullptr, 0))
    {
        cout << "[DB] Connection failed: " << mysql_error(conn_) << endl;
        return false;
    }

    // Set the character encoding to support a wide range of characters.
    mysql_set_character_set(conn_, "utf8mb4");
    cout << "[DB] Connected to MySQL successfully." << endl;
    return true;
}

// Close the MySQL connection and reset the connection pointer.
void Database::disconnect()
{
    if (conn_) mysql_close(conn_);
    conn_ = nullptr;
}

// Execute a SQL statement and record its execution details for server logs.
Result Database::run(const string& sql)
{
    lock_guard<recursive_mutex> lock(mutex_);

    auto started = chrono::steady_clock::now();
    Result result = execute(sql);

    double ms = chrono::duration<double, milli>(chrono::steady_clock::now() - started).count();

    bool isSelect = !result.columns.empty();
    addQueryToCurrentRequest(sql, ms, result.ok, result.error, isSelect,
                             isSelect ? (long long)result.rows.size() : result.affectedRows);

    return result;
}

// Execute the SQL statement and convert its result into a Result object.
Result Database::execute(const string& sql)
{
    Result result;

    if (mysql_query(conn_, sql.c_str()))
    {
        result.error = mysql_error(conn_);
        return result;
    }

    result.ok = true;

    MYSQL_RES* res = mysql_store_result(conn_);

    if (!res)
    {
        result.affectedRows = (long long)mysql_affected_rows(conn_);
        result.insertId = (long long)mysql_insert_id(conn_);
        return result;
    }

    unsigned int n = mysql_num_fields(res);
    MYSQL_FIELD* fields = mysql_fetch_fields(res);

    for (unsigned int i = 0; i < n; i++)
        result.columns.push_back(fields[i].name);

    MYSQL_ROW row;

    while ((row = mysql_fetch_row(res)))
    {
        Row r;

        for (unsigned int i = 0; i < n; i++)
            r[result.columns[i]] = row[i] ? row[i] : "";

        result.rows.push_back(r);
    }

    mysql_free_result(res);  
    return result;
}

// Execute a query and return only its rows.
Rows Database::rows(const string& sql)
{
    Result r = run(sql);

    if (!r.ok)
        cout << "[DB] Query failed: " << r.error << endl;

    return r.rows;
}

// Escape special SQL characters in text and wrap the result in single quotes.
string Database::quote(const string& text)
{
    lock_guard<recursive_mutex> lock(mutex_);

    vector<char> buf(text.size() * 2 + 1);
    unsigned long n = mysql_real_escape_string(conn_, buf.data(), text.c_str(),
                                               (unsigned long)text.size());

    return "'" + string(buf.data(), n) + "'";
}

// Start a transaction when a Transaction object is created.
Transaction::Transaction()
{
    db.run("START TRANSACTION");
}

// Automatically roll back changes if commit() was not called.
Transaction::~Transaction()
{
    if (!done_) db.run("ROLLBACK");
}

// Save all changes made during the transaction.
void Transaction::commit()
{
    db.run("COMMIT");
    done_ = true;
}

// Add a database column only if it does not already exist.
static bool ensureColumn(const string& table, const string& column, const string& definition,
                         const string& why)
{
    Rows has = db.rows(
        "SELECT COUNT(*) AS n FROM information_schema.COLUMNS "
        "WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='" + table + "' AND COLUMN_NAME='" + column + "'");

    if (!has.empty() && col(has[0], "n") != "0")
        return true;

    Result r = db.run("ALTER TABLE " + table + " ADD COLUMN " + column + " " + definition);

    if (!r.ok)
    {
        cout << "[DB] Could not add " << table << "." << column << ": " << r.error << endl;
        return false;
    }

    cout << "[DB] Added " << table << "." << column << " column (" << why << ")." << endl;
    return true;
}

// Apply required schema updates for admin roles and automatic delivery.
bool migrateDatabase()
{
    return ensureColumn("users", "role", "VARCHAR(10) NOT NULL DEFAULT 'CUSTOMER'", "admin support")
        && ensureColumn("orders", "deliver_at", "DATETIME NULL", "demo auto-delivery");
}
