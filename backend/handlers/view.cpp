#include "view.h"
#include "../services/product_service.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace std;

// ---- small helpers -----------------------------------------------------------------------

// Money from the database ("3499.00") shown like "₹3,499" - Indian digit grouping (12,34,567).
string formatMoney(const string& decimalText)
{
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%.2f", fabs(atof(decimalText.c_str())));

    string number = buffer;
    string whole = number.substr(0, number.find('.'));
    string fraction = number.substr(number.find('.') + 1);

    string grouped;
    int length = (int)whole.size();

    for (int i = 0; i < length; i++)
    {
        int digitsLeft = length - i;

        // a comma before the last 3 digits, then before every 2 digits
        if (i > 0 && (digitsLeft == 3 || (digitsLeft > 3 && (digitsLeft - 3) % 2 == 0)))
            grouped += ',';

        grouped += whole[i];
    }

    string out = "₹" + grouped;
    if (fraction != "00") out += "." + fraction;
    return out;
}

string categoryIcon(const string& name)
{
    if (name == "Electronics")            return "💡";
    if (name == "Accessories")            return "🔋";
    if (name == "Audio")                  return "🎧";
    if (name == "Mobile Phones")          return "📱";
    if (name == "Laptops")                return "💻";
    if (name == "Computers")              return "🖥️";
    if (name == "Cameras")                return "📷";
    if (name == "Gaming")                 return "🎮";
    if (name == "Clothing")               return "👕";
    if (name == "Shoes")                  return "👟";
    if (name == "Books")                  return "📚";
    if (name == "Home & Kitchen")         return "🍳";
    if (name == "Beauty & Personal Care") return "💄";
    if (name == "Sports & Fitness")       return "🏋️";
    if (name == "Toys & Games")           return "🧸";
    return "🛍️";
}

// A different pastel colour for each category id.
string categoryTint(int id)
{
    int hue = (id * 47) % 360;

    return "linear-gradient(135deg,hsl(" + to_string(hue) + ",75%,94%),hsl(" +
           to_string((hue + 30) % 360) + ",70%,86%))";
}

string statusClass(const string& status)
{
    if (status == "PAID" || status == "DELIVERED")                         return "green";
    if (status == "PENDING" || status == "PLACED" || status == "SHIPPED")  return "amber";
    if (status == "CANCELLED")                                             return "red";
    return "";
}

string stockLabel(int stock)
{
    if (stock <= 0) return "<span class=\"stk no\">Out of stock</span>";
    if (stock <= 5) return "<span class=\"stk low\">Only " + to_string(stock) + " left</span>";
    return "<span class=\"stk ok\">In stock</span>";
}

string alertBox(const string& text)
{
    if (text.empty()) return "";
    return "<div class=\"err\">" + htmlEscape(text) + "</div>";
}

string optionTag(const string& value, const string& label, bool selected)
{
    return "<option value=\"" + htmlEscape(value) + "\"" + (selected ? " selected" : "") + ">" +
           htmlEscape(label) + "</option>";
}

// ---- repeated pieces ---------------------------------------------------------------------

// One product tile: picture, name, price, stock note and the right button for this visitor.
string productCard(const Row& product, bool loggedIn)
{
    int stock = atoi(col(product, "stock").c_str());

    Vars v;
    v["id"]          = col(product, "product_id");
    v["name"]        = col(product, "name");
    v["description"] = col(product, "description");
    v["price"]       = formatMoney(col(product, "price"));
    v["icon"]        = categoryIcon(col(product, "category_name"));
    v["tint"]        = categoryTint(atoi(col(product, "category_id").c_str()));
    v["stockLabel"]  = stockLabel(stock);

    if (stock <= 0)       v["tags"] = "<span class=\"tag sold\">Sold out</span>";
    else if (stock <= 5)  v["tags"] = "<span class=\"tag low\">Few left</span>";
    else                  v["tags"] = "";

    if (stock <= 0)
        v["buyButton"] = "<button class=\"btn ghost block\" disabled>Unavailable</button>";
    else if (!loggedIn)
        v["buyButton"] = "<button class=\"btn accent block\" type=\"button\" onclick=\"openModal('loginModal')\">Sign in to buy</button>";
    else
        v["buyButton"] = renderTemplate("partials/add_to_cart_form.html", v);

    return renderTemplate("partials/product_card.html", v);
}

// A heading with a "See all" link and a grid of product cards. Empty text when there are no products.
string productSection(const string& title, const string& moreUrl, const Rows& products, bool loggedIn)
{
    if (products.empty()) return "";

    string cards;
    for (size_t i = 0; i < products.size(); i++)
        cards += productCard(products[i], loggedIn);

    Vars v;
    v["title"]   = title;
    v["moreUrl"] = moreUrl;
    v["cards"]   = cards;
    return renderTemplate("partials/product_section.html", v);
}

string adminBackUrl(const Request& req, const string& fallback)
{
    string back = req.param("back");

    bool ok = back.compare(0, 6, "/admin") == 0 && back.find("//") == string::npos &&
              back.find('\n') == string::npos && back.find('\r') == string::npos;

    return ok ? back : fallback;
}

// ---- whole pages -------------------------------------------------------------------------

// Messages shown in the green/red bar after a redirect, e.g. /cart?notice=added.
// The URL only carries a short code, so a crafted link can never display text of its own.
struct Notice { const char* code; const char* text; bool isError; };

static const Notice notices[] = {
    { "login",     "✅ Welcome back! You are signed in.", false },
    { "signup",    "🎉 Account created. Please sign in.", false },
    { "logout",    "👋 You have been signed out.", false },
    { "added",     "🛒 Item added to your cart.", false },
    { "removed",   "🗑️ Item removed from your cart.", false },
    { "updated",   "✅ Cart updated.", false },
    { "ordered",   "📦 Order placed successfully! You can cancel it until it is delivered.", false },
    { "cancelled", "↩️ Order cancelled. The items went back to stock and the payment was cancelled.", false },
    { "needlogin", "Please sign in to continue.", true },
    { "forbidden", "You do not have access to the admin panel.", true },
    { "saved",     "✅ Saved.", false },
    { "created",   "✅ Created.", false },
    { "deleted",   "🗑️ Deleted.", false },
    { "stock",     "✅ Stock updated.", false },
    { "status",    "✅ Order status updated.", false },
};

// The green/red message bar for the ?notice=code in the URL ("" if there is none).
static string noticeBar(const Request& req)
{
    string code = req.param("notice");

    for (size_t i = 0; i < sizeof(notices) / sizeof(notices[0]); i++)
        if (code == notices[i].code)
            return string("<div class=\"flash") + (notices[i].isError ? " err" : "") + "\">" + notices[i].text + "</div>";

    return "";
}

// Fills in the parts every storefront page shows: header, search box, category menu, footer.
Response renderPage(Request& req, const string& title, const string& tpl, Vars vars)
{
    bool loggedIn = req.user.loggedIn();

    string options, links;
    Rows cats = listCategories();

    for (size_t i = 0; i < cats.size(); i++)
    {
        string id = col(cats[i], "category_id");
        string name = col(cats[i], "category_name");

        options += optionTag(id, name, false);
        links += "<a href=\"/products?category=" + id + "\">" + categoryIcon(name) + " " + htmlEscape(name) + "</a>";
    }

    vars["title"]           = title;
    vars["notice"]          = noticeBar(req);
    vars["cartCount"]       = to_string(req.cartCount);
    vars["search"]          = req.param("q");
    vars["categoryOptions"] = options;
    vars["categoryLinks"]   = links;
    vars["userName"]        = req.user.name;
    vars["adminLink"]       = req.user.isAdmin() ? "<a href=\"/admin\">🛠️ Admin</a>" : "";
    vars["adminMenuItem"]   = req.user.isAdmin() ? "<a href=\"/admin\">🛠️ Admin panel</a>" : "";
    vars["accountMenu"]     = renderTemplate(loggedIn ? "partials/account_user.html" : "partials/account_guest.html", vars);
    vars["guestModals"]     = loggedIn ? "" : renderTemplate("partials/guest_modals.html", vars);
    vars["content"]         = renderTemplate(tpl, vars);

    return Response::html(renderTemplate("layout.html", vars));
}

Response renderAdminPage(Request& req, const string& title, const string& tpl, const string& section, Vars vars)
{
    vars["title"]            = title;
    vars["notice"]           = noticeBar(req);
    vars["userName"]         = req.user.name;
    vars["nav_" + section]   = "active";
    vars["content"]          = renderTemplate(tpl, vars);

    return Response::html(renderTemplate("admin/layout.html", vars));
}

Response messagePage(Request& req, const string& title, const string& text,
                     const string& linkUrl, const string& linkText, int status)
{
    Vars v;
    v["heading"]  = title;
    v["text"]     = text;
    v["linkUrl"]  = linkUrl;
    v["linkText"] = linkText;

    Response res = renderPage(req, title, "pages/message.html", v);
    res.status = status;
    return res;
}

Response adminMessagePage(Request& req, const string& title, const string& text,
                          const string& linkUrl, const string& linkText)
{
    Vars v;
    v["text"]     = text;
    v["linkUrl"]  = linkUrl;
    v["linkText"] = linkText;

    return renderAdminPage(req, title, "admin/message.html", "", v);
}

Response notFoundPage(Request& req)
{
    return messagePage(req, "Page not found", "The page you are looking for does not exist.", "/", "Go home", 404);
}
