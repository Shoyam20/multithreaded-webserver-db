#include "order_service.h"
#include "cart_service.h"
#include "../config.h"
#include <chrono>
#include <mutex>
#include <random>
#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace std;

// Convert the price into paise to avoid floating-point rounding errors.
static long long toPaise(const string& decimalText)
{
    return llround(atof(decimalText.c_str()) * 100.0);
}

// Convert paise back into a decimal price string.
static string paiseToText(long long paise)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%lld.%02lld", paise / 100, paise % 100);
    return buf;
}

// Generate a random delivery delay within the configured time range.
static int randomDeliverySeconds()
{
    static mt19937 gen((unsigned)chrono::steady_clock::now().time_since_epoch().count());
    static mutex genMutex;

    // Protect the random number generator during concurrent access.
    lock_guard<mutex> lock(genMutex);
    return uniform_int_distribution<int>(config::deliveryMinSeconds, config::deliveryMaxSeconds)(gen);
}

// ------------------------------------------------------------
// Checkout: create an order from the user's active cart.
// ------------------------------------------------------------
PlaceOrderResult placeOrder(int userId)
{
    PlaceOrderResult out;

    Transaction tx;  

    int cartId = activeCartId(userId, false);
    if (cartId == 0) { out.error = "You have no active cart."; return out; }

    Rows items = db.rows(
        "SELECT ci.product_id, ci.quantity, p.price, p.stock "
        "FROM cart_items ci JOIN products p ON ci.product_id=p.product_id "
        "WHERE ci.cart_id=" + to_string(cartId) + " FOR UPDATE");

    if (items.empty()) { out.error = "Your cart is empty."; return out; }

    long long totalPaise = 0;

    for (const Row& item : items)
    {
        int qty   = atoi(col(item, "quantity").c_str());
        int stock = atoi(col(item, "stock").c_str());

        if (qty <= 0 || qty > stock)
        {
            out.error = "Not enough stock for one or more items. Please review your cart.";
            return out;
        }

        totalPaise += qty * toPaise(col(item, "price"));
    }

    string total = paiseToText(totalPaise);

    // Create the order with PLACED status and a scheduled delivery time.
    Result order = db.run("INSERT INTO orders (user_id, cart_id, total, status, deliver_at) VALUES (" +
                          to_string(userId) + "," + to_string(cartId) + "," + total + ",'PLACED'," +
                          "DATE_ADD(NOW(), INTERVAL " + to_string(randomDeliverySeconds()) + " SECOND))");
    if (!order.ok) { out.error = order.error; return out; }

    out.orderId = order.insertId;

    // Save each ordered product and decrease its available stock.
    for (const Row& item : items)
    {
        string productId = col(item, "product_id");
        string qty = col(item, "quantity");

        Result line = db.run("INSERT INTO order_items (order_id, product_id, quantity, price) VALUES (" +
                             to_string(out.orderId) + "," + productId + "," + qty + "," +
                             col(item, "price") + ")");
        if (!line.ok) { out.error = line.error; return out; }

        // Reduce stock only if enough units are still available.
        Result stock = db.run("UPDATE products SET stock=stock-" + qty +
                              " WHERE product_id=" + productId + " AND stock>=" + qty);
        if (!stock.ok || stock.affectedRows != 1) { out.error = "Stock update failed."; return out; }
    }

    // Create a pending Cash on Delivery payment and mark the cart as checked out.
    Result pay = db.run("INSERT INTO payments (order_id, amount, method, status) VALUES (" +
                        to_string(out.orderId) + "," + total + ",'COD','PENDING')");
    if (!pay.ok) { out.error = pay.error; return out; }

    Result cart = db.run("UPDATE cart SET status='CHECKED_OUT' WHERE cart_id=" + to_string(cartId));
    if (!cart.ok) { out.error = cart.error; return out; }

    // Commit all order-related database changes together.
    tx.commit();
    out.ok = true;
    return out;
}

// Fetch orders with optional user and status filters.
Rows listOrders(const OrderFilter& f)
{
    string where = "WHERE 1=1";

    if (f.userId > 0)        where += " AND o.user_id=" + to_string(f.userId);
    if (!f.status.empty())   where += " AND o.status=" + db.quote(f.status);

    string sql =
        "SELECT o.order_id, o.total, o.status, o.created_at, o.deliver_at, "
        "GREATEST(TIMESTAMPDIFF(SECOND, NOW(), o.deliver_at), 0) AS seconds_left, u.name AS customer, u.email, "
        "COALESCE(p.method,'-') AS payment_method, COALESCE(p.status,'-') AS payment_status "
        "FROM orders o JOIN users u ON u.user_id=o.user_id "
        "LEFT JOIN payments p ON p.order_id=o.order_id " + where +
        " ORDER BY o.created_at DESC, o.order_id DESC";

    if (f.limit > 0) sql += " LIMIT " + to_string(f.limit);

    return db.rows(sql);
}

// Fetch details of one order using its order ID.
Row getOrder(int orderId)
{
    Rows r = db.rows(
        "SELECT o.order_id, o.total, o.status, o.created_at, o.user_id, o.deliver_at, "
        "GREATEST(TIMESTAMPDIFF(SECOND, NOW(), o.deliver_at), 0) AS seconds_left, u.name AS customer, u.email, "
        "COALESCE(p.method,'-') AS payment_method, COALESCE(p.status,'-') AS payment_status "
        "FROM orders o JOIN users u ON u.user_id=o.user_id "
        "LEFT JOIN payments p ON p.order_id=o.order_id WHERE o.order_id=" + to_string(orderId));

    return r.empty() ? Row() : r[0];
}

// Fetch all products belonging to a particular order.
Rows orderItems(int orderId)
{
    return db.rows(
        "SELECT oi.product_id, p.name, oi.quantity, oi.price, (oi.quantity*oi.price) AS subtotal, "
        "p.category_id, c.category_name "
        "FROM order_items oi JOIN products p ON oi.product_id=p.product_id "
        "JOIN categories c ON p.category_id=c.category_id "
        "WHERE oi.order_id=" + to_string(orderId));
}

// Admin: validate and update order or payment status.

// Check whether the order status is supported by the application.
bool isValidOrderStatus(const string& s)
{
    return s == "PLACED" || s == "SHIPPED" || s == "DELIVERED" || s == "CANCELLED";
}

// Check whether the payment status is valid.
bool isValidPaymentStatus(const string& s)
{
    return s == "PENDING" || s == "PAID" || s == "FAILED" || s == "REFUNDED";
}

string updateOrderStatus(int orderId, const string& newStatus)
{
    if (!isValidOrderStatus(newStatus))
        return "Unknown order status.";

    Transaction tx;

    Row order = getOrder(orderId);
    if (order.empty())
        return "Order not found.";

    string oldStatus = col(order, "status");

    if (oldStatus == newStatus)
        return "";

    if (oldStatus == "DELIVERED" && newStatus == "CANCELLED")
        return "A delivered order cannot be cancelled.";

    if (oldStatus == "CANCELLED")
        return "A cancelled order cannot be changed.";

    string id = to_string(orderId);

    if (newStatus == "CANCELLED")
    {
        Result restock = db.run(
            "UPDATE products p JOIN order_items oi ON oi.product_id=p.product_id "
            "SET p.stock=p.stock+oi.quantity WHERE oi.order_id=" + id);
        if (!restock.ok) return "Database error: " + restock.error;

        string payment = (col(order, "payment_status") == "PAID") ? "REFUNDED" : "CANCELLED";
        db.run("UPDATE payments SET status='" + payment + "' WHERE order_id=" + id);
    }
    else if (newStatus == "DELIVERED")
    {
        db.run("UPDATE payments SET status='PAID', paid_at=CURRENT_TIMESTAMP "
               "WHERE order_id=" + id + " AND method='COD' AND status='PENDING'");
    }

    // Save the new order status.
    Result r = db.run("UPDATE orders SET status=" + db.quote(newStatus) + " WHERE order_id=" + id);
    if (!r.ok) return "Database error: " + r.error;

    tx.commit();
    return "";
}

// Update the payment status for a specific order.
string updatePaymentStatus(int orderId, const string& newStatus)
{
    if (!isValidPaymentStatus(newStatus))
        return "Unknown payment status.";

    Result r = db.run("UPDATE payments SET status=" + db.quote(newStatus) +
                      ", paid_at=CURRENT_TIMESTAMP WHERE order_id=" + to_string(orderId));

    if (!r.ok) return "Database error: " + r.error;
    if (r.affectedRows == 0) return "Payment record not found.";

    return "";
}

// Demo auto-delivery: automatically complete due orders.

int deliverDueOrders()
{
    Transaction tx;

    db.run("UPDATE payments p JOIN orders o ON o.order_id=p.order_id "
           "SET p.status='PAID', p.paid_at=CURRENT_TIMESTAMP "
           "WHERE o.status IN ('PLACED','SHIPPED') AND o.deliver_at IS NOT NULL "
           "AND o.deliver_at<=NOW() AND p.method='COD' AND p.status='PENDING'");

    Result r = db.run("UPDATE orders SET status='DELIVERED' "
                      "WHERE status IN ('PLACED','SHIPPED') AND deliver_at IS NOT NULL "
                      "AND deliver_at<=NOW()");

    if (!r.ok)
        return 0; 

    tx.commit();
    return (int)r.affectedRows;
}

// Allow customers to cancel their own orders before delivery.
string cancelOrderByCustomer(int userId, int orderId)
{
    Row order = getOrder(orderId);

    if (order.empty() || atoi(col(order, "user_id").c_str()) != userId)
        return "That order was not found.";

    string status = col(order, "status");

    if (status == "CANCELLED")
        return "This order is already cancelled.";

    if (status == "DELIVERED")
        return "Too late - this order has already been delivered, so it can no longer be cancelled.";

    return updateOrderStatus(orderId, "CANCELLED");
}
