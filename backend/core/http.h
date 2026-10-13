#pragma once
#include <map>
#include <string>
#include <vector>

using StrMap = std::map<std::string, std::string>;

// The person making the request (filled in by app.cpp from the session cookie).
struct User
{
    int         id = 0;
    std::string name;
    std::string email;
    std::string role;                       // "CUSTOMER" or "ADMIN"

    bool loggedIn() const { return id > 0; }
    bool isAdmin()  const { return id > 0 && role == "ADMIN"; }
};

// An incoming HTTP request, already parsed.
struct Request
{
    std::string method;                     // "GET", "POST"
    std::string path;                       // "/products"
    std::string target;                     // path + query string exactly as sent: "/products?category=3"
    StrMap      query;                      // ?a=1&b=2
    StrMap      form;                       // POST body fields
    StrMap      cookies;

    std::string sessionId;                  // from the session_id cookie
    User        user;                       // empty (id 0) when logged out
    int         cartCount = 0;              // items in cart, for the header badge

    // Value of a form field, or of a query-string field if not in the form.
    std::string param(const std::string& key, const std::string& fallback = "") const;
    int         intParam(const std::string& key, int fallback = 0) const;
};

// What a handler returns.
struct Response
{
    int         status = 200;
    std::string contentType = "text/html; charset=utf-8";
    std::string body;
    std::vector<std::string> headers;       // extra header lines (no \r\n)

    static Response html(const std::string& body, int status = 200);
    static Response redirect(const std::string& url);

    void setCookie(const std::string& name, const std::string& value);
    void clearCookie(const std::string& name);
};

std::string urlDecode(const std::string& s);
std::string urlEncode(const std::string& s);

// Parse raw bytes read from the socket. Returns false if malformed.
bool parseRequest(const std::string& raw, Request& out);

// Turn a Response into the bytes sent back to the browser.
std::string buildHttpResponse(const Response& res);

// Reads the Content-Length header value out of a header block (0 if absent).
size_t contentLengthOf(const std::string& headerBlock);
