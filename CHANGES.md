# CHANGES — simplification of Server Sync

Goal: make the project readable and explainable line by line, while keeping the two ideas it exists to show
(thread pool + mutex/condition variable; transactions + constraints). One git commit per step; the first commit
(`Baseline before simplification`) is the state before any change, so `git diff <baseline> HEAD` shows everything.

## Size before and after (measured with `wc -l`)

| | Before | After |
|---|---|---|
| Backend `.cpp` + `.h` files | 43 | 25 |
| Backend lines | 4,270 | 3,097 (of which 2,187 are code, the rest comments and blank lines) |
| Frontend files | 33 | 49 |
| Frontend lines | 1,281 | 901 |

Frontend has more, smaller files because the repeated pieces (product card, cart row, order card, table rows) now
live in tiny row templates in `frontend/templates/partials/`, since the template engine no longer has loops.

## Step 1 — clutter deleted
* `_old_modular/` (33 files: the earlier version of the code), `main.cpp.orig`, `docs/build/` (source of the old PDF),
  `server.exe` (build output; `*.exe` and `*.dll` are now in `.gitignore`), `database/verify_and_seed.sql`
  (duplicated the seed data and only held check queries).
* `docs/images/`: 12 screenshots deleted, 5 kept (home, product, cart, checkout, admin dashboard) and re-taken for the final UI.
* `.vscode/`: deleted, but the editor re-creates it, so it is now in `.gitignore` (not tracked).
* The two DLLs stay in the folder but are not in git; the README says they must be next to `server.exe`.

## Step 2 — security
* The MySQL password was hard-coded in `backend/config.cpp` (and copied in two backup files). It is removed; the password is
  now read **only** from the environment variable `ECOM_DB_PASS` (empty by default). It was blanked before the first commit,
  so it is not in git history. **If that password is used anywhere else, change it.**

## Step 3 — features removed (code, route, handler, template, CSS/JS)
| Feature | Removed | Why |
|---|---|---|
| Server logs (admin page, live feed, SQL log, masking) | `core/log.*`, `handlers/admin_logs.cpp`, `static/js/logs.js`, `admin/logs*.html` | Large and not part of the learning goal. Replaced by one line in `worker()`: `[Worker N] METHOD /path` (printed under a mutex). |
| Admin database-table browser | `readTable`, `browsableTables`, `tableRowCounts`, `admin/tables.html`, `admin/table_view.html` | Extra feature; MySQL Workbench does this. |
| Admin sessions page, role editing | `/admin/sessions`, `/admin/sessions/end`, `/admin/users/role`, `admin/sessions.html`, role form | Extra. `server.exe --make-admin <email>` kept. |
| Auto-delivery | background thread, `deliverDueOrders`, random delay, `orders.deliver_at`, countdown JS, delivery settings | Extra. The admin sets DELIVERED by hand. Customer cancel (before DELIVERED) is kept and still restores stock and cancels the payment. |
| Migration code (`migrateDatabase`, `ensureColumn`) | `db/database.cpp` | The `role` column is now in the `users` `CREATE TABLE` in `database/ecommerce_db.sql`. |
| Plain-text password upgrade at login | `user_service.cpp` | One hashing path only (`sha256$salt$hash`). |
| Separate "change payment status" action | `/admin/order/payment`, `updatePaymentStatus`, form | Payment follows the order: DELIVERED → PAID, CANCELLED → CANCELLED. |
| Admin dashboard widgets | recent orders, orders-by-status, low-stock table, restock box, active sessions | Dashboard is now simple counts. |

## Step 4 — code simplified
* **Routing:** `core/router.*` and `routes.*` deleted (`Router`, `std::function`, `requireLogin`, `requireAdmin`). One `if` chain
  in `handleRequest()` (`app.cpp`); each protected handler checks login/role in its first line.
* **Template engine:** only `{{name}}` (escaped), `{{{name}}}` (raw) and `{{> file}}`. Removed `#if`, `#each`, nested scopes, block
  search. `Data` (lists of nested values) became a plain `map<string,string>` called `Vars`. Handlers build repeated HTML by
  rendering a small row template once per row.
* **Database locking:** `recursive_mutex`, `DbLock`, `Transaction` removed. One `std::mutex dbMutex`; each service function starts
  with `lock_guard<mutex> lock(dbMutex);`. `placeOrder`, status changes and `deleteProduct` write `START TRANSACTION`,
  `COMMIT`, `ROLLBACK` explicitly. `Database` class → plain functions `dbConnect/dbRun/dbRows/dbQuote`.
* **Helpers:** one `formatMoney` (was two overloads), one `statusText`; `toPaise`/`paiseToText` removed — totals are summed by MySQL.
  `ProductFilter`, `OrderFilter`, `ProductInput` structs replaced by plain parameters. Category cache removed (a 15-row query per page instead).
* **Config:** `config.cpp` + environment overrides → `config.h` constants; only the DB password comes from the environment.
  Removed env vars: `ECOM_DB_HOST/USER/NAME/PORT`, `ECOM_DELIVERY_*`, `ECOM_LOG_SQL`. The `frontend` folder search was removed
  (run the server from the project folder).
* **Merged files:** category functions → `product_service`; the five `services/*.h` → `services/services.h`; `handlers/view.h` →
  `handlers/handlers.h`; cart + order handlers → `customer_handlers.cpp`; dashboard + orders + users handlers → `admin_handlers.cpp`.
* No lambdas, `std::function`, `thread_local`, `std::atomic`, `std::deque`, templates or RAII wrapper classes remain in `backend/`.

## Step 5 — frontend
* `admin.css` merged into `style.css`; unused rules removed (`.msg`, `.navbtn`, `.pager`, `.track`, countdown/log styles).
  `app.js` only has the login pop-up, the quantity +/- buttons and the auto-hiding message bar.

## Step 6 — tests and docs
* Added `tests/concurrency_test.bat` and `tests/concurrency_test.md`; `README.md` rewritten (short);
  `docs/viva_questions.md` added; this file added.

## URLs that no longer exist (they now answer 404)
`GET /admin/logs`, `GET /admin/logs/feed`, `POST /admin/logs/clear`, `GET /admin/tables`, `GET /admin/table`,
`GET /admin/sessions`, `POST /admin/sessions/end`, `POST /admin/users/role`, `POST /admin/order/payment`.

## Things to know
* **Existing databases:** `orders.deliver_at` (added earlier by the auto-delivery feature) is no longer used; it is harmless and can be
  dropped with `ALTER TABLE orders DROP COLUMN deliver_at;`. The `users.role` column must exist — a database created from the
  old `ecommerce_db.sql` and run by the earlier server already has it; a fresh database gets it from the new SQL file.
* **Old plain-text accounts** (created before passwords were hashed) can no longer log in, because the upgrade path was removed.
  Sign up again, or set a hashed password.
* **Worker count:** `WORKER_COUNT` in `config.h` is 10 (that was the value in the project before this work); change it there.
* **`docs/Server_Sync_How_It_Works.pdf` is out of date** (it describes the removed router, template loops, logs and auto-delivery).
  It was not part of the cleanup list, so it was left as it is; delete or regenerate it before the viva.
