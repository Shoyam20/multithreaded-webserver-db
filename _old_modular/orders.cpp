#include "orders.h"
#include "cart.h"
#include "catalog.h"
#include "database.h"
#include "html_helpers.h"
#include "request_context.h"
#include <sstream>
#include <vector>
#include <cstdlib>
using namespace std;

// ============================================================
//  PLACE ORDER (transaction)
// ============================================================
string checkoutCart(int userId)
{
    lock_guard<mutex> lock(dbMutex);

    int cartId =
        getActiveCartIdLocked(userId, false);

    if (cartId == 0)
        return pageWrapper(
            "Checkout",
            "<div class='err'>No active cart found.</div>"
        );

    if (mysql_query(dbConn, "START TRANSACTION"))
        return pageWrapper(
            "Checkout",
            "<div class='err'>Unable to start transaction.</div>"
        );

    string errorMessage;
    double total = 0.0;
    unsigned long long orderId = 0;

    string q =
        "SELECT ci.product_id, ci.quantity, "
        "p.price, p.stock "
        "FROM cart_items ci "
        "JOIN products p ON ci.product_id=p.product_id "
        "WHERE ci.cart_id=" +
        to_string(cartId) +
        " FOR UPDATE";

    if (mysql_query(dbConn, q.c_str()))
    {
        errorMessage = mysql_error(dbConn);
    }
    else
    {
        MYSQL_RES* r =
            mysql_store_result(dbConn);

        if (!r)
        {
            errorMessage =
                "Unable to read cart items.";
        }
        else
        {
            struct CartItem
            {
                int productId;
                int quantity;
                double price;
                int stock;
            };

            vector<CartItem> items;

            MYSQL_ROW row;

            while ((row = mysql_fetch_row(r)))
            {
                CartItem item;

                item.productId = atoi(row[0]);
                item.quantity = atoi(row[1]);
                item.price = atof(row[2]);
                item.stock = atoi(row[3]);

                if (item.quantity <= 0 ||
                    item.quantity > item.stock)
                {
                    errorMessage =
                        "Insufficient stock for one or more products.";

                    break;
                }

                total +=
                    item.quantity * item.price;

                items.push_back(item);
            }

            mysql_free_result(r);

            if (errorMessage.empty() &&
                items.empty())
            {
                errorMessage =
                    "Your cart is empty.";
            }

            if (errorMessage.empty())
            {
                string insOrder =
                    "INSERT INTO orders "
                    "(user_id, cart_id, total, status) "
                    "VALUES (" +
                    to_string(userId) +
                    "," +
                    to_string(cartId) +
                    "," +
                    to_string(total) +
                    ",'PLACED')";

                if (mysql_query(
                        dbConn,
                        insOrder.c_str()))
                {
                    errorMessage =
                        mysql_error(dbConn);
                }
                else
                {
                    orderId =
                        mysql_insert_id(dbConn);

                    for (const CartItem& item : items)
                    {
                        string insItem =
                            "INSERT INTO order_items "
                            "(order_id, product_id, quantity, price) "
                            "VALUES (" +
                            to_string(orderId) +
                            "," +
                            to_string(item.productId) +
                            "," +
                            to_string(item.quantity) +
                            "," +
                            to_string(item.price) +
                            ")";

                        if (mysql_query(
                                dbConn,
                                insItem.c_str()))
                        {
                            errorMessage =
                                mysql_error(dbConn);

                            break;
                        }

                        string updateStock =
                            "UPDATE products "
                            "SET stock=stock-" +
                            to_string(item.quantity) +
                            " WHERE product_id=" +
                            to_string(item.productId) +
                            " AND stock >= " +
                            to_string(item.quantity);

                        if (mysql_query(
                                dbConn,
                                updateStock.c_str()) ||
                            mysql_affected_rows(dbConn) != 1)
                        {
                            errorMessage =
                                "Stock update failed.";

                            break;
                        }
                    }

                    if (errorMessage.empty())
                    {
                        string payment =
                            "INSERT INTO payments "
                            "(order_id, amount, method, status) "
                            "VALUES (" +
                            to_string(orderId) +
                            "," +
                            to_string(total) +
                            ",'COD','PENDING')";

                        if (mysql_query(
                                dbConn,
                                payment.c_str()))
                        {
                            errorMessage =
                                mysql_error(dbConn);
                        }
                    }

                    // Keep cart_items as a checkout snapshot for audit/demo purposes.
                    // The cart is marked CHECKED_OUT below, so it will not be treated
                    // as the user's active cart. order_items stores the purchase snapshot.

                    if (errorMessage.empty())
                    {
                        string closeCart =
                            "UPDATE cart "
                            "SET status='CHECKED_OUT' "
                            "WHERE cart_id=" +
                            to_string(cartId);

                        if (mysql_query(
                                dbConn,
                                closeCart.c_str()))
                        {
                            errorMessage =
                                mysql_error(dbConn);
                        }
                    }
                }
            }
        }
    }

    if (!errorMessage.empty())
    {
        mysql_rollback(dbConn);

        return pageWrapper(
            "Checkout",
            string("<div class='err'>"
                   "Checkout failed: ") +
            htmlEscape(errorMessage) +
            "</div>"
        );
    }

    if (mysql_commit(dbConn) != 0)
    {
        mysql_rollback(dbConn);

        return pageWrapper(
            "Checkout",
            "<div class='err'>Commit failed.</div>"
        );
    }


    // Success: go to the order list (Post/Redirect/Get) so a refresh
    // can never place the same order twice.
    (void)orderId;
    (void)total;
    tlRedirect = "/orders?notice=ordered";

    return "";
}

// ============================================================
//  CHECKOUT REVIEW PAGE
// ============================================================
string buildCheckoutPage(int userId)
{
    lock_guard<mutex> lock(dbMutex);

    int cartId = getActiveCartIdLocked(userId, false);

    string q =
        "SELECT p.name, ci.quantity, p.price, (ci.quantity*p.price), p.stock, "
        "p.category_id, c.category_name "
        "FROM cart_items ci JOIN products p ON ci.product_id=p.product_id "
        "JOIN categories c ON p.category_id=c.category_id "
        "WHERE ci.cart_id=" + to_string(cartId) + " ORDER BY ci.cart_item_id";

    MYSQL_RES* r = nullptr;

    if (cartId != 0 && mysql_query(dbConn, q.c_str()) == 0)
        r = mysql_store_result(dbConn);

    ostringstream items;
    double total = 0.0;
    int units = 0;
    bool stockProblem = false;

    if (r)
    {
        MYSQL_ROW row;

        while ((row = mysql_fetch_row(r)))
        {
            int qty = atoi(row[1]);
            int stock = atoi(row[4]);
            double sub = atof(row[3]);

            if (qty > stock)
                stockProblem = true;

            total += sub;
            units += qty;

            items << "<div class='line'><div class='thumb' style='background:"
                  << categoryTint(atoi(row[5])) << "'>" << categoryIcon(row[6] ? row[6] : "")
                  << "</div><div><div class='nm'>" << htmlEscape(row[0] ? row[0] : "")
                  << "</div><div class='sub'>Qty " << qty << " × " << rupees(row[2] ? row[2] : "0")
                  << "</div>";

            if (qty > stock)
                items << "<div class='stk no'>Only " << stock << " in stock</div>";

            items << "</div><div class='rt'>" << rupees(sub) << "</div></div>";
        }

        mysql_free_result(r);
    }

    if (units == 0)
        return pageWrapper("",
            "<div class='empty'><div class='big'>🛒</div><h2>Nothing to check out</h2>"
            "<p>Your cart is empty.</p>"
            "<a class='btn accent lg' href='/products'>Start shopping</a></div>");

    ostringstream b;

    b << "<div class='steps'><span class='s'>1 · Cart</span>"
         "<span class='s on'>2 · Review &amp; pay</span><span class='s'>3 · Done</span></div>"
         "<h1 class='page-title'>Checkout</h1><div class='two'><div>"

         "<div class='panel' style='margin-bottom:18px'><h2>1. Delivering to</h2>"
         "<b>" << htmlEscape(tlUserName) << "</b>"
         "<p class='sub' style='margin:4px 0 0;color:var(--muted)'>"
         "This demo store does not collect a postal address.</p></div>"

         "<div class='panel' style='margin-bottom:18px'><h2>2. Payment method</h2>"
         "<div class='opt sel'><span>💵</span><div><b>Cash on Delivery</b>"
         "<span>Pay when the order arrives. Recorded as PENDING.</span></div></div>"
         "<div class='opt dis'><span>💳</span><div><b>Card / UPI</b>"
         "<span>Not available — no payment gateway is connected.</span></div></div></div>"

         "<div class='panel'><h2>3. Review items (" << units << ")</h2>"
      << items.str() << "</div></div>"

         "<aside class='panel'><h2>Order summary</h2>"
         "<div class='sumrow'><span>Items (" << units << ")</span><span>" << rupees(total) << "</span></div>"
         "<div class='sumrow'><span>Delivery</span><span class='free'>FREE</span></div>"
         "<div class='sumrow total'><span>Order total</span><span>" << rupees(total) << "</span></div>";

    if (stockProblem)
        b << "<div class='err'>Some items exceed available stock. "
             "<a href='/cart'>Fix your cart</a> before ordering.</div>";
    else
        b << "<form method='POST' action='/place-order' style='margin-top:14px'>"
             "<button class='btn accent lg block' type='submit'>Place order (COD)</button></form>";

    b << "<a class='btn ghost block' style='margin-top:10px' href='/cart'>Back to cart</a></aside></div>";

    return pageWrapper("", b.str());
}

// ============================================================
//  ORDER HISTORY
// ============================================================
static string orderPill(const string& status, const string& kind)
{
    string cls = "pill";

    if (status == "PAID" || status == "DELIVERED") cls += " green";
    else if (status == "PENDING" || status == "PLACED" || status == "SHIPPED") cls += " amber";
    else if (status == "CANCELLED" || status == "FAILED") cls += " red";

    return "<span class='" + cls + "' title='" + kind + "'>" + htmlEscape(status) + "</span>";
}

string buildOrdersPage(int userId)
{
    lock_guard<mutex> lock(dbMutex);

    string q =
        "SELECT o.order_id, o.total, o.status, o.created_at, "
        "COALESCE(p.method,'-'), COALESCE(p.status,'-') "
        "FROM orders o LEFT JOIN payments p ON o.order_id=p.order_id "
        "WHERE o.user_id=" + to_string(userId) +
        " ORDER BY o.created_at DESC, o.order_id DESC";

    if (mysql_query(dbConn, q.c_str()))
        return pageWrapper("Orders",
            "<div class='err'>" + htmlEscape(mysql_error(dbConn)) + "</div>");

    MYSQL_RES* r = mysql_store_result(dbConn);

    if (!r)
        return pageWrapper("Orders", "<div class='err'>Order query failed.</div>");

    struct O { string id, total, status, date, method, pay; };
    vector<O> orders;
    MYSQL_ROW row;

    while ((row = mysql_fetch_row(r)))
        orders.push_back({ row[0] ? row[0] : "", row[1] ? row[1] : "0",
                           row[2] ? row[2] : "", row[3] ? row[3] : "",
                           row[4] ? row[4] : "", row[5] ? row[5] : "" });

    mysql_free_result(r);

    ostringstream b;
    b << "<h1 class='page-title'>📦 Your Orders</h1>";

    if (orders.empty())
    {
        b << "<div class='empty'><div class='big'>📦</div><h2>No orders yet</h2>"
             "<p>When you place an order it will show up here.</p>"
             "<a class='btn accent lg' href='/products'>Start shopping</a></div>";

        return pageWrapper("", b.str());
    }

    for (const O& o : orders)
    {
        b << "<div class='order'><div class='ohead'>"
             "<div>ORDER PLACED<b>" << htmlEscape(o.date) << "</b></div>"
             "<div>TOTAL<b>" << rupees(o.total) << "</b></div>"
             "<div>PAYMENT<b>" << htmlEscape(o.method) << " · " << orderPill(o.pay, "Payment status")
          << "</b></div><div>ORDER # <b>" << htmlEscape(o.id) << "</b></div>"
             "<div>STATUS<b>" << orderPill(o.status, "Order status") << "</b></div></div>"
             "<div class='obody'>";

        string iq =
            "SELECT p.product_id, p.name, oi.quantity, oi.price, p.category_id, c.category_name "
            "FROM order_items oi JOIN products p ON oi.product_id=p.product_id "
            "JOIN categories c ON p.category_id=c.category_id "
            "WHERE oi.order_id=" + o.id;

        if (mysql_query(dbConn, iq.c_str()) == 0)
        {
            MYSQL_RES* ir = mysql_store_result(dbConn);

            if (ir)
            {
                MYSQL_ROW irow;

                while ((irow = mysql_fetch_row(ir)))
                {
                    b << "<div class='oitem'><div class='thumb' style='background:"
                      << categoryTint(irow[4] ? atoi(irow[4]) : 0) << "'>"
                      << categoryIcon(irow[5] ? irow[5] : "") << "</div><div style='flex:1'>"
                         "<a class='nm' href='/product?id=" << (irow[0] ? irow[0] : "") << "'>"
                      << htmlEscape(irow[1] ? irow[1] : "") << "</a>"
                         "<div class='sub' style='color:var(--muted);font-size:13px'>Qty "
                      << (irow[2] ? irow[2] : "0") << " × " << rupees(irow[3] ? irow[3] : "0")
                      << "</div></div><a class='btn ghost' href='/product?id="
                      << (irow[0] ? irow[0] : "") << "'>Buy again</a></div>";
                }

                mysql_free_result(ir);
            }
        }

        b << "</div></div>";
    }

    return pageWrapper("", b.str());
}
