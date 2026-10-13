#include "handlers.h"
#include "view.h"
#include "../services/category_service.h"
#include "../services/product_service.h"

using namespace std;

// ------------------------------------------------------------
//  Home page: a few product rows
// ------------------------------------------------------------
Response homePage(Request& req)
{
    Data page;

    ProductFilter budget;
    budget.maxPrice = 1000;
    budget.inStockOnly = true;
    budget.sort = "price_asc";
    budget.limit = 8;
    addProducts(page, "budget", findProducts(budget));

    ProductFilter fresh;
    fresh.inStockOnly = true;
    fresh.sort = "newest";
    fresh.limit = 8;
    addProducts(page, "newArrivals", findProducts(fresh));

    addProducts(page, "bestSellers", bestSellers(4));

    return renderPage(req, "Home", "pages/home.html", page);
}

// ------------------------------------------------------------
//  Product list: filters (category, search, price) and sorting
// ------------------------------------------------------------

// Build a /products?... link that keeps the current filters but changes some.
static string listUrl(StrMap current, const StrMap& changes)
{
    for (const auto& change : changes)
    {
        if (change.second.empty()) current.erase(change.first);
        else current[change.first] = change.second;
    }

    string url = "/products";
    char separator = '?';

    for (const auto& kv : current)
    {
        url += separator + kv.first + "=" + urlEncode(kv.second);
        separator = '&';
    }

    return url;
}

Response productsPage(Request& req)
{
    // Only these query parameters are kept in the filter links.
    StrMap current;
    for (const char* key : { "category", "q", "sort", "min", "max" })
        if (!req.param(key).empty()) current[key] = req.param(key);

    ProductFilter filter;
    filter.categoryId = req.intParam("category", 0);
    filter.search = req.param("q");
    filter.sort = req.param("sort");
    filter.minPrice = req.intParam("min", -1);
    filter.maxPrice = req.intParam("max", -1);

    Rows products = findProducts(filter);

    // Page heading
    string heading = "All products";
    for (const Category& c : categories())
        if (c.id == filter.categoryId)
            heading = categoryIcon(c.name) + " " + c.name;
    if (!filter.search.empty())
        heading = "Results for “" + filter.search + "”";

    Data page;
    page.set("heading", heading);
    page.set("count", (int)products.size());
    page.set("countLabel", products.size() == 1 ? "product" : "products");
    page.set("filterCategory", filter.categoryId > 0 ? to_string(filter.categoryId) : "");
    page.set("filterSearch", filter.search);
    page.set("filterSort", filter.sort);
    page.set("filterMin", filter.minPrice >= 0 ? to_string(filter.minPrice) : "");
    page.set("filterMax", filter.maxPrice >= 0 ? to_string(filter.maxPrice) : "");
    page.set("allCategoriesUrl", listUrl(current, { { "category", "" } }));
    page.flag("allCategoriesActive", filter.categoryId == 0);

    for (const Category& c : categories())
    {
        Data item;
        item.set("name", c.name).set("icon", categoryIcon(c.name));
        item.set("url", listUrl(current, { { "category", to_string(c.id) } }));
        item.flag("active", c.id == filter.categoryId);
        page.add("sideCategories", item);
    }

    struct SortOption { const char* value; const char* label; };
    for (const SortOption& s : { SortOption{ "", "Featured" }, { "price_asc", "Price ↑" },
                                 { "price_desc", "Price ↓" }, { "newest", "Newest" } })
    {
        Data item;
        item.set("label", s.label).set("url", listUrl(current, { { "sort", s.value } }));
        item.flag("active", filter.sort == s.value);
        page.add("sorts", item);
    }

    addProducts(page, "products", products);

    return renderPage(req, "Products", "pages/products.html", page);
}

// ------------------------------------------------------------
//  One product
// ------------------------------------------------------------
Response productDetailPage(Request& req)
{
    Row product = getProduct(req.intParam("id", 0));

    if (product.empty())
        return notFoundPage(req);

    Data page = productData(product);
    page.set("pageTitle", col(product, "name"));
    addProducts(page, "related",
                relatedProducts(atoi(col(product, "category_id").c_str()),
                                atoi(col(product, "product_id").c_str()), 4));

    return renderPage(req, col(product, "name"), "pages/product.html", page);
}
