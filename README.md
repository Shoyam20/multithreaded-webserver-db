# Server Sync — A Multithreaded E-Commerce Web Server in C++ with MySQL

> A complete online store (storefront + admin panel) whose **web server, router, template engine and
> business logic are written from scratch in C++17** on top of Winsock and the MySQL C API — no web
> framework. Built as a PBL project to show how an operating-system concept (threads and
> synchronisation) and a DBMS concept (transactions, keys, constraints) come together in one real
> application.

---

> **New to the code?** Read [`docs/Server_Sync_How_It_Works.pdf`](docs/Server_Sync_How_It_Works.pdf) — a 30-page walkthrough of what runs first, how control passes from file to file, and four worked examples.

## Table of contents

1. [Problem and objectives](#1-problem-and-objectives)
2. [Features](#2-features)
3. [Screenshots](#3-screenshots)
4. [Technology stack](#4-technology-stack)
5. [System architecture](#5-system-architecture)
6. [Project structure](#6-project-structure)
7. [Database design](#7-database-design)
8. [How the important parts work](#8-how-the-important-parts-work)
9. [URL (route) reference](#9-url-route-reference)
10. [Installation and running](#10-installation-and-running)
11. [Using the admin panel](#11-using-the-admin-panel)
12. [Security notes](#12-security-notes)
13. [Testing](#13-testing)
14. [Limitations and future work](#14-limitations-and-future-work)
15. [How to extend the project](#15-how-to-extend-the-project)
16. [Viva question bank](#16-viva-question-bank)

> **Viewing the diagrams:** the diagrams are [Mermaid](https://mermaid.js.org/) code blocks. GitHub and
> GitLab render them automatically. In VS Code install the extension
> *"Markdown Preview Mermaid Support"* and open the Markdown preview.

---

## 1. Problem and objectives

**Problem.** Small shops need a way to list products, accept orders and keep stock accurate. The real
difficulty is not showing a product page — it is keeping data *correct* when many people use the
site at once (two shoppers buying the last unit, a payment recorded without an order, an order
cancelled but stock not returned).

**Who uses it**

| User | Needs |
|---|---|
| **Shopper** | Browse and search products, keep a cart, place a cash-on-delivery order, see order history |
| **Store owner / admin** | See sales and low stock at a glance, add or edit products, update stock, process orders, manage categories and users |

**Objectives**

1. Build the whole request path (sockets → HTTP → routing → logic → database → HTML) to understand what
   frameworks hide.
2. Serve several users at once with a **thread pool**, sharing one database connection safely.
3. Guarantee **data integrity** with foreign keys, unique constraints and an all-or-nothing
   **SQL transaction** at checkout.
4. Keep the code **easy to read and defend**: one place for URLs, SQL only in services, HTML only in
   templates.

**Why not just use a framework?** A framework would hide exactly the parts this project is meant to
teach (concurrency, transactions, request parsing). The trade-off is that this server is not meant
for production traffic — see [Limitations](#14-limitations-and-future-work).

**How success can be measured** (suggested plan — numbers are for you to measure, none are claimed here)

| Goal | Measure |
|---|---|
| Correct under concurrency | N parallel checkouts of a product with stock 1 → exactly 1 succeeds, stock never negative |
| Integrity | After any failed checkout, no partial order/payment/stock change exists |
| Usability | Time for a new user to find a product and place an order (small user test) |
| Responsiveness | Page latency under k simultaneous clients (e.g. with `ab` or `wrk`) |

---

## 2. Features

### Shopper (storefront)
- Home page with hero banner, category tiles, budget picks, new arrivals and best sellers
- Product listing with **category filter, keyword search, price range and sorting**
- Product detail page with stock status, quantity selector, *Add to cart* and *Buy now*
- Cart with quantity update and remove, live item-count badge in the header
- 3-step checkout (review → place order), **Cash on Delivery only** (no payment gateway)
- **Demo delivery:** an order is delivered automatically 1–2 minutes after checkout, with a live countdown. The customer can **cancel until then** — stock and payment are rolled back; after delivery cancelling is refused
- Order history with per-order items, order status and payment status
- Sign up / sign in, login **sessions** stored in the database, "My sessions" page

### Admin panel
- **Dashboard** — revenue, order count, orders waiting to ship, users, products, low-stock count, active sessions, recent orders, quick restock
- **Orders** — filter by status; change order status; change payment status; cancelling **returns items to stock**
- **Products & stock** — search/filter, add, edit, delete, set exact stock inline, low-stock highlighting
- **Categories** — add, rename, delete (blocked while products still use it)
- **Users** — list with order counts, promote/demote admins
- **Sessions** — see all login sessions, force a sign-out
- **Database tables** — read-only paged view of all 9 tables plus a relationship diagram
- **Server logs** — live, terminal-style log of every request: which **worker thread** handled it and every **SQL statement** it ran (with rows and milliseconds), per-worker statistics, filters (worker / GET-POST / errors only) and auto-refresh. The same text is printed in the server console.

```mermaid
flowchart LR
    G([Guest]) --> B1[Browse and search products]
    G --> B2[Sign up / Sign in]
    C([Customer]) --> B1
    C --> C1[Manage cart]
    C --> C2[Checkout - Cash on Delivery]
    C --> C3[View orders and sessions]
    A([Admin]) --> A1[Dashboard and reports]
    A --> A2[Manage products and stock]
    A --> A3[Process and cancel orders]
    A --> A4[Manage categories, users, sessions]
    A --> A5[Browse raw database tables]
```

---

## 3. Screenshots

> Screenshots use demo data. Folder: [`docs/images/`](docs/images).

### Storefront

| | |
|---|---|
| **Home** — hero, categories, product rows<br>![Home page](docs/images/01-home.png) | **Product listing** — filters, sorting, search<br>![Products](docs/images/02-products.png) |
| **Product detail**<br>![Product detail](docs/images/03-product-detail.png) | **Sign in**<br>![Login](docs/images/07-login.png) |
| **Shopping cart**<br>![Cart](docs/images/04-cart.png) | **Checkout**<br>![Checkout](docs/images/05-checkout.png) |
| **My orders**<br>![Orders](docs/images/06-orders.png) | |

### Admin panel

| | |
|---|---|
| **Dashboard**<br>![Admin dashboard](docs/images/10-admin-dashboard.png) | **Products & stock**<br>![Admin products](docs/images/11-admin-products.png) |
| **Edit product**<br>![Edit product](docs/images/12-admin-product-edit.png) | **Orders**<br>![Admin orders](docs/images/13-admin-orders.png) |
| **Order detail** (change status / payment)<br>![Order detail](docs/images/14-admin-order-detail.png) | **Categories**<br>![Categories](docs/images/15-admin-categories.png) |
| **Users**<br>![Users](docs/images/16-admin-users.png) | **Database tables**<br>![Tables](docs/images/17-admin-tables.png) |
| **Table viewer**<br>![Table view](docs/images/18-admin-table-view.png) | **Server logs** — worker + SQL per request<br>![Server logs](docs/images/19-admin-logs.png) |

---

## 4. Technology stack

| Layer | Technology |
|---|---|
| Language | C++17 (g++ from MSYS2 UCRT64) |
| Networking | Winsock2 (raw TCP sockets) — HTTP/1.1 is parsed by hand |
| Concurrency | `std::thread` worker pool, `std::mutex`, `std::condition_variable`, `std::recursive_mutex` |
| Database | MySQL 8 via the MySQL C API (`libmysql`) |
| Front end | Plain HTML templates, CSS and a little JavaScript (no framework) |
| Build | `build.bat` (one `g++` command) |
| Platform | Windows 10/11 |

---

## 5. System architecture

### 5.1 Layers

Each layer only talks to the one below it. SQL lives **only** in `services/`; HTML lives **only** in
`frontend/`.

```mermaid
flowchart TB
    subgraph Frontend["frontend/ — what the browser gets"]
        T["templates/*.html"]
        S["static/ css + js"]
    end
    subgraph Backend["backend/"]
        direction TB
        CORE["core/ — sockets, HTTP, router, template engine"]
        ROUTES["routes.cpp — URL to handler map"]
        H["handlers/ — one function per page or action"]
        SV["services/ — business rules + ALL SQL"]
        DBL["db/ — one MySQL connection, mutex, Transaction"]
    end
    DB[("MySQL ecommerce_db")]

    CORE --> ROUTES --> H --> SV --> DBL --> DB
    H -->|fills Data| T
    CORE -->|serves| S
```

### 5.2 Life of one request

```mermaid
sequenceDiagram
    autonumber
    participant B as Browser
    participant W as Worker thread (core/server)
    participant A as app.cpp
    participant R as Router (routes.cpp)
    participant H as Handler
    participant S as Service
    participant D as MySQL
    participant T as Template engine

    B->>W: HTTP request (bytes)
    W->>W: read + parse (core/http)
    W->>A: handleRequest(req)
    A->>D: session cookie -> which user? (user_service)
    A->>R: dispatch "GET /cart"
    R->>R: requireLogin / requireAdmin check
    R->>H: cartPage(req)
    H->>S: cartItems(userId)
    S->>D: SELECT ...
    D-->>S: rows
    S-->>H: rows
    H->>T: fill Data, render frontend/templates/pages/cart.html
    T-->>H: HTML
    H-->>W: Response (status, headers, body)
    W-->>B: HTTP response, close connection
```

### 5.3 Threading model

```mermaid
flowchart LR
    M["main thread<br/>accept() loop"] -->|push socket| Q[("queue of connections")]
    Q -->|condition_variable wakes one| W1["worker 1"]
    Q --> W2["worker 2"]
    Q --> W3["worker 3"]
    Q --> W4["worker 4"]
    W1 & W2 & W3 & W4 -->|recursive_mutex| DBC[("single MySQL connection")]
```

- The main thread only accepts connections and queues them.
- `workerThreads` (default 4, in `backend/config.cpp`) threads each take one connection, handle it and
  close it (`Connection: close`).
- All threads share **one** MySQL connection protected by a recursive mutex. This is simple and safe;
  the cost is that database calls are serialised (see [Limitations](#14-limitations-and-future-work)).
  A *recursive* mutex lets a function that already holds the lock call another that also locks it —
  `Transaction` relies on this.

---

## 6. Project structure

```
D:\pdb
├── backend/                      C++ source (the server)
│   ├── main.cpp                  start-up: settings -> database -> web server
│   ├── routes.cpp                THE SITE MAP: one line per URL (read this first)
│   ├── app.cpp                   per-request pipeline: static files, who is the user, dispatch
│   ├── config.h / .cpp           port, DB login (env-var overridable), low-stock level
│   ├── core/                     generic web plumbing, no shop logic
│   │   ├── http.*                Request/Response, parsing, cookies, URL encoding
│   │   ├── router.*              URL -> handler table, requireLogin / requireAdmin
│   │   ├── server.*              Winsock sockets + worker thread pool
│   │   ├── template.*            tiny template engine ({{ }}, #if, #each, include)
│   │   └── log.*                 per-request log: worker id + SQL list, ring buffer, terminal format
│   ├── db/
│   │   └── database.*            one MySQL connection, run()/rows()/quote(), Transaction, migration
│   ├── services/                 business rules — ALL SQL lives here
│   │   ├── product_service.*     search/filter, admin create/edit/stock/delete
│   │   ├── category_service.*    cached category list + admin CRUD
│   │   ├── cart_service.*        cart lines, add/update/remove with stock checks
│   │   ├── order_service.*       checkout transaction, order list, status changes
│   │   ├── user_service.*        sign up/in, password hashing, sessions, roles
│   │   └── admin_service.*       dashboard numbers, table browser
│   └── handlers/                 request -> service -> template
│       ├── handlers.h            every handler declared in one place
│       ├── view.*                shared helpers: money(), renderPage(), productData() ...
│       ├── shop_/auth_/cart_/order_handlers.cpp     storefront
│       └── admin_dashboard/catalog/orders/users/logs.cpp admin panel
├── frontend/
│   ├── templates/
│   │   ├── layout.html           page shell (header, notice bar, footer)
│   │   ├── partials/             header, footer, login pop-ups, product card
│   │   ├── pages/                home, products, product, cart, checkout, orders ...
│   │   └── admin/                admin layout + dashboard, products, orders ...
│   └── static/
│       ├── css/style.css  admin.css
│       └── js/app.js
├── database/
│   ├── ecommerce_db.sql          schema + sample data (15 categories, 52 products)
│   └── verify_and_seed.sql       sanity queries
├── docs/images/                  screenshots used in this README
├── build.bat  run.bat
├── libmysql.dll                  MySQL client library (must sit next to server.exe)
└── libwinpthread-1.dll           threading runtime (must sit next to server.exe)
```

`_old_modular/` and `main.cpp.orig` are backups of earlier versions and can be deleted.

---

## 7. Database design

### 7.1 Entity–relationship diagram

```mermaid
erDiagram
    USERS ||--o{ USER_SESSIONS : "logs in"
    USERS ||--o{ CART : owns
    USERS ||--o{ ORDERS : places
    CATEGORIES ||--o{ PRODUCTS : groups
    CART ||--o{ CART_ITEMS : contains
    PRODUCTS ||--o{ CART_ITEMS : "is in"
    CART ||--o| ORDERS : "becomes"
    ORDERS ||--o{ ORDER_ITEMS : contains
    PRODUCTS ||--o{ ORDER_ITEMS : "is sold as"
    ORDERS ||--o| PAYMENTS : "paid by"

    USERS {
        int user_id PK
        varchar email UK
        varchar name
        varchar password "salted SHA-256"
        varchar role "CUSTOMER or ADMIN"
        timestamp created_at
    }
    USER_SESSIONS {
        varchar session_id PK
        int user_id FK
        timestamp login_time
        timestamp last_activity
        timestamp logout_time
        boolean is_active
    }
    CATEGORIES {
        int category_id PK
        varchar category_name UK
    }
    PRODUCTS {
        int product_id PK
        varchar name
        varchar description
        decimal price
        int stock
        int category_id FK
    }
    CART {
        int cart_id PK
        int user_id FK
        varchar status "ACTIVE or CHECKED_OUT"
        timestamp created_at
    }
    CART_ITEMS {
        int cart_item_id PK
        int cart_id FK
        int product_id FK
        int quantity
    }
    ORDERS {
        int order_id PK
        int user_id FK
        int cart_id FK_UK
        decimal total
        varchar status
        timestamp created_at
        datetime deliver_at "demo: auto-delivery time"
    }
    ORDER_ITEMS {
        int order_item_id PK
        int order_id FK
        int product_id FK
        int quantity
        decimal price "price at purchase time"
    }
    PAYMENTS {
        int payment_id PK
        int order_id FK_UK
        decimal amount
        varchar method "COD"
        varchar status
        timestamp paid_at
    }
```

### 7.2 Why it is designed this way

| Decision | Reason |
|---|---|
| `cart_items` has `UNIQUE(cart_id, product_id)` | One line per product; adding again increases the quantity |
| `orders.cart_id` is `UNIQUE` | A cart can become at most one order |
| `payments.order_id` is `UNIQUE` | One payment record per order |
| `order_items.price` copies the price | Later price changes must not rewrite past orders |
| `DECIMAL(10,2)` for money | No floating-point rounding errors (totals are also summed in integer paise in C++) |
| Foreign keys everywhere | The database itself refuses orphan rows |
| `user_sessions` table | Login sessions are stored server-side, can be listed and revoked |
| `users.role` | Added automatically at first start (`migrateDatabase()`), default `CUSTOMER` |

Status vocabulary (stored as text):

| Where | Values |
|---|---|
| `cart.status` | `ACTIVE`, `CHECKED_OUT` |
| `orders.status` | `PLACED`, `SHIPPED`, `DELIVERED`, `CANCELLED` |
| `payments.status` | `PENDING`, `PAID`, `FAILED`, `REFUNDED`, `CANCELLED` |

---

## 8. How the important parts work

### 8.1 Checkout — one atomic transaction

`placeOrder()` in `backend/services/order_service.cpp`. If **any** step fails the `Transaction` object
rolls everything back when it goes out of scope, so a half-finished order can never exist.

```mermaid
flowchart TD
    S([POST /checkout/place]) --> T["START TRANSACTION<br/>(Transaction object, holds the DB lock)"]
    T --> L["SELECT cart items JOIN products FOR UPDATE<br/>locks the product rows"]
    L --> E{"cart empty?"}
    E -->|yes| RB1[["ROLLBACK + message"]]
    E -->|no| ST{"every quantity <= stock?"}
    ST -->|no| RB2[["ROLLBACK + message"]]
    ST -->|yes| O["INSERT orders (status PLACED)"]
    O --> LOOP["for each item:<br/>INSERT order_items (price copy)<br/>UPDATE products SET stock = stock - qty<br/>WHERE stock >= qty"]
    LOOP --> OK{"each UPDATE changed exactly 1 row?"}
    OK -->|no| RB3[["ROLLBACK + message"]]
    OK -->|yes| P["INSERT payments (COD, PENDING)"]
    P --> C["UPDATE cart SET status = CHECKED_OUT"]
    C --> CM[["COMMIT"]]
    CM --> R([redirect to /orders])
```

Three layers stop *overselling*: (1) the `Transaction` object holds the C++ database lock for the
whole checkout, so with the single shared connection a second checkout simply waits its turn;
(2) `FOR UPDATE` locks the product rows inside MySQL — this becomes essential if you later add a
connection pool; (3) the guarded `UPDATE ... WHERE stock >= qty` refuses to go below zero even if
something slipped through.

### 8.2 Order and payment lifecycle (admin)

```mermaid
stateDiagram-v2
    [*] --> PLACED: customer checks out (deliver_at = now + 60-120 s)
    PLACED --> DELIVERED: automatic when deliver_at passes (COD payment becomes PAID)
    PLACED --> CANCELLED: customer or admin BEFORE delivery (stock returned, payment cancelled)
    PLACED --> SHIPPED: admin (optional)
    SHIPPED --> DELIVERED: automatic or admin
    SHIPPED --> CANCELLED: customer or admin before delivery
    DELIVERED --> [*]
    CANCELLED --> [*]
```

**Demo delivery and customer cancel.** At checkout `placeOrder()` stores `orders.deliver_at` = now + a
random 60–120 seconds (`ECOM_DELIVERY_MIN` / `ECOM_DELIVERY_MAX` change the range, e.g. `15` for a quick
demo). A background thread started in `main.cpp` calls `deliverDueOrders()` every 5 seconds: in one
transaction it marks due orders `DELIVERED` and their COD payment `PAID`. The customer's *My orders* page
shows a live countdown and a **Cancel order** button while the order is `PLACED`/`SHIPPED`. Cancelling
(`cancelOrderByCustomer()` → `updateOrderStatus()`) runs one transaction that puts every item back into
`products.stock` and sets the payment to `CANCELLED`. Both the delivery thread and the cancel check the
order's status *inside* a transaction, so if they race, exactly one wins: a delivered order can never be cancelled.

Rules enforced in `updateOrderStatus()`:

- **Cancel** → every item's quantity is added back to `products.stock`, and the payment becomes
  `CANCELLED` (or `REFUNDED` if it was already `PAID`).
- **Delivered** → a cash-on-delivery payment that is `PENDING` becomes `PAID`.
- A **cancelled** order is final (its stock was already returned; reopening it would corrupt stock).
- A **delivered** order cannot be cancelled (that would need a returns process).

### 8.3 Login sessions and access control

```mermaid
sequenceDiagram
    participant B as Browser
    participant S as Server
    participant D as MySQL
    B->>S: POST /login (email, password)
    S->>D: SELECT user by email
    S->>S: compare salted hash
    S->>D: INSERT user_sessions (random 128-bit id)
    S-->>B: 302 + Set-Cookie session_id (HttpOnly, SameSite=Lax)
    B->>S: GET /admin (cookie)
    S->>D: session id -> user + role, update last_activity
    alt role = ADMIN
        S-->>B: admin page
    else not admin
        S-->>B: 302 to /?notice=forbidden
    end
```

Access control is declared next to the URL in `routes.cpp`:

```cpp
r.get ("/cart",  requireLogin(cartPage));      // any signed-in user
r.get ("/admin", requireAdmin(adminDashboard));// role must be ADMIN
```

### 8.4 The template engine (why there is no HTML in C++)

Handlers put values in a `Data` object; the template decides how to show them.

```cpp
// handler (C++)
Data page;
page.set("heading", "All products");
addProducts(page, "products", rows);          // list of product Data
return renderPage(req, "Products", "pages/products.html", page);
```
```html
<!-- frontend/templates/pages/products.html -->
<h1>{{heading}}</h1>
{{#if products}}
  {{#each products}} {{> partials/product_card.html}} {{/each}}
{{else}}
  <p>No products found.</p>
{{/if}}
```

| Syntax | Meaning |
|---|---|
| `{{name}}` | Value, **HTML-escaped** (protects against XSS) |
| `{{{name}}}` | Value inserted as-is (only for HTML the server built itself) |
| `{{#if name}} … {{else}} … {{/if}}` | Condition (true when non-empty, or a non-empty list) |
| `{{#each list}} … {{/each}}` | Repeat for each entry; entry values are looked up first, then page values |
| `{{> partials/x.html}}` | Include another template |

Templates are read from disk on every request, so **editing HTML/CSS needs only a browser refresh —
no rebuild.**

### 8.5 Database access pattern

```cpp
Rows rows = db.rows("SELECT name FROM products WHERE category_id=" + to_string(id));
Result r  = db.run("UPDATE products SET stock=" + to_string(n) + " WHERE product_id=" + to_string(id));
string safe = db.quote(userText);   // 'escaped' -> prevents SQL injection
```

Numbers come from `to_string(int)`; all text goes through `db.quote()` (wraps
`mysql_real_escape_string`). `LIKE` searches additionally escape `%`, `_` and `\`.

### 8.6 Server logs — which worker, which SQL

Every request (except static files and the log pages themselves) is recorded and shown in
**Admin → Server logs**. The admin page shows the full log below. The terminal (e.g. the VS Code terminal) only shows which worker took which request, one line each (`[W3] POST /checkout/place`); to also print the SQL there, start the server with `ECOM_LOG_SQL=1`.

```
[2026-10-13 18:48:35] [W3] POST /checkout/place
    mysql> START TRANSACTION;
           -> OK, 0 rows affected (0.32 ms)
    mysql> SELECT ci.product_id, ci.quantity, p.price, p.stock FROM cart_items ci JOIN products p ON ci.product_id=p.product_id WHERE ci.cart_id=13 FOR UPDATE;
           -> 1 row (0.67 ms)
    mysql> UPDATE products SET stock=stock-1 WHERE product_id=2 AND stock>=1;
           -> OK, 1 row affected (1.92 ms)
    mysql> COMMIT;
           -> OK, 0 rows affected (12.55 ms)
[W3] 302 Found | 13 queries | 32.77 ms | user: Log Tester (admin)
```

```mermaid
sequenceDiagram
    participant W as Worker thread (server.cpp)
    participant L as RequestLog (core/log.cpp)
    participant D as Database::run (database.cpp)
    W->>L: beginRequestLog(&log)  (thread-local)
    W->>D: handler runs, calls db.run(sql) many times
    D->>L: addQueryToCurrentRequest(sql, ms, rows)
    W->>L: recordRequest(log)  (ring buffer + console)
```

- **How the worker is known:** `worker(int id, ...)` in `core/server.cpp` is started once per thread with
  its number; that number is stored in the request's `RequestLog`.
- **How queries are attached:** a thread-local pointer to the current `RequestLog`. `Database::run()` times
  each statement and adds it; because the pointer is per-thread, workers never mix their queries.
- **Console output** is one short line per request (`[W2] GET /products`), written under a mutex so lines from
  different workers never interleave. The SQL is only printed there if `ECOM_LOG_SQL=1` is set
  (the admin page always has the full log).
- **Size limits:** the last `logBufferSize` (300) requests are kept; at most 150 queries per request are shown.
- **Masked on purpose:** the password (`SHA2(CONCAT('<salt>','<hidden>'),256)`), password hashes
  (`'<password-hash>'`) and session ids (`'67f805...'`) never appear in the log. Form bodies are never logged.
  Query strings are logged as sent, so avoid putting secrets in URLs.

---

## 9. URL (route) reference

The authoritative list is [`backend/routes.cpp`](backend/routes.cpp).

**Anyone**

| Method | URL | Page |
|---|---|---|
| GET | `/` | Home |
| GET | `/products?category=&q=&sort=&min=&max=` | Product list |
| GET | `/product?id=` | Product detail |
| GET/POST | `/login`, `/signup` | Forms / submit |
| GET | `/logout` | Sign out |
| GET | `/static/...` | CSS, JS |

**Signed-in customers**

| Method | URL | Action |
|---|---|---|
| GET | `/cart` | View cart |
| POST | `/cart/add`, `/cart/update`, `/cart/remove` | Change cart |
| GET | `/checkout` | Review + payment |
| POST | `/checkout/place` | Place order (transaction) |
| GET | `/orders`, `/sessions` | History, login sessions |
| POST | `/orders/cancel` | Cancel own order until it is delivered (rolls back stock + payment) |

**Admin only** (`requireAdmin`)

| Method | URL | Action |
|---|---|---|
| GET | `/admin` | Dashboard |
| GET | `/admin/products`, `/admin/products/edit?id=` | List, add/edit form |
| POST | `/admin/products/save`, `/stock`, `/delete` | Save, set stock, delete |
| GET/POST | `/admin/categories`, `/save`, `/delete` | Categories |
| GET | `/admin/orders`, `/admin/order?id=` | Orders, detail |
| POST | `/admin/order/status`, `/admin/order/payment` | Change status |
| GET/POST | `/admin/users`, `/admin/users/role` | Users, roles |
| GET/POST | `/admin/sessions`, `/admin/sessions/end` | Sessions, force logout |
| GET | `/admin/tables`, `/admin/table?name=&page=` | Table browser |
| GET | `/admin/logs?worker=&method=&errors=&limit=` | Server log page |
| GET | `/admin/logs/feed` | Log panel only (used by auto-refresh) |
| POST | `/admin/logs/clear` | Clear the log history |

Every action that **changes data is a POST** form.

---

## 10. Installation and running

### 10.1 Prerequisites

| Need | Notes |
|---|---|
| Windows 10/11 | Uses Winsock |
| MySQL Server 8.x | Running locally; this project assumes `C:\Program Files\MySQL\MySQL Server 8.0` |
| MSYS2 UCRT64 g++ (C++17) | `C:\msys64\ucrt64\bin` on PATH |
| A browser | Chrome, Edge, Firefox |

### 10.2 Create the database

In MySQL Workbench (or the `mysql` command line) run `database/ecommerce_db.sql`. It creates
`ecommerce_db`, all 9 tables, 15 categories and 52 sample products.

> The sample products are inserted by a plain `INSERT`; running that script a second time duplicates them.

### 10.3 Configure the database login

Default (in `backend/config.cpp`): host `localhost`, user `root`, database `ecommerce_db`, port `3306`.
Set your password **without editing code**:

```powershell
$env:ECOM_DB_PASS = "your_mysql_password"
```

Other overrides: `ECOM_DB_HOST`, `ECOM_DB_USER`, `ECOM_DB_NAME`, `ECOM_DB_PORT`.
Demo options: `ECOM_DELIVERY_MIN` / `ECOM_DELIVERY_MAX` (auto-delivery delay in seconds, default 60 / 120) and `ECOM_LOG_SQL=1` (also print SQL in the terminal).

### 10.4 Build

```powershell
cd D:\pdb
.\build.bat
```

This runs one `g++` command over everything in `backend/` and produces `server.exe`. The C++ runtime is
linked statically (`-static-libgcc -static-libstdc++`). Two DLLs must sit next to `server.exe`: `libmysql.dll` and `libwinpthread-1.dll` (both are already in the project folder).

### 10.5 Run

```powershell
.\run.bat            # or:  .\server.exe
```
Expected output:
```
[DB] Connected to MySQL successfully.
Server running on http://localhost:8080
```
Open **http://localhost:8080**. Press `Ctrl+C` to stop.

### 10.6 Troubleshooting

| Symptom | Cause / fix |
|---|---|
| `Bind failed` | Another copy is still running: `taskkill /F /IM server.exe` |
| `Connection failed` | MySQL not running or wrong password (see 10.3) |
| Page says `missing template` | Run `server.exe` from `D:\pdb`, not from `backend\` |
| Program exits immediately, no message | Rebuild with `build.bat` (static runtime flags) |
| Build error `mysql.h not found` | Edit the `MYSQL=` path at the top of `build.bat` |

---

## 11. Using the admin panel

1. Open the site, click **Sign up** and create your account.
2. In a terminal:
   ```powershell
   cd D:\pdb
   .\server.exe --make-admin you@email.com
   ```
   (It prints `… is now an admin.`)
3. Sign out and sign in again — you are taken to **http://localhost:8080/admin**.
4. Later you can promote others from **Admin → Users**.

Typical workflow: **Dashboard** shows *Waiting to ship* → open **Orders** → *Manage* → set **SHIPPED**
then **DELIVERED**. When **Low stock** appears on the dashboard, use the *Restock to* box or
**Products & stock**.

---

## 12. Security notes

| Topic | What is done |
|---|---|
| **SQL injection** | All text is escaped with `db.quote()`; numbers are converted from integers |
| **XSS** | `{{value}}` is always HTML-escaped; flash messages are chosen from a fixed table, never from the URL text |
| **Passwords** | Salted SHA-256 (`sha256$salt$hash`, computed by MySQL `SHA2`). Old plain-text accounts are upgraded at next login. *A slow hash (bcrypt/argon2) is the production choice.* |
| **Sessions** | 128-bit random id from the OS generator; cookie is `HttpOnly; SameSite=Lax`; sessions are revocable |
| **CSRF** | Every data-changing action is a POST, and `SameSite=Lax` stops browsers sending the cookie on cross-site POSTs. No separate CSRF token yet. |
| **Authorisation** | `requireAdmin` / `requireLogin` declared per route; an admin cannot demote themselves |
| **Open redirect** | The admin "go back" URL is accepted only if it begins with `/admin` |
| **Path traversal** | `/static/` rejects `..` and backslashes |
| **Table browser** | Table names are checked against a fixed whitelist; password column is masked |
| **Not done** | HTTPS, login rate-limiting, CSRF tokens, password reset, e-mail verification |

Do **not** put real passwords or API keys in the repository. Use `ECOM_DB_PASS`.

---

## 13. Testing

### 13.1 What has been checked

The following were exercised against a real MySQL database over HTTP (with `curl`) during development:

- Every storefront page and admin page returns HTTP 200 with no unrendered `{{ }}` tags; unknown URLs return 404
- Sign up → sign in → add to cart → update quantity → checkout → order appears with `PLACED` / `PENDING`
- Data-changing URLs reject `GET` (404); admin URLs redirect a customer away and a guest to sign-in
- Cancelling an order returns stock (15 → 13 after ordering 2 → 15 after cancel) and sets payment `CANCELLED`
- Marking an order `DELIVERED` sets the COD payment to `PAID`
- A delivered order cannot be cancelled; a cancelled order cannot be changed
- Customer cancel inside the delivery window: stock restored (15 → 14 → 15) and payment `CANCELLED`; after the window the order becomes `DELIVERED` with payment `PAID` automatically, a late cancel is refused, and a customer cannot cancel someone else's order
- The terminal shows only `[W3] POST /checkout/place`-style lines; the SQL appears only in Admin → Server logs
- Product create / edit / set-stock / delete, including validation messages and refusing to delete a product that is in past orders
- Category add / duplicate / rename / delete (blocked while it still has products); storefront menu updates immediately
- An admin cannot change their own role; the "back" parameter cannot redirect to another site
- A legacy plain-text password account logs in and is upgraded to a hash
- Password hashes never appear in the table browser

### 13.2 Not yet tested (honest gaps)

- **Concurrent checkout of the last unit** (the main reason for `FOR UPDATE`) — see the plan below
- Load/latency measurements
- Browsers other than Edge (screenshots were produced with headless Edge)
- Automated unit tests — everything above was manual

### 13.3 Suggested concurrency test

1. Set a product's stock to `1` in Admin.
2. Create two customer accounts; add that product to each cart.
3. Fire both `POST /checkout/place` requests at the same moment (two terminals, or a script).
4. **Expected:** exactly one order is created, the other gets *"Not enough stock…"*, and `stock` is `0`, never negative.

---

## 14. Limitations and future work

| Limitation | Why it matters | Possible improvement |
|---|---|---|
| One shared DB connection behind a mutex | Queries from different users run one at a time | A connection pool (one connection per worker) |
| `Connection: close`, no keep-alive, no TLS | Extra connection setup per page; traffic unencrypted | Keep-alive; put a TLS reverse proxy in front |
| Cash on Delivery only; delivery is simulated (auto-delivered after 1-2 min) | No real payment or courier | Payment gateway (webhook → `payments.status`) and real shipping states |
| No product images | Tiles use category icons | Add `image_url` column + upload route |
| No postal address / shipping | Not a full order flow | `addresses` table, delivery fee |
| No password reset / e-mail verification | Account recovery impossible | E-mail service |
| No pagination on storefront list | Fine for 52 products, slow for thousands | `LIMIT/OFFSET` paging |
| No automated tests | Regressions found by hand | Unit tests for services, scripted HTTP tests |
| Admin edits only through forms | Safe, but no audit trail | `audit_log` table (who changed what, when) |

---

## 15. How to extend the project

**Add a new customer page** (example: a wishlist) in four steps:

1. **Service** — add `backend/services/wishlist_service.*` with the SQL (`Rows listWishlist(int userId)`).
2. **Handler** — add `Response wishlistPage(Request& req)` in `handlers/`, declare it in `handlers.h`.
   Call the service, fill a `Data`, `return renderPage(req, "Wishlist", "pages/wishlist.html", page);`
3. **Template** — create `frontend/templates/pages/wishlist.html`.
4. **Route** — one line in `routes.cpp`: `r.get("/wishlist", requireLogin(wishlistPage));`

Then run `build.bat`. (New `.cpp` files are picked up automatically because the build compiles every
file in the folders.)

**Change the look** — edit `frontend/static/css/style.css` (storefront) or `admin.css`; refresh the browser.

**Change the low-stock warning level** — `lowStockThreshold` in `backend/config.cpp`.

---

## 16. Viva question bank

| Question | Where the answer is |
|---|---|
| How does the server handle many users at once? | §5.3, `core/server.cpp` (thread pool + queue + condition variable) |
| Why a *recursive* mutex? | `db/database.h` — `Transaction` and services both lock; the same thread may lock twice |
| How do you prevent two people buying the last item? | §8.1 — C++ transaction lock + `FOR UPDATE` + guarded `UPDATE … WHERE stock >= qty` |
| What happens if payment insert fails after stock was reduced? | `Transaction` destructor → `ROLLBACK`; nothing is saved |
| Why store `price` in `order_items`? | Price history: later price edits must not change old orders |
| How do you stop SQL injection / XSS? | §12 — `db.quote()`, template escaping |
| How are admin pages protected? | §8.3 — `requireAdmin` in `routes.cpp` + role in `users` |
| Why are templates read from disk each request? | Edit-and-refresh development; trade-off is a little I/O (could be cached) |
| What are the weaknesses of this design? | §14 — single connection, no TLS, no CSRF token |
| How would you scale it? | Connection pool, keep-alive, caching, reverse proxy, move to a framework/async I/O |

---

*Server Sync — Graphic Era PBL project. Demo store only: no real payments are processed.*
