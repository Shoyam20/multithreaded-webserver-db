#include "products.h"
#include "catalog.h"
#include "database.h"
#include "html_helpers.h"
#include "http_utils.h"
#include "request_context.h"
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <sstream>

using namespace std;

// ------------------------------------------------------------
//  shared pieces
// ------------------------------------------------------------
static string stockLabel(int stock)
{
    if (stock <= 0) return "<span class='stk no'>Out of stock</span>";
    if (stock <= 5) return "<span class='stk low'>Only " + to_string(stock) + " left</span>";
    return "<span class='stk ok'>In stock</span>";
}

static string param(const map<string, string>& p, const string& k)
{
    auto it = p.find(k);
    return it == p.end() ? "" : it->second;
}

// Digits only -> safe to splice into SQL.
static string digits(const string& s)
{
    string out;
    for (char c : s)
        if (c >= '0' && c <= '9') out += c;
    return out.size() > 9 ? out.substr(0, 9) : out;
}

string productCard(MYSQL_ROW row)
{
    string id = row[0] ? row[0] : "";
    string name = row[1] ? row[1] : "";
    string desc = row[2] ? row[2] : "";
    string price = row[3] ? row[3] : "0";
    int stock = row[4] ? atoi(row[4]) : 0;
    int catId = row[5] ? atoi(row[5]) : 0;
    string cat = row[6] ? row[6] : "";

    ostringstream t;

    t << "<div class='card'>"
         "<a href='/product?id=" << htmlEscape(id) << "'>"
         "<div class='thumb' style='background:" << categoryTint(catId) << "'>"
      << categoryIcon(cat);

    if (stock <= 0)
        t << "<span class='tag sold'>Sold out</span>";
    else if (stock <= 5)
        t << "<span class='tag low'>Few left</span>";

    t << "</div></a><div class='cbody'>"
         "<a class='cname' href='/product?id=" << htmlEscape(id) << "'>"
      << htmlEscape(name) << "</a>"
         "<div class='cdesc'>" << htmlEscape(desc) << "</div>"
         "<div class='price'>" << rupees(price) << "</div>"
      << stockLabel(stock)
      << "<div class='cbottom'>";

    if (stock <= 0)
    {
        t << "<button class='btn ghost block' disabled>Unavailable</button>";
    }
    else if (!isLoggedIn())
    {
        t << "<button class='btn accent block' type='button' "
             "onclick=\"openModal('loginModal')\">Sign in to buy</button>";
    }
    else
    {
        t << "<form method='GET' action='/add-cart'>"
             "<input type='hidden' name='product_id' value='" << htmlEscape(id) << "'>"
             "<input type='hidden' name='quantity' value='1'>"
             "<button class='btn accent block' type='submit'>Add to Cart</button></form>";
    }

    t << "</div></div></div>";

    return t.str();
}

// ------------------------------------------------------------
//  listing page
// ------------------------------------------------------------
// Rebuild the query string with one or more keys replaced.
static string listUrl(map<string, string> p, const map<string, string>& over)
{
    for (const auto& kv : over)
    {
        if (kv.second.empty()) p.erase(kv.first);
        else p[kv.first] = kv.second;
    }

    string url = "/products";
    char sep = '?';

    for (const auto& kv : p)
    {
        // Minimal encoding: only the characters that matter in a query string.
        string v;
        for (char c : kv.second)
        {
            if (isalnum((unsigned char)c) || c == '_' || c == '-' || c == '.') v += c;
            else { char b[8]; snprintf(b, sizeof(b), "%%%02X", (unsigned char)c); v += b; }
        }
        url += sep + kv.first + "=" + v;
        sep = '&';
    }

    return url;
}

string buildProductsPage(const map<string, string>& params)
{
    string catId = digits(param(params, "category"));
    string search = param(params, "q");
    string sort = param(params, "sort");
    string minP = digits(param(params, "min"));
    string maxP = digits(param(params, "max"));

    tlSearch = search;

    lock_guard<mutex> lock(dbMutex);

    string where = " WHERE 1=1";

    if (!catId.empty())
        where += " AND p.category_id=" + catId;

    if (!search.empty())
    {
        string like = esc(search);
        // Escape LIKE wildcards the user typed so they match literally.
        string safe;
        for (char c : like)
        {
            if (c == '%' || c == '_') safe += "\\\\";
            safe += c;
        }
        where += " AND (p.name LIKE '%" + safe + "%' OR p.description LIKE '%" + safe +
                 "%' OR c.category_name LIKE '%" + safe + "%')";
    }

    if (!minP.empty()) where += " AND p.price>=" + minP;
    if (!maxP.empty()) where += " AND p.price<=" + maxP;

    string order = "p.product_id";
    if (sort == "price_asc")  order = "p.price ASC, p.product_id";
    if (sort == "price_desc") order = "p.price DESC, p.product_id";
    if (sort == "newest")     order = "p.product_id DESC";
    if (sort == "name")       order = "p.name ASC";

    string q = "SELECT " PRODUCT_COLUMNS " FROM products p "
               "JOIN categories c ON p.category_id=c.category_id" +
               where + " ORDER BY " + order;

    if (mysql_query(dbConn, q.c_str()))
        return pageWrapper("Error",
            "<div class='err'>" + htmlEscape(mysql_error(dbConn)) + "</div>");

    MYSQL_RES* result = mysql_store_result(dbConn);

    if (!result)
        return pageWrapper("Error", "<div class='err'>No result</div>");

    // Heading
    string heading = "All products";
    for (const Category& c : categories())
        if (to_string(c.id) == catId)
            heading = categoryIcon(c.name) + " " + c.name;
    if (!search.empty())
        heading = "Results for “" + search + "”";

    ostringstream cards;
    int count = 0;
    MYSQL_ROW row;

    while ((row = mysql_fetch_row(result)))
    {
        cards << productCard(row);
        count++;
    }

    mysql_free_result(result);

    ostringstream t;

    // ----- sidebar -----
    t << "<div class='shop'><aside class='side'>"
         "<div class='grp'><h3>Categories</h3>"
         "<a class='f" << (catId.empty() ? " on" : "") << "' href='"
      << listUrl(params, {{"category", ""}}) << "'>All</a>";

    for (const Category& c : categories())
    {
        bool on = (to_string(c.id) == catId);
        t << "<a class='f" << (on ? " on" : "") << "' href='"
          << htmlEscape(listUrl(params, {{"category", to_string(c.id)}})) << "'><span>"
          << categoryIcon(c.name) << " " << htmlEscape(c.name) << "</span></a>";
    }

    t << "</div><div class='grp'><h3>Price (₹)</h3>"
         "<form method='GET' action='/products'>";

    if (!catId.empty())
        t << "<input type='hidden' name='category' value='" << catId << "'>";
    if (!search.empty())
        t << "<input type='hidden' name='q' value='" << htmlEscape(search) << "'>";
    if (!sort.empty())
        t << "<input type='hidden' name='sort' value='" << htmlEscape(sort) << "'>";

    t << "<div class='pricebox'>"
         "<input name='min' type='number' min='0' placeholder='Min' value='" << minP << "'>"
         "<input name='max' type='number' min='0' placeholder='Max' value='" << maxP << "'>"
         "</div><button class='btn block' type='submit'>Apply</button></form>"
         "</div></aside>";

    // ----- main column -----
    t << "<section><div class='toolbar'><div><h1>" << htmlEscape(heading) << "</h1>"
         "<span class='count'>" << count << (count == 1 ? " product" : " products")
      << "</span></div><div class='sort'>Sort:"
         "<a class='" << (sort.empty() ? "on" : "") << "' href='"
      << htmlEscape(listUrl(params, {{"sort", ""}})) << "'>Featured</a>"
         "<a class='" << (sort == "price_asc" ? "on" : "") << "' href='"
      << htmlEscape(listUrl(params, {{"sort", "price_asc"}})) << "'>Price ↑</a>"
         "<a class='" << (sort == "price_desc" ? "on" : "") << "' href='"
      << htmlEscape(listUrl(params, {{"sort", "price_desc"}})) << "'>Price ↓</a>"
         "<a class='" << (sort == "newest" ? "on" : "") << "' href='"
      << htmlEscape(listUrl(params, {{"sort", "newest"}})) << "'>Newest</a>"
         "</div></div>";

    if (count == 0)
    {
        t << "<div class='empty'><div class='big'>🔎</div>"
             "<h2>No products found</h2>"
             "<p>Try a different keyword or clear the filters.</p>"
             "<a class='btn' href='/products'>Clear filters</a></div>";
    }
    else
    {
        t << "<div class='grid'>" << cards.str() << "</div>";
    }

    t << "</section></div>";

    return pageWrapper("", t.str());
}

// ------------------------------------------------------------
//  detail page
// ------------------------------------------------------------
string buildProductDetailPage(int productId)
{
    lock_guard<mutex> lock(dbMutex);

    string q = "SELECT " PRODUCT_COLUMNS " FROM products p "
               "JOIN categories c ON p.category_id=c.category_id "
               "WHERE p.product_id=" + to_string(productId);

    if (mysql_query(dbConn, q.c_str()))
        return pageWrapper("Error",
            "<div class='err'>" + htmlEscape(mysql_error(dbConn)) + "</div>");

    MYSQL_RES* r = mysql_store_result(dbConn);
    MYSQL_ROW row = r ? mysql_fetch_row(r) : nullptr;

    if (!row)
    {
        if (r) mysql_free_result(r);
        tlStatus = 404;
        return pageWrapper("Product not found",
            "<div class='empty'><div class='big'>😕</div>"
            "<p>We couldn't find that product.</p>"
            "<a class='btn' href='/products'>Browse products</a></div>");
    }

    string id = row[0];
    string name = row[1] ? row[1] : "";
    string desc = row[2] ? row[2] : "";
    string price = row[3] ? row[3] : "0";
    int stock = row[4] ? atoi(row[4]) : 0;
    int catId = row[5] ? atoi(row[5]) : 0;
    string cat = row[6] ? row[6] : "";

    mysql_free_result(r);

    ostringstream t;

    t << "<div class='crumbs'><a href='/'>Home</a> › <a href='/products?category="
      << catId << "'>" << htmlEscape(cat) << "</a> › " << htmlEscape(name) << "</div>"
         "<div class='pd'>"
         "<div class='thumb big' style='background:" << categoryTint(catId) << "'>"
      << categoryIcon(cat) << "</div>"
         "<div class='info'><span class='chip'>" << htmlEscape(cat) << "</span>"
         "<h1>" << htmlEscape(name) << "</h1>"
         "<p>" << htmlEscape(desc) << "</p>"
         "<div class='price'>" << rupees(price) << " <small>inclusive of all taxes</small></div>"
      << stockLabel(stock);

    t << "<div class='buybox'>";

    if (stock <= 0)
    {
        t << "<button class='btn ghost lg' disabled>Currently unavailable</button>";
    }
    else if (!isLoggedIn())
    {
        t << "<button class='btn accent lg' type='button' "
             "onclick=\"openModal('loginModal')\">Sign in to buy</button>";
    }
    else
    {
        t << "<form method='GET' action='/add-cart' style='display:flex;gap:10px;flex-wrap:wrap'>"
             "<input type='hidden' name='product_id' value='" << id << "'>"
             "<div class='qty'><button type='button' onclick='stepQty(this,-1)'>−</button>"
             "<input type='number' name='quantity' value='1' min='1' max='" << stock << "'>"
             "<button type='button' onclick='stepQty(this,1)'>+</button></div>"
             "<button class='btn accent lg' type='submit'>Add to Cart</button>"
             "<button class='btn lg' type='submit' name='buy' value='1'>Buy Now</button>"
             "</form>";
    }

    t << "</div><ul class='facts'>"
         "<li>💵 Cash on Delivery available</li>"
         "<li>📦 Live stock — reserved when you place the order</li>"
         "<li>🔐 Order history saved to your account</li></ul></div></div>";

    // More from the same category.
    string more = "SELECT " PRODUCT_COLUMNS " FROM products p "
                  "JOIN categories c ON p.category_id=c.category_id "
                  "WHERE p.category_id=" + to_string(catId) +
                  " AND p.product_id<>" + to_string(productId) +
                  " ORDER BY p.product_id LIMIT 4";

    if (mysql_query(dbConn, more.c_str()) == 0)
    {
        MYSQL_RES* mr = mysql_store_result(dbConn);

        if (mr)
        {
            ostringstream cards;
            int n = 0;
            MYSQL_ROW mrow;

            while ((mrow = mysql_fetch_row(mr)))
            {
                cards << productCard(mrow);
                n++;
            }

            mysql_free_result(mr);

            if (n > 0)
                t << "<div class='sec'><h2>More in " << htmlEscape(cat) << "</h2></div>"
                     "<div class='grid'>" << cards.str() << "</div>";
        }
    }

    return pageWrapper("", t.str());
}
