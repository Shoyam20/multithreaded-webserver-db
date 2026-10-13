#include "handlers.h"
#include "view.h"
#include "../services/order_service.h"
#include <cstdlib>

using namespace std;

static const char* ORDER_STATUSES[]   = { "PLACED", "SHIPPED", "DELIVERED", "CANCELLED" };
static const char* PAYMENT_STATUSES[] = { "PENDING", "PAID", "FAILED", "REFUNDED" };

// All orders, with tabs to filter by status.
Response adminOrders(Request& req)
{
    string status = req.param("status");
    if (!status.empty() && !isValidOrderStatus(status)) status = "";

    OrderFilter filter;
    filter.status = status;
    filter.limit = 200;

    Data page;

    for (const Row& o : listOrders(filter))
    {
        Data item;
        item.set("id", col(o, "order_id")).set("customer", col(o, "customer"));
        item.set("email", col(o, "email")).set("date", col(o, "created_at"));
        item.set("total", money(col(o, "total")));
        item.set("status", col(o, "status")).set("statusClass", statusClass(col(o, "status")));
        item.set("paymentStatus", col(o, "payment_status"));
        item.set("paymentClass", statusClass(col(o, "payment_status")));
        page.add("orders", item);
    }

    Data all;
    all.set("label", "All").set("url", "/admin/orders");
    all.flag("active", status.empty());
    page.add("tabs", all);

    for (const char* s : ORDER_STATUSES)
    {
        Data tab;
        tab.set("label", s).set("url", string("/admin/orders?status=") + s);
        tab.flag("active", status == s);
        page.add("tabs", tab);
    }

    return renderAdminPage(req, "Orders", "admin/orders.html", "orders", page);
}

Response adminOrderDetail(Request& req)
{
    int id = req.intParam("id", 0);
    Row order = getOrder(id);

    if (order.empty())
        return adminMessagePage(req, "Not found", "That order does not exist.",
                                "/admin/orders", "Back to orders");

    Data page;
    page.set("id", col(order, "order_id")).set("date", col(order, "created_at"));
    page.set("customer", col(order, "customer")).set("email", col(order, "email"));
    page.set("total", money(col(order, "total")));
    page.set("status", col(order, "status")).set("statusClass", statusClass(col(order, "status")));
    page.set("paymentMethod", col(order, "payment_method"));
    page.set("paymentStatus", col(order, "payment_status"));
    page.set("paymentClass", statusClass(col(order, "payment_status")));
    page.set("deliverAt", col(order, "deliver_at")).set("secondsLeft", col(order, "seconds_left"));
    page.flag("onTheWay", (col(order, "status") == "PLACED" || col(order, "status") == "SHIPPED") && !col(order, "seconds_left").empty());

    // A cancelled order is final (its stock was already returned).
    page.flag("canChange", col(order, "status") != "CANCELLED");

    for (const Row& i : orderItems(id))
    {
        Data item = productData(i);
        item.set("quantity", col(i, "quantity"));
        item.set("price", money(col(i, "price")));
        item.set("subtotal", money(col(i, "subtotal")));
        page.add("items", item);
    }

    for (const char* s : ORDER_STATUSES)
    {
        Data opt;
        opt.set("value", s).flag("selected", col(order, "status") == s);
        page.add("statusOptions", opt);
    }

    for (const char* s : PAYMENT_STATUSES)
    {
        Data opt;
        opt.set("value", s).flag("selected", col(order, "payment_status") == s);
        page.add("paymentOptions", opt);
    }

    return renderAdminPage(req, "Order #" + to_string(id), "admin/order_detail.html", "orders", page);
}

Response adminOrderStatus(Request& req)
{
    int id = req.intParam("id", 0);
    string error = updateOrderStatus(id, req.param("status"));

    if (!error.empty())
        return adminMessagePage(req, "Could not update order", error,
                                "/admin/order?id=" + to_string(id), "Back to order");

    return Response::redirect("/admin/order?id=" + to_string(id) + "&notice=status");
}

Response adminOrderPayment(Request& req)
{
    int id = req.intParam("id", 0);
    string error = updatePaymentStatus(id, req.param("status"));

    if (!error.empty())
        return adminMessagePage(req, "Could not update payment", error,
                                "/admin/order?id=" + to_string(id), "Back to order");

    return Response::redirect("/admin/order?id=" + to_string(id) + "&notice=payment");
}
