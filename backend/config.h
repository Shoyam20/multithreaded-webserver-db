#pragma once

// Settings for the whole project. Change a value here and rebuild.
// The only setting NOT here is the MySQL password: it is read from the
// environment variable ECOM_DB_PASS (see db/database.cpp), never from code.

const int         SERVER_PORT     = 8080;   // http://localhost:8080
const int         WORKER_COUNT    = 10;     // threads in the pool that handle requests
const int         LOW_STOCK_LEVEL = 5;      // admin "low stock" warning level

const char* const DB_HOST = "localhost";
const char* const DB_USER = "root";
const char* const DB_NAME = "ecommerce_db";
const int         DB_PORT = 3306;
