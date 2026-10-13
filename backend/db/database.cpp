#include "database.h"
#include "../config.h"
#include <cstdlib>
#include <iostream>

using namespace std;

std::mutex dbMutex;
static MYSQL* conn = nullptr;

string col(const Row& row, const string& name)
{
    Row::const_iterator it = row.find(name);
    return (it == row.end()) ? "" : it->second;
}

// Opens the connection to MySQL. The password is read from the environment, not from the code.
bool dbConnect()
{
    const char* password = getenv("ECOM_DB_PASS");
    if (password == nullptr) password = "";

    conn = mysql_init(nullptr);

    if (!conn)
    {
        cout << "[DB] mysql_init failed!" << endl;
        return false;
    }

    if (!mysql_real_connect(conn, DB_HOST, DB_USER, password, DB_NAME, DB_PORT, nullptr, 0))
    {
        cout << "[DB] Connection failed: " << mysql_error(conn) << endl;
        cout << "[DB] Is MySQL running, and is the ECOM_DB_PASS environment variable set to your MySQL password?" << endl;
        return false;
    }

    mysql_set_character_set(conn, "utf8mb4");
    cout << "[DB] Connected to MySQL successfully." << endl;
    return true;
}

void dbClose()
{
    if (conn) mysql_close(conn);
    conn = nullptr;
}

// Runs one SQL statement. For a SELECT the rows are copied into result.rows.
Result dbRun(const string& sql)
{
    Result result;

    if (mysql_query(conn, sql.c_str()))
    {
        result.error = mysql_error(conn);
        return result;
    }

    result.ok = true;

    MYSQL_RES* res = mysql_store_result(conn);

    if (!res)   // not a SELECT: INSERT / UPDATE / DELETE / START TRANSACTION ...
    {
        result.affectedRows = (long long)mysql_affected_rows(conn);
        result.insertId = (long long)mysql_insert_id(conn);
        return result;
    }

    unsigned int columnCount = mysql_num_fields(res);
    MYSQL_FIELD* fields = mysql_fetch_fields(res);

    MYSQL_ROW data;
    while ((data = mysql_fetch_row(res)))
    {
        Row row;

        for (unsigned int i = 0; i < columnCount; i++)
            row[fields[i].name] = data[i] ? data[i] : "";

        result.rows.push_back(row);
    }

    mysql_free_result(res);
    return result;
}

// Like dbRun for a SELECT, but returns only the rows (an error gives an empty list).
Rows dbRows(const string& sql)
{
    Result r = dbRun(sql);

    if (!r.ok)
        cout << "[DB] Query failed: " << r.error << endl;

    return r.rows;
}

// Puts text between quotes and escapes dangerous characters (prevents SQL injection).
string dbQuote(const string& text)
{
    string buffer(text.size() * 2 + 1, '\0');
    unsigned long length = mysql_real_escape_string(conn, &buffer[0], text.c_str(), (unsigned long)text.size());

    return "'" + buffer.substr(0, length) + "'";
}
