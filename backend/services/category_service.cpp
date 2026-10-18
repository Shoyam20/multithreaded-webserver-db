#include "category_service.h"
#include <cstdlib>
#include <mutex>

using namespace std;

// Cache category data in memory for quick access.
static vector<Category> cache;
static mutex cacheMutex;  // Protect the cache during concurrent access.

// Load categories from the database and refresh the cache.
void loadCategories()
{
    Rows rows = db.rows("SELECT category_id, category_name FROM categories ORDER BY category_id");

    vector<Category> fresh;

    for (const Row& r : rows)
        fresh.push_back({ atoi(col(r, "category_id").c_str()), col(r, "category_name") });

    // Lock the cache before updating it to avoid thread conflicts.
    lock_guard<mutex> lock(cacheMutex);
    cache = fresh;
}

// Return a copy of the cached categories safely.
vector<Category> categories()
{
    lock_guard<mutex> lock(cacheMutex);
    return cache;
}

// Get each category along with the number of products it contains.
Rows categoriesWithProductCount()
{
    return db.rows(
        "SELECT c.category_id, c.category_name, COUNT(p.product_id) AS products "
        "FROM categories c LEFT JOIN products p ON p.category_id=c.category_id "
        "GROUP BY c.category_id, c.category_name ORDER BY c.category_id");
}

// Validate the category name before inserting or updating it.
static string validName(const string& name)
{
    if (name.empty() || name.size() > 50)
        return "Category name is required (max 50 characters).";
    return "";
}

// Create a new category in the database.
string createCategory(const string& name)
{
    string error = validName(name);
    if (!error.empty()) return error;

    Result r = db.run("INSERT INTO categories (category_name) VALUES (" + db.quote(name) + ")");

    if (!r.ok)
        return r.error.find("Duplicate") != string::npos
               ? "A category with that name already exists."
               : "Database error: " + r.error;

    loadCategories();  
    return "";
}

// Rename an existing category.
string renameCategory(int categoryId, const string& name)
{
    string error = validName(name);
    if (!error.empty()) return error;

    Result r = db.run("UPDATE categories SET category_name=" + db.quote(name) +
                      " WHERE category_id=" + to_string(categoryId));

    if (!r.ok)
        return r.error.find("Duplicate") != string::npos
               ? "A category with that name already exists."
               : "Database error: " + r.error;

    loadCategories();  
    return "";
}

// Delete a category only if no products are assigned to it.
string deleteCategory(int categoryId)
{
    Rows used = db.rows("SELECT COUNT(*) AS n FROM products WHERE category_id=" +
                        to_string(categoryId));

    if (!used.empty() && col(used[0], "n") != "0")
        return "This category still has products. Move or delete them first.";

    Result r = db.run("DELETE FROM categories WHERE category_id=" + to_string(categoryId));
    if (!r.ok) return "Database error: " + r.error;

    loadCategories();  
    return "";
}
