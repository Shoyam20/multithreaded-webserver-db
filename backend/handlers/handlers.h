#pragma once
#include "../core/http.h"

// Every page/action of the website. Each one takes the request and returns a
// response. See routes.cpp for the URL each one answers.

// ---- storefront: browsing (shop_handlers.cpp) ----
Response homePage(Request& req);
Response productsPage(Request& req);
Response productDetailPage(Request& req);

// ---- accounts (auth_handlers.cpp) ----
Response loginPage(Request& req);
Response loginSubmit(Request& req);
Response signupPage(Request& req);
Response signupSubmit(Request& req);
Response logout(Request& req);

// ---- shopping cart (cart_handlers.cpp) ----
Response cartPage(Request& req);
Response cartAdd(Request& req);
Response cartUpdate(Request& req);
Response cartRemove(Request& req);

// ---- checkout & orders (order_handlers.cpp) ----
Response checkoutPage(Request& req);
Response placeOrderSubmit(Request& req);
Response myOrdersPage(Request& req);
Response cancelMyOrder(Request& req);          // POST: cancel until delivered (rolls back stock + payment)
Response mySessionsPage(Request& req);

// ---- admin: overview & raw tables (admin_dashboard.cpp) ----
Response adminDashboard(Request& req);

// ---- admin: products, stock, categories (admin_catalog.cpp) ----
Response adminProducts(Request& req);
Response adminProductForm(Request& req);       // add or edit form
Response adminProductSave(Request& req);
Response adminProductStock(Request& req);
Response adminProductDelete(Request& req);
Response adminCategories(Request& req);
Response adminCategorySave(Request& req);
Response adminCategoryDelete(Request& req);

// ---- admin: orders (admin_orders.cpp) ----
Response adminOrders(Request& req);
Response adminOrderDetail(Request& req);
Response adminOrderStatus(Request& req);

// ---- admin: users & sessions (admin_users.cpp) ----
Response adminUsers(Request& req);

