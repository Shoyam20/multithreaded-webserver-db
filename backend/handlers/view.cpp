#include "view.h"
#include "../config.h"
#include "../services/category_service.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace std;

// ------------------------------------------------------------
//  Formatting
// ------------------------------------------------------------
string money(double amount)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "%.2f", fabs(amount));

    string num = buf;
    string whole = num.substr(0, num.find('.'));
    string frac = num.substr(num.find('.') + 1);

    // Indian digit grouping: 12,34,567
    string grouped;
    int n = (int)whole.size();

    for (int i = 0; i < n; i++)
    {
        int fromEnd = n - i;
        if (i > 0 && (fromEnd == 3 || (fromEnd > 3 && (fromEnd - 3) % 2 == 0)))
            grouped += ',';
        grouped += whole[i];
    }

    string out = string(amount < 0 ? "-" : "") + "₹" + grouped;
    if (frac != "00") out += "." + frac;
    return out;
}

string money(const string& decimalText)
{
    return money(atof(decimalText.c_str()));
}

string categoryIcon(const string& n)
{
    if (n == "Electronics")            return "💡";
    if (n == "Accessories")            return "🔋";
    if (n == "Audio")                  return "🎧";
    if (n == "Mobile Phones")          return "📱";
    if (n == "Laptops")                return "💻";
    if (n == "Computers")              return "🖥️";
    if (n == "Cameras")                return "📷";
    if (n == "Gaming")                 return "🎮";
    if (n == "Clothing")               return "👕";
    if (n == "Shoes")                  return "👟";
    if (n == "Books")                  return "📚";
    if (n == "Home & Kitchen")         return "🍳";
    if (n == "Beauty & Personal Care") return "💄";
    if (n == "Sports & Fitness")       return "🏋️";
    if (n == "Toys & Games")           return "🧸";
    return "🛍️";
}

string categoryTint(int id)
{
    int hue = (id * 47) % 360;
    return "linear-gradient(135deg,hsl(" + to_string(hue) + ",75%,94%),hsl(" +
           to_string((hue + 30) % 360) + ",70%,86%))";
}

// ------------------------------------------------------------
//  Flash messages: pages link to "?notice=code" and the code is looked up
//  here, so a crafted link can never print arbitrary text.
// ------------------------------------------------------------
struct Notice { const char* code; const char* text; bool isError; };

static const Notice notices[] = {
    { "login",        "✅ Welcome back! You are signed in.", false },
    { "signup",       "🎉 Account created. Please sign in.", false },
    { "logout",       "👋 You have been signed out.", false },
    { "added",        "🛒 Item added to your cart.", false },
    { "removed",      "🗑️ Item removed from your cart.", false },
    { "updated",      "✅ Cart updated.", false },
    { "ordered",      "📦 Order placed! Demo: it will be delivered in about 1-2 minutes - you can cancel until then.", false },
    { "cancelled",    "↩️ Order cancelled. The items went back to stock and the payment was cancelled.", false },
    { "needlogin",    "Please sign in to continue.", true },
    { "forbidden",    "You do not have access to the admin panel.", true },
    { "saved",        "✅ Saved.", false },
    { "created",      "✅ Created.", false },
    { "deleted",      "🗑️ Deleted.", false },
    { "stock",        "✅ Stock updated.", false },
    { "status",       "✅ Order status updated.", false },
    { "payment",      "✅ Payment status updated.", false },
    { "role",         "✅ User role updated.", false },
    { "sessionended", "✅ Session ended.", false },
};

// ------------------------------------------------------------
//  Page building
// ------------------------------------------------------------
static Data baseData(const Request& req)
{
    Data d;

    d.flag("loggedIn", req.user.loggedIn());
    d.flag("isAdmin", req.user.isAdmin());
    d.set("userName", req.user.name);
    d.set("cartCount", req.cartCount);

    auto q = req.query.find("q");
    d.set("search", q == req.query.end() ? "" : q->second);

    for (const Category& c : categories())
    {
        Data item;
        item.set("id", c.id).set("name", c.name).set("icon", categoryIcon(c.name));
        d.add("categories", item);
    }

    auto n = req.query.find("notice");
    if (n != req.query.end())
        for (const Notice& notice : notices)
            if (n->second == notice.code)
                d.set("noticeText", notice.text).set("noticeClass", notice.isError ? "err" : "");

    return d;
}

Response renderPage(Request& req, const string& title, const string& tpl, const Data& page)
{
    Data d = baseData(req);
    d.merge(page);
    d.set("title", title);
    d.set("content", renderTemplate(tpl, d));

    return Response::html(renderTemplate("layout.html", d));
}

Response renderAdminPage(Request& req, const string& title, const string& tpl,
                         const string& section, const Data& page)
{
    Data d = baseData(req);
    d.merge(page);
    d.set("title", title);
    d.set("nav_" + section, "active");
    d.set("content", renderTemplate(tpl, d));

    return Response::html(renderTemplate("admin/layout.html", d));
}

Response messagePage(Request& req, const string& title, const string& text,
                     const string& linkUrl, const string& linkText, int status)
{
    Data d;
    d.set("heading", title).set("text", text).set("linkUrl", linkUrl).set("linkText", linkText);

    Response res = renderPage(req, title, "pages/message.html", d);
    res.status = status;
    return res;
}

Response adminMessagePage(Request& req, const string& title, const string& text,
                          const string& linkUrl, const string& linkText)
{
    Data d;
    d.set("heading", title).set("text", text).set("linkUrl", linkUrl).set("linkText", linkText);

    return renderAdminPage(req, title, "admin/message.html", "", d);
}

Response notFoundPage(Request& req)
{
    return messagePage(req, "Page not found",
                       "The page you are looking for does not exist.", "/", "Go home", 404);
}

// ------------------------------------------------------------
//  Product data
// ------------------------------------------------------------
Data productData(const Row& p)
{
    int stock = atoi(col(p, "stock").c_str());
    int categoryId = atoi(col(p, "category_id").c_str());

    Data d;
    d.set("id", col(p, "product_id"));
    d.set("name", col(p, "name"));
    d.set("description", col(p, "description"));
    d.set("price", money(col(p, "price")));
    d.set("priceNumber", col(p, "price"));               // plain number for edit forms
    d.set("stock", stock);
    d.set("categoryId", categoryId);
    d.set("categoryName", col(p, "category_name"));
    d.set("icon", categoryIcon(col(p, "category_name")));
    d.set("tint", categoryTint(categoryId));
    d.flag("soldOut", stock <= 0);
    d.flag("lowStock", stock > 0 && stock <= 5);
    d.flag("lowForAdmin", stock <= config::lowStockThreshold);
    d.set("maxQty", stock > 0 ? stock : 1);
    return d;
}

void addProducts(Data& page, const string& listName, const Rows& products)
{
    for (const Row& p : products)
        page.add(listName, productData(p));
}

string adminBackUrl(const Request& req, const string& fallback)
{
    string back = req.param("back");

    bool ok = back.compare(0, 6, "/admin") == 0 && back.find("//") == string::npos &&
              back.find('\n') == string::npos && back.find('\r') == string::npos;

    return ok ? back : fallback;
}

string statusClass(const string& status)
{
    if (status == "PAID" || status == "DELIVERED")                           return "green";
    if (status == "PENDING" || status == "PLACED" || status == "SHIPPED")    return "amber";
    if (status == "CANCELLED" || status == "FAILED" || status == "REFUNDED") return "red";
    return "";
}
