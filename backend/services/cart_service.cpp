#include "services.h"
#include <cstdlib>

using namespace std;

// ---- small helpers: the caller must already hold dbMutex ---------------------------

// Id of the user's ACTIVE cart; 0 if there is none (and createIfMissing is false). Caller must hold dbMutex.
int findActiveCart(int userId, bool createIfMissing)
{
    Rows found = dbRows("SELECT cart_id FROM cart WHERE user_id=" + to_string(userId) +
                        " AND status='ACTIVE' ORDER BY cart_id DESC LIMIT 1");

    if (!found.empty())
        return atoi(col(found[0], "cart_id").c_str());

    if (!createIfMissing)
        return 0;

    Result r = dbRun("INSERT INTO cart (user_id, status) VALUES (" + to_string(userId) + ",'ACTIVE')");
    return r.ok ? (int)r.insertId : 0;
}

// Stock of a product, or -1 if the product does not exist.
static int stockOf(int productId)
{
    Rows r = dbRows("SELECT stock FROM products WHERE product_id=" + to_string(productId));
    return r.empty() ? -1 : atoi(col(r[0], "stock").c_str());
}

// How many of this product are already in the cart.
static int quantityInCart(int cartId, int productId)
{
    Rows r = dbRows("SELECT quantity FROM cart_items WHERE cart_id=" + to_string(cartId) +
                    " AND product_id=" + to_string(productId));
    return r.empty() ? 0 : atoi(col(r[0], "quantity").c_str());
}

// Deletes one line from the cart.
static string removeLine(int userId, int productId)
{
    int cartId = findActiveCart(userId, false);
    if (cartId == 0)
        return "Your cart is empty.";

    Result r = dbRun("DELETE FROM cart_items WHERE cart_id=" + to_string(cartId) +
                     " AND product_id=" + to_string(productId));
    return r.ok ? "" : "Database error: " + r.error;
}

// ---- the public functions ------------------------------------------------------------

int cartItemCount(int userId)
{
    lock_guard<mutex> lock(dbMutex);

    int cartId = findActiveCart(userId, false);
    if (cartId == 0) return 0;

    Rows r = dbRows("SELECT COALESCE(SUM(quantity),0) AS n FROM cart_items WHERE cart_id=" + to_string(cartId));
    return r.empty() ? 0 : atoi(col(r[0], "n").c_str());
}

Rows cartItems(int userId)
{
    lock_guard<mutex> lock(dbMutex);

    int cartId = findActiveCart(userId, false);
    if (cartId == 0) return Rows();

    return dbRows("SELECT ci.product_id, p.name, p.price, ci.quantity, (ci.quantity*p.price) AS subtotal, "
                  "p.stock, p.category_id, c.category_name "
                  "FROM cart_items ci "
                  "JOIN products p ON ci.product_id=p.product_id "
                  "JOIN categories c ON p.category_id=c.category_id "
                  "WHERE ci.cart_id=" + to_string(cartId) + " ORDER BY ci.cart_item_id");
}

Row cartTotals(int userId)
{
    lock_guard<mutex> lock(dbMutex);

    int cartId = findActiveCart(userId, false);
    if (cartId == 0) return Row();

    Rows r = dbRows("SELECT COALESCE(SUM(ci.quantity),0) AS units, COALESCE(SUM(ci.quantity*p.price),0) AS total "
                    "FROM cart_items ci JOIN products p ON ci.product_id=p.product_id "
                    "WHERE ci.cart_id=" + to_string(cartId));
    return r.empty() ? Row() : r[0];
}

// Adds to the quantity already in the cart, but never more than the stock.
string addToCart(int userId, int productId, int quantity)
{
    if (quantity <= 0)
        return "Quantity must be at least 1.";

    lock_guard<mutex> lock(dbMutex);

    int stock = stockOf(productId);
    if (stock < 0)
        return "That product no longer exists.";

    int cartId = findActiveCart(userId, true);
    if (cartId == 0)
        return "Could not open your cart. Please try again.";

    int already = quantityInCart(cartId, productId);

    if (already + quantity > stock)
        return "Only " + to_string(stock) + " in stock (you already have " + to_string(already) + " in your cart).";

    Result r;
    if (already > 0)
        r = dbRun("UPDATE cart_items SET quantity=quantity+" + to_string(quantity) +
                  " WHERE cart_id=" + to_string(cartId) + " AND product_id=" + to_string(productId));
    else
        r = dbRun("INSERT INTO cart_items (cart_id, product_id, quantity) VALUES (" +
                  to_string(cartId) + "," + to_string(productId) + "," + to_string(quantity) + ")");

    return r.ok ? "" : "Database error: " + r.error;
}

string setCartQuantity(int userId, int productId, int quantity)
{
    lock_guard<mutex> lock(dbMutex);

    if (quantity <= 0)
        return removeLine(userId, productId);

    int cartId = findActiveCart(userId, false);
    if (cartId == 0)
        return "Your cart is empty.";

    int stock = stockOf(productId);
    if (stock < 0)
        return "That product no longer exists.";

    if (quantity > stock)
        return "Only " + to_string(stock) + " in stock.";

    Result r = dbRun("UPDATE cart_items SET quantity=" + to_string(quantity) +
                     " WHERE cart_id=" + to_string(cartId) + " AND product_id=" + to_string(productId));
    return r.ok ? "" : "Database error: " + r.error;
}

string removeFromCart(int userId, int productId)
{
    lock_guard<mutex> lock(dbMutex);
    return removeLine(userId, productId);
}
