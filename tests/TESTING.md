# Testing the server

`test_server.py` checks the running server end to end over real HTTP. It covers pages,
signup and login, sessions, the cart, checkout, 60 simultaneous requests, and a race where
10 buyers try to buy 3 items at once. It uses **only the Python standard library**, so there
is nothing to `pip install`.

## What you need

- MySQL Server 8.0 running on `localhost:3306` (the same setup you build the project with)
- g++ (MinGW), the same compiler as the VS Code build task
- Python 3.8 or newer (`python --version`)

## Steps (Windows, PowerShell, from the project folder)

**1. Load a fresh database.** This resets all data, which makes results repeatable.

```powershell
mysql -u root -p -e "DROP DATABASE IF EXISTS ecommerce_db"
Get-Content ecommerce_db.sql | mysql -u root -p
```

> `ecommerce_db.sql` must be the fixed version, where every comment starts with `-- `
> (dash, dash, space). The old file had `--Stores…` on line 5 and `--- 7. ORDERS`, which
> make MySQL stop with error 1064 before any table is created.

**2. Build the current code.** Use `Ctrl+Shift+B` in VS Code, or run:

```powershell
g++ -g main.cpp -o server.exe -I "C:/Program Files/MySQL/MySQL Server 8.0/include" -L "C:/Program Files/MySQL/MySQL Server 8.0/lib" -lmysql -lws2_32 -static-libgcc -static-libstdc++
```

**3. Start the server** in its own terminal and leave it running:

```powershell
.\server.exe
```

You should see `Server on http://localhost:8080`.

**4. Run the tests** in a second terminal:

```powershell
# HTTP-only checks
python tests/test_server.py

# Full run: adds database checks and the overselling race test
python tests/test_server.py --mysql "C:/Program Files/MySQL/MySQL Server 8.0/bin/mysql.exe" --db-password YOUR_ROOT_PASSWORD
```

The script exits with code 0 if everything passed and 1 otherwise. At the end it lists
every failure with the reason.

## What is tested

| # | Section | Checks |
|---|---|---|
| 1 | Pages and routing | Home page 200, unknown URL 404, products page lists products |
| 2 | Signup and login | New account, duplicate email, empty fields, wrong password, unknown email, SQL-injection attempt, 32-hex session id, `HttpOnly` + `SameSite=Lax` cookie |
| 3 | XSS | A name like `<script>` is shown escaped, not executed |
| 4 | Sessions | Protected pages require login, fake ids rejected, two browsers = two sessions, logout ends only that session and expires the cookie. With `--mysql`: the row is kept with `is_active=0`, and `last_activity` updates |
| 5 | Cart and checkout | Invalid, missing, zero and over-stock quantities rejected; same product adds up; remove; checkout; order shows COD / PENDING; empty-cart checkout refused. With `--mysql`: stock drops by exactly the quantity bought, order PLACED, cart CHECKED_OUT, payment = total, price snapshot stored |
| 6 | Thread pool | 60 simultaneous requests all answered |
| 7 | Race condition (`--mysql` only) | Stock set to 3, then 10 users check out at the same instant. Exactly 3 succeed, 7 get "Insufficient stock", stock ends at 0 (never negative). Stock is restored afterwards |

## Expected result with the current code

One failure is **expected** until the bug is fixed:

```
FAILED: GET /products lists products -> products query failed: Unknown column 'category' in 'field list'
```

`buildProductsPage()` in `main.cpp` selects a column named `category`, but the table only
has `category_id`. Fix: join the `categories` table and select `c.category_name`.

## Notes

- Each run creates new users with unique emails, so you can run it again without resetting.
- Each run permanently buys 5 units of product 1 (Wireless Mouse, starting stock 45). After
  about 8 runs, reload the database (step 1).
- Only run this against a **test** database, never one with real data.
