#pragma once
#include "../db/database.h"
#include <string>

// Order status:   PLACED -> SHIPPED -> DELIVERED      (or CANCELLED before delivery)
// Payment status follows the order: PENDING, then PAID when delivered, or CANCELLED.

struct PlaceOrderResult
{
    bool        ok = false;
    std::string error;
    long long   orderId = 0;
};

// Turns the user's active cart into an order, in ONE database transaction:
// check stock -> create order -> copy items -> reduce stock -> create payment
// -> close cart. If any step fails, nothing is saved.
PlaceOrderResult placeOrder(int userId);

struct OrderFilter
{
    int         userId = 0;        // 0 = all customers (admin)
    std::string status;            // "" = any
    int         limit = 0;
};

// order_id, total, status, created_at, customer, email, payment_method, payment_status
Rows listOrders(const OrderFilter& filter);
Row  getOrder(int orderId);                       // one row, empty if not found
Rows orderItems(int orderId);                     // product_id, name, quantity, price, category_id, category_name

// ---- admin ----
bool        isValidOrderStatus(const std::string& status);
std::string updateOrderStatus(int orderId, const std::string& newStatus);

// Customer-side cancel: allowed until the order is delivered. Restores stock and
// cancels the payment. Returns "" on success or a message for the customer.
std::string cancelOrderByCustomer(int userId, int orderId);
