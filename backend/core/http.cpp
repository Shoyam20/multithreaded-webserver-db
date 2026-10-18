// Parses raw HTTP request text and builds the response text.
#include "http.h"
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <sstream>

using namespace std;

// ------------------------------------------------------------
//  Request / Response helpers
// ------------------------------------------------------------
string Request::param(const string& key, const string& fallback) const
{
    auto f = form.find(key);
    if (f != form.end()) return f->second;

    auto q = query.find(key);
    if (q != query.end()) return q->second;

    return fallback;
}

int Request::intParam(const string& key, int fallback) const
{
    string v = param(key);
    if (v.empty()) return fallback;

    char* end = nullptr;
    long n = strtol(v.c_str(), &end, 10);

    return (end && *end == '\0') ? (int)n : fallback;
}

Response Response::html(const string& body, int status)
{
    Response r;
    r.body = body;
    r.status = status;
    return r;
}

Response Response::redirect(const string& url)
{
    Response r;
    r.status = 302;
    r.headers.push_back("Location: " + url);
    return r;
}

void Response::setCookie(const string& name, const string& value)
{
    headers.push_back("Set-Cookie: " + name + "=" + value +
                      "; Path=/; HttpOnly; SameSite=Lax");
}

void Response::clearCookie(const string& name)
{
    headers.push_back("Set-Cookie: " + name +
                      "=; Path=/; HttpOnly; SameSite=Lax; "
                      "Expires=Thu, 01 Jan 1970 00:00:00 GMT");
}

// ------------------------------------------------------------
//  URL encoding
// ------------------------------------------------------------
string urlDecode(const string& s)
{
    string out;

    for (size_t i = 0; i < s.size(); i++)
    {
        if (s[i] == '+')
        {
            out += ' ';
        }
        else if (s[i] == '%' && i + 2 < s.size() + 0 && isxdigit((unsigned char)s[i + 1]) &&
                 isxdigit((unsigned char)s[i + 2]))
        {
            out += (char)strtol(s.substr(i + 1, 2).c_str(), nullptr, 16);
            i += 2;
        }
        else
        {
            out += s[i];
        }
    }

    return out;
}

string urlEncode(const string& s)
{
    string out;

    for (unsigned char c : s)
    {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
        {
            out += (char)c;
        }
        else
        {
            char buf[4];
            snprintf(buf, sizeof(buf), "%%%02X", c);
            out += buf;
        }
    }

    return out;
}

// ------------------------------------------------------------
//  Parsing
// ------------------------------------------------------------
static StrMap parsePairs(const string& text)
{
    StrMap out;
    istringstream in(text);
    string pair;

    while (getline(in, pair, '&'))
    {
        size_t eq = pair.find('=');
        if (eq == string::npos) continue;

        out[urlDecode(pair.substr(0, eq))] = urlDecode(pair.substr(eq + 1));
    }

    return out;
}

static string lowercase(string s)
{
    for (char& c : s) c = (char)tolower((unsigned char)c);
    return s;
}

// Value of one header line, e.g. headerValue(block, "cookie").
static string headerValue(const string& headerBlock, const string& name)
{
    string lower = lowercase(headerBlock);
    size_t p = lower.find("\r\n" + name + ":");
    if (p == string::npos) return "";

    size_t start = p + 2 + name.size() + 1;
    size_t end = headerBlock.find("\r\n", start);
    if (end == string::npos) end = headerBlock.size();

    string v = headerBlock.substr(start, end - start);
    size_t first = v.find_first_not_of(" \t");

    return first == string::npos ? "" : v.substr(first);
}

size_t contentLengthOf(const string& headerBlock)
{
    string v = headerValue(headerBlock, "content-length");
    return v.empty() ? 0 : (size_t)strtoul(v.c_str(), nullptr, 10);
}

bool parseRequest(const string& raw, Request& req)
{
    size_t headerEnd = raw.find("\r\n\r\n");
    if (headerEnd == string::npos) return false;

    string headerBlock = raw.substr(0, headerEnd);
    string body = raw.substr(headerEnd + 4);

    // Request line: METHOD /path?query HTTP/1.1
    size_t lineEnd = headerBlock.find("\r\n");
    istringstream line(headerBlock.substr(0, lineEnd));
    string target, version;
    line >> req.method >> target >> version;

    if (req.method.empty() || target.empty()) return false;

    size_t q = target.find('?');
    req.target = target;
    req.path = urlDecode(q == string::npos ? target : target.substr(0, q));
    if (q != string::npos) req.query = parsePairs(target.substr(q + 1));

    req.form = parsePairs(body);

    // Cookie: a=1; b=2
    string cookieLine = headerValue(headerBlock, "cookie");
    size_t pos = 0;

    while (pos < cookieLine.size())
    {
        size_t semi = cookieLine.find(';', pos);
        if (semi == string::npos) semi = cookieLine.size();

        string pair = cookieLine.substr(pos, semi - pos);
        size_t start = pair.find_first_not_of(" \t");
        size_t eq = pair.find('=');

        if (start != string::npos && eq != string::npos && eq > start)
            req.cookies[pair.substr(start, eq - start)] = pair.substr(eq + 1);

        pos = semi + 1;
    }

    return true;
}

// ------------------------------------------------------------
//  Building the response
// ------------------------------------------------------------
static const char* statusText(int status)
{
    switch (status)
    {
        case 200: return "OK";
        case 302: return "Found";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 500: return "Internal Server Error";
        default:  return "OK";
    }
}

string buildHttpResponse(const Response& res)
{
    ostringstream out;

    out << "HTTP/1.1 " << res.status << " " << statusText(res.status) << "\r\n"
        << "Content-Type: " << res.contentType << "\r\n"
        << "Content-Length: " << res.body.size() << "\r\n"
        << "X-Content-Type-Options: nosniff\r\n";

    for (const string& h : res.headers)
        out << h << "\r\n";

    out << "Connection: close\r\n\r\n" << res.body;

    return out.str();
}
