#pragma once
#include <string>

// Caller must already hold dbMutex.
int getActiveCartIdLocked(int userId, bool createIfMissing);

// Total quantity in the user's active cart (header badge). Locks dbMutex.
int getCartItemCount(int userId);

std::string buildCartPage(int userId);

// Mutations redirect (tlRedirect) on success and return an error page otherwise.
std::string addCartItem(int userId, int productId, int quantity);
std::string setCartItemQuantity(int userId, int productId, int quantity);
std::string removeCartItem(int userId, int productId);
