#include "config.h"
#include <cstdlib>
#include <filesystem>

namespace config
{
    std::string dbHost = "localhost";
    std::string dbUser = "root";
    std::string dbPass = "";          // never hard-code this: it is read from the ECOM_DB_PASS environment variable
    std::string dbName = "ecommerce_db";
    int         dbPort = 3306;

    int         serverPort = 8080;
    int         workerThreads = 10;
    int         lowStockThreshold = 5;

    bool        logQueriesToConsole = false;
    int         logBufferSize = 300;

    int         deliveryMinSeconds = 60;
    int         deliveryMaxSeconds = 120;
    int         deliveryCheckSeconds = 5;

    std::string frontendDir = "frontend";

    static void fromEnv(const char* name, std::string& target)
    {
        const char* v = std::getenv(name);
        if (v && *v) target = v;
    }

    void load()
    {
        fromEnv("ECOM_DB_HOST", dbHost);
        fromEnv("ECOM_DB_USER", dbUser);
        fromEnv("ECOM_DB_PASS", dbPass);
        fromEnv("ECOM_DB_NAME", dbName);

        const char* port = std::getenv("ECOM_DB_PORT");
        if (port && *port) dbPort = std::atoi(port);

        const char* logSql = std::getenv("ECOM_LOG_SQL");     // set to 1 to also print SQL in the terminal
        if (logSql && *logSql) logQueriesToConsole = (std::string(logSql) == "1");

        const char* minSec = std::getenv("ECOM_DELIVERY_MIN");
        const char* maxSec = std::getenv("ECOM_DELIVERY_MAX");     // seconds, e.g. 15 for a quick demo
        if (minSec && *minSec) deliveryMinSeconds = std::atoi(minSec);
        if (maxSec && *maxSec) deliveryMaxSeconds = std::atoi(maxSec);
        if (deliveryMaxSeconds < deliveryMinSeconds) deliveryMaxSeconds = deliveryMinSeconds;

        // Works whether the exe is started from the project root or from backend/.
        for (const char* candidate : { "frontend", "../frontend", "../../frontend" })
        {
            if (std::filesystem::exists(std::string(candidate) + "/templates/layout.html"))
            {
                frontendDir = candidate;
                break;
            }
        }
    }
}
