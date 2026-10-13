#include "product_service.h"
#include "category_service.h"
#include <cstdio>

using namespace std;

static const char* SELECT_PRODUCTS =
    "SELECT p.product_id, p.name, p.description, p.price, p.stock, "
    "p.category_id, c.category_name "
    "FROM products p JOIN categories c ON p.category_id=c.category_id ";

// Make a user's search text safe for LIKE: %, _ and \ must match literally.
static string likePattern(const string& text)
{
    string escaped;

    for (char c : text)
    {
        if (c == '%' || c == '_' || c == '\\') escaped += '\\';
        escaped += c;
    }

    return db.quote("%" + escaped + "%");
}

// ------------------------------------------------------------
//  Reading
// ------------------------------------------------------------
Rows findProducts(const ProductFilter& f)
{
    string where = "WHERE 1=1";

    if (f.categoryId > 0)
        where += " AND p.category_id=" + to_string(f.categoryId);

    if (!f.search.empty())
    {
        string like = likePattern(f.search);
        where += " AND (p.name LIKE " + like + " OR p.description LIKE " + like +
                 " OR c.category_name LIKE " + like + ")";
    }

    if (f.minPrice >= 0)  where += " AND p.price>=" + to_string(f.minPrice);
    if (f.maxPrice >= 0)  where += " AND p.price<=" + to_string(f.maxPrice);
    if (f.maxStock >= 0)  where += " AND p.stock<=" + to_string(f.maxStock);
    if (f.inStockOnly)    where += " AND p.stock>0";

    string order = "p.product_id";
    if (f.sort == "price_asc")  order = "p.price ASC, p.product_id";
    if (f.sort == "price_desc") order = "p.price DESC, p.product_id";
    if (f.sort == "newest")     order = "p.product_id DESC";
    if (f.sort == "name")       order = "p.name ASC";
    if (f.sort == "stock_asc")  order = "p.stock ASC, p.product_id";

    string sql = string(SELECT_PRODUCTS) + where + " ORDER BY " + order;

    if (f.limit > 0)
        sql += " LIMIT " + to_string(f.limit);

    return db.rows(sql);
}

Row getProduct(int productId)
{
    Rows r = db.rows(string(SELECT_PRODUCTS) + "WHERE p.product_id=" + to_string(productId));
    return r.empty() ? Row() : r[0];
}

Rows relatedProducts(int categoryId, int excludeProductId, int limit)
{
    return db.rows(string(SELECT_PRODUCTS) +
                   "WHERE p.category_id=" + to_string(categoryId) +
                   " AND p.product_id<>" + to_string(excludeProductId) +
                   " ORDER BY p.product_id LIMIT " + to_string(limit));
}

Rows bestSellers(int limit)
{
    return db.rows(string(SELECT_PRODUCTS) +
        "JOIN (SELECT oi.product_id, SUM(oi.quantity) AS sold "
        "      FROM order_items oi JOIN orders o ON o.order_id=oi.order_id "
        "      WHERE o.status<>'CANCELLED' GROUP BY oi.product_id) s "
        "ON s.product_id=p.product_id "
        "WHERE p.stock>0 ORDER BY s.sold DESC LIMIT " + to_string(limit));
}

// ------------------------------------------------------------
//  Admin writes
// ------------------------------------------------------------
static string money2(double v)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%.2f", v);
    return buf;
}

static string validate(const ProductInput& in)
{
    if (in.name.empty() || in.name.size() > 100)
        return "Name is required (max 100 characters).";

    if (in.description.size() > 255)
        return "Description can be at most 255 characters.";

    if (in.price < 0 || in.price > 99999999)
        return "Price must be between 0 and 99,999,999.";

    if (in.stock < 0)
        return "Stock cannot be negative.";

    bool categoryExists = false;
    for (const Category& c : categories())
        if (c.id == in.categoryId) categoryExists = true;

    if (!categoryExists)
        return "Please choose a valid category.";

    return "";
}

string createProduct(const ProductInput& in, int& newId)
{
    string error = validate(in);
    if (!error.empty()) return error;

    Result r = db.run(
        "INSERT INTO products (name, description, price, stock, category_id) VALUES (" +
        db.quote(in.name) + "," + db.quote(in.description) + "," + money2(in.price) + "," +
        to_string(in.stock) + "," + to_string(in.categoryId) + ")");

    if (!r.ok) return "Database error: " + r.error;

    newId = (int)r.insertId;
    return "";
}

string updateProduct(int productId, const ProductInput& in)
{
    string error = validate(in);
    if (!error.empty()) return error;

    Result r = db.run(
        "UPDATE products SET name=" + db.quote(in.name) +
        ", description=" + db.quote(in.description) +
        ", price=" + money2(in.price) +
        ", stock=" + to_string(in.stock) +
        ", category_id=" + to_string(in.categoryId) +
        " WHERE product_id=" + to_string(productId));

    return r.ok ? "" : "Database error: " + r.error;
}

string setProductStock(int productId, int stock)
{
    if (stock < 0) return "Stock cannot be negative.";

    Result r = db.run("UPDATE products SET stock=" + to_string(stock) +
                      " WHERE product_id=" + to_string(productId));

    return r.ok ? "" : "Database error: " + r.error;
}

string deleteProduct(int productId)
{
    Transaction tx;

    Rows used = db.rows("SELECT COUNT(*) AS n FROM order_items WHERE product_id=" +
                        to_string(productId));

    if (!used.empty() && col(used[0], "n") != "0")
        return "This product appears in past orders, so it cannot be deleted. "
               "Set its stock to 0 instead.";

    // Remove it from any carts first (foreign key), then delete it.
    db.run("DELETE FROM cart_items WHERE product_id=" + to_string(productId));
    Result r = db.run("DELETE FROM products WHERE product_id=" + to_string(productId));

    if (!r.ok) return "Database error: " + r.error;

    tx.commit();
    return "";
}
