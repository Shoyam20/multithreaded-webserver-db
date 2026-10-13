#include "dashboard.h"
#include "database.h"
#include "html_helpers.h"
#include <sstream>
#include <cstdlib>
using namespace std;


// ============================================================
//  DATABASE DASHBOARD / RELATIONSHIP VISUALIZATION
// ============================================================
string buildDatabaseDashboard()
{
    lock_guard<mutex> lock(dbMutex);
    ostringstream b;

    const char* tables[] = {
        "users", "user_sessions", "categories", "products",
        "cart", "cart_items", "orders", "order_items", "payments"
    };

    const char* labels[] = {
        "Users", "User Sessions", "Categories", "Products",
        "Carts", "Cart Items", "Orders", "Order Items", "Payments"
    };

    b << "<style>"
         ".dashgrid{display:grid;grid-template-columns:repeat(auto-fit,minmax(145px,1fr));gap:12px;margin:18px 0;}"
         ".stat{background:white;padding:16px;border-radius:8px;border:1px solid #ddd;}"
         ".stat strong{display:block;font-size:26px;color:#2c3e50;margin-top:6px;}"
         ".flow{display:flex;flex-wrap:wrap;align-items:center;gap:8px;margin:18px 0;}"
         ".flowbox{background:#e8f0fe;border:1px solid #9bb9ef;border-radius:8px;padding:12px;text-align:center;min-width:120px;}"
         ".arrow{font-size:22px;color:#555;}"
         ".section{background:white;padding:18px;border-radius:8px;margin:18px 0;overflow-x:auto;}"
         ".hint{color:#555;font-size:14px;}"
         "</style>";

    b << "<p class='hint'>Live counts and order-flow records read directly from ecommerce_db. Refresh this page after signup, adding products to a cart, or checkout.</p>";
    b << "<div class='dashgrid'>";

    for (int i = 0; i < 9; ++i)
    {
        string q = string("SELECT COUNT(*) FROM ") + tables[i];
        long long count = 0;

        if (mysql_query(dbConn, q.c_str()) == 0)
        {
            MYSQL_RES* r = mysql_store_result(dbConn);
            if (r)
            {
                MYSQL_ROW row = mysql_fetch_row(r);
                if (row && row[0]) count = atoll(row[0]);
                mysql_free_result(r);
            }
        }

        b << "<div class='stat'><span>" << labels[i]
          << "</span><strong>" << count << "</strong><small>records</small></div>";
    }
    b << "</div>";

    b << "<div class='section'><h2>Relationship / ER Flow</h2>"
         "<div class='flow'>"
         "<div class='flowbox'><b>users</b><br>user_id (PK)</div><span class='arrow'>→</span>"
         "<div class='flowbox'><b>user_sessions</b><br>user_id (FK)</div></div>"
         "<div class='flow'>"
         "<div class='flowbox'><b>categories</b><br>category_id (PK)</div><span class='arrow'>→</span>"
         "<div class='flowbox'><b>products</b><br>category_id (FK)</div></div>"
         "<div class='flow'>"
         "<div class='flowbox'><b>users</b></div><span class='arrow'>→</span>"
         "<div class='flowbox'><b>cart</b><br>user_id (FK)</div><span class='arrow'>→</span>"
         "<div class='flowbox'><b>cart_items</b><br>cart_id, product_id</div><span class='arrow'>←</span>"
         "<div class='flowbox'><b>products</b></div></div>"
         "<div class='flow'>"
         "<div class='flowbox'><b>cart</b></div><span class='arrow'>→</span>"
         "<div class='flowbox'><b>orders</b><br>cart_id (unique)</div><span class='arrow'>→</span>"
         "<div class='flowbox'><b>order_items</b><br>order_id, product_id</div><span class='arrow'>←</span>"
         "<div class='flowbox'><b>products</b></div></div>"
         "<div class='flow'>"
         "<div class='flowbox'><b>orders</b></div><span class='arrow'>→</span>"
         "<div class='flowbox'><b>payments</b><br>order_id (unique)</div></div>"
         "<p class='hint'>PK = Primary Key; FK = Foreign Key. The application coordinates stock changes and checkout through a SQL transaction.</p>"
         "</div>";

    b << "<div class='section'><h2>Order Journey: Cart → Order → Payment</h2>"
         "<p class='hint'>Cart rows are retained after checkout for demonstration/audit. Order items are the final purchased-item snapshot.</p>"
         "<table><tr><th>Order</th><th>Customer</th><th>Cart</th><th>Cart Product</th>"
         "<th>Cart Qty</th><th>Order Product</th><th>Order Qty</th><th>Category</th>"
         "<th>Total</th><th>Order Status</th><th>Payment Method</th><th>Payment Status</th></tr>";

    const char* flowQuery =
        "SELECT o.order_id, u.name, c.cart_id, "
        "COALESCE(cp.name,'(no cart snapshot)'), COALESCE(ci.quantity,0), "
        "COALESCE(op.name,'(no order item)'), COALESCE(oi.quantity,0), "
        "COALESCE(cat.category_name,'-'), o.total, o.status, "
        "COALESCE(pay.method,'-'), COALESCE(pay.status,'-') "
        "FROM orders o "
        "JOIN users u ON u.user_id=o.user_id "
        "JOIN cart c ON c.cart_id=o.cart_id "
        "LEFT JOIN cart_items ci ON ci.cart_id=c.cart_id "
        "LEFT JOIN products cp ON cp.product_id=ci.product_id "
        "LEFT JOIN categories cat ON cat.category_id=cp.category_id "
        "LEFT JOIN order_items oi ON oi.order_id=o.order_id "
        "LEFT JOIN products op ON op.product_id=oi.product_id "
        "LEFT JOIN payments pay ON pay.order_id=o.order_id "
        "ORDER BY o.created_at DESC, o.order_id DESC LIMIT 200";

    if (mysql_query(dbConn, flowQuery) == 0)
    {
        MYSQL_RES* r = mysql_store_result(dbConn);
        if (r)
        {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(r)))
            {
                b << "<tr>";
                for (int i = 0; i < 12; ++i)
                {
                    string value = row[i] ? row[i] : "";
                    if (i == 8) value = "₹" + value;
                    b << "<td>" << htmlEscape(value) << "</td>";
                }
                b << "</tr>";
            }
            mysql_free_result(r);
        }
    }
    else
    {
        b << "<tr><td colspan='12'>Query error: "
          << htmlEscape(mysql_error(dbConn)) << "</td></tr>";
    }

    b << "</table></div>"
         "<div class='section'><h2>Checkout Transaction</h2>"
         "<div class='flow'>"
         "<div class='flowbox'>1. Validate stock</div><span class='arrow'>→</span>"
         "<div class='flowbox'>2. Create orders</div><span class='arrow'>→</span>"
         "<div class='flowbox'>3. Insert order_items</div><span class='arrow'>→</span>"
         "<div class='flowbox'>4. Deduct stock</div><span class='arrow'>→</span>"
         "<div class='flowbox'>5. Insert payments</div><span class='arrow'>→</span>"
         "<div class='flowbox'>6. COMMIT</div></div>"
         "<p>If a database operation fails, the server calls ROLLBACK so the order, order items, payment, and stock changes do not partially persist.</p>"
         "<p><b>Payment note:</b> the current implementation records Cash on Delivery (COD) as PENDING. It does not process real online payments or mark COD as PAID.</p>"
         "</div>";

    b << "<p><a class='navbtn' href='/products'>Browse Products</a> "
         "<a class='navbtn' href='/orders'>My Orders</a></p>";

    return pageWrapper("Database Dashboard", b.str());
}

