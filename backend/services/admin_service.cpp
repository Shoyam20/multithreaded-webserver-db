#include "admin_service.h"
#include "../config.h"

using namespace std;

// One query with six small sub-selects, one for each number on the admin dashboard.
Row dashboardStats()
{
    lock_guard<mutex> lock(dbMutex);

    Rows r = dbRows("SELECT "
                    "(SELECT COUNT(*) FROM users) AS users, "
                    "(SELECT COUNT(*) FROM products) AS products, "
                    "(SELECT COUNT(*) FROM orders) AS orders, "
                    "(SELECT COALESCE(SUM(total),0) FROM orders WHERE status<>'CANCELLED') AS revenue, "
                    "(SELECT COUNT(*) FROM orders WHERE status='PLACED') AS pending_orders, "
                    "(SELECT COUNT(*) FROM products WHERE stock<=" + to_string(LOW_STOCK_LEVEL) + ") AS low_stock");

    return r.empty() ? Row() : r[0];
}
