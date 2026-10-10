USE ecommerce_db;

INSERT INTO categories (category_name) VALUES
('Electronics'),('Accessories'),('Audio'),('Mobile Phones'),('Laptops'),
('Computers'),('Cameras'),('Gaming'),('Clothing'),('Shoes'),('Books'),
('Home & Kitchen'),('Beauty & Personal Care'),('Sports & Fitness'),('Toys & Games')
ON DUPLICATE KEY UPDATE category_name = VALUES(category_name);

SELECT 'users' AS table_name, COUNT(*) AS records FROM users
UNION ALL SELECT 'user_sessions', COUNT(*) FROM user_sessions
UNION ALL SELECT 'categories', COUNT(*) FROM categories
UNION ALL SELECT 'products', COUNT(*) FROM products
UNION ALL SELECT 'cart', COUNT(*) FROM cart
UNION ALL SELECT 'cart_items', COUNT(*) FROM cart_items
UNION ALL SELECT 'orders', COUNT(*) FROM orders
UNION ALL SELECT 'order_items', COUNT(*) FROM order_items
UNION ALL SELECT 'payments', COUNT(*) FROM payments;

-- Full order journey inspection query.
SELECT
    u.user_id, u.name AS customer,
    c.cart_id, c.status AS cart_status,
    ci.product_id AS cart_product_id, ci.quantity AS cart_quantity,
    o.order_id, o.total, o.status AS order_status,
    oi.product_id AS ordered_product_id, oi.quantity AS ordered_quantity,
    oi.price AS price_at_purchase,
    p.payment_id, p.method, p.status AS payment_status, p.amount
FROM users u
LEFT JOIN cart c ON c.user_id = u.user_id
LEFT JOIN cart_items ci ON ci.cart_id = c.cart_id
LEFT JOIN orders o ON o.cart_id = c.cart_id
LEFT JOIN order_items oi ON oi.order_id = o.order_id
LEFT JOIN payments p ON p.order_id = o.order_id
ORDER BY u.user_id, c.cart_id, o.order_id;

-- Product/category relationship.
SELECT p.product_id, p.name, p.price, p.stock, c.category_name
FROM products p
JOIN categories c ON c.category_id = p.category_id
ORDER BY p.product_id;
