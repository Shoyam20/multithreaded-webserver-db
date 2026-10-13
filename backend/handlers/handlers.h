#pragma once
#include "../core/http.h"

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
