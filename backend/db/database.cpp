#include "database.h"
#include "../config.h"
#include <iostream>
#include <vector>

using namespace std;

Database db;

bool Database::connect()
{
    conn_ = mysql_init(nullptr);

    if (!conn_)
    {
        cout << "[DB] mysql_init failed!" << endl;
        return false;
    }

    if (!mysql_real_connect(conn_, config::dbHost.c_str(), config::dbUser.c_str(),
                            config::dbPass.c_str(), config::dbName.c_str(),
                            config::dbPort, nullptr, 0))
    {
        cout << "[DB] Connection failed: " << mysql_error(conn_) << endl;
        cout << "[DB] Is MySQL running, and is the ECOM_DB_PASS environment variable set to your MySQL password?" << endl;
        return false;
    }

    mysql_set_character_set(conn_, "utf8mb4");
    cout << "[DB] Connected to MySQL successfully." << endl;
    return true;
}

void Database::disconnect()
{
    if (conn_) mysql_close(conn_);
    conn_ = nullptr;
}

// Run one SQL statement and return its rows / error.
Result Database::run(const string& sql)
{
    lock_guard<recursive_mutex> lock(mutex_);

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
        // Not a SELECT (INSERT / UPDATE / DELETE ...)
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

Rows Database::rows(const string& sql)
{
    Result r = run(sql);

    if (!r.ok)
        cout << "[DB] Query failed: " << r.error << endl;

    return r.rows;
}

string Database::quote(const string& text)
{
    lock_guard<recursive_mutex> lock(mutex_);

    vector<char> buf(text.size() * 2 + 1);
    unsigned long n = mysql_real_escape_string(conn_, buf.data(), text.c_str(),
                                               (unsigned long)text.size());

    return "'" + string(buf.data(), n) + "'";
}

// ------------------------------------------------------------
//  Transaction
// ------------------------------------------------------------
Transaction::Transaction()
{
    db.run("START TRANSACTION");
}

Transaction::~Transaction()
{
    if (!done_) db.run("ROLLBACK");
}

void Transaction::commit()
{
    db.run("COMMIT");
    done_ = true;
}

// ------------------------------------------------------------
//  Schema upgrade: the admin panel needs users.role
// ------------------------------------------------------------
