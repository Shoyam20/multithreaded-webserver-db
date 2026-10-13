#include "handlers.h"
#include "view.h"
#include "../services/user_service.h"

using namespace std;

// Admin: the list of all users (read-only).
Response adminUsers(Request& req)
{
    if (!req.user.isAdmin()) return Response::redirect(req.user.loggedIn() ? "/?notice=forbidden" : "/login?notice=needlogin");

    string rows;
    Rows users = listUsers();

    for (size_t i = 0; i < users.size(); i++)
    {
        Vars row;
        row["id"]        = col(users[i], "user_id");
        row["name"]      = col(users[i], "name");
        row["email"]     = col(users[i], "email");
        row["role"]      = col(users[i], "role");
        row["roleClass"] = col(users[i], "role") == "ADMIN" ? "green" : "";
        row["joined"]    = col(users[i], "created_at");
        row["orders"]    = col(users[i], "orders");
        rows += renderTemplate("partials/admin_user_row.html", row);
    }

    Vars v;
    v["rows"] = rows;
    return renderAdminPage(req, "Users", "admin/users.html", "users", v);
}
