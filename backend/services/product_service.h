#pragma once
#include "../db/database.h"
#include <string>

// Products and categories: everything that reads or changes the products and categories tables.
//
// A product row has the columns:
//   product_id, name, description, price, stock, category_id, category_name

// ---- reading (storefront) ----
Rows listCategories();                          // category_id, category_name
Row  getProduct(int productId);                 // empty Row if there is no such product

// The product list page. Use 0 / "" / -1 for "no filter".
//   sort: "price_asc", "price_desc", "newest", or "" (by id)
Rows findProducts(int categoryId, const std::string& search, const std::string& sort, int minPrice, int maxPrice);

Rows cheapestProducts(int maxPrice, int limit); // in stock, cheapest first (home page)
Rows newestProducts(int limit);                 // in stock, newest first (home page)
Rows bestSellers(int limit);                    // most units sold in orders that were not cancelled
Rows relatedProducts(int categoryId, int excludeProductId, int limit);

// ---- admin: products ----
Rows adminProductList(const std::string& search, int categoryId, bool lowStockOnly);

// These return "" when everything went well, otherwise a message to show the admin.
std::string createProduct(const std::string& name, const std::string& description, double price, int stock, int categoryId);
std::string updateProduct(int productId, const std::string& name, const std::string& description, double price, int stock, int categoryId);
std::string setProductStock(int productId, int stock);
std::string deleteProduct(int productId);

// ---- admin: categories ----
Rows categoriesWithProductCount();              // category_id, category_name, products
std::string createCategory(const std::string& name);
std::string renameCategory(int categoryId, const std::string& name);
std::string deleteCategory(int categoryId);
