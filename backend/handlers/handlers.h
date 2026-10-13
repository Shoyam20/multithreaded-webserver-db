#pragma once
#include "../core/http.h"
#include "../core/template.h"
#include "../db/database.h"
#include <string>

// Every page and action of the website, one function each. The URL for each one is chosen in app.cpp.
// A handler gets the Request and returns a Response. Handlers that need a signed-in user (or an admin)
// check that in their first line and redirect if the visitor is not allowed.

// ---- anyone: browsing (shop_handlers.cpp) ----
Response homePage(Request& req);
Response productsPage(Request& req);
Response productDetailPage(Request& req);

// ---- accounts (auth_handlers.cpp) ----
Response loginPage(Request& req);
Response loginSubmit(Request& req);
Response signupPage(Request& req);
Response signupSubmit(Request& req);
Response logout(Request& req);

// ---- signed-in customers: cart (cart_handlers.cpp) ----
Response cartPage(Request& req);
Response cartAdd(Request& req);
Response cartUpdate(Request& req);
Response cartRemove(Request& req);

// ---- signed-in customers: checkout, orders, sessions (order_handlers.cpp) ----
Response checkoutPage(Request& req);
Response placeOrderSubmit(Request& req);
Response myOrdersPage(Request& req);
Response cancelMyOrder(Request& req);
Response mySessionsPage(Request& req);

// ---- admin ----
Response adminDashboard(Request& req);          // admin_dashboard.cpp

Response adminProducts(Request& req);           // admin_catalog.cpp
Response adminProductForm(Request& req);        // the add / edit form
Response adminProductSave(Request& req);
Response adminProductStock(Request& req);
Response adminProductDelete(Request& req);
Response adminCategories(Request& req);
Response adminCategorySave(Request& req);
Response adminCategoryDelete(Request& req);

Response adminOrders(Request& req);             // admin_orders.cpp
Response adminOrderDetail(Request& req);
Response adminOrderStatus(Request& req);

Response adminUsers(Request& req);              // admin_users.cpp

// ============================================================
//  Helpers shared by the handlers (view.cpp)
// ============================================================
// They build pages and the small repeated pieces of HTML.

// ---- whole pages ----
// Storefront page: the template <tpl> goes inside the shared layout (header, footer, message bar).
Response renderPage(Request& req, const std::string& title, const std::string& tpl, Vars vars = Vars());

// Admin page: same idea with the admin sidebar layout. 'section' highlights the sidebar link.
Response renderAdminPage(Request& req, const std::string& title, const std::string& tpl,
                         const std::string& section, Vars vars = Vars());

// A page with a heading, a text and one button - used for errors and "nothing here" pages.
Response messagePage(Request& req, const std::string& title, const std::string& text,
                     const std::string& linkUrl, const std::string& linkText, int status = 200);
Response adminMessagePage(Request& req, const std::string& title, const std::string& text,
                          const std::string& linkUrl, const std::string& linkText);
Response notFoundPage(Request& req);

// ---- small helpers ----
std::string formatMoney(const std::string& decimalText);   // "3499.00" -> "&#8377;3,499"
std::string categoryIcon(const std::string& categoryName); // an emoji for each category
std::string categoryTint(int categoryId);                  // a pastel CSS gradient for product tiles
std::string statusClass(const std::string& status);        // "green", "amber" or "red" for status badges
std::string stockLabel(int stock);                         // "In stock" / "Only 3 left" / "Out of stock"
std::string alertBox(const std::string& text);             // a red message box, or "" when text is empty
std::string optionTag(const std::string& value, const std::string& label, bool selected);

// ---- repeated pieces ----
std::string productCard(const Row& product, bool loggedIn);   // one product tile
std::string productSection(const std::string& title, const std::string& moreUrl,
                           const Rows& products, bool loggedIn);   // heading + grid of cards ("" if no products)

// Where to go back to after an admin action. Only paths inside /admin are accepted.
std::string adminBackUrl(const Request& req, const std::string& fallback);
