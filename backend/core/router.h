#pragma once
#include "http.h"
#include <functional>
#include <map>
#include <string>

// A handler turns a Request into a Response.
using Handler = std::function<Response(Request&)>;

// Maps "METHOD /path" to a handler.
class Router
{
public:
    void get(const std::string& path, Handler h)  { routes_["GET "  + path] = h; }
    void post(const std::string& path, Handler h) { routes_["POST " + path] = h; }

    // Runs the matching handler. Returns false if no route matches.
    bool dispatch(Request& req, Response& out) const;

private:
    std::map<std::string, Handler> routes_;
};

// Wrap a handler so only signed-in users / admins can reach it.
Handler requireLogin(Handler next);
Handler requireAdmin(Handler next);
