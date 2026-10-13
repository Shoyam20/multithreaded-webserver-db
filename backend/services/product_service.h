#pragma once
#include "../db/database.h"
#include <string>

// Every product row has these columns:
//   product_id, name, description, price, stock, category_id, category_name

struct ProductFilter
{
    int         categoryId = 0;       // 0 = any
    std::string search;               // matches name, description or category
    std::string sort;                 // price_asc | price_desc | newest | name | (default: by id)
    int         minPrice = -1;        // -1 = no limit
    int         maxPrice = -1;
    int         maxStock = -1;        // admin: "stock at or below N"
    bool        inStockOnly = false;
    int         limit = 0;            // 0 = no limit
};

Rows findProducts(const ProductFilter& filter);
Row  getProduct(int productId);                       // empty Row if not found
Rows relatedProducts(int categoryId, int excludeProductId, int limit);
Rows bestSellers(int limit);                          // by units sold in non-cancelled orders

// ---- admin: create / edit / stock / delete ----
struct ProductInput
{
    std::string name;
    std::string description;
    double      price = 0;
    int         stock = 0;
    int         categoryId = 0;
};

// Each returns "" on success, or a message to show the admin.
std::string createProduct(const ProductInput& in, int& newId);
std::string updateProduct(int productId, const ProductInput& in);
std::string setProductStock(int productId, int stock);
std::string deleteProduct(int productId);
