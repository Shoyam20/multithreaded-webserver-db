#pragma once
#include "../db/database.h"
#include <string>
#include <vector>

// Headline numbers for the admin dashboard:
//   users, products, orders, revenue, pending_orders, low_stock, active_sessions
Row  dashboardStats();
Rows ordersByStatus();                  // status, n

// ---- table browser (read-only view of every table) ----
const std::vector<std::string>& browsableTables();
Rows tableRowCounts();                  // table_name, records

struct TablePage
{
    bool        found = false;          // false if the table name is not allowed
    std::string error;
    std::vector<std::string> columns;
    Rows        rows;
    long long   totalRows = 0;
    int         page = 1;
    int         pageCount = 1;
};

TablePage readTable(const std::string& tableName, int page, int perPage);
