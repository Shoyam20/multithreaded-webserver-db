#include "home.h"
#include "catalog.h"
#include "database.h"
#include "html_helpers.h"
#include "products.h"
#include "request_context.h"
#include <sstream>

using namespace std;

// Runs a product query (PRODUCT_COLUMNS order) and renders it as a grid.
// Caller must hold dbMutex.
static string productRow(const string& title, const string& moreLink,
                         const string& whereAndOrder)
{
    string q = "SELECT " PRODUCT_COLUMNS " FROM products p "
               "JOIN categories c ON p.category_id=c.category_id " + whereAndOrder;

    if (mysql_query(dbConn, q.c_str()))
        return "";

    MYSQL_RES* r = mysql_store_result(dbConn);

    if (!r)
        return "";

    ostringstream cards;
    int n = 0;
    MYSQL_ROW row;

    while ((row = mysql_fetch_row(r)))
    {
        cards << productCard(row);
        n++;
    }

    mysql_free_result(r);

    if (n == 0)
        return "";

    return "<div class='sec'><h2>" + title + "</h2><a href='" + moreLink +
           "'>See all →</a></div><div class='grid'>" + cards.str() + "</div>";
}

string buildHomePage()
{
    ostringstream b;

    b << "<section class='hero'><div>"
         "<h1>Everything you need,<br>delivered to your door.</h1>"
         "<p>Shop electronics, fashion, books and more. Live stock, a simple cart, "
         "and Cash on Delivery at checkout.</p>";

    if (isLoggedIn())
        b << "<a class='btn accent lg' href='/products'>Start shopping</a> "
             "<a class='btn ghost lg' href='/orders'>My orders</a>";
    else
        b << "<a class='btn accent lg' href='/products'>Start shopping</a> "
             "<button class='btn ghost lg' type='button' "
             "onclick=\"openModal('signupModal')\">Create account</button>";

    b << "</div><div class='art'>🛍️📱🎧</div></section>";

    b << "<div class='perks'>"
         "<div class='perk'><span class='i'>💵</span><div><b>Cash on Delivery</b>"
         "<span>Pay when your order arrives</span></div></div>"
         "<div class='perk'><span class='i'>📦</span><div><b>Live stock</b>"
         "<span>Quantities update as people buy</span></div></div>"
         "<div class='perk'><span class='i'>🧾</span><div><b>Order history</b>"
         "<span>Every order saved to your account</span></div></div>"
         "<div class='perk'><span class='i'>🔐</span><div><b>Session login</b>"
         "<span>See where you are signed in</span></div></div></div>";

    b << "<div class='sec'><h2>Shop by category</h2><a href='/products'>All products →</a></div>"
         "<div class='cats'>";

    for (const Category& c : categories())
        b << "<a class='cat' href='/products?category=" << c.id << "'>"
             "<span class='ci'>" << categoryIcon(c.name) << "</span><b>"
          << htmlEscape(c.name) << "</b></a>";

    b << "</div>";

    {
        lock_guard<mutex> lock(dbMutex);

        b << productRow("🔥 Budget picks under ₹1,000",
                        "/products?sort=price_asc&max=1000",
                        "WHERE p.price<=1000 AND p.stock>0 ORDER BY p.price ASC LIMIT 8");

        b << productRow("✨ New arrivals", "/products?sort=newest",
                        "WHERE p.stock>0 ORDER BY p.product_id DESC LIMIT 8");

        b << productRow("📱 Phones &amp; laptops", "/products?category=4",
                        "WHERE p.category_id IN (4,5) AND p.stock>0 "
                        "ORDER BY p.price DESC LIMIT 4");
    }

    return pageWrapper("", b.str());
}
