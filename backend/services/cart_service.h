#pragma once
#include "../db/database.h"
#include <string>

// A user has at most one ACTIVE cart. Checkout (order_service.cpp) marks it CHECKED_OUT.

// Id of the user's ACTIVE cart (0 if none). The caller must already hold dbMutex - used by checkout.
int  findActiveCart(int userId, bool createIfMissing);

int  cartItemCount(int userId);                // total units in the cart (the number in the header)

// The cart lines: product_id, name, price, quantity, subtotal, stock, category_id, category_name
Rows cartItems(int userId);

// units and total (as text, e.g. "6998.00") of the whole cart
Row  cartTotals(int userId);

// These return "" when everything went well, otherwise a message for the shopper.
std::string addToCart(int userId, int productId, int quantity);
std::string setCartQuantity(int userId, int productId, int quantity);   // 0 removes the line
std::string removeFromCart(int userId, int productId);
