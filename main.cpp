#include <iostream>
#include <winsock2.h>
#include <cstring>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <sstream>
#include <map>
#include <queue>
#include <mutex>
#include <thread>
#include <vector>
#include <windows.h>
#include <random>

#include <mysql.h>

using namespace std;

// ============================================================
//  CONFIG
// ============================================================
const char* DB_HOST = "localhost";
const char* DB_USER = "root";
const char* DB_PASS = "shoyam123";
const char* DB_NAME = "ecommerce_db";
const int   DB_PORT = 3306;

// ============================================================
//  REQUEST / QUEUE
// ============================================================
struct Request { SOCKET clientSocket; };

queue<Request> requestQueue;
mutex queueMutex;
HANDLE requestSemaphore;

// ============================================================
//  DB
// ============================================================
MYSQL* dbConn = nullptr;
mutex dbMutex;

bool initDatabase()
{
    dbConn = mysql_init(nullptr);

    if (!dbConn)
    {
        cout << "[DB] mysql_init failed!" << endl;
        return false;
    }

    if (!mysql_real_connect(dbConn, DB_HOST, DB_USER, DB_PASS, DB_NAME,
                            DB_PORT, nullptr, 0))
    {
        cout << "[DB] Connection failed: " << mysql_error(dbConn) << endl;
        return false;
    }

    mysql_set_character_set(dbConn, "utf8mb4");

    cout << "[DB] Connected to MySQL successfully." << endl;
    return true;
}

// Escape a string for safe use in SQL (prevents SQL injection)
string esc(const string& s)
{
    vector<char> buf(s.size() * 2 + 1);

    unsigned long n = mysql_real_escape_string(
        dbConn,
        buf.data(),
        s.c_str(),
        (unsigned long)s.size()
    );

    return string(buf.data(), n);
}

// ============================================================
//  HTML HELPERS
// ============================================================
string htmlEscape(const string& s)
{
    string out;

    for (char ch : s)
    {
        if (ch == '&') out += "&amp;";
        else if (ch == '<') out += "&lt;";
        else if (ch == '>') out += "&gt;";
        else if (ch == '"') out += "&quot;";
        else if (ch == '\'') out += "&#39;";
        else out += ch;
    }

    return out;
}

string pageWrapper(const string& title, const string& body)
{
    ostringstream h;

    h << "<html><head><title>" << title << "</title><style>"
         "body{font-family:Arial;background:#f5f5f5;padding:30px;}"
         "table{border-collapse:collapse;background:white;"
         "box-shadow:0 2px 6px rgba(0,0,0,0.1);}"
         "th,td{border:1px solid #ddd;padding:10px 15px;text-align:left;}"
         "th{background:#2c3e50;color:white;}"
         "tr:nth-child(even){background:#f9f9f9;}"
         "h1{color:#2c3e50;}"
         "input{display:block;margin:8px 0;padding:8px;width:250px;}"
         "button{padding:8px 20px;background:#2c3e50;color:white;border:none;cursor:pointer;}"
         ".msg{padding:10px;background:#e8f5e9;border-left:4px solid #4caf50;margin:10px 0;}"
         ".err{padding:10px;background:#ffebee;border-left:4px solid #f44336;margin:10px 0;}"
         "nav a{margin-right:15px;color:#2c3e50;}"
         "</style></head><body>";

    h << "<nav><a href='/'>Home</a><a href='/products'>Products</a>"
         "<a href='/signup'>Signup</a><a href='/login'>Login</a></nav><hr>";

    h << "<h1>" << title << "</h1>";
    h << body;
    h << "</body></html>";

    return h.str();
}

// ============================================================
//  SESSION MANAGEMENT
// ============================================================
string generateSessionId()
{
    static const char chars[] = "0123456789abcdef";

    random_device rd;
    mt19937_64 gen(rd());
    uniform_int_distribution<int> dist(0, 15);

    string id;

    for (int i = 0; i < 32; i++)
        id += chars[dist(gen)];

    return id;
}

bool createSession(int userId, string& sessionId)
{
    lock_guard<mutex> lock(dbMutex);

    for (int attempt = 0; attempt < 5; attempt++)
    {
        sessionId = generateSessionId();

        string q =
            "INSERT INTO user_sessions (session_id, user_id) VALUES ('" +
            esc(sessionId) + "'," +
            to_string(userId) + ")";

        if (!mysql_query(dbConn, q.c_str()))
            return true;
    }

    return false;
}

int getSessionUserId(const string& sessionId)
{
    if (sessionId.empty())
        return 0;

    lock_guard<mutex> lock(dbMutex);

    string q =
        "SELECT user_id FROM user_sessions "
        "WHERE session_id='" + esc(sessionId) +
        "' AND is_active=TRUE";

    if (mysql_query(dbConn, q.c_str()))
        return 0;

    MYSQL_RES* r = mysql_store_result(dbConn);

    if (!r)
        return 0;

    MYSQL_ROW row = mysql_fetch_row(r);

    if (!row)
    {
        mysql_free_result(r);
        return 0;
    }

    int userId = atoi(row[0]);

    mysql_free_result(r);

    string update =
        "UPDATE user_sessions "
        "SET last_activity=CURRENT_TIMESTAMP "
        "WHERE session_id='" + esc(sessionId) +
        "' AND is_active=TRUE";

    mysql_query(dbConn, update.c_str());

    return userId;
}

bool logoutSession(const string& sessionId)
{
    if (sessionId.empty())
        return false;

    lock_guard<mutex> lock(dbMutex);

    string q =
        "UPDATE user_sessions "
        "SET is_active=FALSE, logout_time=CURRENT_TIMESTAMP "
        "WHERE session_id='" + esc(sessionId) +
        "' AND is_active=TRUE";

    return mysql_query(dbConn, q.c_str()) == 0 &&
           mysql_affected_rows(dbConn) > 0;
}

// ============================================================
//  PRODUCTS PAGE
// ============================================================
string buildProductsPage()
{
    lock_guard<mutex> lock(dbMutex);

    const char* q =
        "SELECT p.product_id, p.name, p.description, p.price, p.stock, "
        "c.category_name "
        "FROM products p "
        "JOIN categories c ON p.category_id=c.category_id "
        "ORDER BY p.product_id";

    if (mysql_query(dbConn, q))
        return pageWrapper(
            "Error",
            string("<p>") + htmlEscape(mysql_error(dbConn)) + "</p>"
        );

    MYSQL_RES* result = mysql_store_result(dbConn);

    if (!result)
        return pageWrapper("Error", "<p>No result</p>");

    ostringstream t;

    t << "<table><tr><th>ID</th><th>Name</th><th>Description</th>"
         "<th>Price</th><th>Stock</th><th>Category</th></tr>";

    MYSQL_ROW row;

    while ((row = mysql_fetch_row(result)))
    {
        t << "<tr>";

        for (int i = 0; i < 6; i++)
            t << "<td>" << htmlEscape(row[i] ? row[i] : "") << "</td>";

        t << "</tr>";
    }

    t << "</table>";

    mysql_free_result(result);

    return pageWrapper("Our Products", t.str());
}

// ============================================================
//  SIGNUP / LOGIN PAGES (GET — show form)
// ============================================================
string signupForm(const string& msg = "", bool isError = false)
{
    ostringstream b;

    if (!msg.empty())
    {
        b << "<div class='"
          << (isError ? "err" : "msg")
          << "'>"
          << htmlEscape(msg)
          << "</div>";
    }

    b << "<form method='POST' action='/signup'>"
         "<input name='name' placeholder='Full Name' required>"
         "<input name='email' type='email' placeholder='Email' required>"
         "<input name='password' type='password' placeholder='Password' required>"
         "<button type='submit'>Sign Up</button>"
         "</form>";

    return pageWrapper("Sign Up", b.str());
}

string loginForm(const string& msg = "", bool isError = false)
{
    ostringstream b;

    if (!msg.empty())
    {
        b << "<div class='"
          << (isError ? "err" : "msg")
          << "'>"
          << htmlEscape(msg)
          << "</div>";
    }

    b << "<form method='POST' action='/login'>"
         "<input name='email' type='email' placeholder='Email' required>"
         "<input name='password' type='password' placeholder='Password' required>"
         "<button type='submit'>Log In</button>"
         "</form>";

    return pageWrapper("Login", b.str());
}

// ============================================================
//  HANDLE SIGNUP POST
// ============================================================
string handleSignup(const map<string,string>& form)
{
    string name =
        form.count("name") ? form.at("name") : "";

    string email =
        form.count("email") ? form.at("email") : "";

    string pass =
        form.count("password") ? form.at("password") : "";

    if (name.empty() || email.empty() || pass.empty())
        return signupForm("All fields are required.", true);

    lock_guard<mutex> lock(dbMutex);

    // Check if email already exists
    string check =
        "SELECT user_id FROM users WHERE email='" +
        esc(email) + "'";

    if (mysql_query(dbConn, check.c_str()))
        return signupForm(
            string("DB error: ") + mysql_error(dbConn),
            true
        );

    MYSQL_RES* r = mysql_store_result(dbConn);

    if (!r)
        return signupForm("Query error.", true);

    bool exists = mysql_num_rows(r) > 0;

    mysql_free_result(r);

    if (exists)
        return signupForm(
            "Email already registered. Try logging in.",
            true
        );

    // Insert
    string ins =
        "INSERT INTO users (name, email, password) VALUES ('" +
        esc(name) + "','" +
        esc(email) + "','" +
        esc(pass) + "')";

    if (mysql_query(dbConn, ins.c_str()))
        return signupForm(
            string("Insert failed: ") +
            mysql_error(dbConn),
            true
        );

    return loginForm("Signup successful! Please log in.");
}

// ============================================================
//  HANDLE LOGIN POST
// ============================================================
string handleLogin(const map<string,string>& form)
{
    string email =
        form.count("email") ? form.at("email") : "";

    string pass =
        form.count("password") ? form.at("password") : "";

    if (email.empty() || pass.empty())
        return loginForm(
            "Both fields are required.",
            true
        );

    int userId = 0;
    string dbName;
    string dbPass;

    {
        lock_guard<mutex> lock(dbMutex);

        string q =
            "SELECT user_id, name, password "
            "FROM users WHERE email='" +
            esc(email) + "'";

        if (mysql_query(dbConn, q.c_str()))
            return loginForm(
                string("DB error: ") +
                mysql_error(dbConn),
                true
            );

        MYSQL_RES* r = mysql_store_result(dbConn);

        if (!r)
            return loginForm("Query error", true);

        MYSQL_ROW row = mysql_fetch_row(r);

        if (!row)
        {
            mysql_free_result(r);

            return loginForm(
                "No account found with that email.",
                true
            );
        }

        userId = atoi(row[0]);
        dbName = row[1] ? row[1] : "";
        dbPass = row[2] ? row[2] : "";

        mysql_free_result(r);
    }

    if (dbPass != pass)
        return loginForm(
            "Incorrect password.",
            true
        );

    // Session management
    string sessionId;

    if (!createSession(userId, sessionId))
        return loginForm(
            "Login successful, but session creation failed.",
            true
        );

    ostringstream b;

    b << "<div class='msg'>Welcome back, <b>"
      << htmlEscape(dbName)
      << "</b>! You are now logged in.</div>"

      << "<p>Session ID: <code>"
      << sessionId
      << "</code></p>"

      << "<p><a href='/products'>Browse products →</a></p>"

      << "<p><a href='/cart?session_id="
      << sessionId
      << "'>View Cart</a></p>"

      << "<p><a href='/orders?session_id="
      << sessionId
      << "'>View Orders</a></p>"

      << "<p><a href='/logout?session_id="
      << sessionId
      << "'>Logout</a></p>";

    return pageWrapper("Logged In", b.str());
}

// ============================================================
//  CART
// ============================================================
int getActiveCartIdLocked(int userId, bool createIfMissing)
{
    string q =
        "SELECT cart_id FROM cart "
        "WHERE user_id=" +
        to_string(userId) +
        " AND status='ACTIVE' "
        "ORDER BY cart_id DESC LIMIT 1";

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
        "INSERT INTO cart (user_id, status) "
        "VALUES (" +
        to_string(userId) +
        ",'ACTIVE')";

    if (mysql_query(dbConn, ins.c_str()))
        return 0;

    return (int)mysql_insert_id(dbConn);
}

string buildCartPage(
    const string& sessionId,
    int userId
)
{
    lock_guard<mutex> lock(dbMutex);

    int cartId =
        getActiveCartIdLocked(userId, false);

    if (cartId == 0)
    {
        ostringstream b;

        b << "<p>Your cart is empty.</p>"
          << "<p><a href='/products'>Browse products →</a></p>";

        return pageWrapper("Shopping Cart", b.str());
    }

    string q =
        "SELECT ci.product_id, p.name, ci.quantity, "
        "p.price, (ci.quantity * p.price) AS subtotal "
        "FROM cart_items ci "
        "JOIN products p ON ci.product_id=p.product_id "
        "WHERE ci.cart_id=" +
        to_string(cartId) +
        " ORDER BY ci.cart_item_id";

    if (mysql_query(dbConn, q.c_str()))
        return pageWrapper(
            "Error",
            string("<p>") +
            htmlEscape(mysql_error(dbConn)) +
            "</p>"
        );

    MYSQL_RES* r = mysql_store_result(dbConn);

    if (!r)
        return pageWrapper(
            "Error",
            "<p>Cart query failed.</p>"
        );

    ostringstream b;

    b << "<p>Cart ID: <b>"
      << cartId
      << "</b></p>"

      << "<table>"
         "<tr>"
         "<th>Product ID</th>"
         "<th>Product</th>"
         "<th>Quantity</th>"
         "<th>Price</th>"
         "<th>Subtotal</th>"
         "<th>Action</th>"
         "</tr>";

    MYSQL_ROW row;

    double total = 0.0;

    while ((row = mysql_fetch_row(r)))
    {
        double subtotal =
            row[4] ? atof(row[4]) : 0.0;

        total += subtotal;

        b << "<tr>"
          << "<td>"
          << (row[0] ? row[0] : "")
          << "</td>"

          << "<td>"
          << htmlEscape(row[1] ? row[1] : "")
          << "</td>"

          << "<td>"
          << (row[2] ? row[2] : "")
          << "</td>"

          << "<td>₹"
          << (row[3] ? row[3] : "0.00")
          << "</td>"

          << "<td>₹"
          << (row[4] ? row[4] : "0.00")
          << "</td>"

          << "<td>"
          << "<a href='/remove-cart?session_id="
          << sessionId
          << "&product_id="
          << (row[0] ? row[0] : "")
          << "'>Remove</a>"
          << "</td>"

          << "</tr>";
    }

    mysql_free_result(r);

    b << "</table>"

      << "<h3>Total: ₹"
      << total
      << "</h3>"

      << "<p><a href='/checkout?session_id="
      << sessionId
      << "'>Checkout</a></p>"

      << "<p><a href='/products'>Continue Shopping</a></p>";

    return pageWrapper(
        "Shopping Cart",
        b.str()
    );
}

string addCartItem(
    const string& sessionId,
    int userId,
    int productId,
    int quantity
)
{
    if (quantity <= 0)
        return pageWrapper(
            "Cart",
            "<div class='err'>Quantity must be greater than 0.</div>"
        );

    lock_guard<mutex> lock(dbMutex);

    int cartId =
        getActiveCartIdLocked(userId, true);

    if (cartId == 0)
        return pageWrapper(
            "Cart",
            "<div class='err'>Unable to create cart.</div>"
        );

    string q =
        "SELECT stock FROM products "
        "WHERE product_id=" +
        to_string(productId);

    if (mysql_query(dbConn, q.c_str()))
        return pageWrapper(
            "Cart",
            string("<div class='err'>") +
            htmlEscape(mysql_error(dbConn)) +
            "</div>"
        );

    MYSQL_RES* r = mysql_store_result(dbConn);

    if (!r)
        return pageWrapper(
            "Cart",
            "<div class='err'>Product query failed.</div>"
        );

    MYSQL_ROW row = mysql_fetch_row(r);

    if (!row)
    {
        mysql_free_result(r);

        return pageWrapper(
            "Cart",
            "<div class='err'>Product not found.</div>"
        );
    }

    int stock = atoi(row[0]);

    mysql_free_result(r);

    string existingQ =
        "SELECT quantity FROM cart_items "
        "WHERE cart_id=" +
        to_string(cartId) +
        " AND product_id=" +
        to_string(productId);

    int existingQty = 0;

    if (mysql_query(dbConn, existingQ.c_str()))
        return pageWrapper(
            "Cart",
            "<div class='err'>Cart query failed.</div>"
        );

    MYSQL_RES* existingR =
        mysql_store_result(dbConn);

    if (existingR)
    {
        MYSQL_ROW existingRow =
            mysql_fetch_row(existingR);

        if (existingRow)
            existingQty = atoi(existingRow[0]);

        mysql_free_result(existingR);
    }

    if (existingQty + quantity > stock)
        return pageWrapper(
            "Cart",
            "<div class='err'>Requested quantity exceeds available stock.</div>"
        );

    if (existingQty > 0)
    {
        string update =
            "UPDATE cart_items "
            "SET quantity=quantity+" +
            to_string(quantity) +
            " WHERE cart_id=" +
            to_string(cartId) +
            " AND product_id=" +
            to_string(productId);

        if (mysql_query(dbConn, update.c_str()))
            return pageWrapper(
                "Cart",
                string("<div class='err'>") +
                htmlEscape(mysql_error(dbConn)) +
                "</div>"
            );
    }
    else
    {
        string ins =
            "INSERT INTO cart_items "
            "(cart_id, product_id, quantity) "
            "VALUES (" +
            to_string(cartId) +
            "," +
            to_string(productId) +
            "," +
            to_string(quantity) +
            ")";

        if (mysql_query(dbConn, ins.c_str()))
            return pageWrapper(
                "Cart",
                string("<div class='err'>") +
                htmlEscape(mysql_error(dbConn)) +
                "</div>"
            );
    }

    ostringstream b;

    b << "<div class='msg'>"
      << "Product added to cart successfully."
      << "</div>"

      << "<p><a href='/cart?session_id="
      << sessionId
      << "'>View Cart</a></p>"

      << "<p><a href='/products'>Continue Shopping</a></p>";

    return pageWrapper(
        "Cart Updated",
        b.str()
    );
}

string removeCartItem(
    const string& sessionId,
    int userId,
    int productId
)
{
    lock_guard<mutex> lock(dbMutex);

    int cartId =
        getActiveCartIdLocked(userId, false);

    if (cartId == 0)
        return pageWrapper(
            "Cart",
            "<div class='err'>No active cart found.</div>"
        );

    string q =
        "DELETE FROM cart_items "
        "WHERE cart_id=" +
        to_string(cartId) +
        " AND product_id=" +
        to_string(productId);

    if (mysql_query(dbConn, q.c_str()))
        return pageWrapper(
            "Cart",
            string("<div class='err'>") +
            htmlEscape(mysql_error(dbConn)) +
            "</div>"
        );

    ostringstream b;

    b << "<div class='msg'>"
      << "Item removed from cart."
      << "</div>"

      << "<p><a href='/cart?session_id="
      << sessionId
      << "'>View Cart</a></p>";

    return pageWrapper(
        "Cart Updated",
        b.str()
    );
}

// ============================================================
//  ORDERS AND PAYMENTS
// ============================================================
string checkoutCart(
    const string& sessionId,
    int userId
)
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

                    if (errorMessage.empty())
                    {
                        string clearCart =
                            "DELETE FROM cart_items "
                            "WHERE cart_id=" +
                            to_string(cartId);

                        if (mysql_query(
                                dbConn,
                                clearCart.c_str()))
                        {
                            errorMessage =
                                mysql_error(dbConn);
                        }
                    }

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

    ostringstream b;

    b << "<div class='msg'>"
      << "Order placed successfully."
      << "</div>"

      << "<p>Order ID: <b>"
      << orderId
      << "</b></p>"

      << "<p>Total: <b>₹"
      << total
      << "</b></p>"

      << "<p>Payment Method: <b>COD</b></p>"

      << "<p>Payment Status: <b>PENDING</b></p>"

      << "<p><a href='/orders?session_id="
      << sessionId
      << "'>View Orders</a></p>"

      << "<p><a href='/products'>Continue Shopping</a></p>";

    return pageWrapper(
        "Checkout Complete",
        b.str()
    );
}

string buildOrdersPage(
    const string& sessionId,
    int userId
)
{
    lock_guard<mutex> lock(dbMutex);

    string q =
        "SELECT o.order_id, o.total, o.status, "
        "o.created_at, p.method, p.status "
        "FROM orders o "
        "LEFT JOIN payments p "
        "ON o.order_id=p.order_id "
        "WHERE o.user_id=" +
        to_string(userId) +
        " ORDER BY o.created_at DESC";

    if (mysql_query(dbConn, q.c_str()))
        return pageWrapper(
            "Orders",
            string("<div class='err'>") +
            htmlEscape(mysql_error(dbConn)) +
            "</div>"
        );

    MYSQL_RES* r =
        mysql_store_result(dbConn);

    if (!r)
        return pageWrapper(
            "Orders",
            "<div class='err'>Order query failed.</div>"
        );

    ostringstream b;

    b << "<table>"
         "<tr>"
         "<th>Order ID</th>"
         "<th>Total</th>"
         "<th>Status</th>"
         "<th>Created At</th>"
         "<th>Payment Method</th>"
         "<th>Payment Status</th>"
         "</tr>";

    MYSQL_ROW row;

    while ((row = mysql_fetch_row(r)))
    {
        b << "<tr>"

          << "<td>"
          << (row[0] ? row[0] : "")
          << "</td>"

          << "<td>₹"
          << (row[1] ? row[1] : "0.00")
          << "</td>"

          << "<td>"
          << (row[2] ? row[2] : "")
          << "</td>"

          << "<td>"
          << (row[3] ? row[3] : "")
          << "</td>"

          << "<td>"
          << (row[4] ? row[4] : "")
          << "</td>"

          << "<td>"
          << (row[5] ? row[5] : "")
          << "</td>"

          << "</tr>";
    }

    b << "</table>"

      << "<p><a href='/products'>Browse Products</a></p>"

      << "<p><a href='/cart?session_id="
      << sessionId
      << "'>View Cart</a></p>";

    mysql_free_result(r);

    return pageWrapper(
        "My Orders",
        b.str()
    );
}

// ============================================================
//  HTTP PARSING
// ============================================================
string urlDecode(const string& v)
{
    string decoded;

    for (size_t i = 0; i < v.size(); i++)
    {
        if (v[i] == '+')
        {
            decoded += ' ';
        }
        else if (v[i] == '%' &&
                 i + 2 < v.size())
        {
            int c = 0;

            if (sscanf(
                    v.substr(i + 1, 2).c_str(),
                    "%x",
                    &c) == 1)
            {
                decoded += (char)c;
                i += 2;
            }
            else
            {
                decoded += v[i];
            }
        }
        else
        {
            decoded += v[i];
        }
    }

    return decoded;
}

string parsePath(const string& raw)
{
    size_t lineEnd =
        raw.find("\r\n");

    if (lineEnd == string::npos)
        return "/";

    string firstLine =
        raw.substr(0, lineEnd);

    istringstream iss(firstLine);

    string method;
    string path;
    string version;

    iss >> method >> path >> version;

    size_t q = path.find('?');

    if (q != string::npos)
        path = path.substr(0, q);

    return path;
}

// Read the Content-Length value out of the header block (0 if absent)
size_t contentLength(const string& headers)
{
    string lower;

    for (size_t i = 0;
         i < headers.size();
         i++)
    {
        lower +=
            (char)tolower(
                (unsigned char)headers[i]
            );
    }

    size_t p =
        lower.find("content-length:");

    if (p == string::npos)
        return 0;

    return (size_t)strtoul(
        headers.c_str() + p + 15,
        nullptr,
        10
    );
}

string parseMethod(const string& raw)
{
    istringstream iss(raw);

    string method;

    iss >> method;

    return method;
}

map<string,string> parseParameters(
    const string& input
)
{
    map<string,string> out;

    istringstream iss(input);

    string pair;

    while (getline(iss, pair, '&'))
    {
        size_t eq =
            pair.find('=');

        if (eq == string::npos)
            continue;

        string k =
            urlDecode(pair.substr(0, eq));

        string v =
            urlDecode(pair.substr(eq + 1));

        out[k] = v;
    }

    return out;
}

// Parse "name=John&email=a@b.com&password=123"
map<string,string> parseFormBody(
    const string& raw
)
{
    size_t headerEnd =
        raw.find("\r\n\r\n");

    if (headerEnd == string::npos)
        return {};

    return parseParameters(
        raw.substr(headerEnd + 4)
    );
}

map<string,string> parseQuery(
    const string& raw
)
{
    size_t lineEnd =
        raw.find("\r\n");

    if (lineEnd == string::npos)
        return {};

    string firstLine =
        raw.substr(0, lineEnd);

    istringstream iss(firstLine);

    string method;
    string path;
    string version;

    iss >> method >> path >> version;

    size_t q =
        path.find('?');

    if (q == string::npos)
        return {};

    return parseParameters(
        path.substr(q + 1)
    );
}

int getIntParameter(
    const map<string,string>& params,
    const string& key,
    int defaultValue = 0
)
{
    if (!params.count(key))
        return defaultValue;

    try
    {
        return stoi(params.at(key));
    }
    catch (...)
    {
        return defaultValue;
    }
}

// ============================================================
//  HTTP RESPONSE
// ============================================================
string makeHttpResponse(
    const string& body,
    int status = 200
)
{
    string text = "OK";

    if (status == 400)
        text = "Bad Request";
    else if (status == 401)
        text = "Unauthorized";
    else if (status == 404)
        text = "Not Found";
    else if (status == 500)
        text = "Internal Server Error";

    ostringstream r;

    r << "HTTP/1.1 "
      << status
      << " "
      << text
      << "\r\n";

    r << "Content-Type: text/html; charset=utf-8\r\n";

    r << "Content-Length: "
      << body.size()
      << "\r\n";

    r << "Connection: close\r\n\r\n";

    r << body;

    return r.str();
}

// ============================================================
//  ROUTER
// ============================================================
string route(
    const string& method,
    const string& path,
    const string& rawRequest
)
{
    if (path == "/products")
        return buildProductsPage();

    if (path == "/signup")
    {
        if (method == "POST")
            return handleSignup(
                parseFormBody(rawRequest)
            );

        return signupForm();
    }

    if (path == "/login")
    {
        if (method == "POST")
            return handleLogin(
                parseFormBody(rawRequest)
            );

        return loginForm();
    }

    if (path == "/logout")
    {
        map<string,string> params =
            parseQuery(rawRequest);

        string sessionId =
            params.count("session_id")
            ? params["session_id"]
            : "";

        if (logoutSession(sessionId))
        {
            return pageWrapper(
                "Logout",
                "<div class='msg'>"
                "You have been logged out successfully."
                "</div>"
                "<p><a href='/login'>Login again</a></p>"
            );
        }

        return pageWrapper(
            "Logout",
            "<div class='err'>"
            "Invalid or already inactive session."
            "</div>"
        );
    }

    if (path == "/cart")
    {
        map<string,string> params =
            parseQuery(rawRequest);

        string sessionId =
            params.count("session_id")
            ? params["session_id"]
            : "";

        int userId =
            getSessionUserId(sessionId);

        if (userId == 0)
        {
            return pageWrapper(
                "Login Required",
                "<div class='err'>"
                "Please log in first."
                "</div>"
                "<p><a href='/login'>Login</a></p>"
            );
        }

        return buildCartPage(
            sessionId,
            userId
        );
    }

    if (path == "/add-cart")
    {
        map<string,string> params =
            parseQuery(rawRequest);

        string sessionId =
            params.count("session_id")
            ? params["session_id"]
            : "";

        int userId =
            getSessionUserId(sessionId);

        if (userId == 0)
        {
            return pageWrapper(
                "Login Required",
                "<div class='err'>"
                "Please log in first."
                "</div>"
                "<p><a href='/login'>Login</a></p>"
            );
        }

        int productId =
            getIntParameter(
                params,
                "product_id"
            );

        int quantity =
            getIntParameter(
                params,
                "quantity",
                1
            );

        if (productId <= 0)
        {
            return pageWrapper(
                "Cart",
                "<div class='err'>"
                "Invalid product ID."
                "</div>"
            );
        }

        return addCartItem(
            sessionId,
            userId,
            productId,
            quantity
        );
    }

    if (path == "/remove-cart")
    {
        map<string,string> params =
            parseQuery(rawRequest);

        string sessionId =
            params.count("session_id")
            ? params["session_id"]
            : "";

        int userId =
            getSessionUserId(sessionId);

        if (userId == 0)
        {
            return pageWrapper(
                "Login Required",
                "<div class='err'>"
                "Please log in first."
                "</div>"
                "<p><a href='/login'>Login</a></p>"
            );
        }

        int productId =
            getIntParameter(
                params,
                "product_id"
            );

        if (productId <= 0)
        {
            return pageWrapper(
                "Cart",
                "<div class='err'>"
                "Invalid product ID."
                "</div>"
            );
        }

        return removeCartItem(
            sessionId,
            userId,
            productId
        );
    }

    if (path == "/checkout")
    {
        map<string,string> params =
            parseQuery(rawRequest);

        string sessionId =
            params.count("session_id")
            ? params["session_id"]
            : "";

        int userId =
            getSessionUserId(sessionId);

        if (userId == 0)
        {
            return pageWrapper(
                "Login Required",
                "<div class='err'>"
                "Please log in first."
                "</div>"
                "<p><a href='/login'>Login</a></p>"
            );
        }

        return checkoutCart(
            sessionId,
            userId
        );
    }

    if (path == "/orders")
    {
        map<string,string> params =
            parseQuery(rawRequest);

        string sessionId =
            params.count("session_id")
            ? params["session_id"]
            : "";

        int userId =
            getSessionUserId(sessionId);

        if (userId == 0)
        {
            return pageWrapper(
                "Login Required",
                "<div class='err'>"
                "Please log in first."
                "</div>"
                "<p><a href='/login'>Login</a></p>"
            );
        }

        return buildOrdersPage(
            sessionId,
            userId
        );
    }

    if (path == "/")
    {
        string b =
            "<p>Welcome! Try "
            "<a href='/signup'>Signup</a> or "
            "<a href='/login'>Login</a>, "
            "or browse "
            "<a href='/products'>Products</a>.</p>";

        return pageWrapper(
            "Home",
            b
        );
    }

    return pageWrapper(
        "404",
        "<p>Page not found. "
        "<a href='/'>Home</a></p>"
    );
}

// ============================================================
//  WORKER
// ============================================================
void worker(int workerId)
{
    while (true)
    {
        Request req;

        WaitForSingleObject(
            requestSemaphore,
            INFINITE
        );

        {
            lock_guard<mutex> lock(queueMutex);

            req =
                requestQueue.front();

            requestQueue.pop();
        }

        char buffer[8192];

        string raw;

        // Read until the end of the header block has arrived
        size_t headerEnd =
            string::npos;

        while (
            (headerEnd =
                raw.find("\r\n\r\n"))
            == string::npos
        )
        {
            if (raw.size() > 65536)
                break;

            int n =
                recv(
                    req.clientSocket,
                    buffer,
                    sizeof(buffer),
                    0
                );

            if (n <= 0)
                break;

            raw.append(
                buffer,
                n
            );
        }

        if (headerEnd ==
            string::npos)
        {
            closesocket(
                req.clientSocket
            );

            continue;
        }

        // Keep reading until the whole body has arrived
        size_t need =
            contentLength(
                raw.substr(
                    0,
                    headerEnd
                )
            );

        while (
            raw.size() -
            (headerEnd + 4) <
            need
        )
        {
            int n =
                recv(
                    req.clientSocket,
                    buffer,
                    sizeof(buffer),
                    0
                );

            if (n <= 0)
                break;

            raw.append(
                buffer,
                n
            );
        }

        string method =
            parseMethod(raw);

        string path =
            parsePath(raw);

        cout << "[W"
             << workerId
             << "] "
             << method
             << " "
             << path
             << endl;

        string body =
            route(
                method,
                path,
                raw
            );

        int status = 200;

        if (body.find("<h1>404</h1>") !=
            string::npos)
        {
            status = 404;
        }

        string resp =
            makeHttpResponse(
                body,
                status
            );

        send(
            req.clientSocket,
            resp.c_str(),
            (int)resp.size(),
            0
        );

        closesocket(
            req.clientSocket
        );
    }
}

// ============================================================
//  MAIN
// ============================================================
int main()
{
    WSADATA wsaData;

    if (WSAStartup(
            MAKEWORD(2, 2),
            &wsaData) != 0)
    {
        cout << "Winsock init failed!"
             << endl;

        return 1;
    }

    if (!initDatabase())
    {
        WSACleanup();
        return 1;
    }

    requestSemaphore =
        CreateSemaphore(
            NULL,
            0,
            1000,
            NULL
        );

    if (!requestSemaphore)
    {
        cout << "Semaphore failed!"
             << endl;

        mysql_close(dbConn);
        WSACleanup();

        return 1;
    }

    SOCKET serverSocket =
        socket(
            AF_INET,
            SOCK_STREAM,
            IPPROTO_TCP
        );

    if (serverSocket ==
        INVALID_SOCKET)
    {
        cout << "Socket failed!"
             << endl;

        CloseHandle(
            requestSemaphore
        );

        mysql_close(dbConn);
        WSACleanup();

        return 1;
    }

    sockaddr_in addr;

    addr.sin_family =
        AF_INET;

    addr.sin_addr.s_addr =
        INADDR_ANY;

    addr.sin_port =
        htons(8080);

    if (bind(
            serverSocket,
            (sockaddr*)&addr,
            sizeof(addr)
        ) == SOCKET_ERROR)
    {
        cout << "Bind failed!"
             << endl;

        closesocket(
            serverSocket
        );

        CloseHandle(
            requestSemaphore
        );

        mysql_close(dbConn);
        WSACleanup();

        return 1;
    }

    if (listen(
            serverSocket,
            SOMAXCONN
        ) == SOCKET_ERROR)
    {
        cout << "Listen failed!"
             << endl;

        closesocket(
            serverSocket
        );

        CloseHandle(
            requestSemaphore
        );

        mysql_close(dbConn);
        WSACleanup();

        return 1;
    }

    cout << "Server on "
         << "http://localhost:8080"
         << endl;

    const int NUM_WORKERS = 4;

    vector<thread> workers;

    for (int i = 0;
         i < NUM_WORKERS;
         i++)
    {
        workers.emplace_back(
            worker,
            i + 1
        );
    }

    while (true)
    {
        SOCKET client =
            accept(
                serverSocket,
                nullptr,
                nullptr
            );

        if (client ==
            INVALID_SOCKET)
        {
            continue;
        }

        {
            lock_guard<mutex>
                lock(queueMutex);

            requestQueue.push(
                {client}
            );
        }

        ReleaseSemaphore(
            requestSemaphore,
            1,
            NULL
        );
    }

    closesocket(
        serverSocket
    );

    CloseHandle(
        requestSemaphore
    );

    mysql_close(
        dbConn
    );

    WSACleanup();

    return 0;
}
