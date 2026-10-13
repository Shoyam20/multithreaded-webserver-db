#include "services.h"
#include "../config.h"
#include <cstdio>

using namespace std;

// The columns every product query selects (see services.h).
static const string PRODUCT_SELECT =
    "SELECT p.product_id, p.name, p.description, p.price, p.stock, p.category_id, c.category_name "
    "FROM products p JOIN categories c ON p.category_id=c.category_id ";

// Makes the text a user typed safe for a LIKE search: %, _ and \ must match themselves.
static string likePattern(const string& text)
{
    string escaped;

    for (size_t i = 0; i < text.size(); i++)
    {
        if (text[i] == '%' || text[i] == '_' || text[i] == '\\') escaped += '\\';
        escaped += text[i];
    }

    return dbQuote("%" + escaped + "%");
}

// ---- reading (storefront) -----------------------------------------------------------

Rows listCategories()
{
    lock_guard<mutex> lock(dbMutex);
    return dbRows("SELECT category_id, category_name FROM categories ORDER BY category_id");
}

Row getProduct(int productId)
{
    lock_guard<mutex> lock(dbMutex);

    Rows rows = dbRows(PRODUCT_SELECT + "WHERE p.product_id=" + to_string(productId));
    return rows.empty() ? Row() : rows[0];
}

// Builds a WHERE clause from the filters that are set, then adds ORDER BY.
Rows findProducts(int categoryId, const string& search, const string& sort, int minPrice, int maxPrice)
{
    lock_guard<mutex> lock(dbMutex);

    string where = "WHERE 1=1";

    if (categoryId > 0)
        where += " AND p.category_id=" + to_string(categoryId);

    if (!search.empty())
    {
        string like = likePattern(search);
        where += " AND (p.name LIKE " + like + " OR p.description LIKE " + like +
                 " OR c.category_name LIKE " + like + ")";
    }

    if (minPrice >= 0) where += " AND p.price>=" + to_string(minPrice);
    if (maxPrice >= 0) where += " AND p.price<=" + to_string(maxPrice);

    // The ORDER BY text is chosen from this fixed list, never copied from the URL.
    string order = "p.product_id";
    if (sort == "price_asc")  order = "p.price ASC, p.product_id";
    if (sort == "price_desc") order = "p.price DESC, p.product_id";
    if (sort == "newest")     order = "p.product_id DESC";

    return dbRows(PRODUCT_SELECT + where + " ORDER BY " + order);
}

Rows cheapestProducts(int maxPrice, int limit)
{
    lock_guard<mutex> lock(dbMutex);

    return dbRows(PRODUCT_SELECT + "WHERE p.stock>0 AND p.price<=" + to_string(maxPrice) +
                  " ORDER BY p.price ASC LIMIT " + to_string(limit));
}

Rows newestProducts(int limit)
{
    lock_guard<mutex> lock(dbMutex);

    return dbRows(PRODUCT_SELECT + "WHERE p.stock>0 ORDER BY p.product_id DESC LIMIT " + to_string(limit));
}

// Joins each product with the total number of units sold in orders that were not cancelled.
Rows bestSellers(int limit)
{
    lock_guard<mutex> lock(dbMutex);

    return dbRows(PRODUCT_SELECT +
        "JOIN (SELECT oi.product_id, SUM(oi.quantity) AS sold "
        "      FROM order_items oi JOIN orders o ON o.order_id=oi.order_id "
        "      WHERE o.status<>'CANCELLED' GROUP BY oi.product_id) s "
        "ON s.product_id=p.product_id "
        "WHERE p.stock>0 ORDER BY s.sold DESC LIMIT " + to_string(limit));
}

Rows relatedProducts(int categoryId, int excludeProductId, int limit)
{
    lock_guard<mutex> lock(dbMutex);

    return dbRows(PRODUCT_SELECT + "WHERE p.category_id=" + to_string(categoryId) +
                  " AND p.product_id<>" + to_string(excludeProductId) +
                  " ORDER BY p.product_id LIMIT " + to_string(limit));
}

// ---- admin: products ----------------------------------------------------------------

Rows adminProductList(const string& search, int categoryId, bool lowStockOnly)
{
    lock_guard<mutex> lock(dbMutex);

    string where = "WHERE 1=1";

    if (categoryId > 0)
        where += " AND p.category_id=" + to_string(categoryId);

    if (!search.empty())
    {
        string like = likePattern(search);
        where += " AND (p.name LIKE " + like + " OR p.description LIKE " + like + ")";
    }

    if (lowStockOnly)
        return dbRows(PRODUCT_SELECT + where + " AND p.stock<=" + to_string(LOW_STOCK_LEVEL) +
                      " ORDER BY p.stock ASC, p.product_id");

    return dbRows(PRODUCT_SELECT + where + " ORDER BY p.product_id");
}

// Checks the values from the admin form. Returns "" if they are fine. (Caller holds dbMutex.)
static string checkProductValues(const string& name, const string& description, double price, int stock, int categoryId)
{
    if (name.empty() || name.size() > 100)
        return "Name is required (max 100 characters).";

    if (description.size() > 255)
        return "Description can be at most 255 characters.";

    if (price < 0 || price > 99999999)
        return "Price must be between 0 and 99,999,999.";

    if (stock < 0)
        return "Stock cannot be negative.";

    Rows found = dbRows("SELECT category_id FROM categories WHERE category_id=" + to_string(categoryId));
    if (found.empty())
        return "Please choose a valid category.";

    return "";
}

// Price as text with two decimals, for SQL: 499 -> "499.00".
static string priceText(double price)
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.2f", price);
    return buffer;
}

string createProduct(const string& name, const string& description, double price, int stock, int categoryId)
{
    lock_guard<mutex> lock(dbMutex);

    string error = checkProductValues(name, description, price, stock, categoryId);
    if (!error.empty()) return error;

    Result r = dbRun("INSERT INTO products (name, description, price, stock, category_id) VALUES (" +
                     dbQuote(name) + "," + dbQuote(description) + "," + priceText(price) + "," +
                     to_string(stock) + "," + to_string(categoryId) + ")");

    return r.ok ? "" : "Database error: " + r.error;
}

string updateProduct(int productId, const string& name, const string& description, double price, int stock, int categoryId)
{
    lock_guard<mutex> lock(dbMutex);

    string error = checkProductValues(name, description, price, stock, categoryId);
    if (!error.empty()) return error;

    Result r = dbRun("UPDATE products SET name=" + dbQuote(name) +
                     ", description=" + dbQuote(description) +
                     ", price=" + priceText(price) +
                     ", stock=" + to_string(stock) +
                     ", category_id=" + to_string(categoryId) +
                     " WHERE product_id=" + to_string(productId));

    return r.ok ? "" : "Database error: " + r.error;
}

string setProductStock(int productId, int stock)
{
    if (stock < 0) return "Stock cannot be negative.";

    lock_guard<mutex> lock(dbMutex);

    Result r = dbRun("UPDATE products SET stock=" + to_string(stock) +
                     " WHERE product_id=" + to_string(productId));

    return r.ok ? "" : "Database error: " + r.error;
}

// A product that was ever ordered cannot be deleted (the order history needs it).
// Otherwise it is removed from the carts first (foreign key), then deleted - in one transaction.
string deleteProduct(int productId)
{
    lock_guard<mutex> lock(dbMutex);

    Rows used = dbRows("SELECT COUNT(*) AS n FROM order_items WHERE product_id=" + to_string(productId));

    if (!used.empty() && col(used[0], "n") != "0")
        return "This product appears in past orders, so it cannot be deleted. Set its stock to 0 instead.";

    dbRun("START TRANSACTION");

    Result carts = dbRun("DELETE FROM cart_items WHERE product_id=" + to_string(productId));
    if (!carts.ok) { dbRun("ROLLBACK"); return "Database error: " + carts.error; }

    Result product = dbRun("DELETE FROM products WHERE product_id=" + to_string(productId));
    if (!product.ok) { dbRun("ROLLBACK"); return "Database error: " + product.error; }

    dbRun("COMMIT");
    return "";
}

// ---- admin: categories --------------------------------------------------------------

Rows categoriesWithProductCount()
{
    lock_guard<mutex> lock(dbMutex);

    return dbRows("SELECT c.category_id, c.category_name, COUNT(p.product_id) AS products "
                  "FROM categories c LEFT JOIN products p ON p.category_id=c.category_id "
                  "GROUP BY c.category_id, c.category_name ORDER BY c.category_id");
}

// Turns a MySQL error into a friendlier message for category names.
static string categoryError(const string& mysqlError)
{
    if (mysqlError.find("Duplicate") != string::npos)
        return "A category with that name already exists.";

    return "Database error: " + mysqlError;
}

string createCategory(const string& name)
{
    if (name.empty() || name.size() > 50)
        return "Category name is required (max 50 characters).";

    lock_guard<mutex> lock(dbMutex);

    Result r = dbRun("INSERT INTO categories (category_name) VALUES (" + dbQuote(name) + ")");
    return r.ok ? "" : categoryError(r.error);
}

string renameCategory(int categoryId, const string& name)
{
    if (name.empty() || name.size() > 50)
        return "Category name is required (max 50 characters).";

    lock_guard<mutex> lock(dbMutex);

    Result r = dbRun("UPDATE categories SET category_name=" + dbQuote(name) +
                     " WHERE category_id=" + to_string(categoryId));
    return r.ok ? "" : categoryError(r.error);
}

string deleteCategory(int categoryId)
{
    lock_guard<mutex> lock(dbMutex);

    Rows used = dbRows("SELECT COUNT(*) AS n FROM products WHERE category_id=" + to_string(categoryId));

    if (!used.empty() && col(used[0], "n") != "0")
        return "This category still has products. Move or delete them first.";

    Result r = dbRun("DELETE FROM categories WHERE category_id=" + to_string(categoryId));
    return r.ok ? "" : "Database error: " + r.error;
}
