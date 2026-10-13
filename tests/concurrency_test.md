# Concurrency test: two shoppers, one last item

**Goal:** show that the checkout transaction never oversells. When two shoppers try to buy the
last unit of a product at the same moment, exactly one order must be created and the stock must end at 0,
never negative.

**Status:** this test needs MySQL and the running server, so it cannot run inside the repository by itself.
Run it on your machine (steps below) and write down what you see. No result is recorded in this file.

## Run it (script)

1. Start MySQL and the server (`run.bat`), with `ECOM_DB_PASS` set in that window.
2. Open a second terminal in the project folder, set the same password, and run the script:
   ```
   set ECOM_DB_PASS=your_mysql_password
   tests\concurrency_test.bat
   ```
   Optional: `tests\concurrency_test.bat 5` to use product 5 instead of the default product 2.

The script:

1. remembers the real stock of the product;
2. signs up and logs in two temporary users (`race1_...@example.com`, `race2_...@example.com`);
3. sets the product's stock to **1** and puts 1 unit in each user's cart;
4. sends two `POST /checkout/place` requests at the same time (two background `curl` processes);
5. counts the orders of the two users and reads the stock;
6. prints `RESULT: PASS` or `RESULT: FAIL`, then deletes the temporary users/orders and restores the stock.

**Expected:** `Orders created by the two users: 1`, `Stock ... afterwards: 0`, `RESULT: PASS`.
One `curl` is redirected to `/orders` (order placed); the other gets a page saying *"Not enough stock..."*.

## Run it by hand (to see the console)

1. In MySQL: `UPDATE products SET stock=1 WHERE product_id=2;`
2. Create two accounts in two different browsers (or one normal and one private window), add product 2
   to both carts (before step 1 if the stock was already 0).
3. Click **Place order (COD)** in both windows as close together as you can.
4. Check: `SELECT COUNT(*) FROM orders;` grew by 1, `SELECT stock FROM products WHERE product_id=2;` is 0.
5. In the server console you will see two `[Worker N] POST /checkout/place` lines - usually from two
   different workers.

## Why it works (for the viva)

* `placeOrder()` (`backend/services/order_service.cpp`) takes `dbMutex` first, so with the single shared
  MySQL connection the second checkout waits until the first one has finished.
* Inside the transaction `SELECT ... FOR UPDATE` locks the product rows in MySQL (this matters if you ever
  add more than one connection).
* `UPDATE products SET stock=stock-qty WHERE product_id=... AND stock>=qty` refuses to go below zero, and
  the code rolls back if it did not change exactly one row.

## What this test can and cannot show

A race condition is not guaranteed to appear on any single run, so a PASS shows the rule holds, not that no
interleaving could ever break it. The invariant to check every time is the same:
**never two orders for one unit, and never negative stock.**

With one shared MySQL connection and `dbMutex`, the two checkouts are already serialised by the C++ lock, so
this test mainly exercises that lock plus the transaction. It cannot tell the other two protections
(`FOR UPDATE` and the guarded `UPDATE`) apart from the mutex; they become important if the server is later
changed to use one connection per worker.
