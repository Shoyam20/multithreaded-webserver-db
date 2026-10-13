#include "handlers.h"
#include "view.h"
#include "../config.h"
#include "../services/admin_service.h"
#include "../services/order_service.h"
#include "../services/product_service.h"

using namespace std;

// Landing page of the admin panel: key numbers, recent orders, low stock.
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
    page.set("activeSessions", col(stats, "active_sessions"));
    page.set("lowStockLevel", config::lowStockThreshold);

    for (const Row& s : ordersByStatus())
    {
        Data item;
        item.set("status", col(s, "status")).set("n", col(s, "n"));
        item.set("statusClass", statusClass(col(s, "status")));
        page.add("statuses", item);
    }

    OrderFilter recent;
    recent.limit = 8;

    for (const Row& o : listOrders(recent))
    {
        Data item;
        item.set("id", col(o, "order_id")).set("customer", col(o, "customer"));
        item.set("date", col(o, "created_at")).set("total", money(col(o, "total")));
        item.set("status", col(o, "status")).set("statusClass", statusClass(col(o, "status")));
        page.add("recentOrders", item);
    }

    ProductFilter low;
    low.maxStock = config::lowStockThreshold;
    low.sort = "stock_asc";
    low.limit = 10;
    addProducts(page, "lowStock", findProducts(low));

    return renderAdminPage(req, "Dashboard", "admin/dashboard.html", "dashboard", page);
}

// List of all database tables with their row counts.
Response adminTables(Request& req)
{
    Data page;

    for (const Row& t : tableRowCounts())
    {
        Data item;
        item.set("name", col(t, "table_name")).set("records", col(t, "records"));
        page.add("tables", item);
    }

    return renderAdminPage(req, "Database tables", "admin/tables.html", "tables", page);
}

// Contents of one table, 25 rows per page.
Response adminTableView(Request& req)
{
    string name = req.param("name");
    TablePage table = readTable(name, req.intParam("page", 1), 25);

    if (!table.found)
        return adminMessagePage(req, "Unknown table", "That table does not exist.",
                                "/admin/tables", "Back to tables");

    Data page;
    page.set("tableName", name);
    page.set("totalRows", table.totalRows);
    page.set("page", table.page).set("pageCount", table.pageCount);
    page.set("error", table.error);
    page.flag("hasPrev", table.page > 1).flag("hasNext", table.page < table.pageCount);
    page.set("prevPage", table.page - 1).set("nextPage", table.page + 1);

    for (const string& c : table.columns)
    {
        Data column;
        column.set("name", c);
        page.add("columns", column);
    }

    for (const Row& row : table.rows)
    {
        Data r;

        // Cells in column order, so they line up under the headers.
        for (const string& c : table.columns)
        {
            Data cell;
            cell.set("value", col(row, c));
            r.add("cells", cell);
        }

        page.add("rows", r);
    }

    return renderAdminPage(req, "Table: " + name, "admin/table_view.html", "tables", page);
}
