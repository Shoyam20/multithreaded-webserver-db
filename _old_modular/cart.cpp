#include "cart.h"
#include "catalog.h"
#include "database.h"
#include "html_helpers.h"
#include "request_context.h"
#include <cstdlib>
#include <sstream>

using namespace std;

// ============================================================
//  CART LOOKUP
// ============================================================
int getActiveCartIdLocked(int userId, bool createIfMissing)
{
    string q =
        "SELECT cart_id FROM cart WHERE user_id=" + to_string(userId) +
        " AND status='ACTIVE' ORDER BY cart_id DESC LIMIT 1";

    if (mysql_query(dbConn, q.c_str()))
        return 0;

    MYSQL_RES* r = mysql_store_result(dbConn);

    if (!r)
        return 0;

    MYSQL_ROW row = mysql_fetch_row(r);

    if (row)
    {
        int cartId = atoi(row[0]);
        mysql_free_result(r);
        return cartId;
    }

    mysql_free_result(r);

    if (!createIfMissing)
        return 0;

    string ins =
        "INSERT INTO cart (user_id, status) VALUES (" +
        to_string(userId) + ",'ACTIVE')";

    if (mysql_query(dbConn, ins.c_str()))
        return 0;

    return (int)mysql_insert_id(dbConn);
}

int getCartItemCount(int userId)
{
    if (userId <= 0)
        return 0;

    lock_guard<mutex> lock(dbMutex);

    int cartId = getActiveCartIdLocked(userId, false);

    if (cartId == 0)
        return 0;

    string q =
        "SELECT COALESCE(SUM(quantity),0) FROM cart_items WHERE cart_id=" +
        to_string(cartId);

    if (mysql_query(dbConn, q.c_str()))
        return 0;

    MYSQL_RES* r = mysql_store_result(dbConn);

    if (!r)
        return 0;

    MYSQL_ROW row = mysql_fetch_row(r);
    int n = (row && row[0]) ? atoi(row[0]) : 0;

    mysql_free_result(r);

    return n;
}

static string cartError(const string& msg)
{
    return pageWrapper("Cart",
        "<div class='err'>" + htmlEscape(msg) + "</div>"
        "<p><a class='btn' href='/cart'>Back to cart</a> "
        "<a class='btn ghost' href='/products'>Continue shopping</a></p>");
}

// ============================================================
//  CART PAGE
// ============================================================
string buildCartPage(int userId)
{
    lock_guard<mutex> lock(dbMutex);

    int cartId = getActiveCartIdLocked(userId, false);

    ostringstream b;
    b << "<h1 class='page-title'>🛒 Shopping Cart</h1>";

    MYSQL_RES* r = nullptr;

    if (cartId != 0)
    {
        string q =
            "SELECT ci.product_id, p.name, ci.quantity, p.price, "
            "(ci.quantity * p.price), p.stock, p.category_id, c.category_name "
            "FROM cart_items ci "
            "JOIN products p ON ci.product_id=p.product_id "
            "JOIN categories c ON p.category_id=c.category_id "
            "WHERE ci.cart_id=" + to_string(cartId) +
            " ORDER BY ci.cart_item_id";

        if (mysql_query(dbConn, q.c_str()))
            return pageWrapper("Error",
                "<div class='err'>" + htmlEscape(mysql_error(dbConn)) + "</div>");

        r = mysql_store_result(dbConn);
    }

    ostringstream lines;
    double total = 0.0;
    int units = 0;
    int kinds = 0;

    if (r)
    {
        MYSQL_ROW row;

        while ((row = mysql_fetch_row(r)))
        {
            string pid = row[0] ? row[0] : "";
            string name = row[1] ? row[1] : "";
            int qty = row[2] ? atoi(row[2]) : 0;
            string price = row[3] ? row[3] : "0";
            double subtotal = row[4] ? atof(row[4]) : 0.0;
            int stock = row[5] ? atoi(row[5]) : 0;
            int catId = row[6] ? atoi(row[6]) : 0;
            string cat = row[7] ? row[7] : "";

            total += subtotal;
            units += qty;
            kinds++;

            lines << "<div class='line'>"
                     "<a href='/product?id=" << pid << "'>"
                     "<div class='thumb' style='background:" << categoryTint(catId) << "'>"
                  << categoryIcon(cat) << "</div></a>"
                     "<div><a class='nm' href='/product?id=" << pid << "'>"
                  << htmlEscape(name) << "</a>"
                     "<div class='sub'>" << rupees(price) << " each</div>";

            if (qty > stock)
                lines << "<div class='stk no'>Only " << stock << " in stock — reduce the quantity</div>";

            lines << "<div class='acts'>"
                     "<form method='GET' action='/update-cart' style='display:flex;gap:8px;align-items:center'>"
                     "<input type='hidden' name='product_id' value='" << pid << "'>"
                     "<div class='qty'><button type='button' onclick='stepQty(this,-1)'>−</button>"
                     "<input type='number' name='quantity' value='" << qty
                  << "' min='1' max='" << (stock > 0 ? stock : 1) << "'>"
                     "<button type='button' onclick='stepQty(this,1)'>+</button></div>"
                     "<button class='btn ghost' type='submit'>Update</button></form>"
                     "<a class='linkbtn' href='/remove-cart?product_id=" << pid
                  << "'>🗑️ Remove</a></div></div>"
                     "<div class='rt'>" << rupees(subtotal) << "</div></div>";
        }

        mysql_free_result(r);
    }

    if (kinds == 0)
    {
        b << "<div class='empty'><div class='big'>🛒</div>"
             "<h2>Your cart is empty</h2>"
             "<p>Browse the catalogue and add something you like.</p>"
             "<a class='btn accent lg' href='/products'>Start shopping</a></div>";

        return pageWrapper("", b.str());
    }

    b << "<div class='two'><div class='panel'><h2>Items (" << units << ")</h2>"
      << lines.str() << "</div>"

         "<aside class='panel'><h2>Order summary</h2>"
         "<div class='sumrow'><span>Items (" << units << ")</span><span>"
      << rupees(total) << "</span></div>"
         "<div class='sumrow'><span>Delivery</span><span class='free'>FREE</span></div>"
         "<div class='sumrow total'><span>Total</span><span>" << rupees(total) << "</span></div>"
         "<a class='btn accent lg block' style='margin-top:14px' href='/checkout'>"
         "Proceed to checkout</a>"
         "<a class='btn ghost block' style='margin-top:10px' href='/products'>"
         "Continue shopping</a></aside></div>";

    return pageWrapper("", b.str());
}

// ============================================================
//  MUTATIONS
// ============================================================
string addCartItem(int userId, int productId, int quantity)
{
    if (quantity <= 0)
        return cartError("Quantity must be greater than 0.");

    lock_guard<mutex> lock(dbMutex);

    int cartId = getActiveCartIdLocked(userId, true);

    if (cartId == 0)
        return cartError("Could not open your cart. Please try again.");

    string q = "SELECT stock FROM products WHERE product_id=" + to_string(productId);

    if (mysql_query(dbConn, q.c_str()))
        return cartError(mysql_error(dbConn));

    MYSQL_RES* r = mysql_store_result(dbConn);

    if (!r)
        return cartError("Product query failed.");

    MYSQL_ROW row = mysql_fetch_row(r);

    if (!row)
    {
        mysql_free_result(r);
        return cartError("Product not found.");
    }

    int stock = atoi(row[0]);

    mysql_free_result(r);

    string existingQ =
        "SELECT quantity FROM cart_items WHERE cart_id=" + to_string(cartId) +
        " AND product_id=" + to_string(productId);

    int existingQty = 0;

    if (mysql_query(dbConn, existingQ.c_str()))
        return cartError("Cart query failed.");

    MYSQL_RES* existingR = mysql_store_result(dbConn);

    if (existingR)
    {
        MYSQL_ROW existingRow = mysql_fetch_row(existingR);

        if (existingRow)
            existingQty = atoi(existingRow[0]);

        mysql_free_result(existingR);
    }

    if (existingQty + quantity > stock)
        return cartError("Requested quantity exceeds available stock (" +
                         to_string(stock) + " available).");

    string sql;

    if (existingQty > 0)
        sql = "UPDATE cart_items SET quantity=quantity+" + to_string(quantity) +
              " WHERE cart_id=" + to_string(cartId) +
              " AND product_id=" + to_string(productId);
    else
        sql = "INSERT INTO cart_items (cart_id, product_id, quantity) VALUES (" +
              to_string(cartId) + "," + to_string(productId) + "," +
              to_string(quantity) + ")";

    if (mysql_query(dbConn, sql.c_str()))
        return cartError(mysql_error(dbConn));

    tlRedirect = "/cart?notice=added";

    return "";
}

string setCartItemQuantity(int userId, int productId, int quantity)
{
    if (quantity <= 0)
        return removeCartItem(userId, productId);

    lock_guard<mutex> lock(dbMutex);

    int cartId = getActiveCartIdLocked(userId, false);

    if (cartId == 0)
        return cartError("No active cart found.");

    string q = "SELECT stock FROM products WHERE product_id=" + to_string(productId);

    if (mysql_query(dbConn, q.c_str()))
        return cartError(mysql_error(dbConn));

    MYSQL_RES* r = mysql_store_result(dbConn);
    MYSQL_ROW row = r ? mysql_fetch_row(r) : nullptr;

    if (!row)
    {
        if (r) mysql_free_result(r);
        return cartError("Product not found.");
    }

    int stock = atoi(row[0]);

    mysql_free_result(r);

    if (quantity > stock)
        return cartError("Only " + to_string(stock) + " in stock.");

    string up =
        "UPDATE cart_items SET quantity=" + to_string(quantity) +
        " WHERE cart_id=" + to_string(cartId) +
        " AND product_id=" + to_string(productId);

    if (mysql_query(dbConn, up.c_str()))
        return cartError(mysql_error(dbConn));

    tlRedirect = "/cart?notice=updated";

    return "";
}

string removeCartItem(int userId, int productId)
{
    lock_guard<mutex> lock(dbMutex);

    int cartId = getActiveCartIdLocked(userId, false);

    if (cartId == 0)
        return cartError("No active cart found.");

    string q =
        "DELETE FROM cart_items WHERE cart_id=" + to_string(cartId) +
        " AND product_id=" + to_string(productId);

    if (mysql_query(dbConn, q.c_str()))
        return cartError(mysql_error(dbConn));

    tlRedirect = "/cart?notice=removed";

    return "";
}
