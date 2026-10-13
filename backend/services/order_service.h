#pragma once
#include "../db/database.h"
#include <string>

// Order status:   PLACED -> SHIPPED -> DELIVERED      (or CANCELLED at any time before delivery)
// Payment status: PENDING, PAID, FAILED, REFUNDED, CANCELLED

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
bool        isValidPaymentStatus(const std::string& status);
std::string updateOrderStatus(int orderId, const std::string& newStatus);
std::string updatePaymentStatus(int orderId, const std::string& newStatus);

// ---- demo delivery ----
// Marks orders whose delivery time (orders.deliver_at) has passed as DELIVERED.
// Returns how many orders were delivered. Called by a background thread.
int deliverDueOrders();

// Customer-side cancel: allowed until the order is delivered. Restores stock and
// cancels the payment. Returns "" on success or a message for the customer.
std::string cancelOrderByCustomer(int userId, int orderId);
