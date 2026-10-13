#pragma once
#include "../core/http.h"
#include "../core/template.h"
#include "../db/database.h"
#include <string>

// ------------------------------------------------------------
//  Helpers shared by all handlers: turn data into pages.
// ------------------------------------------------------------

// Full storefront page (header + footer around frontend/templates/<tpl>).
Response renderPage(Request& req, const std::string& title, const std::string& tpl,
                    const Data& page = Data());

// Admin page (sidebar layout). 'section' highlights the sidebar item.
Response renderAdminPage(Request& req, const std::string& title, const std::string& tpl,
                         const std::string& section, const Data& page = Data());

// Simple "something happened" pages.
Response messagePage(Request& req, const std::string& title, const std::string& text,
                     const std::string& linkUrl = "/", const std::string& linkText = "Go home",
                     int status = 200);
Response adminMessagePage(Request& req, const std::string& title, const std::string& text,
                          const std::string& linkUrl, const std::string& linkText);
Response notFoundPage(Request& req);

// "₹1,23,456" formatting
std::string money(double amount);
std::string money(const std::string& decimalText);

// Product row -> template values (name, price, icon, stock flags ...)
Data productData(const Row& product);
void addProducts(Data& page, const std::string& listName, const Rows& products);

std::string categoryIcon(const std::string& categoryName);
std::string categoryTint(int categoryId);

// Where to go back to after an admin action (only paths inside /admin are allowed).
std::string adminBackUrl(const Request& req, const std::string& fallback);

// CSS class (green / amber / red) for an order or payment status badge.
std::string statusClass(const std::string& status);
