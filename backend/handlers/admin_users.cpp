#include "handlers.h"
#include "view.h"
#include "../services/user_service.h"
#include <cstdlib>

using namespace std;

Response adminUsers(Request& req)
{
    Data page;

    for (const Row& u : listUsers())
    {
        Data item;
        item.set("id", col(u, "user_id")).set("name", col(u, "name")).set("email", col(u, "email"));
        item.set("role", col(u, "role")).set("joined", col(u, "created_at"));
        item.set("orders", col(u, "orders")).set("activeSessions", col(u, "active_sessions"));
        item.flag("isAdmin", col(u, "role") == "ADMIN");
        item.flag("isMe", atoi(col(u, "user_id").c_str()) == req.user.id);
        page.add("users", item);
    }

    return renderAdminPage(req, "Users", "admin/users.html", "users", page);
}

Response adminUserRole(Request& req)
{
    int id = req.intParam("id", 0);

    // Stops an admin from locking themselves (and possibly everyone) out.
    if (id == req.user.id)
        return adminMessagePage(req, "Not allowed", "You cannot change your own role.",
                                "/admin/users", "Back to users");

    string error = setUserRole(id, req.param("role"));

    if (!error.empty())
        return adminMessagePage(req, "Could not change role", error, "/admin/users", "Back to users");

    return Response::redirect("/admin/users?notice=role");
}

Response adminSessions(Request& req)
{
    Data page;

    for (const Row& s : allSessions(100))
    {
        Data item;
        item.set("sessionId", col(s, "session_id")).set("name", col(s, "name"));
        item.set("email", col(s, "email")).set("loginTime", col(s, "login_time"));
        item.set("lastActivity", col(s, "last_activity")).set("logoutTime", col(s, "logout_time"));
        item.flag("active", col(s, "is_active") == "1");
        item.flag("current", col(s, "session_id") == req.sessionId);
        page.add("sessions", item);
    }

    return renderAdminPage(req, "Sessions", "admin/sessions.html", "sessions", page);
}

// Force a login session to end (the user is signed out on their next click).
Response adminSessionEnd(Request& req)
{
    endSession(req.param("session_id"));
    return Response::redirect("/admin/sessions?notice=sessionended");
}
