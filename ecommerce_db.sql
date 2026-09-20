CREATE DATABASE IF NOT EXISTS ecommerce_db;
USE ecommerce_db;

-- 1. USERS
--Stores information about registered customers.
CREATE TABLE IF NOT EXISTS users (
    user_id    INT AUTO_INCREMENT PRIMARY KEY,
    email      VARCHAR(100) NOT NULL UNIQUE,
    name       VARCHAR(100) NOT NULL,
    password   VARCHAR(255) NOT NULL,    
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 2. CATEGORIES
-- Stores different product categories.
-- Example: Electronics, Audio, Accessories
CREATE TABLE IF NOT EXISTS categories (
    category_id INT AUTO_INCREMENT PRIMARY KEY,
    category_name VARCHAR(50) NOT NULL UNIQUE
);

-- INSERT CATEGORIES
INSERT INTO categories (category_name)
VALUES
('Electronics'),
('Accessories'),
('Audio'),
('Mobile Phones'),
('Laptops'),
('Computers'),
('Cameras'),
('Gaming'),
('Clothing'),
('Shoes'),
('Books'),
('Home & Kitchen'),
('Beauty & Personal Care'),
('Sports & Fitness'),
('Toys & Games');

-- 3. PRODUCTS 
-- Stores information about products available in the store. 
CREATE TABLE IF NOT EXISTS products (
    product_id   INT AUTO_INCREMENT PRIMARY KEY,
    name         VARCHAR(100)   NOT NULL,
    description  VARCHAR(255),
    price        DECIMAL(10,2)  NOT NULL,
    stock        INT            NOT NULL DEFAULT 0,
    category_id INT NOT NULL,
    FOREIGN KEY (category_id) REFERENCES categories(category_id)
);

-- SAMPLE PRODUCTS
INSERT INTO products
(name, description, price, stock, category_id)
VALUES
('Wireless Mouse','Ergonomic 2.4GHz optical mouse',599.00, 45, 1),
('Laptop Stand','Aluminium adjustable laptop stand',899.00, 50, 2),
('Bluetooth Speaker','Portable speaker with 12-hour battery',1899.00, 15, 3),
('Smartphone X1','6.5 inch display with 128GB storage',24999.00, 25, 4),
('Gaming Laptop','15.6 inch laptop with dedicated graphics',74999.00, 10, 5),
('Desktop PC','Core i5 desktop computer',52999.00, 12, 6),
('DSLR Camera','24MP DSLR camera with 18-55mm lens',54999.00, 8, 7),
('Gaming Mouse','High precision RGB gaming mouse',1499.00, 35, 8),
('Women Casual Kurti','Printed cotton casual kurti',999.00, 45, 9),
('Casual Sneakers','Comfortable everyday sneakers',2499.00, 25, 10),
('Database Management Systems','Fundamentals of DBMS and SQL',799.00, 30, 11),
('Electric Kettle','1.5 litre stainless steel electric kettle',1299.00, 25, 12),
('Face Wash','Gentle face cleanser for daily use',299.00, 50, 13),
('Yoga Mat','Anti-slip exercise and yoga mat',799.00, 40, 14),
('Building Blocks','Creative building block set for children',899.00, 25, 15);

-- 4. CART
-- Stores shopping carts belonging to users.
-- A user can have multiple carts over time.
-- However, the application should allow only ONE cart with status = 'ACTIVE' for a user at a time.
-- Example:
-- Cart 1 -> CHECKED_OUT
-- Cart 2 -> CHECKED_OUT
-- Cart 3 -> ACTIVE
CREATE TABLE IF NOT EXISTS cart (
    cart_id  INT AUTO_INCREMENT PRIMARY KEY,
    user_id INT NOT NULL,
    status VARCHAR(20) NOT NULL DEFAULT 'ACTIVE',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES users(user_id)
);

-- 5. CART ITEMS
-- Stores products added to a shopping cart.
-- One cart can contain many products.
CREATE TABLE IF NOT EXISTS cart_items (
    cart_item_id INT AUTO_INCREMENT PRIMARY KEY,
    cart_id      INT NOT NULL,
    product_id   INT NOT NULL,
    quantity     INT NOT NULL DEFAULT 1,
    FOREIGN KEY (cart_id)    REFERENCES cart(cart_id),
    FOREIGN KEY (product_id) REFERENCES products(product_id),
    -- Same product should appear only once
    -- in a particular cart.
    -- Quantity should be updated instead.
    UNIQUE(cart_id, product_id)
);

--- 6. ORDERS
-- Stores orders created after checkout.
-- A cart is converted into an order during checkout.
-- The cart_id keeps track of which cart created the order.
CREATE TABLE IF NOT EXISTS orders (
    order_id    INT AUTO_INCREMENT PRIMARY KEY,
    user_id     INT NOT NULL,
    cart_id     INT NOT NULL,
    total       DECIMAL(10,2) NOT NULL,
    status      VARCHAR(20) DEFAULT 'PENDING',
    created_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES users(user_id),
    FOREIGN KEY (cart_id) REFERENCES cart(cart_id),
    UNIQUE(cart_id)
);

-- 7. ORDER ITEMS
-- Stores products that were purchased.
CREATE TABLE IF NOT EXISTS order_items (
    order_item_id INT AUTO_INCREMENT PRIMARY KEY,
    order_id      INT NOT NULL,
    product_id    INT NOT NULL,
    quantity      INT NOT NULL,
    price         DECIMAL(10,2) NOT NULL,
    FOREIGN KEY (order_id)   REFERENCES orders(order_id),
    FOREIGN KEY (product_id) REFERENCES products(product_id),
    UNIQUE(order_id, product_id)
);

-- 8. PAYMENTS
-- Stores payment information for orders.
CREATE TABLE IF NOT EXISTS payments (
    payment_id INT AUTO_INCREMENT PRIMARY KEY,
    order_id   INT NOT NULL,
    amount     DECIMAL(10,2) NOT NULL,
    method     VARCHAR(30) DEFAULT 'COD',
    status     VARCHAR(20) DEFAULT 'PENDING',
    paid_at    TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (order_id) REFERENCES orders(order_id),
    UNIQUE(order_id)
);
