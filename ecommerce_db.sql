CREATE DATABASE IF NOT EXISTS ecommerce_db;
USE ecommerce_db;

-- 1. USERS
-- Stores information about registered customers.
CREATE TABLE IF NOT EXISTS users (
    user_id    INT AUTO_INCREMENT PRIMARY KEY,
    email      VARCHAR(100) NOT NULL UNIQUE,
    name       VARCHAR(100) NOT NULL,
    password   VARCHAR(255) NOT NULL,    
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 2. USER SESSIONS
-- Stores every login session of every user.
-- One user can have multiple sessions.
-- Example:
-- Radhika -> Chrome  -> Session A123
-- Radhika -> Edge    -> Session B456
-- Shreya -> Chrome -> Session C789

CREATE TABLE IF NOT EXISTS user_sessions (
    session_id VARCHAR(64) PRIMARY KEY,
    user_id INT NOT NULL,
    login_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    logout_time TIMESTAMP NULL,
    last_activity TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    is_active BOOLEAN NOT NULL DEFAULT TRUE,
    FOREIGN KEY (user_id) REFERENCES users(user_id)
);

-- Indexes for faster session lookup
CREATE INDEX idx_sessions_user
ON user_sessions(user_id);

CREATE INDEX idx_sessions_active
ON user_sessions(is_active);

CREATE INDEX idx_sessions_login
ON user_sessions(login_time);

-- 3. CATEGORIES
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

-- 4. PRODUCTS 
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
INSERT INTO products(name, description, price, stock, category_id)
VALUES
-- 1. Electronics (category_id = 1)
('Smart LED Bulb', 'WiFi-enabled LED bulb with app control', 499.00, 40, 1),
('Portable SSD', '500GB portable solid state drive', 3999.00, 15, 1),
('Power Bank 20000mAh', 'Fast charging power bank with USB-C', 1799.00, 30, 1),
('Smart Plug', 'WiFi smart plug with energy monitoring', 799.00, 25, 1),

-- 2. Accessories (category_id = 2)
('USB-C Fast Charger', '30W fast charging wall adapter', 899.00, 45, 2),
('Magnetic Phone Holder', 'Adjustable magnetic mobile holder', 499.00, 35, 2),
('Laptop Backpack', 'Water-resistant backpack for laptops', 1499.00, 20, 2),
('Wireless Charging Pad', 'Qi-compatible wireless charging pad', 999.00, 25, 2),

-- 3. Audio (category_id = 3)
('TWS Wireless Earbuds', 'Bluetooth earbuds with charging case', 1499.00, 40, 3),
('Bluetooth Party Speaker', 'Portable wireless speaker with deep bass', 2499.00, 18, 3),
('ANC Headphones', 'Wireless headphones with noise cancellation', 3499.00, 15, 3),
('Neckband Earphones', 'Wireless neckband with long battery life', 999.00, 30, 3),

-- 4. Mobile Phones (category_id = 4)
('Smartphone X3', '128GB smartphone with high-resolution camera', 18999.00, 12, 4),
('5G Smartphone Y5', '5G smartphone with 256GB storage', 24999.00, 10, 4),
('Clear Phone Case', 'Transparent shockproof phone case', 299.00, 60, 4),
('Privacy Screen Protector', 'Privacy glass screen protector', 349.00, 50, 4),

-- 5. Laptops (category_id = 5)
('Student Laptop Pro', 'Laptop for study and everyday productivity', 45999.00, 8, 5),
('Laptop Cooling Stand', 'Adjustable laptop stand with cooling fan', 1299.00, 20, 5),
('Laptop Privacy Screen', 'Anti-glare privacy screen for laptops', 999.00, 15, 5),

-- 6. Computers (category_id = 6)
('Mechanical RGB Keyboard', 'Mechanical keyboard with RGB lighting', 2499.00, 20, 6),
('Wireless Gaming Mouse', 'Rechargeable mouse with adjustable DPI', 1599.00, 25, 6),
('Full HD Webcam', '1080p webcam for meetings and streaming', 1799.00, 15, 6),

-- 7. Cameras (category_id = 7)
('Action Camera', 'Compact camera for travel and outdoor recording', 7999.00, 8, 7),
('Flexible Camera Tripod', 'Portable adjustable tripod for cameras', 899.00, 20, 7),
('Ring Light with Stand', 'LED ring light for videos and online meetings', 1199.00, 25, 7),

-- 8. Gaming (category_id = 8)
('Wireless Game Controller', 'Wireless controller for PC gaming', 1999.00, 15, 8),
('Gaming Headset Pro', 'Over-ear gaming headset with microphone', 2299.00, 18, 8),
('Extended Gaming Mouse Pad', 'Large non-slip mouse pad for gaming desks', 699.00, 30, 8),
('Gaming Desk Light', 'RGB ambient light for gaming setups', 999.00, 20, 8),

-- 9. Clothing (category_id = 9)
('Oversized Graphic T-Shirt', 'Casual oversized cotton graphic T-shirt', 599.00, 40, 9),
('Wide-Leg Jeans', 'Relaxed-fit denim jeans for everyday wear', 1399.00, 25, 9),
('Cotton Co-ord Set', 'Comfortable matching casual clothing set', 1299.00, 20, 9),
('Lightweight Hoodie', 'Soft casual hoodie for everyday use', 1199.00, 22, 9),

-- 10. Shoes (category_id = 10)
('Chunky Sneakers', 'Casual chunky sole everyday sneakers', 1999.00, 20, 10),
('Running Shoes Pro', 'Lightweight cushioned running shoes', 2499.00, 18, 10),
('Slides Sandals', 'Comfortable lightweight casual slides', 499.00, 35, 10),

-- 11. Books (category_id = 11)
('Python Programming Guide', 'Beginner-friendly Python programming book', 599.00, 20, 11),
('Data Science Handbook', 'Introduction to data analysis and visualization', 799.00, 15, 11),
('AI and Machine Learning', 'Introduction to artificial intelligence concepts', 899.00, 12, 11),

-- 12. Home & Kitchen (category_id = 12)
('Digital Air Fryer', 'Compact air fryer for everyday cooking', 4999.00, 10, 12),
('Electric Coffee Frother', 'Handheld milk and coffee frother', 699.00, 25, 12),
('Rechargeable Table Lamp', 'Dimmable LED lamp for study desks', 799.00, 30, 12),
('Kitchen Storage Organizer', 'Multi-purpose kitchen storage rack', 999.00, 20, 12),

-- 13. Beauty & Personal Care (category_id = 13)
('Daily Sunscreen SPF 50', 'Lightweight sunscreen for daily use', 499.00, 35, 13),
('Hydrating Face Serum', 'Hydrating facial serum for skincare routines', 599.00, 25, 13),
('Electric Grooming Trimmer', 'Rechargeable trimmer for personal grooming', 1299.00, 18, 13),
('Lip Care Balm Set', 'Everyday lip care balm multipack', 299.00, 40, 13),

-- 14. Sports & Fitness (category_id = 14)
('Smart Fitness Band', 'Activity tracker with step counting', 1999.00, 15, 14),
('Resistance Band Set', 'Exercise resistance bands for home workouts', 499.00, 30, 14),
('Insulated Water Bottle', 'Reusable insulated stainless steel bottle', 799.00, 25, 14),

-- 15. Toys & Games (category_id = 15)
('STEM Building Kit', 'Educational construction kit for children', 999.00, 15, 15),
('Family Board Game', 'Strategy board game for family game nights', 699.00, 20, 15);


-- 5. CART
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

-- 6. CART ITEMS
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

-- 7. ORDERS
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

-- 8. ORDER ITEMS
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

-- 9. PAYMENTS
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
