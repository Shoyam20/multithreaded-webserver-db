#include "handlers.h"
#include "view.h"
#include "../services/cart_service.h"
#include <cstdlib>

using namespace std;

Response cartPage(Request& req)
{
    Rows items = cartItems(req.user.id);

    Data page;
    long long units = 0;
    double total = 0;

    for (const Row& item : items)
    {
        Data line = productData(item);          // name, icon, tint, stock ...
        line.set("quantity", col(item, "quantity"));
        line.set("price", money(col(item, "price")));
        line.set("subtotal", money(col(item, "subtotal")));
        line.flag("overStock", atoi(col(item, "quantity").c_str()) > atoi(col(item, "stock").c_str()));
        page.add("lines", line);

        units += atoi(col(item, "quantity").c_str());
        total += atof(col(item, "subtotal").c_str());
    }

    page.set("units", units);
    page.set("total", money(total));

    return renderPage(req, "Your cart", "pages/cart.html", page);
}

// The forms on the product and cart pages POST here.
Response cartAdd(Request& req)
{
    string error = addToCart(req.user.id, req.intParam("product_id"), req.intParam("quantity", 1));

    if (!error.empty())
        return messagePage(req, "Could not add to cart", error, "/cart", "Back to cart");

    // "Buy now" skips the cart page.
    return Response::redirect(req.param("buy").empty() ? "/cart?notice=added" : "/checkout");
}

Response cartUpdate(Request& req)
{
    string error = setCartQuantity(req.user.id, req.intParam("product_id"), req.intParam("quantity", 1));

    if (!error.empty())
        return messagePage(req, "Could not update cart", error, "/cart", "Back to cart");

    return Response::redirect("/cart?notice=updated");
}

Response cartRemove(Request& req)
{
    string error = removeFromCart(req.user.id, req.intParam("product_id"));

    if (!error.empty())
        return messagePage(req, "Could not update cart", error, "/cart", "Back to cart");

    return Response::redirect("/cart?notice=removed");
}
