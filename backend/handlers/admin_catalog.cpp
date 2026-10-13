#include "handlers.h"
#include "view.h"
#include "../services/category_service.h"
#include "../services/product_service.h"
#include <cstdio>
#include <cstdlib>

using namespace std;

static string trimmed(const string& s)
{
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == string::npos) return "";
    return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}

// ============================================================
//  PRODUCTS
// ============================================================
Response adminProducts(Request& req)
{
    ProductFilter filter;
    filter.search = trimmed(req.param("q"));
    filter.categoryId = req.intParam("category", 0);
    filter.sort = req.param("sort", "");
    bool lowOnly = !req.param("low").empty();

    if (lowOnly)
    {
        filter.maxStock = 5;
        filter.sort = "stock_asc";
    }

    Rows products = findProducts(filter);

    Data page;
    page.set("search", filter.search);
    page.set("count", (int)products.size());
    page.flag("lowOnly", lowOnly);
    addProducts(page, "products", products);

    for (const Category& c : categories())
    {
        Data item;
        item.set("id", c.id).set("name", c.name);
        item.flag("selected", c.id == filter.categoryId);
        page.add("categoryOptions", item);
    }

    // After changing stock the admin returns to this same filtered list.
    string back = "/admin/products";
    if (lowOnly) back += "?low=1";
    page.set("backUrl", back);

    return renderAdminPage(req, "Products", "admin/products.html", "products", page);
}

// Show the add/edit form (also used to show errors after a failed save).
static Response productFormPage(Request& req, int productId, const ProductInput& in,
                                const string& error)
{
    Data page;
    page.set("formTitle", productId > 0 ? "Edit product" : "Add product");
    page.set("productId", productId > 0 ? to_string(productId) : "");
    page.set("name", in.name).set("description", in.description);
    page.set("stock", in.stock);
    page.set("error", error);

    // Show the price with at most 2 decimals, e.g. 499 or 499.5
    char buf[32];
    snprintf(buf, sizeof(buf), "%g", in.price);
    page.set("price", in.price > 0 || productId > 0 ? buf : "");

    for (const Category& c : categories())
    {
        Data item;
        item.set("id", c.id).set("name", c.name);
        item.flag("selected", c.id == in.categoryId);
        page.add("categoryOptions", item);
    }

    return renderAdminPage(req, page.values["formTitle"], "admin/product_form.html", "products", page);
}

Response adminProductForm(Request& req)
{
    int id = req.intParam("id", 0);
    ProductInput in;

    if (id > 0)
    {
        Row p = getProduct(id);

        if (p.empty())
            return adminMessagePage(req, "Not found", "That product does not exist.",
                                    "/admin/products", "Back to products");

        in.name = col(p, "name");
        in.description = col(p, "description");
        in.price = atof(col(p, "price").c_str());
        in.stock = atoi(col(p, "stock").c_str());
        in.categoryId = atoi(col(p, "category_id").c_str());
    }

    return productFormPage(req, id, in, "");
}

Response adminProductSave(Request& req)
{
    int id = req.intParam("id", 0);          // 0 = new product

    ProductInput in;
    in.name = trimmed(req.param("name"));
    in.description = trimmed(req.param("description"));
    in.price = atof(req.param("price").c_str());
    in.stock = req.intParam("stock", 0);
    in.categoryId = req.intParam("category_id", 0);

    int newId = 0;
    string error = (id > 0) ? updateProduct(id, in) : createProduct(in, newId);

    if (!error.empty())
        return productFormPage(req, id, in, error);

    return Response::redirect(id > 0 ? "/admin/products?notice=saved"
                                     : "/admin/products?notice=created");
}

// Set the stock of one product to an exact number.
Response adminProductStock(Request& req)
{
    string error = setProductStock(req.intParam("id", 0), req.intParam("stock", -1));

    if (!error.empty())
        return adminMessagePage(req, "Could not update stock", error,
                                "/admin/products", "Back to products");

    string back = adminBackUrl(req, "/admin/products");
    back += (back.find('?') == string::npos ? "?" : "&");
    return Response::redirect(back + "notice=stock");
}

Response adminProductDelete(Request& req)
{
    string error = deleteProduct(req.intParam("id", 0));

    if (!error.empty())
        return adminMessagePage(req, "Could not delete product", error,
                                "/admin/products", "Back to products");

    return Response::redirect("/admin/products?notice=deleted");
}

// ============================================================
//  CATEGORIES
// ============================================================
Response adminCategories(Request& req)
{
    Data page;

    for (const Row& c : categoriesWithProductCount())
    {
        Data item;
        item.set("id", col(c, "category_id")).set("name", col(c, "category_name"));
        item.set("products", col(c, "products"));
        item.set("icon", categoryIcon(col(c, "category_name")));
        item.flag("canDelete", col(c, "products") == "0");
        page.add("categories", item);
    }

    return renderAdminPage(req, "Categories", "admin/categories.html", "categories", page);
}

// One handler for "add" (no id) and "rename" (with id).
Response adminCategorySave(Request& req)
{
    int id = req.intParam("id", 0);
    string name = trimmed(req.param("name"));

    string error = (id > 0) ? renameCategory(id, name) : createCategory(name);

    if (!error.empty())
        return adminMessagePage(req, "Could not save category", error,
                                "/admin/categories", "Back to categories");

    return Response::redirect(id > 0 ? "/admin/categories?notice=saved"
                                     : "/admin/categories?notice=created");
}

Response adminCategoryDelete(Request& req)
{
    string error = deleteCategory(req.intParam("id", 0));

    if (!error.empty())
        return adminMessagePage(req, "Could not delete category", error,
                                "/admin/categories", "Back to categories");

    return Response::redirect("/admin/categories?notice=deleted");
}
