#include "app.h"
#include "config.h"
#include "routes.h"
#include "handlers/view.h"
#include "services/cart_service.h"
#include "services/user_service.h"
#include <fstream>
#include <sstream>

using namespace std;

// Routes are built once at startup and shared by all threads.
static const Router router = buildRoutes();

// ------------------------------------------------------------
//  Static files: /static/css/style.css -> frontend/static/css/style.css
// ------------------------------------------------------------
static string contentTypeFor(const string& path)
{
    auto endsWith = [&](const string& ext)
    {
        return path.size() >= ext.size() && path.compare(path.size() - ext.size(), ext.size(), ext) == 0;
    };

    if (endsWith(".css"))  return "text/css; charset=utf-8";
    if (endsWith(".js"))   return "application/javascript; charset=utf-8";
    if (endsWith(".svg"))  return "image/svg+xml";
    if (endsWith(".png"))  return "image/png";
    if (endsWith(".jpg") || endsWith(".jpeg")) return "image/jpeg";
    if (endsWith(".ico"))  return "image/x-icon";
    if (endsWith(".woff2")) return "font/woff2";
    return "application/octet-stream";
}

static bool serveStatic(const Request& req, Response& res)
{
    if (req.path.compare(0, 8, "/static/") != 0)
        return false;

    // Never allow "../" to escape the static folder.
    if (req.path.find("..") != string::npos || req.path.find('\\') != string::npos)
    {
        res = Response::html("Forbidden", 403);
        return true;
    }

    ifstream in(config::frontendDir + req.path, ios::binary);

    if (!in)
    {
        res = Response::html("Not found", 404);
        return true;
    }

    stringstream data;
    data << in.rdbuf();

    res.body = data.str();
    res.contentType = contentTypeFor(req.path);
    res.headers.push_back("Cache-Control: no-cache");
    return true;
}

// ------------------------------------------------------------
//  Who is making this request?
// ------------------------------------------------------------
static void identifyUser(Request& req)
{
    auto cookie = req.cookies.find("session_id");
    if (cookie == req.cookies.end())
        return;

    req.sessionId = cookie->second;

    if (userForSession(req.sessionId, req.user))
        req.cartCount = cartItemCount(req.user.id);
}

Response handleRequest(Request& req)
{
    Response res;

    if (serveStatic(req, res))
        return res;

    identifyUser(req);

    if (!router.dispatch(req, res))
        return notFoundPage(req);

    return res;
}
