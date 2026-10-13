#pragma once
#include <string>

// Places the order in one transaction; redirects to /orders on success.
std::string checkoutCart(int userId);
std::string buildCheckoutPage(int userId);   // review + payment choice
std::string buildOrdersPage(int userId);
