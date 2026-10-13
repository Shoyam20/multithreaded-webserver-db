#pragma once
#include "../db/database.h"
#include <string>

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
