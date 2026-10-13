#include "handlers.h"
#include "view.h"
#include "../services/user_service.h"

using namespace std;

// Admin: list of all users (read-only).
Response adminUsers(Request& req)
{
    Data page;

    for (const Row& u : listUsers())
    {
        Data item;
        item.set("id", col(u, "user_id")).set("name", col(u, "name")).set("email", col(u, "email"));
        item.set("role", col(u, "role")).set("joined", col(u, "created_at"));
        item.set("orders", col(u, "orders"));
        item.flag("isAdmin", col(u, "role") == "ADMIN");
        page.add("users", item);
    }

    return renderAdminPage(req, "Users", "admin/users.html", "users", page);
}
