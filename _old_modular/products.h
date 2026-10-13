#pragma once
#include <map>
#include <string>
#include <mysql.h>

// Columns every product query must select, in this order:
//   product_id, name, description, price, stock, category_id, category_name
#define PRODUCT_COLUMNS \
    "p.product_id, p.name, p.description, p.price, p.stock, p.category_id, c.category_name"

// One product tile (image, name, price, stock note, add-to-cart).
std::string productCard(MYSQL_ROW row);

// Listing with filters: category, q (search), sort, min, max.
std::string buildProductsPage(const std::map<std::string, std::string>& params);

std::string buildProductDetailPage(int productId);
