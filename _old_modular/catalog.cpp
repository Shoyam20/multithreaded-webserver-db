#include "catalog.h"
#include "database.h"
#include <mutex>
#include <cstdlib>

using namespace std;

static vector<Category> gCategories;

bool loadCategories()
{
    lock_guard<mutex> lock(dbMutex);

    if (mysql_query(dbConn,
        "SELECT category_id, category_name FROM categories ORDER BY category_id"))
        return false;

    MYSQL_RES* r = mysql_store_result(dbConn);
    if (!r)
        return false;

    gCategories.clear();

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(r)))
        gCategories.push_back({ atoi(row[0]), row[1] ? row[1] : "" });

    mysql_free_result(r);
    return true;
}

const vector<Category>& categories()
{
    return gCategories;
}

string categoryIcon(const string& n)
{
    if (n == "Electronics")            return "💡";
    if (n == "Accessories")            return "🔋";
    if (n == "Audio")                  return "🎧";
    if (n == "Mobile Phones")          return "📱";
    if (n == "Laptops")                return "💻";
    if (n == "Computers")              return "🖥️";
    if (n == "Cameras")                return "📷";
    if (n == "Gaming")                 return "🎮";
    if (n == "Clothing")               return "👕";
    if (n == "Shoes")                  return "👟";
    if (n == "Books")                  return "📚";
    if (n == "Home & Kitchen")         return "🍳";
    if (n == "Beauty & Personal Care") return "💄";
    if (n == "Sports & Fitness")       return "🏋️";
    if (n == "Toys & Games")           return "🧸";
    return "🛍️";
}

string categoryTint(int id)
{
    int hue = (id * 47) % 360;
    return "linear-gradient(135deg,hsl(" + to_string(hue) +
           ",75%,94%),hsl(" + to_string((hue + 30) % 360) + ",70%,86%))";
}
