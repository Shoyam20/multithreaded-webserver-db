#include "order_service.h"
#include "cart_service.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace std;

// Money is added up in paise (integers) so rounding errors cannot creep in.
static long long toPaise(const string& decimalText)
{
    return llround(atof(decimalText.c_str()) * 100.0);
}

static string paiseToText(long long paise)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%lld.%02lld", paise / 100, paise % 100);
    return buf;
}

// ------------------------------------------------------------
//  Checkout
// ------------------------------------------------------------
PlaceOrderResult placeOrder(int userId)
{
    PlaceOrderResult out;

    Transaction tx;      // rolls back automatically unless we reach commit()

    int cartId = activeCartId(userId, false);
    if (cartId == 0) { out.error = "You have no active cart."; return out; }

    // FOR UPDATE locks these product rows so two shoppers cannot buy the last item together.
    Rows items = db.rows(
        "SELECT ci.product_id, ci.quantity, p.price, p.stock "
        "FROM cart_items ci JOIN products p ON ci.product_id=p.product_id "
        "WHERE ci.cart_id=" + to_string(cartId) + " FOR UPDATE");

    if (items.empty()) { out.error = "Your cart is empty."; return out; }

    // 1. Check stock and add up the total.
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

    // 2. Create the order.
    Result order = db.run("INSERT INTO orders (user_id, cart_id, total, status) VALUES (" +
                          to_string(userId) + "," + to_string(cartId) + "," + total + ",'PLACED')");
    if (!order.ok) { out.error = order.error; return out; }

    out.orderId = order.insertId;

    // 3. Copy each cart line into order_items and reduce the stock.
    for (const Row& item : items)
    {
        string productId = col(item, "product_id");
        string qty = col(item, "quantity");

        Result line = db.run("INSERT INTO order_items (order_id, product_id, quantity, price) VALUES (" +
                             to_string(out.orderId) + "," + productId + "," + qty + "," +
                             col(item, "price") + ")");
        if (!line.ok) { out.error = line.error; return out; }

        Result stock = db.run("UPDATE products SET stock=stock-" + qty +
                              " WHERE product_id=" + productId + " AND stock>=" + qty);
        if (!stock.ok || stock.affectedRows != 1) { out.error = "Stock update failed."; return out; }
    }

    // 4. Record the (cash on delivery) payment and close the cart.
    Result pay = db.run("INSERT INTO payments (order_id, amount, method, status) VALUES (" +
                        to_string(out.orderId) + "," + total + ",'COD','PENDING')");
    if (!pay.ok) { out.error = pay.error; return out; }

    Result cart = db.run("UPDATE cart SET status='CHECKED_OUT' WHERE cart_id=" + to_string(cartId));
    if (!cart.ok) { out.error = cart.error; return out; }

    tx.commit();
    out.ok = true;
    return out;
}

// ------------------------------------------------------------
//  Reading orders
// ------------------------------------------------------------
Rows listOrders(const OrderFilter& f)
{
    string where = "WHERE 1=1";

    if (f.userId > 0)        where += " AND o.user_id=" + to_string(f.userId);
    if (!f.status.empty())   where += " AND o.status=" + db.quote(f.status);

    string sql =
        "SELECT o.order_id, o.total, o.status, o.created_at, u.name AS customer, u.email, "
        "COALESCE(p.method,'-') AS payment_method, COALESCE(p.status,'-') AS payment_status "
        "FROM orders o JOIN users u ON u.user_id=o.user_id "
        "LEFT JOIN payments p ON p.order_id=o.order_id " + where +
        " ORDER BY o.created_at DESC, o.order_id DESC";

    if (f.limit > 0) sql += " LIMIT " + to_string(f.limit);

    return db.rows(sql);
}

Row getOrder(int orderId)
{
    Rows r = db.rows(
        "SELECT o.order_id, o.total, o.status, o.created_at, o.user_id, u.name AS customer, u.email, "
        "COALESCE(p.method,'-') AS payment_method, COALESCE(p.status,'-') AS payment_status "
        "FROM orders o JOIN users u ON u.user_id=o.user_id "
        "LEFT JOIN payments p ON p.order_id=o.order_id WHERE o.order_id=" + to_string(orderId));

    return r.empty() ? Row() : r[0];
}

Rows orderItems(int orderId)
{
    return db.rows(
        "SELECT oi.product_id, p.name, oi.quantity, oi.price, (oi.quantity*oi.price) AS subtotal, "
        "p.category_id, c.category_name "
        "FROM order_items oi JOIN products p ON oi.product_id=p.product_id "
        "JOIN categories c ON p.category_id=c.category_id "
        "WHERE oi.order_id=" + to_string(orderId));
}

// ------------------------------------------------------------
//  Admin: change status
// ------------------------------------------------------------
bool isValidOrderStatus(const string& s)
{
    return s == "PLACED" || s == "SHIPPED" || s == "DELIVERED" || s == "CANCELLED";
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

    // A cancelled order has already given its stock back; reopening it would
    // make the stock numbers wrong, so it is final.
    if (oldStatus == "DELIVERED" && newStatus == "CANCELLED")
        return "A delivered order cannot be cancelled.";

    if (oldStatus == "CANCELLED")
        return "A cancelled order cannot be changed.";

    string id = to_string(orderId);

    if (newStatus == "CANCELLED")
    {
        // Put the items back on the shelf.
        Result restock = db.run(
            "UPDATE products p JOIN order_items oi ON oi.product_id=p.product_id "
            "SET p.stock=p.stock+oi.quantity WHERE oi.order_id=" + id);
        if (!restock.ok) return "Database error: " + restock.error;

        // The payment follows the order: cancelled order -> cancelled payment.
        db.run("UPDATE payments SET status='CANCELLED' WHERE order_id=" + id);
    }
    else if (newStatus == "DELIVERED")
    {
        // Cash on delivery is collected when the parcel is handed over.
        db.run("UPDATE payments SET status='PAID', paid_at=CURRENT_TIMESTAMP "
               "WHERE order_id=" + id + " AND method='COD' AND status='PENDING'");
    }

    Result r = db.run("UPDATE orders SET status=" + db.quote(newStatus) + " WHERE order_id=" + id);
    if (!r.ok) return "Database error: " + r.error;

    tx.commit();
    return "";
}

// A customer may cancel their own order until it has been delivered.
// The cancel itself is the same rolled-back-to-stock logic the admin uses.
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

    // updateOrderStatus checks the status again inside its own transaction.
    return updateOrderStatus(orderId, "CANCELLED");
}
