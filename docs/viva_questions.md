# Viva questions and answers

Short answers, with the file where the answer lives. Paths are relative to the project folder.

## Operating-system side

**1. How does the server handle many users at once?**
With a *thread pool* (`backend/core/server.cpp`). The main thread only calls `accept()` and puts the connection on a queue.
`WORKER_COUNT` worker threads (started once, `config.h`) each loop: wait until the queue has a connection, take it,
handle the request, send the response, close. If all workers are busy, new connections simply wait in the queue.

**2. Why a thread pool and not one new thread per request?**
Creating a thread per request is slow and has no upper limit. A fixed pool re-uses threads and limits the resources used.

**3. What is the `condition_variable` for?**
So idle workers sleep instead of repeatedly checking the queue ("busy waiting"). The main thread calls `notify_one()` after
pushing a connection and exactly one sleeping worker wakes up. The worker waits in a `while (pending.empty())` loop, so a
spurious wake-up is harmless.

**4. Which mutexes exist and what does each protect?**
`pendingMutex` – the connection queue. `printMutex` – the console, so `[Worker N]` lines are not mixed.
`dbMutex` – the single MySQL connection. Each protects data that more than one thread can touch.

**5. Why is `dbMutex` needed?**
All workers share one `MYSQL*` connection and the MySQL C client does not allow two threads to use one connection at the
same time. Every service function starts with `lock_guard<mutex> lock(dbMutex);` (see `backend/services/`).

**6. Can this deadlock?**
The rule that prevents it: a function that holds `dbMutex` never calls another function that also locks it (a plain mutex
cannot be locked twice by the same thread). Public service functions lock; small `static` helpers inside the same file
assume the lock is already held (for example `findCart`, `findOrder`). `placeOrder` uses `findActiveCart`, which is
documented as "caller must hold dbMutex".

**7. What is the cost of one shared connection?**
Database work is serialised – only one query runs at a time. It is simple and safe; a connection pool (one connection per
worker) is the upgrade, and then the `FOR UPDATE` lock in MySQL becomes essential.

## DBMS side

**8. What does the checkout transaction guarantee?**
All-or-nothing (atomicity): the order, its items, the stock reduction, the payment and closing the cart either all happen
(`COMMIT`) or none does (`ROLLBACK`). See `placeOrder()` in `backend/services/order_service.cpp`.

**9. How do you prevent selling more than the stock?**
Three layers: (1) `dbMutex` serialises checkouts in the C++ code; (2) `SELECT ... FOR UPDATE` locks the product rows in
MySQL until the transaction ends; (3) `UPDATE products SET stock = stock - qty WHERE ... AND stock >= qty` cannot go below
zero, and the code rolls back unless exactly one row changed. `tests/concurrency_test.md` describes how to test it.

**10. Why does `order_items` store the price?**
So the order history keeps the price paid even if the product's price changes later.

**11. Which constraints does the schema use and why?**
Primary keys on every table; foreign keys (a cart item must refer to a real cart and product, an order to a real user, ...);
`UNIQUE(cart_id, product_id)` in `cart_items` (one line per product); `UNIQUE` on `orders.cart_id` (one order per cart) and
`payments.order_id` (one payment per order); `UNIQUE` email and category name. The database refuses data that breaks the rules.

**12. What happens when an order is cancelled?**
Another transaction (`changeStatus` in `order_service.cpp`): the quantities are added back to `products.stock`, the payment
becomes CANCELLED, the order becomes CANCELLED. A delivered order cannot be cancelled, and a cancelled order cannot be
changed (its stock has already been returned).

**13. Why can a product that was ordered not be deleted?**
`order_items` has a foreign key to it and the order history needs it. `deleteProduct` refuses and suggests setting the stock
to 0. A product that is only in carts is removed from the carts first, then deleted, in one transaction.

## Security and web

**14. How do you stop SQL injection?**
Text typed by users goes into SQL only through `dbQuote()` (it escapes quotes); numbers are converted with `to_string(int)`;
`ORDER BY` text is chosen from a fixed list, never copied from the URL; `LIKE` patterns escape `%`, `_` and `\`.

**15. How do you stop XSS?**
`{{name}}` in a template always HTML-escapes the value (`htmlEscape` in `core/template.cpp`). `{{{name}}}` (raw) is used
only for HTML that the server built itself, from escaped pieces.

**16. How are passwords stored?**
As `sha256$<salt>$<hash>`: a random salt and SHA-256(salt + password) computed by MySQL's `SHA2()` (`user_service.cpp`).
The salt makes equal passwords give different hashes. A slow hash like bcrypt/argon2 would be better in production.

**17. How does login work?**
`logIn()` checks the hash, `startSession()` stores a random 32-character id in `user_sessions` and the server sends it as a
cookie (`HttpOnly; SameSite=Lax`). On every request `identifyUser()` in `app.cpp` looks the id up to find the user.
Logout marks the session inactive, so the cookie stops working.

**18. Why redirect after a POST?**
So pressing Refresh repeats a harmless GET instead of re-sending the form (for example placing an order twice).

**19. How are admin pages protected?**
Each admin handler starts with a check of `req.user.isAdmin()` and redirects otherwise. The role is a column in `users`.
There is no "become admin" page on purpose: an admin is created with `server.exe --make-admin <email>`.

**20. Why does each handler check the login itself instead of a shared guard?**
It is plain and visible in each function. The trade-off is that a new handler could forget the check, so every new
protected handler must start with it.

**21. How does the template engine work?**
`renderTemplate()` (`core/template.cpp`) reads an HTML file and walks through it: plain text is copied, `{{name}}` is replaced
by the escaped value, `{{{name}}}` by the raw value, `{{> file}}` by another template. Repeated parts (product cards, cart rows)
are built in the handlers by rendering a small row template once per row and joining the results.

**22. What happens if the customer cancels at the same moment the admin marks the order delivered?**
Both take `dbMutex`, and each reads the order's current status only after it has the lock, so one runs completely first and
the other sees the result: either the cancel wins (the order is CANCELLED), or the delivery wins and the cancel is refused
("Too late").

## Design and limits

**23. What are the weaknesses of this design?**
One shared database connection; one request per connection (no keep-alive); no HTTPS; no CSRF tokens; Cash on Delivery
only; no automated unit tests.

**24. How would you scale it?**
Use one connection per worker (a pool), keep-alive connections, put a TLS reverse proxy in front, add caching and pagination,
and add automated tests.

**25. Why not use a framework?**
A framework would hide threads, locking, transactions and request parsing – exactly what this project is meant to teach.
