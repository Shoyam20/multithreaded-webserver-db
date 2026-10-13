#include "handlers.h"
#include "view.h"
#include "../services/user_service.h"

using namespace std;

Response loginPage(Request& req)
{
    return renderPage(req, "Sign in", "pages/login.html");
}

Response loginSubmit(Request& req)
{
    User user;
    string error = logIn(req.param("email"), req.param("password"), user);

    if (!error.empty())
    {
        Data page;
        page.set("error", error).set("email", req.param("email"));
        return renderPage(req, "Sign in", "pages/login.html", page);
    }

    string sessionId = startSession(user.id);

    if (sessionId.empty())
    {
        Data page;
        page.set("error", "Signed in, but could not start a session. Please try again.");
        return renderPage(req, "Sign in", "pages/login.html", page);
    }

    // Admins land on their panel, customers on the home page.
    Response res = Response::redirect(user.role == "ADMIN" ? "/admin" : "/?notice=login");
    res.setCookie("session_id", sessionId);
    return res;
}

Response signupPage(Request& req)
{
    return renderPage(req, "Create account", "pages/signup.html");
}

Response signupSubmit(Request& req)
{
    string error = signUp(req.param("name"), req.param("email"), req.param("password"));

    if (!error.empty())
    {
        Data page;
        page.set("error", error).set("name", req.param("name")).set("email", req.param("email"));
        return renderPage(req, "Create account", "pages/signup.html", page);
    }

    return Response::redirect("/login?notice=signup");
}

Response logout(Request& req)
{
    endSession(req.sessionId);

    Response res = Response::redirect("/?notice=logout");
    res.clearCookie("session_id");
    return res;
}
