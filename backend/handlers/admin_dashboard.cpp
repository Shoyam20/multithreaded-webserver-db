#include "handlers.h"
#include "view.h"
#include "../config.h"
#include "../services/admin_service.h"

using namespace std;

// Admin home page: a few simple counts.
Response adminDashboard(Request& req)
{
    Row stats = dashboardStats();

    Data page;
    page.set("users", col(stats, "users"));
    page.set("products", col(stats, "products"));
    page.set("orders", col(stats, "orders"));
    page.set("revenue", money(col(stats, "revenue")));
    page.set("pendingOrders", col(stats, "pending_orders"));
    page.set("lowStockCount", col(stats, "low_stock"));
    page.set("lowStockLevel", config::lowStockThreshold);

    return renderAdminPage(req, "Dashboard", "admin/dashboard.html", "dashboard", page);
}
