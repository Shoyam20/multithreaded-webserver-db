#include "router.h"
#include "auth.h"
#include "cart.h"
#include "dashboard.h"
#include "home.h"
#include "html_helpers.h"
#include "http_utils.h"
#include "orders.h"
#include "products.h"
#include "request_context.h"
#include "session.h"
#include <map>

using namespace std;

// ============================================================
//  ROUTER
// ============================================================
static string loginRequiredPage()
{
    return loginForm("Please sign in to continue.", true);
}

static string notFoundPage()
{
    tlStatus = 404;

    return pageWrapper("",
        "<div class='empty'><div class='big'>🧭</div>"
        "<h2>404 — Page not found</h2>"
        "<p>The page you are looking for does not exist.</p>"
        "<a class='btn accent' href='/'>Go home</a></div>");
}

string route(
    const string& method,
    const string& path,
    const string& rawRequest
)
{
    map<string, string> query = parseQuery(rawRequest);

    // Cookie first; fall back to the older ?session_id= links so
    // any bookmarked URL keeps working.
    tlSessionId = getCookie(rawRequest, "session_id");

    if (tlSessionId.empty() && query.count("session_id"))
        tlSessionId = query["session_id"];

    tlUserId   = getSessionUserId(tlSessionId);
    tlUserName = getUserName(tlUserId);

    if (tlUserName.empty())
    {
        tlUserId = 0;
        tlSessionId.clear();
    }

    tlCartCount = getCartItemCount(tlUserId);
    tlNotice    = query.count("notice") ? query["notice"] : "";
    tlSearch    = query.count("q") ? query["q"] : "";

    // ---------- public pages ----------
    if (path == "/")
        return buildHomePage();

    if (path == "/products")
        return buildProductsPage(query);

    if (path == "/product")
    {
        int id = getIntParameter(query, "id");

        if (id <= 0)
            return notFoundPage();

        return buildProductDetailPage(id);
    }

    if (path == "/database")
        return buildDatabaseDashboard();

    if (path == "/signup")
    {
        if (method == "POST")
            return handleSignup(parseFormBody(rawRequest));

        return signupForm();
    }

    if (path == "/login")
    {
        if (method == "POST")
            return handleLogin(parseFormBody(rawRequest));

        return loginForm();
    }

    if (path == "/logout")
    {
        logoutSession(tlSessionId);

        tlSessionId.clear();
        tlUserName.clear();
        tlUserId = 0;

        tlExtraHeaders =
            "Set-Cookie: session_id=; Path=/; HttpOnly; SameSite=Lax; "
            "Expires=Thu, 01 Jan 1970 00:00:00 GMT\r\n";

        tlRedirect = "/?notice=logout";

        return "";
    }

    // ---------- pages that need a signed-in user ----------
    bool known =
        path == "/session" || path == "/cart" || path == "/add-cart" ||
        path == "/update-cart" || path == "/remove-cart" ||
        path == "/checkout" || path == "/place-order" || path == "/orders";

    if (!known)
        return notFoundPage();

    if (tlUserId == 0)
        return loginRequiredPage();

    if (path == "/session")
        return buildSessionPage(tlSessionId, tlUserId);

    if (path == "/cart")
        return buildCartPage(tlUserId);

    if (path == "/orders")
        return buildOrdersPage(tlUserId);

    if (path == "/checkout")
        return buildCheckoutPage(tlUserId);

    if (path == "/place-order")
    {
        // Placing an order changes data: accept it only as a form POST.
        if (method != "POST")
        {
            tlRedirect = "/checkout";
            return "";
        }

        return checkoutCart(tlUserId);
    }

    int productId = getIntParameter(query, "product_id");

    if (productId <= 0)
        return pageWrapper("Cart",
            "<div class='err'>Invalid product ID.</div>"
            "<p><a class='btn' href='/products'>Browse products</a></p>");

    if (path == "/add-cart")
    {
        string page = addCartItem(tlUserId, productId,
                                  getIntParameter(query, "quantity", 1));

        // "Buy Now" skips the cart and goes straight to checkout.
        if (!tlRedirect.empty() && query.count("buy"))
            tlRedirect = "/checkout";

        return page;
    }

    if (path == "/update-cart")
        return setCartItemQuantity(tlUserId, productId,
                                   getIntParameter(query, "quantity", 1));

    // /remove-cart
    return removeCartItem(tlUserId, productId);
}
