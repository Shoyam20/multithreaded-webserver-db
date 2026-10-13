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
