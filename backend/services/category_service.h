#pragma once
#include "../db/database.h"
#include <string>
#include <vector>

struct Category
{
    int         id;
    std::string name;
};

// Categories are cached in memory (they rarely change) so every page can show
// the menu without a database query. Reloaded after an admin edits them.
void loadCategories();
std::vector<Category> categories();

Rows categoriesWithProductCount();    // category_id, category_name, products

// Admin: each returns "" on success or an error message.
std::string createCategory(const std::string& name);
std::string renameCategory(int categoryId, const std::string& name);
std::string deleteCategory(int categoryId);
