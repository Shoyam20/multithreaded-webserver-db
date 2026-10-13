#pragma once
#include <string>

// All tunable settings live here. Database settings can be overridden with
// environment variables so the password does not have to be edited in code:
//   ECOM_DB_HOST, ECOM_DB_USER, ECOM_DB_PASS, ECOM_DB_NAME, ECOM_DB_PORT
namespace config
{
    extern std::string dbHost;
    extern std::string dbUser;
    extern std::string dbPass;
    extern std::string dbName;
    extern int         dbPort;

    extern int         serverPort;
    extern int         workerThreads;
    extern int         lowStockThreshold;   // "low stock" warning level in the admin panel



    // Folder that contains templates/ and static/ (found automatically).
    extern std::string frontendDir;

    void load();   // apply environment overrides and locate the frontend folder
}
