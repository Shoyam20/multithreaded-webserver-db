#include "handlers.h"
#include "view.h"
#include "../config.h"
#include "../services/admin_service.h"

using namespace std;

// Admin home page: a few simple counts.
Response adminDashboard(Request& req)
{
    if (!req.user.isAdmin()) return Response::redirect(req.user.loggedIn() ? "/?notice=forbidden" : "/login?notice=needlogin");

    Row stats = dashboardStats();

    Vars v;
    v["users"]         = col(stats, "users");
    v["products"]      = col(stats, "products");
    v["orders"]        = col(stats, "orders");
    v["revenue"]       = formatMoney(col(stats, "revenue"));
    v["pendingOrders"] = col(stats, "pending_orders");
    v["lowStockCount"] = col(stats, "low_stock");
    v["lowStockLevel"] = to_string(LOW_STOCK_LEVEL);

    return renderAdminPage(req, "Dashboard", "admin/dashboard.html", "dashboard", v);
}
