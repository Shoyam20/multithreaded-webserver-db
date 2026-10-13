#include "handlers.h"
#include "view.h"
#include "../services/cart_service.h"
#include "../services/order_service.h"
#include "../services/user_service.h"
#include <cstdlib>

using namespace std;


// Step 2 of 3: review the cart, see the payment method, press "Place order".
Response checkoutPage(Request& req)
{
    Rows items = cartItems(req.user.id);

    if (items.empty())
        return messagePage(req, "Nothing to check out", "Your cart is empty.",
                           "/products", "Start shopping");

    Data page;
    long long units = 0;
    double total = 0;
    bool stockProblem = false;

    for (const Row& item : items)
    {
        int qty = atoi(col(item, "quantity").c_str());
        int stock = atoi(col(item, "stock").c_str());

        Data line = productData(item);
        line.set("quantity", qty);
        line.set("price", money(col(item, "price")));
        line.set("subtotal", money(col(item, "subtotal")));
        line.flag("overStock", qty > stock);
        page.add("lines", line);

        if (qty > stock) stockProblem = true;
        units += qty;
        total += atof(col(item, "subtotal").c_str());
    }

    page.set("units", units);
    page.set("total", money(total));
    page.flag("stockProblem", stockProblem);

    return renderPage(req, "Checkout", "pages/checkout.html", page);
}

Response placeOrderSubmit(Request& req)
{
    PlaceOrderResult result = placeOrder(req.user.id);

    if (!result.ok)
        return messagePage(req, "Checkout failed", result.error, "/cart", "Back to cart");

    return Response::redirect("/orders?notice=ordered");
}

Response myOrdersPage(Request& req)
{
    OrderFilter filter;
    filter.userId = req.user.id;

    Data page;

    for (const Row& order : listOrders(filter))
    {
        Data o;
        o.set("id", col(order, "order_id"));
        o.set("date", col(order, "created_at"));
        o.set("total", money(col(order, "total")));
        o.set("status", col(order, "status"));
        o.set("statusClass", statusClass(col(order, "status")));
        o.set("paymentMethod", col(order, "payment_method"));
        o.set("paymentStatus", col(order, "payment_status"));
        o.set("paymentClass", statusClass(col(order, "payment_status")));

        // Demo delivery: show a countdown while the order is on its way, and let the
        // customer cancel until it is delivered.
        string status = col(order, "status");
        bool onTheWay = (status == "PLACED" || status == "SHIPPED");
        string secondsLeft = col(order, "seconds_left");

        o.flag("canCancel", onTheWay);
        o.flag("hasCountdown", onTheWay && !secondsLeft.empty());
        o.set("secondsLeft", secondsLeft);
        o.flag("isDelivered", status == "DELIVERED");
        o.flag("isCancelled", status == "CANCELLED");

        for (const Row& item : orderItems(atoi(col(order, "order_id").c_str())))
        {
            Data i = productData(item);
            i.set("quantity", col(item, "quantity"));
            i.set("price", money(col(item, "price")));
            o.add("items", i);
        }

        page.add("orders", o);
    }

    return renderPage(req, "Your orders", "pages/orders.html", page);
}

Response mySessionsPage(Request& req)
{
    Data page;

    for (const Row& s : sessionsForUser(req.user.id))
    {
        Data item;
        item.set("sessionId", col(s, "session_id"));
        item.set("loginTime", col(s, "login_time"));
        item.set("lastActivity", col(s, "last_activity"));
        item.set("logoutTime", col(s, "logout_time"));
        item.flag("active", col(s, "is_active") == "1");
        item.flag("current", col(s, "session_id") == req.sessionId);
        page.add("sessions", item);
    }

    return renderPage(req, "Your sessions", "pages/sessions.html", page);
}

// Customer cancels their own order (allowed until it is delivered).
// Stock goes back on the shelf and the payment is cancelled - see order_service.cpp.
Response cancelMyOrder(Request& req)
{
    string error = cancelOrderByCustomer(req.user.id, req.intParam("id", 0));

    if (!error.empty())
        return messagePage(req, "Could not cancel the order", error, "/orders", "Back to my orders");

    return Response::redirect("/orders?notice=cancelled");
}
