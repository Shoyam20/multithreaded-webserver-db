#include "http_utils.h"
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <cctype>
using namespace std;

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
    int defaultValue
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
    int status,
    const string& extraHeaders
)
{
    string text = "OK";

    if (status == 400)
        text = "Bad Request";
    else if (status == 302)
        text = "Found";
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

    if (!extraHeaders.empty())
        r << extraHeaders;

    r << "Connection: close\r\n\r\n";

    r << body;

    return r.str();
}

