#include "admin_service.h"
#include "../config.h"

using namespace std;

// One query with six small sub-selects, one per number on the dashboard.
Row dashboardStats()
{
    Rows r = db.rows(
        "SELECT "
        "(SELECT COUNT(*) FROM users) AS users, "
        "(SELECT COUNT(*) FROM products) AS products, "
        "(SELECT COUNT(*) FROM orders) AS orders, "
        "(SELECT COALESCE(SUM(total),0) FROM orders WHERE status<>'CANCELLED') AS revenue, "
        "(SELECT COUNT(*) FROM orders WHERE status='PLACED') AS pending_orders, "
        "(SELECT COUNT(*) FROM products WHERE stock<=" + to_string(config::lowStockThreshold) + ") AS low_stock");

    return r.empty() ? Row() : r[0];
}
