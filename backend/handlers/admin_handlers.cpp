#include "handlers.h"
#include "../services/services.h"
#include "../config.h"
#include <cstdlib>

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

static const char* ORDER_STATUSES[] = { "PLACED", "SHIPPED", "DELIVERED", "CANCELLED" };

// All orders, with tabs to filter by status.
Response adminOrders(Request& req)
{
    if (!req.user.isAdmin()) return Response::redirect(req.user.loggedIn() ? "/?notice=forbidden" : "/login?notice=needlogin");

    string status = req.param("status");
    if (!isValidOrderStatus(status)) status = "";

    // the tabs: All, PLACED, SHIPPED, DELIVERED, CANCELLED
    string tabs = string("<a class=\"") + (status.empty() ? "on" : "") + "\" href=\"/admin/orders\">All</a>";

    for (int i = 0; i < 4; i++)
        tabs += string("<a class=\"") + (status == ORDER_STATUSES[i] ? "on" : "") +
                "\" href=\"/admin/orders?status=" + ORDER_STATUSES[i] + "\">" + ORDER_STATUSES[i] + "</a>";

    string rows;
    Rows orders = listOrders(0, status, 200);

    for (size_t i = 0; i < orders.size(); i++)
    {
        Vars row;
        row["id"]            = col(orders[i], "order_id");
        row["customer"]      = col(orders[i], "customer");
        row["email"]         = col(orders[i], "email");
        row["date"]          = col(orders[i], "created_at");
        row["total"]         = formatMoney(col(orders[i], "total"));
        row["status"]        = col(orders[i], "status");
        row["statusClass"]   = statusClass(col(orders[i], "status"));
        row["paymentStatus"] = col(orders[i], "payment_status");
        row["paymentClass"]  = statusClass(col(orders[i], "payment_status"));
        rows += renderTemplate("partials/admin_order_row.html", row);
    }

    Vars v;
    v["tabs"]      = tabs;
    v["rows"]      = rows;
    v["emptyNote"] = orders.empty() ? "<p class=\"muted\">No orders in this view.</p>" : "";

    return renderAdminPage(req, "Orders", "admin/orders.html", "orders", v);
}

// One order: its items, the customer, and the form to change its status.
Response adminOrderDetail(Request& req)
{
    if (!req.user.isAdmin()) return Response::redirect(req.user.loggedIn() ? "/?notice=forbidden" : "/login?notice=needlogin");

    int id = req.intParam("id", 0);
    Row order = getOrder(id);

    if (order.empty())
        return adminMessagePage(req, "Not found", "That order does not exist.", "/admin/orders", "Back to orders");

    string itemRows;
    Rows items = orderItems(id);

    for (size_t i = 0; i < items.size(); i++)
    {
        Vars row;
        row["id"]       = col(items[i], "product_id");
        row["name"]     = col(items[i], "name");
        row["icon"]     = categoryIcon(col(items[i], "category_name"));
        row["quantity"] = col(items[i], "quantity");
        row["price"]    = formatMoney(col(items[i], "price"));
        row["subtotal"] = formatMoney(col(items[i], "subtotal"));
        itemRows += renderTemplate("partials/admin_order_item_row.html", row);
    }

    string status = col(order, "status");

    // A cancelled order is final (its stock was already returned), so it gets no form.
    string statusForm = "<div class=\"err\">This order was cancelled and can no longer be changed.</div>";

    if (status != "CANCELLED")
    {
        string options;
        for (int i = 0; i < 4; i++)
            options += optionTag(ORDER_STATUSES[i], ORDER_STATUSES[i], status == ORDER_STATUSES[i]);

        Vars form;
        form["id"]      = to_string(id);
        form["options"] = options;
        statusForm = renderTemplate("partials/order_status_form.html", form);
    }

    Vars v;
    v["id"]            = to_string(id);
    v["date"]          = col(order, "created_at");
    v["customer"]      = col(order, "customer");
    v["email"]         = col(order, "email");
    v["total"]         = formatMoney(col(order, "total"));
    v["status"]        = status;
    v["statusClass"]   = statusClass(status);
    v["paymentMethod"] = col(order, "payment_method");
    v["paymentStatus"] = col(order, "payment_status");
    v["paymentClass"]  = statusClass(col(order, "payment_status"));
    v["items"]         = itemRows;
    v["statusForm"]    = statusForm;

    return renderAdminPage(req, "Order #" + to_string(id), "admin/order_detail.html", "orders", v);
}

// POST: set the order status (CANCELLED returns the stock, DELIVERED marks the cash payment PAID).
Response adminOrderStatus(Request& req)
{
    if (!req.user.isAdmin()) return Response::redirect(req.user.loggedIn() ? "/?notice=forbidden" : "/login?notice=needlogin");

    int id = req.intParam("id", 0);
    string error = updateOrderStatus(id, req.param("status"));

    if (!error.empty())
        return adminMessagePage(req, "Could not update order", error, "/admin/order?id=" + to_string(id), "Back to order");

    return Response::redirect("/admin/order?id=" + to_string(id) + "&notice=status");
}

// Admin: the list of all users (read-only).
Response adminUsers(Request& req)
{
    if (!req.user.isAdmin()) return Response::redirect(req.user.loggedIn() ? "/?notice=forbidden" : "/login?notice=needlogin");

    string rows;
    Rows users = listUsers();

    for (size_t i = 0; i < users.size(); i++)
    {
        Vars row;
        row["id"]        = col(users[i], "user_id");
        row["name"]      = col(users[i], "name");
        row["email"]     = col(users[i], "email");
        row["role"]      = col(users[i], "role");
        row["roleClass"] = col(users[i], "role") == "ADMIN" ? "green" : "";
        row["joined"]    = col(users[i], "created_at");
        row["orders"]    = col(users[i], "orders");
        rows += renderTemplate("partials/admin_user_row.html", row);
    }

    Vars v;
    v["rows"] = rows;
    return renderAdminPage(req, "Users", "admin/users.html", "users", v);
}
