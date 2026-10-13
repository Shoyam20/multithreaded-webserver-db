#include "admin_service.h"
#include "../config.h"
#include <cstdlib>

using namespace std;

Row dashboardStats()
{
    Rows r = db.rows(
        "SELECT "
        "(SELECT COUNT(*) FROM users) AS users, "
        "(SELECT COUNT(*) FROM products) AS products, "
        "(SELECT COUNT(*) FROM orders) AS orders, "
        "(SELECT COALESCE(SUM(total),0) FROM orders WHERE status<>'CANCELLED') AS revenue, "
        "(SELECT COUNT(*) FROM orders WHERE status='PLACED') AS pending_orders, "
        "(SELECT COUNT(*) FROM products WHERE stock<=" + to_string(config::lowStockThreshold) + ") AS low_stock, "
        "(SELECT COUNT(*) FROM user_sessions WHERE is_active=TRUE) AS active_sessions");

    return r.empty() ? Row() : r[0];
}

Rows ordersByStatus()
{
    return db.rows("SELECT status, COUNT(*) AS n FROM orders GROUP BY status ORDER BY n DESC");
}

// ------------------------------------------------------------
//  Table browser. Only these names are ever put into SQL.
// ------------------------------------------------------------
const vector<string>& browsableTables()
{
    static const vector<string> tables = {
        "users", "user_sessions", "categories", "products",
        "cart", "cart_items", "orders", "order_items", "payments"
    };
    return tables;
}

Rows tableRowCounts()
{
    Rows out;

    for (const string& name : browsableTables())
    {
        Rows r = db.rows("SELECT COUNT(*) AS n FROM " + name);

        Row row;
        row["table_name"] = name;
        row["records"] = r.empty() ? "0" : col(r[0], "n");
        out.push_back(row);
    }

    return out;
}

TablePage readTable(const string& tableName, int page, int perPage)
{
    TablePage out;

    bool allowed = false;
    for (const string& t : browsableTables())
        if (t == tableName) allowed = true;

    if (!allowed)
        return out;                       // found stays false

    out.found = true;

    Rows count = db.rows("SELECT COUNT(*) AS n FROM " + tableName);
    out.totalRows = count.empty() ? 0 : atoll(col(count[0], "n").c_str());

    out.pageCount = (int)((out.totalRows + perPage - 1) / perPage);
    if (out.pageCount < 1) out.pageCount = 1;

    out.page = page < 1 ? 1 : (page > out.pageCount ? out.pageCount : page);

    // Newest first: every table's first column is its auto-increment id
    // (user_sessions uses a text id, which still sorts fine).
    Result r = db.run("SELECT * FROM " + tableName + " ORDER BY 1 DESC LIMIT " +
                      to_string((out.page - 1) * perPage) + "," + to_string(perPage));

    if (!r.ok)
    {
        out.error = r.error;
        return out;
    }

    out.columns = r.columns;
    out.rows = r.rows;

    // Never show password hashes, even to an admin.
    for (Row& row : out.rows)
        if (row.count("password")) row["password"] = "••••••••";

    return out;
}
