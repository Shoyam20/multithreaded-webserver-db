#pragma once
#include <string>
#include <vector>

struct Category
{
    int id;
    std::string name;
};

// Loaded once at startup (before workers run) so page layout can list
// categories without touching the DB while dbMutex is held.
bool loadCategories();
const std::vector<Category>& categories();

std::string categoryIcon(const std::string& categoryName);
std::string categoryTint(int categoryId);   // CSS background for a product tile
