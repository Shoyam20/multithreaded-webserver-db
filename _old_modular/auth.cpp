#include "auth.h"
#include "database.h"
#include "html_helpers.h"
#include "request_context.h"
#include "session.h"
#include <cstdlib>
#include <sstream>

using namespace std;

static string banner(const string& msg, bool isError)
{
    if (msg.empty())
        return "";

    return string("<div class='") + (isError ? "err" : "msg") + "'>" +
           htmlEscape(msg) + "</div>";
}

// ============================================================
//  FORMS (GET)
// ============================================================
string signupForm(const string& msg, bool isError)
{
    string form =
        banner(msg, isError) +
        "<form method='POST' action='/signup'>"
        "<div class='field'><label>Full name</label>"
        "<input name='name' placeholder='Your name' required></div>"
        "<div class='field'><label>Email</label>"
        "<input name='email' type='email' placeholder='you@example.com' required></div>"
        "<div class='field'><label>Password</label>"
        "<input name='password' type='password' placeholder='Choose a password' required></div>"
        "<button class='btn accent lg' type='submit'>Create account</button>"
        "</form>"
        "<p class='alt'>Already have an account? <a href='/login'>Sign in</a></p>";

    return pageWrapper("", authCard("Create account", "Join ShopEra in a few seconds.", form));
}

string loginForm(const string& msg, bool isError)
{
    string form =
        banner(msg, isError) +
        "<form method='POST' action='/login'>"
        "<div class='field'><label>Email</label>"
        "<input name='email' type='email' placeholder='you@example.com' required></div>"
        "<div class='field'><label>Password</label>"
        "<input name='password' type='password' placeholder='Your password' required></div>"
        "<button class='btn accent lg' type='submit'>Sign in</button>"
        "</form>"
        "<p class='alt'>New to ShopEra? <a href='/signup'>Create an account</a></p>";

    return pageWrapper("", authCard("Sign in", "Welcome back.", form));
}

// ============================================================
//  HANDLE SIGNUP POST
// ============================================================
string handleSignup(const map<string,string>& form)
{
    string name  = form.count("name")     ? form.at("name")     : "";
    string email = form.count("email")    ? form.at("email")    : "";
    string pass  = form.count("password") ? form.at("password") : "";

    if (name.empty() || email.empty() || pass.empty())
        return signupForm("All fields are required.", true);

    lock_guard<mutex> lock(dbMutex);

    // Check if email already exists
    string check =
        "SELECT user_id FROM users WHERE email='" + esc(email) + "'";

    if (mysql_query(dbConn, check.c_str()))
        return signupForm(string("DB error: ") + mysql_error(dbConn), true);

    MYSQL_RES* r = mysql_store_result(dbConn);

    if (!r)
        return signupForm("Query error.", true);

    bool exists = mysql_num_rows(r) > 0;

    mysql_free_result(r);

    if (exists)
        return signupForm("Email already registered. Try signing in.", true);

    // Insert
    string ins =
        "INSERT INTO users (name, email, password) VALUES ('" +
        esc(name) + "','" + esc(email) + "','" + esc(pass) + "')";

    if (mysql_query(dbConn, ins.c_str()))
        return signupForm(string("Insert failed: ") + mysql_error(dbConn), true);

    return loginForm("Account created! Please sign in.");
}

// ============================================================
//  HANDLE LOGIN POST
// ============================================================
string handleLogin(const map<string,string>& form)
{
    string email = form.count("email")    ? form.at("email")    : "";
    string pass  = form.count("password") ? form.at("password") : "";

    if (email.empty() || pass.empty())
        return loginForm("Both fields are required.", true);

    string dbName;
    string dbPass;
    int userId = 0;

    {
        lock_guard<mutex> lock(dbMutex);

        string q =
            "SELECT user_id, name, password FROM users WHERE email='" +
            esc(email) + "'";

        if (mysql_query(dbConn, q.c_str()))
            return loginForm(string("DB error: ") + mysql_error(dbConn), true);

        MYSQL_RES* r = mysql_store_result(dbConn);

        if (!r)
            return loginForm("Query error", true);

        MYSQL_ROW row = mysql_fetch_row(r);

        if (!row)
        {
            mysql_free_result(r);
            return loginForm("No account found with that email.", true);
        }

        userId = atoi(row[0]);
        dbName = row[1] ? row[1] : "";
        dbPass = row[2] ? row[2] : "";

        mysql_free_result(r);
    }

    if (dbPass != pass)
        return loginForm("Incorrect password.", true);

    string sessionId;

    if (!createSession(userId, sessionId))
        return loginForm("Login successful, but session creation failed.", true);

    // The browser keeps the session as a cookie; then we bounce to the
    // home page so a refresh does not re-submit the login form.
    tlSessionId = sessionId;
    tlUserName  = dbName;

    tlExtraHeaders =
        "Set-Cookie: session_id=" + sessionId +
        "; Path=/; HttpOnly; SameSite=Lax\r\n";

    tlRedirect = "/?notice=login";

    return "";
}
