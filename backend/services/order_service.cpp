#include "services.h"
#include "services.h"
#include <cstdlib>

using namespace std;

// Undoes the open transaction and returns the error message (so errors can be written as one line).
static string rollbackWith(const string& message)
{
    dbRun("ROLLBACK");
    return message;
}

// ---- checkout ---------------------------------------------------------------------------

string placeOrder(int userId)
{
    lock_guard<mutex> lock(dbMutex);

    int cartId = findActiveCart(userId, false);
    if (cartId == 0)
        return "You have no active cart.";

    string cart = to_string(cartId);

    dbRun("START TRANSACTION");

    // FOR UPDATE locks these product rows in MySQL until COMMIT / ROLLBACK.
    Rows items = dbRows("SELECT ci.product_id, ci.quantity, p.price, p.stock "
                        "FROM cart_items ci JOIN products p ON ci.product_id=p.product_id "
                        "WHERE ci.cart_id=" + cart + " FOR UPDATE");

    if (items.empty())
        return rollbackWith("Your cart is empty.");

    // 1. Is there enough stock for every item?
    for (size_t i = 0; i < items.size(); i++)
    {
        int quantity = atoi(col(items[i], "quantity").c_str());
        int stock    = atoi(col(items[i], "stock").c_str());

        if (quantity <= 0 || quantity > stock)
            return rollbackWith("Not enough stock for one or more items. Please review your cart.");
    }

    // 2. The order total, added up by MySQL (exact decimal arithmetic).
    Rows sum = dbRows("SELECT SUM(ci.quantity*p.price) AS total "
                      "FROM cart_items ci JOIN products p ON ci.product_id=p.product_id "
                      "WHERE ci.cart_id=" + cart);
    string total = sum.empty() ? "0" : col(sum[0], "total");

    // 3. Create the order.
    Result order = dbRun("INSERT INTO orders (user_id, cart_id, total, status) VALUES (" +
                         to_string(userId) + "," + cart + "," + total + ",'PLACED')");
    if (!order.ok)
        return rollbackWith(order.error);

    string orderId = to_string(order.insertId);

    // 4. For each item: copy it into order_items (with today's price) and reduce the stock.
    for (size_t i = 0; i < items.size(); i++)
    {
        string productId = col(items[i], "product_id");
        string quantity  = col(items[i], "quantity");

        Result line = dbRun("INSERT INTO order_items (order_id, product_id, quantity, price) VALUES (" +
                            orderId + "," + productId + "," + quantity + "," + col(items[i], "price") + ")");
        if (!line.ok)
            return rollbackWith(line.error);

        // "AND stock>=quantity" means the stock can never go below zero, even if something else changed it.
        Result stock = dbRun("UPDATE products SET stock=stock-" + quantity +
                             " WHERE product_id=" + productId + " AND stock>=" + quantity);
        if (!stock.ok || stock.affectedRows != 1)
            return rollbackWith("Stock update failed.");
    }

    // 5. Record the cash-on-delivery payment, then close the cart.
    Result payment = dbRun("INSERT INTO payments (order_id, amount, method, status) VALUES (" +
                           orderId + "," + total + ",'COD','PENDING')");
    if (!payment.ok)
        return rollbackWith(payment.error);

    Result closeCart = dbRun("UPDATE cart SET status='CHECKED_OUT' WHERE cart_id=" + cart);
    if (!closeCart.ok)
        return rollbackWith(closeCart.error);

    dbRun("COMMIT");
    return "";
}

// ---- reading orders ----------------------------------------------------------------------

Rows listOrders(int userId, const string& status, int limit)
{
    lock_guard<mutex> lock(dbMutex);

    string where = "WHERE 1=1";
    if (userId > 0)        where += " AND o.user_id=" + to_string(userId);
    if (!status.empty())   where += " AND o.status=" + dbQuote(status);

    string sql = "SELECT o.order_id, o.total, o.status, o.created_at, u.name AS customer, u.email, "
                 "COALESCE(p.method,'-') AS payment_method, COALESCE(p.status,'-') AS payment_status "
                 "FROM orders o JOIN users u ON u.user_id=o.user_id "
                 "LEFT JOIN payments p ON p.order_id=o.order_id " + where +
                 " ORDER BY o.created_at DESC, o.order_id DESC";

    if (limit > 0) sql += " LIMIT " + to_string(limit);

    return dbRows(sql);
}

// One order (caller must hold dbMutex).
static Row findOrder(int orderId)
{
    Rows rows = dbRows("SELECT o.order_id, o.total, o.status, o.created_at, o.user_id, u.name AS customer, u.email, "
                       "COALESCE(p.method,'-') AS payment_method, COALESCE(p.status,'-') AS payment_status "
                       "FROM orders o JOIN users u ON u.user_id=o.user_id "
                       "LEFT JOIN payments p ON p.order_id=o.order_id WHERE o.order_id=" + to_string(orderId));

    return rows.empty() ? Row() : rows[0];
}

Row getOrder(int orderId)
{
    lock_guard<mutex> lock(dbMutex);
    return findOrder(orderId);
}

Rows orderItems(int orderId)
{
    lock_guard<mutex> lock(dbMutex);

    return dbRows("SELECT oi.product_id, p.name, oi.quantity, oi.price, (oi.quantity*oi.price) AS subtotal, "
                  "p.category_id, c.category_name "
                  "FROM order_items oi JOIN products p ON oi.product_id=p.product_id "
                  "JOIN categories c ON p.category_id=c.category_id "
                  "WHERE oi.order_id=" + to_string(orderId));
}

// ---- changing the status ------------------------------------------------------------------

bool isValidOrderStatus(const string& status)
{
    return status == "PLACED" || status == "SHIPPED" || status == "DELIVERED" || status == "CANCELLED";
}

// The rules for changing an order's status (caller must hold dbMutex).
static string changeStatus(int orderId, const string& newStatus)
{
    if (!isValidOrderStatus(newStatus))
        return "Unknown order status.";

    Row order = findOrder(orderId);
    if (order.empty())
        return "Order not found.";

    string oldStatus = col(order, "status");
    string id = to_string(orderId);

    if (oldStatus == newStatus)
        return "";

    if (oldStatus == "DELIVERED" && newStatus == "CANCELLED")
        return "A delivered order cannot be cancelled.";

    // A cancelled order has already given its stock back; reopening it would make the stock wrong.
    if (oldStatus == "CANCELLED")
        return "A cancelled order cannot be changed.";

    dbRun("START TRANSACTION");

    if (newStatus == "CANCELLED")
    {
        // Put the items back on the shelf ...
        Result restock = dbRun("UPDATE products p JOIN order_items oi ON oi.product_id=p.product_id "
                               "SET p.stock=p.stock+oi.quantity WHERE oi.order_id=" + id);
        if (!restock.ok)
            return rollbackWith("Database error: " + restock.error);

        // ... and cancel the payment (the payment follows the order).
        dbRun("UPDATE payments SET status='CANCELLED' WHERE order_id=" + id);
    }
    else if (newStatus == "DELIVERED")
    {
        // Cash on delivery is collected when the parcel is handed over.
        dbRun("UPDATE payments SET status='PAID', paid_at=CURRENT_TIMESTAMP "
              "WHERE order_id=" + id + " AND method='COD' AND status='PENDING'");
    }

    Result update = dbRun("UPDATE orders SET status=" + dbQuote(newStatus) + " WHERE order_id=" + id);
    if (!update.ok)
        return rollbackWith("Database error: " + update.error);

    dbRun("COMMIT");
    return "";
}

string updateOrderStatus(int orderId, const string& newStatus)
{
    lock_guard<mutex> lock(dbMutex);
    return changeStatus(orderId, newStatus);
}

string cancelOrderByCustomer(int userId, int orderId)
{
    lock_guard<mutex> lock(dbMutex);

    Row order = findOrder(orderId);

    // A customer may only cancel their own orders.
    if (order.empty() || atoi(col(order, "user_id").c_str()) != userId)
        return "That order was not found.";

    if (col(order, "status") == "DELIVERED")
        return "Too late - this order has already been delivered, so it can no longer be cancelled.";

    if (col(order, "status") == "CANCELLED")
        return "This order is already cancelled.";

    return changeStatus(orderId, "CANCELLED");
}
