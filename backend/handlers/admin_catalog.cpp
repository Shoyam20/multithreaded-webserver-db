#include "handlers.h"
#include "view.h"
#include "../config.h"
#include "../services/product_service.h"
#include <cstdlib>

using namespace std;

// Removes spaces and line breaks at both ends of text typed in a form.
static string trimmed(const string& text)
{
    size_t first = text.find_first_not_of(" \t\r\n");
    if (first == string::npos) return "";

    size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

// The <option> tags of a category drop-down; the category with id 'chosen' is selected.
static string categoryOptions(int chosen)
{
    string options;
    Rows cats = listCategories();

    for (size_t i = 0; i < cats.size(); i++)
        options += optionTag(col(cats[i], "category_id"), col(cats[i], "category_name"),
                             atoi(col(cats[i], "category_id").c_str()) == chosen);

    return options;
}

// ---- products -------------------------------------------------------------------------

// The product table, with search, category filter and a "low stock only" switch.
Response adminProducts(Request& req)
{
    if (!req.user.isAdmin()) return Response::redirect(req.user.loggedIn() ? "/?notice=forbidden" : "/login?notice=needlogin");

    string search   = trimmed(req.param("q"));
    int categoryId  = req.intParam("category", 0);
    bool lowOnly    = !req.param("low").empty();

    Rows products = adminProductList(search, categoryId, lowOnly);

    // After changing a stock number the admin comes back to this same list.
    string backUrl = lowOnly ? "/admin/products?low=1" : "/admin/products";

    string rows;
    for (size_t i = 0; i < products.size(); i++)
    {
        int stock = atoi(col(products[i], "stock").c_str());

        Vars row;
        row["id"]           = col(products[i], "product_id");
        row["name"]         = col(products[i], "name");
        row["icon"]         = categoryIcon(col(products[i], "category_name"));
        row["categoryName"] = col(products[i], "category_name");
        row["price"]        = formatMoney(col(products[i], "price"));
        row["stock"]        = to_string(stock);
        row["lowClass"]     = stock <= LOW_STOCK_LEVEL ? "lowinput" : "";
        row["backUrl"]      = backUrl;
        rows += renderTemplate("partials/admin_product_row.html", row);
    }

    Vars v;
    v["search"]          = search;
    v["categoryOptions"] = categoryOptions(categoryId);
    v["count"]           = to_string(products.size());
    v["lowNote"]         = lowOnly ? " - showing low stock only (<a href=\"/admin/products\">show all</a>)" : "";
    v["rows"]            = rows;

    return renderAdminPage(req, "Products", "admin/products.html", "products", v);
}

// The add / edit form. It is also used to show the form again with an error message.
static Response productFormPage(Request& req, int productId, const string& name, const string& description,
                                const string& price, const string& stock, int categoryId, const string& error)
{
    Vars v;
    v["formTitle"]       = productId > 0 ? "Edit product" : "Add product";
    v["productId"]       = productId > 0 ? to_string(productId) : "";
    v["name"]            = name;
    v["description"]     = description;
    v["price"]           = price;
    v["stock"]           = stock;
    v["error"]           = alertBox(error);
    v["categoryOptions"] = categoryOptions(categoryId);

    return renderAdminPage(req, v["formTitle"], "admin/product_form.html", "products", v);
}

// GET /admin/products/edit?id=3 (edit) or without id (new product).
Response adminProductForm(Request& req)
{
    if (!req.user.isAdmin()) return Response::redirect(req.user.loggedIn() ? "/?notice=forbidden" : "/login?notice=needlogin");

    int id = req.intParam("id", 0);

    if (id == 0)
        return productFormPage(req, 0, "", "", "", "0", 0, "");

    Row p = getProduct(id);
    if (p.empty())
        return adminMessagePage(req, "Not found", "That product does not exist.", "/admin/products", "Back to products");

    return productFormPage(req, id, col(p, "name"), col(p, "description"), col(p, "price"),
                           col(p, "stock"), atoi(col(p, "category_id").c_str()), "");
}

// POST: save the form. A form with an id edits that product, without an id it creates a new one.
Response adminProductSave(Request& req)
{
    if (!req.user.isAdmin()) return Response::redirect(req.user.loggedIn() ? "/?notice=forbidden" : "/login?notice=needlogin");

    int    id          = req.intParam("id", 0);
    string name        = trimmed(req.param("name"));
    string description = trimmed(req.param("description"));
    string price       = trimmed(req.param("price"));
    int    stock       = req.intParam("stock", 0);
    int    categoryId  = req.intParam("category_id", 0);

    string error;
    if (id > 0) error = updateProduct(id, name, description, atof(price.c_str()), stock, categoryId);
    else        error = createProduct(name, description, atof(price.c_str()), stock, categoryId);

    if (!error.empty())
        return productFormPage(req, id, name, description, price, to_string(stock), categoryId, error);

    return Response::redirect(id > 0 ? "/admin/products?notice=saved" : "/admin/products?notice=created");
}

// POST: set the stock of one product to an exact number.
Response adminProductStock(Request& req)
{
    if (!req.user.isAdmin()) return Response::redirect(req.user.loggedIn() ? "/?notice=forbidden" : "/login?notice=needlogin");

    string error = setProductStock(req.intParam("id", 0), req.intParam("stock", -1));

    if (!error.empty())
        return adminMessagePage(req, "Could not update stock", error, "/admin/products", "Back to products");

    string back = adminBackUrl(req, "/admin/products");
    back += (back.find('?') == string::npos) ? "?notice=stock" : "&notice=stock";
    return Response::redirect(back);
}

Response adminProductDelete(Request& req)
{
    if (!req.user.isAdmin()) return Response::redirect(req.user.loggedIn() ? "/?notice=forbidden" : "/login?notice=needlogin");

    string error = deleteProduct(req.intParam("id", 0));

    if (!error.empty())
        return adminMessagePage(req, "Could not delete product", error, "/admin/products", "Back to products");

    return Response::redirect("/admin/products?notice=deleted");
}

// ---- categories -----------------------------------------------------------------------

Response adminCategories(Request& req)
{
    if (!req.user.isAdmin()) return Response::redirect(req.user.loggedIn() ? "/?notice=forbidden" : "/login?notice=needlogin");

    string rows;
    Rows cats = categoriesWithProductCount();

    for (size_t i = 0; i < cats.size(); i++)
    {
        bool empty = (col(cats[i], "products") == "0");

        Vars row;
        row["id"]       = col(cats[i], "category_id");
        row["name"]     = col(cats[i], "category_name");
        row["icon"]     = categoryIcon(col(cats[i], "category_name"));
        row["products"] = col(cats[i], "products");
        row["deleteButton"] = empty ? renderTemplate("partials/delete_category_form.html", row)
                                    : "<span class=\"muted\">has products</span>";
        rows += renderTemplate("partials/admin_category_row.html", row);
    }

    Vars v;
    v["rows"] = rows;
    return renderAdminPage(req, "Categories", "admin/categories.html", "categories", v);
}

// POST: with an id it renames that category, without an id it adds a new one.
Response adminCategorySave(Request& req)
{
    if (!req.user.isAdmin()) return Response::redirect(req.user.loggedIn() ? "/?notice=forbidden" : "/login?notice=needlogin");

    int id = req.intParam("id", 0);
    string name = trimmed(req.param("name"));

    string error = (id > 0) ? renameCategory(id, name) : createCategory(name);

    if (!error.empty())
        return adminMessagePage(req, "Could not save category", error, "/admin/categories", "Back to categories");

    return Response::redirect(id > 0 ? "/admin/categories?notice=saved" : "/admin/categories?notice=created");
}

Response adminCategoryDelete(Request& req)
{
    if (!req.user.isAdmin()) return Response::redirect(req.user.loggedIn() ? "/?notice=forbidden" : "/login?notice=needlogin");

    string error = deleteCategory(req.intParam("id", 0));

    if (!error.empty())
        return adminMessagePage(req, "Could not delete category", error, "/admin/categories", "Back to categories");

    return Response::redirect("/admin/categories?notice=deleted");
}
