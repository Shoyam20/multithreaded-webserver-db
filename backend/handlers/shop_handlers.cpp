#include "handlers.h"
#include "../services/services.h"
#include <cstdlib>
#include <map>

using namespace std;

// Home page: hero banner, category tiles and a few rows of products.
Response homePage(Request& req)
{
    bool loggedIn = req.user.loggedIn();

    string tiles;
    Rows cats = listCategories();

    for (size_t i = 0; i < cats.size(); i++)
    {
        Vars tile;
        tile["id"]   = col(cats[i], "category_id");
        tile["name"] = col(cats[i], "category_name");
        tile["icon"] = categoryIcon(col(cats[i], "category_name"));
        tiles += renderTemplate("partials/category_tile.html", tile);
    }

    Vars v;
    v["categoryTiles"] = tiles;
    v["heroButtons"]   = loggedIn ? "<a class=\"btn ghost lg\" href=\"/orders\">My orders</a>"
                                  : "<button class=\"btn ghost lg\" type=\"button\" onclick=\"openModal('signupModal')\">Create account</button>";
    v["budget"]        = productSection("🔥 Budget picks under ₹1,000", "/products?sort=price_asc&max=1000", cheapestProducts(1000, 8), loggedIn);
    v["newArrivals"]   = productSection("✨ New arrivals", "/products?sort=newest", newestProducts(8), loggedIn);
    v["bestSellers"]   = productSection("🏆 Best sellers", "/products", bestSellers(4), loggedIn);

    return renderPage(req, "Home", "pages/home.html", v);
}

// ---- the product list page -----------------------------------------------------------

// An <a> link with a CSS class.
static string linkTag(const string& cssClass, const string& url, const string& label)
{
    return "<a class=\"" + cssClass + "\" href=\"" + htmlEscape(url) + "\">" + htmlEscape(label) + "</a>";
}

// A hidden form field, so the price filter form keeps the other filters.
static string hiddenInput(const string& name, const string& value)
{
    if (value.empty()) return "";
    return "<input type=\"hidden\" name=\"" + name + "\" value=\"" + htmlEscape(value) + "\">";
}

// The /products?... link for the current filters, but with one filter changed.
static string listUrl(map<string, string> filters, const string& key, const string& value)
{
    if (value.empty()) filters.erase(key);
    else               filters[key] = value;

    string url = "/products";
    char separator = '?';

    for (map<string, string>::iterator it = filters.begin(); it != filters.end(); ++it)
    {
        url += separator;
        url += it->first + "=" + urlEncode(it->second);
        separator = '&';
    }

    return url;
}

// Filters (category, search, price) and sorting all come from the URL.
Response productsPage(Request& req)
{
    // The filters that are set, so the links can keep them.
    map<string, string> filters;
    const char* names[] = { "category", "q", "sort", "min", "max" };

    for (int i = 0; i < 5; i++)
        if (!req.param(names[i]).empty())
            filters[names[i]] = req.param(names[i]);

    int    categoryId = req.intParam("category", 0);
    string search     = req.param("q");
    string sort       = req.param("sort");
    int    minPrice   = req.intParam("min", -1);
    int    maxPrice   = req.intParam("max", -1);

    Rows products = findProducts(categoryId, search, sort, minPrice, maxPrice);

    // The category links on the left; the chosen category also becomes the page heading.
    string heading = "All products";
    string sideLinks = linkTag(categoryId == 0 ? "f on" : "f", listUrl(filters, "category", ""), "All");

    Rows cats = listCategories();
    for (size_t i = 0; i < cats.size(); i++)
    {
        string id   = col(cats[i], "category_id");
        string name = col(cats[i], "category_name");
        bool chosen = (atoi(id.c_str()) == categoryId);

        if (chosen) heading = categoryIcon(name) + " " + name;
        sideLinks += linkTag(chosen ? "f on" : "f", listUrl(filters, "category", id), categoryIcon(name) + " " + name);
    }

    if (!search.empty())
        heading = "Results for “" + search + "”";

    // The four "Sort:" links.
    const char* sortValues[] = { "", "price_asc", "price_desc", "newest" };
    const char* sortLabels[] = { "Featured", "Price ↑", "Price ↓", "Newest" };
    string sortLinks;

    for (int i = 0; i < 4; i++)
        sortLinks += linkTag(sort == sortValues[i] ? "on" : "", listUrl(filters, "sort", sortValues[i]), sortLabels[i]);

    string results;
    if (products.empty())
    {
        results = renderTemplate("partials/no_products.html", Vars());
    }
    else
    {
        string cards;
        for (size_t i = 0; i < products.size(); i++)
            cards += productCard(products[i], req.user.loggedIn());

        results = "<div class=\"grid\">" + cards + "</div>";
    }

    Vars v;
    v["heading"]       = heading;
    v["count"]         = to_string(products.size()) + (products.size() == 1 ? " product" : " products");
    v["sideLinks"]     = sideLinks;
    v["sortLinks"]     = sortLinks;
    v["hiddenFilters"] = hiddenInput("category", categoryId > 0 ? to_string(categoryId) : "") +
                         hiddenInput("q", search) + hiddenInput("sort", sort);
    v["filterMin"]     = minPrice >= 0 ? to_string(minPrice) : "";
    v["filterMax"]     = maxPrice >= 0 ? to_string(maxPrice) : "";
    v["results"]       = results;

    return renderPage(req, "Products", "pages/products.html", v);
}

// One product with its buy box and a row of related products.
Response productDetailPage(Request& req)
{
    Row product = getProduct(req.intParam("id", 0));

    if (product.empty())
        return notFoundPage(req);

    int stock = atoi(col(product, "stock").c_str());

    Vars v;
    v["id"]           = col(product, "product_id");
    v["name"]         = col(product, "name");
    v["description"]  = col(product, "description");
    v["price"]        = formatMoney(col(product, "price"));
    v["categoryId"]   = col(product, "category_id");
    v["categoryName"] = col(product, "category_name");
    v["icon"]         = categoryIcon(col(product, "category_name"));
    v["tint"]         = categoryTint(atoi(col(product, "category_id").c_str()));
    v["stockLabel"]   = stockLabel(stock);
    v["maxQty"]       = to_string(stock > 0 ? stock : 1);

    if (stock <= 0)
        v["buyBox"] = "<button class=\"btn ghost lg\" disabled>Currently unavailable</button>";
    else if (!req.user.loggedIn())
        v["buyBox"] = "<button class=\"btn accent lg\" type=\"button\" onclick=\"openModal('loginModal')\">Sign in to buy</button>";
    else
        v["buyBox"] = renderTemplate("partials/buy_form.html", v);

    Rows related = relatedProducts(atoi(col(product, "category_id").c_str()), atoi(col(product, "product_id").c_str()), 4);
    v["related"] = productSection("More in " + col(product, "category_name"), "/products?category=" + col(product, "category_id"),
                                  related, req.user.loggedIn());

    return renderPage(req, col(product, "name"), "pages/product.html", v);
}
