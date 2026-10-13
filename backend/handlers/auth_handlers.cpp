#include "handlers.h"
#include "../services/services.h"

using namespace std;

Response loginPage(Request& req)
{
    return renderPage(req, "Sign in", "pages/login.html");
}

// POST /login: check the password, start a session, send the session id to the browser as a cookie.
Response loginSubmit(Request& req)
{
    User user;
    string error = logIn(req.param("email"), req.param("password"), user);

    if (error.empty())
    {
        string sessionId = startSession(user.id);

        if (sessionId.empty())
            error = "Signed in, but could not start a session. Please try again.";
        else
        {
            // Admins go to their panel, customers to the home page.
            Response res = Response::redirect(user.role == "ADMIN" ? "/admin" : "/?notice=login");
            res.setCookie("session_id", sessionId);
            return res;
        }
    }

    Vars v;
    v["error"] = alertBox(error);
    v["email"] = req.param("email");
    return renderPage(req, "Sign in", "pages/login.html", v);
}

Response signupPage(Request& req)
{
    return renderPage(req, "Create account", "pages/signup.html");
}

Response signupSubmit(Request& req)
{
    string error = signUp(req.param("name"), req.param("email"), req.param("password"));

    if (error.empty())
        return Response::redirect("/login?notice=signup");

    Vars v;
    v["error"] = alertBox(error);
    v["name"]  = req.param("name");
    v["email"] = req.param("email");
    return renderPage(req, "Create account", "pages/signup.html", v);
}

// Logout: mark the session inactive and tell the browser to forget the cookie.
Response logout(Request& req)
{
    endSession(req.sessionId);

    Response res = Response::redirect("/?notice=logout");
    res.clearCookie("session_id");
    return res;
}
