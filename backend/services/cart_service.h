#pragma once
#include "../db/database.h"
#include <string>

// A user has at most one ACTIVE cart; checkout marks it CHECKED_OUT.

int  activeCartId(int userId, bool createIfMissing);   // 0 if none
int  cartItemCount(int userId);                        // total units, for the header badge

// Cart lines: product_id, name, price, quantity, subtotal, stock, category_id, category_name
Rows cartItems(int userId);

// Each returns "" on success, or a message for the shopper.
std::string addToCart(int userId, int productId, int quantity);
std::string setCartQuantity(int userId, int productId, int quantity);   // 0 removes the line
std::string removeFromCart(int userId, int productId);
