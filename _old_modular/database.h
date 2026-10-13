#pragma once
#include <mysql.h>
#include <mutex>
#include <string>

extern MYSQL* dbConn;
extern std::mutex dbMutex;

bool initDatabase();
std::string esc(const std::string& s);   // SQL-escape (needs dbConn)
