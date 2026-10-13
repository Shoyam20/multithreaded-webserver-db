#include "routes.h"
#include "handlers/handlers.h"

// ============================================================
//  THE SITE MAP
//  One line per URL: which handler answers it, and who may use it.
//    r.get(...)   - opening a page or link
//    r.post(...)  - submitting a form (anything that changes data)
//    requireLogin(...)  - customers must be signed in
//    requireAdmin(...)  - only users with the ADMIN role
// ============================================================
Router buildRoutes()
{
    Router r;

    // ---------- anyone ----------
    r.get ("/",         homePage);
    r.get ("/products", productsPage);
    r.get ("/product",  productDetailPage);       // /product?id=3

    r.get ("/login",    loginPage);
    r.post("/login",    loginSubmit);
    r.get ("/signup",   signupPage);
    r.post("/signup",   signupSubmit);
    r.get ("/logout",   logout);

    // ---------- signed-in customers ----------
    r.get ("/cart",          requireLogin(cartPage));
    r.post("/cart/add",      requireLogin(cartAdd));
    r.post("/cart/update",   requireLogin(cartUpdate));
    r.post("/cart/remove",   requireLogin(cartRemove));

    r.get ("/checkout",       requireLogin(checkoutPage));
    r.post("/checkout/place", requireLogin(placeOrderSubmit));
    r.get ("/orders",         requireLogin(myOrdersPage));
    r.post("/orders/cancel",  requireLogin(cancelMyOrder));
    r.get ("/sessions",       requireLogin(mySessionsPage));

    // ---------- admin panel ----------
    r.get ("/admin",                    requireAdmin(adminDashboard));

    r.get ("/admin/products",           requireAdmin(adminProducts));
    r.get ("/admin/products/edit",      requireAdmin(adminProductForm));      // ?id=3 (or none = new)
    r.post("/admin/products/save",      requireAdmin(adminProductSave));
    r.post("/admin/products/stock",     requireAdmin(adminProductStock));
    r.post("/admin/products/delete",    requireAdmin(adminProductDelete));

    r.get ("/admin/categories",         requireAdmin(adminCategories));
    r.post("/admin/categories/save",    requireAdmin(adminCategorySave));
    r.post("/admin/categories/delete",  requireAdmin(adminCategoryDelete));

    r.get ("/admin/orders",             requireAdmin(adminOrders));
    r.get ("/admin/order",              requireAdmin(adminOrderDetail));      // ?id=7
    r.post("/admin/order/status",       requireAdmin(adminOrderStatus));
    r.post("/admin/order/payment",      requireAdmin(adminOrderPayment));

    r.get ("/admin/users",              requireAdmin(adminUsers));
    r.post("/admin/users/role",         requireAdmin(adminUserRole));
    r.get ("/admin/sessions",           requireAdmin(adminSessions));
    r.post("/admin/sessions/end",       requireAdmin(adminSessionEnd));

    r.get ("/admin/logs",               requireAdmin(adminLogs));             // ?worker=2&errors=1
    r.get ("/admin/logs/feed",          requireAdmin(adminLogsFeed));         // live refresh fragment
    r.post("/admin/logs/clear",         requireAdmin(adminLogsClear));

    r.get ("/admin/tables",             requireAdmin(adminTables));
    r.get ("/admin/table",              requireAdmin(adminTableView));        // ?name=orders&page=2

    return r;
}
