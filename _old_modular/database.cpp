#include "database.h"
#include "config.h"
#include <iostream>
#include <vector>
using namespace std;

// ============================================================
//  DB
// ============================================================
MYSQL* dbConn = nullptr;
mutex dbMutex;

bool initDatabase()
{
    dbConn = mysql_init(nullptr);

    if (!dbConn)
    {
        cout << "[DB] mysql_init failed!" << endl;
        return false;
    }

    if (!mysql_real_connect(dbConn, DB_HOST, DB_USER, DB_PASS, DB_NAME,
                            DB_PORT, nullptr, 0))
    {
        cout << "[DB] Connection failed: " << mysql_error(dbConn) << endl;
        return false;
    }

    mysql_set_character_set(dbConn, "utf8mb4");

    cout << "[DB] Connected to MySQL successfully." << endl;
    return true;
}

// Escape a string for safe use in SQL (prevents SQL injection)
string esc(const string& s)
{
    vector<char> buf(s.size() * 2 + 1);

    unsigned long n = mysql_real_escape_string(
        dbConn,
        buf.data(),
        s.c_str(),
        (unsigned long)s.size()
    );

    return string(buf.data(), n);
}

