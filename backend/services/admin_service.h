#pragma once
#include "../db/database.h"

// Numbers for the admin dashboard: users, products, orders, revenue,
// pending_orders (status PLACED) and low_stock (products at or below the warning level).
Row dashboardStats();
