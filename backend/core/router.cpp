#include "router.h"

using namespace std;

bool Router::dispatch(Request& req, Response& out) const
{
    // A HEAD request is answered like GET.
    string method = (req.method == "HEAD") ? "GET" : req.method;

    auto it = routes_.find(method + " " + req.path);
    if (it == routes_.end())
        return false;

    out = it->second(req);
    return true;
}

Handler requireLogin(Handler next)
{
    return [next](Request& req)
    {
        if (!req.user.loggedIn())
            return Response::redirect("/login?notice=needlogin");

        return next(req);
    };
}

Handler requireAdmin(Handler next)
{
    return [next](Request& req)
    {
        if (!req.user.loggedIn())
            return Response::redirect("/login?notice=needlogin");

        if (!req.user.isAdmin())
            return Response::redirect("/?notice=forbidden");

        return next(req);
    };
}
