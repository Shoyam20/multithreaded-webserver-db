#include "handlers.h"
#include "../services/services.h"
#include <cstdlib>

using namespace std;

// The cart page: one row per item and the order summary.
Response cartPage(Request& req)
{
    if (!req.user.loggedIn()) return Response::redirect("/login?notice=needlogin");

    Rows items = cartItems(req.user.id);

    if (items.empty())
        return renderPage(req, "Your cart", "pages/cart_empty.html");

    string lines;

    for (size_t i = 0; i < items.size(); i++)
    {
        int quantity = atoi(col(items[i], "quantity").c_str());
        int stock    = atoi(col(items[i], "stock").c_str());

        Vars line;
        line["id"]       = col(items[i], "product_id");
        line["name"]     = col(items[i], "name");
        line["price"]    = formatMoney(col(items[i], "price"));
        line["subtotal"] = formatMoney(col(items[i], "subtotal"));
        line["quantity"] = to_string(quantity);
        line["maxQty"]   = to_string(stock > 0 ? stock : 1);
        line["icon"]     = categoryIcon(col(items[i], "category_name"));
        line["tint"]     = categoryTint(atoi(col(items[i], "category_id").c_str()));
        line["warning"]  = quantity > stock ? "<div class=\"stk no\">Only " + to_string(stock) + " in stock - reduce the quantity</div>" : "";

        lines += renderTemplate("partials/cart_line.html", line);
    }

    Row totals = cartTotals(req.user.id);

    Vars v;
    v["lines"] = lines;
    v["units"] = col(totals, "units");
    v["total"] = formatMoney(col(totals, "total"));
    return renderPage(req, "Your cart", "pages/cart.html", v);
}

// POST /cart/add (the forms on the product pages).
Response cartAdd(Request& req)
{
    if (!req.user.loggedIn()) return Response::redirect("/login?notice=needlogin");

    string error = addToCart(req.user.id, req.intParam("product_id"), req.intParam("quantity", 1));

    if (!error.empty())
        return messagePage(req, "Could not add to cart", error, "/cart", "Back to cart");

    // "Buy now" skips the cart page and goes straight to checkout.
    return Response::redirect(req.param("buy").empty() ? "/cart?notice=added" : "/checkout");
}

Response cartUpdate(Request& req)
{
    if (!req.user.loggedIn()) return Response::redirect("/login?notice=needlogin");

    string error = setCartQuantity(req.user.id, req.intParam("product_id"), req.intParam("quantity", 1));

    if (!error.empty())
        return messagePage(req, "Could not update cart", error, "/cart", "Back to cart");

    return Response::redirect("/cart?notice=updated");
}

Response cartRemove(Request& req)
{
    if (!req.user.loggedIn()) return Response::redirect("/login?notice=needlogin");

    string error = removeFromCart(req.user.id, req.intParam("product_id"));

    if (!error.empty())
        return messagePage(req, "Could not update cart", error, "/cart", "Back to cart");

    return Response::redirect("/cart?notice=removed");
}

// Step 2 of 3: review the cart, see the payment method, press "Place order".
Response checkoutPage(Request& req)
{
    if (!req.user.loggedIn()) return Response::redirect("/login?notice=needlogin");

    Rows items = cartItems(req.user.id);

    if (items.empty())
        return messagePage(req, "Nothing to check out", "Your cart is empty.", "/products", "Start shopping");

    string lines;
    bool enoughStock = true;

    for (size_t i = 0; i < items.size(); i++)
    {
        int quantity = atoi(col(items[i], "quantity").c_str());
        int stock    = atoi(col(items[i], "stock").c_str());

        if (quantity > stock) enoughStock = false;

        Vars line;
        line["name"]     = col(items[i], "name");
        line["quantity"] = to_string(quantity);
        line["price"]    = formatMoney(col(items[i], "price"));
        line["subtotal"] = formatMoney(col(items[i], "subtotal"));
        line["icon"]     = categoryIcon(col(items[i], "category_name"));
        line["tint"]     = categoryTint(atoi(col(items[i], "category_id").c_str()));
        line["warning"]  = quantity > stock ? "<div class=\"stk no\">Only " + to_string(stock) + " in stock</div>" : "";

        lines += renderTemplate("partials/checkout_line.html", line);
    }

    Row totals = cartTotals(req.user.id);

    Vars v;
    v["lines"] = lines;
    v["units"] = col(totals, "units");
    v["total"] = formatMoney(col(totals, "total"));
    v["placeOrderBox"] = enoughStock
        ? renderTemplate("partials/place_order_form.html", Vars())
        : "<div class=\"err\">Some items exceed the available stock. <a href=\"/cart\">Fix your cart</a> before ordering.</div>";

    return renderPage(req, "Checkout", "pages/checkout.html", v);
}

// POST /checkout/place: the checkout transaction (see placeOrder in order_service.cpp).
Response placeOrderSubmit(Request& req)
{
    if (!req.user.loggedIn()) return Response::redirect("/login?notice=needlogin");

    string error = placeOrder(req.user.id);

    if (!error.empty())
        return messagePage(req, "Checkout failed", error, "/cart", "Back to cart");

    return Response::redirect("/orders?notice=ordered");
}

// The customer's order history: one card per order, with its items.
Response myOrdersPage(Request& req)
{
    if (!req.user.loggedIn()) return Response::redirect("/login?notice=needlogin");

    Rows orders = listOrders(req.user.id, "", 0);

    if (orders.empty())
        return renderPage(req, "Your orders", "pages/orders_empty.html");

    string cards;

    for (size_t i = 0; i < orders.size(); i++)
    {
        string orderId = col(orders[i], "order_id");
        string status  = col(orders[i], "status");

        // the items of this order
        string itemRows;
        Rows items = orderItems(atoi(orderId.c_str()));

        for (size_t j = 0; j < items.size(); j++)
        {
            Vars item;
            item["id"]       = col(items[j], "product_id");
            item["name"]     = col(items[j], "name");
            item["quantity"] = col(items[j], "quantity");
            item["price"]    = formatMoney(col(items[j], "price"));
            item["icon"]     = categoryIcon(col(items[j], "category_name"));
            item["tint"]     = categoryTint(atoi(col(items[j], "category_id").c_str()));
            itemRows += renderTemplate("partials/order_item_row.html", item);
        }

        // what the customer can do (or sees) at the bottom of the card
        string footer;
        if (status == "PLACED" || status == "SHIPPED")
        {
            Vars cancel;
            cancel["id"] = orderId;
            footer = renderTemplate("partials/cancel_order_form.html", cancel);
        }
        else if (status == "DELIVERED")
            footer = "<span class=\"done\">✅ Delivered - cash collected on delivery</span>";
        else if (status == "CANCELLED")
            footer = "<span class=\"muted\">↩️ Cancelled - items returned to stock, payment cancelled</span>";

        Vars card;
        card["id"]            = orderId;
        card["date"]          = col(orders[i], "created_at");
        card["total"]         = formatMoney(col(orders[i], "total"));
        card["status"]        = status;
        card["statusClass"]   = statusClass(status);
        card["paymentMethod"] = col(orders[i], "payment_method");
        card["paymentStatus"] = col(orders[i], "payment_status");
        card["paymentClass"]  = statusClass(col(orders[i], "payment_status"));
        card["items"]         = itemRows;
        card["footer"]        = footer;
        cards += renderTemplate("partials/order_card.html", card);
    }

    Vars v;
    v["orders"] = cards;
    return renderPage(req, "Your orders", "pages/orders.html", v);
}

// POST /orders/cancel: allowed until the order is delivered. Stock and payment are rolled back.
Response cancelMyOrder(Request& req)
{
    if (!req.user.loggedIn()) return Response::redirect("/login?notice=needlogin");

    string error = cancelOrderByCustomer(req.user.id, req.intParam("id", 0));

    if (!error.empty())
        return messagePage(req, "Could not cancel the order", error, "/orders", "Back to my orders");

    return Response::redirect("/orders?notice=cancelled");
}

// The login sessions of this customer (one row per browser login).
Response mySessionsPage(Request& req)
{
    if (!req.user.loggedIn()) return Response::redirect("/login?notice=needlogin");

    string rows;
    Rows sessions = sessionsForUser(req.user.id);

    for (size_t i = 0; i < sessions.size(); i++)
    {
        bool active  = (col(sessions[i], "is_active") == "1");
        bool current = (col(sessions[i], "session_id") == req.sessionId);

        Vars row;
        row["rowClass"]     = current ? "current" : "";
        row["sessionId"]    = col(sessions[i], "session_id");
        row["youMark"]      = current ? " <b>(this browser)</b>" : "";
        row["loginTime"]    = col(sessions[i], "login_time");
        row["lastActivity"] = col(sessions[i], "last_activity");
        row["logoutTime"]   = col(sessions[i], "logout_time").empty() ? "-" : col(sessions[i], "logout_time");
        row["status"]       = active ? "<span class=\"active\">Active</span>" : "<span class=\"out\">Logged out</span>";
        rows += renderTemplate("partials/session_row.html", row);
    }

    Vars v;
    v["rows"] = rows;
    return renderPage(req, "Your sessions", "pages/sessions.html", v);
}
