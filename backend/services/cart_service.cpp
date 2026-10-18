#include "cart_service.h"
#include <cstdlib>

using namespace std;

// Get the user's active cart, optionally creating one if it doesn't exist.
int activeCartId(int userId, bool createIfMissing)
{
    DbLock lock;  

    Rows found = db.rows("SELECT cart_id FROM cart WHERE user_id=" + to_string(userId) +
                         " AND status='ACTIVE' ORDER BY cart_id DESC LIMIT 1");

    if (!found.empty())
        return atoi(col(found[0], "cart_id").c_str());

    if (!createIfMissing)
        return 0;

    Result r = db.run("INSERT INTO cart (user_id, status) VALUES (" + to_string(userId) +
                      ",'ACTIVE')");

    return r.ok ? (int)r.insertId : 0;
}

// Calculate the total quantity of products in the user's cart.
int cartItemCount(int userId)
{
    if (userId <= 0) return 0;

    int cartId = activeCartId(userId, false);
    if (cartId == 0) return 0;

    Rows r = db.rows("SELECT COALESCE(SUM(quantity),0) AS n FROM cart_items WHERE cart_id=" +
                     to_string(cartId));

    return r.empty() ? 0 : atoi(col(r[0], "n").c_str());
}

// Fetch cart products with their prices, quantities, subtotals, and categories.
Rows cartItems(int userId)
{
    int cartId = activeCartId(userId, false);
    if (cartId == 0) return {};

    return db.rows(
        "SELECT ci.product_id, p.name, p.price, ci.quantity, (ci.quantity*p.price) AS subtotal, "
        "p.stock, p.category_id, c.category_name "
        "FROM cart_items ci "
        "JOIN products p ON ci.product_id=p.product_id "
        "JOIN categories c ON p.category_id=c.category_id "
        "WHERE ci.cart_id=" + to_string(cartId) + " ORDER BY ci.cart_item_id");
}

// Get the stock of a product, or -1 if the product does not exist.
static int stockOf(int productId)
{
    Rows r = db.rows("SELECT stock FROM products WHERE product_id=" + to_string(productId));
    return r.empty() ? -1 : atoi(col(r[0], "stock").c_str());
}

// Get the quantity of a particular product already present in the cart.
static int quantityInCart(int cartId, int productId)
{
    Rows r = db.rows("SELECT quantity FROM cart_items WHERE cart_id=" + to_string(cartId) +
                     " AND product_id=" + to_string(productId));
    return r.empty() ? 0 : atoi(col(r[0], "quantity").c_str());
}

// Add a product to the cart after checking its availability and stock.
string addToCart(int userId, int productId, int quantity)
{
    if (quantity <= 0)
        return "Quantity must be at least 1.";

    DbLock lock;

    int stock = stockOf(productId);
    if (stock < 0)
        return "That product no longer exists.";

    int cartId = activeCartId(userId, true);
    if (cartId == 0)
        return "Could not open your cart. Please try again.";

    int already = quantityInCart(cartId, productId);

    // Prevent the requested quantity from exceeding available stock.
    if (already + quantity > stock)
        return "Only " + to_string(stock) + " in stock (you already have " +
               to_string(already) + " in your cart).";

    // Increase the existing quantity or insert a new cart item.
    Result r = (already > 0)
        ? db.run("UPDATE cart_items SET quantity=quantity+" + to_string(quantity) +
                 " WHERE cart_id=" + to_string(cartId) + " AND product_id=" + to_string(productId))
        : db.run("INSERT INTO cart_items (cart_id, product_id, quantity) VALUES (" +
                 to_string(cartId) + "," + to_string(productId) + "," + to_string(quantity) + ")");

    return r.ok ? "" : "Database error: " + r.error;
}

// Set the quantity of a product already in the cart.
string setCartQuantity(int userId, int productId, int quantity)
{
    // A quantity of zero or less removes the product from the cart.
    if (quantity <= 0)
        return removeFromCart(userId, productId);

    DbLock lock;

    int cartId = activeCartId(userId, false);
    if (cartId == 0)
        return "Your cart is empty.";

    int stock = stockOf(productId);
    if (stock < 0)
        return "That product no longer exists.";

    if (quantity > stock)
        return "Only " + to_string(stock) + " in stock.";

    // Update the selected product's quantity in the cart.
    Result r = db.run("UPDATE cart_items SET quantity=" + to_string(quantity) +
                      " WHERE cart_id=" + to_string(cartId) +
                      " AND product_id=" + to_string(productId));

    return r.ok ? "" : "Database error: " + r.error;
}

// Remove a product from the user's active cart.
string removeFromCart(int userId, int productId)
{
    int cartId = activeCartId(userId, false);
    if (cartId == 0)
        return "Your cart is empty.";

    Result r = db.run("DELETE FROM cart_items WHERE cart_id=" + to_string(cartId) +
                      " AND product_id=" + to_string(productId));

    return r.ok ? "" : "Database error: " + r.error;
}
