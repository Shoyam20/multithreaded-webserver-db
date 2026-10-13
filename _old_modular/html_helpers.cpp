#include "html_helpers.h"
#include "catalog.h"
#include "request_context.h"
#include "theme.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>

using namespace std;

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

string rupees(double amount)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "%.2f", fabs(amount));

    string num = buf;
    string whole = num.substr(0, num.find('.'));
    string frac = num.substr(num.find('.') + 1);

    // Indian grouping: last three digits, then groups of two.
    string grouped;
    int n = (int)whole.size();

    for (int i = 0; i < n; i++)
    {
        int fromEnd = n - i;

        if (i > 0 && (fromEnd == 3 || (fromEnd > 3 && (fromEnd - 3) % 2 == 0)))
            grouped += ',';

        grouped += whole[i];
    }

    string out = string(amount < 0 ? "-" : "") + "₹" + grouped;

    if (frac != "00")
        out += "." + frac;

    return out;
}

string rupees(const string& decimalText)
{
    return rupees(atof(decimalText.c_str()));
}

// Flash messages are chosen by code (?notice=...) so a crafted link can
// never put arbitrary text on the page.
static string noticeText(const string& code)
{
    if (code == "login")   return "✅ Welcome back! You are signed in.";
    if (code == "signup")  return "🎉 Account created. Please sign in.";
    if (code == "logout")  return "👋 You have been signed out.";
    if (code == "added")   return "🛒 Item added to your cart.";
    if (code == "removed") return "🗑️ Item removed from your cart.";
    if (code == "updated") return "✅ Cart updated.";
    if (code == "ordered") return "📦 Order placed successfully!";
    return "";
}

static void appendHeader(ostringstream& h)
{
    h << "<header class='topbar'><div class='wrap row'>"
         "<a class='logo' href='/'>Shop<span>Era</span></a>"

         "<form class='search' action='/products' method='GET'>"
         "<select name='category' aria-label='Category'>"
         "<option value=''>All</option>";

    for (const Category& c : categories())
        h << "<option value='" << c.id << "'>" << htmlEscape(c.name) << "</option>";

    h << "</select>"
         "<input name='q' placeholder='Search products' value='"
      << htmlEscape(tlSearch)
      << "' aria-label='Search'>"
         "<button type='submit' aria-label='Search'>🔍</button>"
         "</form>"

         "<nav class='hnav'>";

    if (isLoggedIn())
    {
        h << "<div class='dd'>"
             "<button class='hlink' type='button'><small>Hello, "
          << htmlEscape(tlUserName)
          << "</small><b>Account ▾</b></button>"
             "<div class='ddmenu'>"
             "<a href='/orders'>📦 My Orders</a>"
             "<a href='/cart'>🛒 My Cart</a>"
             "<a href='/session'>🔐 My Sessions</a>"
             "<a href='/logout'>🚪 Sign out</a>"
             "</div></div>"
             "<a class='hlink' href='/orders'><small>Returns &amp;</small><b>Orders</b></a>";
    }
    else
    {
        h << "<button class='hlink' type='button' onclick=\"openModal('loginModal')\">"
             "<small>Hello, guest</small><b>Sign in</b></button>"
             "<button class='hlink' type='button' onclick=\"openModal('signupModal')\">"
             "<small>New here?</small><b>Sign up</b></button>";
    }

    h << "<a class='hlink cartlink' href='/cart'><span class='ico'>🛒</span>"
         "<span class='badge'>" << tlCartCount << "</span><b>Cart</b></a>"
         "</nav></div>"

         "<div class='catnav'><div class='wrap'>"
         "<a class='hot' href='/products'>☰ All Products</a>"
         "<a href='/products?sort=price_asc&max=1000'>🔥 Under ₹1,000</a>"
         "<a href='/products?sort=newest'>✨ New</a>";

    for (const Category& c : categories())
        h << "<a href='/products?category=" << c.id << "'>"
          << categoryIcon(c.name) << " " << htmlEscape(c.name) << "</a>";

    h << "<a href='/database'>📊 Dashboard</a>"
         "</div></div></header>";
}

static void appendGuestModals(ostringstream& h)
{
    h << "<div class='modal' id='loginModal'><div class='modal-box'>"
         "<button class='modal-close' onclick=\"closeModal('loginModal')\">&times;</button>"
         "<h2>Sign in</h2>"
         "<form method='POST' action='/login'>"
         "<input name='email' type='email' placeholder='Email' required>"
         "<input name='password' type='password' placeholder='Password' required>"
         "<button class='btn accent' type='submit'>Sign in</button>"
         "</form>"
         "<p class='alt'>New to ShopEra? <a href='#' "
         "onclick=\"closeModal('loginModal');openModal('signupModal');return false;\">"
         "Create an account</a></p></div></div>";

    h << "<div class='modal' id='signupModal'><div class='modal-box'>"
         "<button class='modal-close' onclick=\"closeModal('signupModal')\">&times;</button>"
         "<h2>Create account</h2>"
         "<form method='POST' action='/signup'>"
         "<input name='name' placeholder='Full name' required>"
         "<input name='email' type='email' placeholder='Email' required>"
         "<input name='password' type='password' placeholder='Password' required>"
         "<button class='btn accent' type='submit'>Create account</button>"
         "</form>"
         "<p class='alt'>Already a customer? <a href='#' "
         "onclick=\"closeModal('signupModal');openModal('loginModal');return false;\">"
         "Sign in</a></p></div></div>";
}

static void appendFooter(ostringstream& h)
{
    h << "<footer><div class='wrap fcols'>"
         "<div><h4>Shop</h4><a href='/products'>All products</a>"
         "<a href='/products?sort=newest'>New arrivals</a>"
         "<a href='/products?sort=price_asc&max=1000'>Budget picks</a></div>"
         "<div><h4>Your account</h4><a href='/orders'>Orders</a>"
         "<a href='/cart'>Cart</a><a href='/session'>Sessions</a></div>"
         "<div><h4>Payments</h4><a>Cash on Delivery</a></div>"
         "<div><h4>Project</h4><a href='/database'>Database dashboard</a>"
         "<a>Multithreaded C++ server + MySQL</a></div>"
         "</div><div class='fbot'>ShopEra — a Graphic Era DBMS/OS course demo store. "
         "Not a real shop; no real payments are processed.</div></footer>";
}

static const char* pageScript =
    "<script>"
    "function openModal(id){document.getElementById(id).classList.add('open');}"
    "function closeModal(id){document.getElementById(id).classList.remove('open');}"
    "document.addEventListener('click',function(e){"
    "if(e.target.classList&&e.target.classList.contains('modal'))e.target.classList.remove('open');});"
    "document.addEventListener('keydown',function(e){if(e.key==='Escape'){"
    "document.querySelectorAll('.modal.open').forEach(function(m){m.classList.remove('open');});}});"
    "function stepQty(btn,d){var i=btn.parentNode.querySelector('input');"
    "var v=parseInt(i.value||'1',10)+d,mn=parseInt(i.min||'1',10),"
    "mx=parseInt(i.max||'99',10);if(v<mn)v=mn;if(v>mx)v=mx;i.value=v;}"
    "</script>";

string pageWrapper(const string& title, const string& body)
{
    ostringstream h;

    h << "<!DOCTYPE html><html lang='en'><head><meta charset='utf-8'>"
         "<meta name='viewport' content='width=device-width,initial-scale=1'>"
         "<title>" << htmlEscape(title.empty() ? "ShopEra" : title + " — ShopEra")
      << "</title><style>"
      << siteCssPart1() << siteCssPart2()
      << "</style></head><body>";

    appendHeader(h);

    h << "<main class='wrap'>";

    string notice = noticeText(tlNotice);

    if (!notice.empty())
        h << "<div class='flash'>" << notice << "</div>";

    if (!title.empty())
        h << "<h1 class='page-title'>" << title << "</h1>";

    h << body << "</main>";

    appendFooter(h);

    if (!isLoggedIn())
        appendGuestModals(h);

    h << pageScript << "</body></html>";

    return h.str();
}

string authCard(const string& heading, const string& sub, const string& inner)
{
    return "<div class='authwrap'><div class='authcard'><h1>" + heading +
           "</h1><p class='sub'>" + sub + "</p>" + inner + "</div></div>";
}
