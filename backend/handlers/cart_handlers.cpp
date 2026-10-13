#include "handlers.h"
#include "view.h"
#include "../services/cart_service.h"
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
