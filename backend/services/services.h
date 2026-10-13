#pragma once
#include "../core/http.h"      // the User struct
#include "../db/database.h"
#include <string>

// All the "services": the functions that hold the shop's rules and ALL the SQL.
// Handlers call these; nothing else talks to the database.
// Each service function takes dbMutex first (one shared database connection).

// ============================================================
//  products and categories
// ============================================================
// Products and categories: everything that reads or changes the products and categories tables.
//
// A product row has the columns:
//   product_id, name, description, price, stock, category_id, category_name

// ---- reading (storefront) ----
Rows listCategories();                          // category_id, category_name
Row  getProduct(int productId);                 // empty Row if there is no such product

// The product list page. Use 0 / "" / -1 for "no filter".
//   sort: "price_asc", "price_desc", "newest", or "" (by id)
Rows findProducts(int categoryId, const std::string& search, const std::string& sort, int minPrice, int maxPrice);

Rows cheapestProducts(int maxPrice, int limit); // in stock, cheapest first (home page)
Rows newestProducts(int limit);                 // in stock, newest first (home page)
Rows bestSellers(int limit);                    // most units sold in orders that were not cancelled
Rows relatedProducts(int categoryId, int excludeProductId, int limit);

// ---- admin: products ----
Rows adminProductList(const std::string& search, int categoryId, bool lowStockOnly);

// These return "" when everything went well, otherwise a message to show the admin.
std::string createProduct(const std::string& name, const std::string& description, double price, int stock, int categoryId);
std::string updateProduct(int productId, const std::string& name, const std::string& description, double price, int stock, int categoryId);
std::string setProductStock(int productId, int stock);
std::string deleteProduct(int productId);

// ---- admin: categories ----
Rows categoriesWithProductCount();              // category_id, category_name, products
std::string createCategory(const std::string& name);
std::string renameCategory(int categoryId, const std::string& name);
std::string deleteCategory(int categoryId);

// ============================================================
//  cart
// ============================================================
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

// ============================================================
//  orders and checkout
// ============================================================
// Order status:   PLACED -> SHIPPED -> DELIVERED      (or CANCELLED, but only before it is delivered)
// Payment status follows the order: PENDING, then PAID when delivered, or CANCELLED when cancelled.

// Turns the user's cart into an order in ONE database transaction:
//   check stock -> create order -> copy items -> reduce stock -> create payment -> close cart.
// If any step fails everything is rolled back. Returns "" on success, otherwise a message.
std::string placeOrder(int userId);

// Orders, newest first. userId 0 = every customer (admin); status "" = any status; limit 0 = no limit.
// Columns: order_id, total, status, created_at, customer, email, payment_method, payment_status
Rows listOrders(int userId, const std::string& status, int limit);

Row  getOrder(int orderId);          // like listOrders plus user_id; empty Row if not found
Rows orderItems(int orderId);        // product_id, name, quantity, price, subtotal, category_id, category_name

bool isValidOrderStatus(const std::string& status);

// Admin: change the status. CANCELLED puts the items back in stock and cancels the payment,
// DELIVERED marks the cash payment PAID. Returns "" or a message.
std::string updateOrderStatus(int orderId, const std::string& newStatus);

// Customer: cancel your own order until it is delivered (same rollback as above).
std::string cancelOrderByCustomer(int userId, int orderId);

// ============================================================
//  users, login sessions, admin
// ============================================================
// ---- accounts ----
// Both return "" on success, otherwise a message to show on the form.
std::string signUp(const std::string& name, const std::string& email, const std::string& password);
std::string logIn(const std::string& email, const std::string& password, User& user);

// ---- sessions (one row in user_sessions for every browser login) ----
std::string startSession(int userId);                       // returns the new session id
bool        userForSession(const std::string& sessionId, User& user);
void        endSession(const std::string& sessionId);
Rows        sessionsForUser(int userId);                    // the 20 newest

// ---- admin ----
Rows        listUsers();                                    // with the number of orders of each
bool        makeAdminByEmail(const std::string& email);     // used by: server.exe --make-admin <email>
int         countAdmins();

// ============================================================
//  admin dashboard
// ============================================================
// Numbers for the admin dashboard: users, products, orders, revenue,
// pending_orders (status PLACED) and low_stock (products at or below the warning level).
Row dashboardStats();
