#include "app.h"
#include "handlers/handlers.h"
#include "handlers/view.h"
#include "services/cart_service.h"
#include "services/user_service.h"
#include <fstream>
#include <sstream>

using namespace std;

// The Content-Type for a static file, chosen from its ending.
static string contentTypeFor(const string& path)
{
    if (path.size() >= 4 && path.compare(path.size() - 4, 4, ".css") == 0) return "text/css; charset=utf-8";
    if (path.size() >= 3 && path.compare(path.size() - 3, 3, ".js") == 0)  return "application/javascript; charset=utf-8";
    if (path.size() >= 4 && path.compare(path.size() - 4, 4, ".png") == 0) return "image/png";
    if (path.size() >= 4 && path.compare(path.size() - 4, 4, ".svg") == 0) return "image/svg+xml";
    return "application/octet-stream";
}

// Static files: /static/css/style.css is the file frontend/static/css/style.css.
static Response staticFile(const string& path)
{
    // Never let "../" climb out of the static folder.
    if (path.find("..") != string::npos || path.find('\\') != string::npos)
        return Response::html("Forbidden", 403);

    ifstream in(("frontend" + path).c_str(), ios::binary);
    if (!in)
        return Response::html("Not found", 404);

    stringstream data;
    data << in.rdbuf();

    Response res;
    res.body = data.str();
    res.contentType = contentTypeFor(path);
    return res;
}

// Who is making this request? The browser sends its session id in a cookie; the database knows which user it belongs to.
static void identifyUser(Request& req)
{
    map<string, string>::iterator cookie = req.cookies.find("session_id");
    if (cookie == req.cookies.end())
        return;                                   // no cookie: a guest (req.user.id stays 0)

    req.sessionId = cookie->second;

    if (userForSession(req.sessionId, req.user))
        req.cartCount = cartItemCount(req.user.id);
}

// Decides which handler answers the request. Read it top to bottom: one line per URL.
Response handleRequest(Request& req)
{
    if (req.path.compare(0, 8, "/static/") == 0)
        return staticFile(req.path);

    identifyUser(req);

    const string& path = req.path;
    string method = (req.method == "HEAD") ? "GET" : req.method;

    // ---- anyone ----
    if (path == "/"         && method == "GET")  return homePage(req);
    if (path == "/products" && method == "GET")  return productsPage(req);
    if (path == "/product"  && method == "GET")  return productDetailPage(req);    // /product?id=3

    if (path == "/login"    && method == "GET")  return loginPage(req);
    if (path == "/login"    && method == "POST") return loginSubmit(req);
    if (path == "/signup"   && method == "GET")  return signupPage(req);
    if (path == "/signup"   && method == "POST") return signupSubmit(req);
    if (path == "/logout"   && method == "GET")  return logout(req);

    // ---- signed-in customers (each handler checks the login itself) ----
    if (path == "/cart"           && method == "GET")  return cartPage(req);
    if (path == "/cart/add"       && method == "POST") return cartAdd(req);
    if (path == "/cart/update"    && method == "POST") return cartUpdate(req);
    if (path == "/cart/remove"    && method == "POST") return cartRemove(req);

    if (path == "/checkout"       && method == "GET")  return checkoutPage(req);
    if (path == "/checkout/place" && method == "POST") return placeOrderSubmit(req);
    if (path == "/orders"         && method == "GET")  return myOrdersPage(req);
    if (path == "/orders/cancel"  && method == "POST") return cancelMyOrder(req);
    if (path == "/sessions"       && method == "GET")  return mySessionsPage(req);

    // ---- admin only (each handler checks the role itself) ----
    if (path == "/admin"                 && method == "GET")  return adminDashboard(req);

    if (path == "/admin/products"        && method == "GET")  return adminProducts(req);
    if (path == "/admin/products/edit"   && method == "GET")  return adminProductForm(req);   // ?id=3, or no id = new
    if (path == "/admin/products/save"   && method == "POST") return adminProductSave(req);
    if (path == "/admin/products/stock"  && method == "POST") return adminProductStock(req);
    if (path == "/admin/products/delete" && method == "POST") return adminProductDelete(req);

    if (path == "/admin/categories"        && method == "GET")  return adminCategories(req);
    if (path == "/admin/categories/save"   && method == "POST") return adminCategorySave(req);
    if (path == "/admin/categories/delete" && method == "POST") return adminCategoryDelete(req);

    if (path == "/admin/orders"        && method == "GET")  return adminOrders(req);
    if (path == "/admin/order"         && method == "GET")  return adminOrderDetail(req);   // ?id=7
    if (path == "/admin/order/status"  && method == "POST") return adminOrderStatus(req);

    if (path == "/admin/users"         && method == "GET")  return adminUsers(req);

    return notFoundPage(req);
}
