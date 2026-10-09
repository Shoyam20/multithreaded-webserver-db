"""
End-to-end tests for the multithreaded web server.

Talks to a running server over real HTTP (standard library only, no pip installs).
Optional --mysql enables database-level checks (stock, row counts) and the
concurrent-checkout overselling test.

Usage:
    python tests/test_server.py
    python tests/test_server.py --mysql "C:/path/to/mysql.exe" --db-password <pw>

Prerequisites: ecommerce_db.sql loaded, server.exe running on localhost:8080.
Every run creates fresh users (unique emails), so it can be re-run.
"""
import argparse
import http.client
import os
import re
import subprocess
import sys
import threading
import time
import urllib.parse

HOST, PORT = "localhost", 8080
RUN = str(int(time.time()))           # makes emails unique per run
PASSWORD = "Test@123"

results = []                          # (name, passed, detail)


# ------------------------------------------------------------------ helpers
def request(method, path, cookie=None, form=None):
    """One HTTP request. Returns (status, headers dict lower-cased, body)."""
    conn = http.client.HTTPConnection(HOST, PORT, timeout=20)
    headers = {}
    body = None
    if cookie:
        headers["Cookie"] = "session_id=" + cookie
    if form is not None:
        body = urllib.parse.urlencode(form)
        headers["Content-Type"] = "application/x-www-form-urlencoded"
    try:
        conn.request(method, path, body=body, headers=headers)
        r = conn.getresponse()
        data = r.read().decode("utf-8", "replace")
        hdrs = {k.lower(): v for k, v in r.getheaders()}
        return r.status, hdrs, data
    except (OSError, http.client.HTTPException) as e:
        # A dropped connection is a test failure, not a crash of the suite.
        return 0, {}, f"<connection error: {e!r}>"
    finally:
        conn.close()


def get(path, cookie=None):
    return request("GET", path, cookie)


def check(name, condition, detail=""):
    detail = str(detail)
    if len(detail) > 160:
        detail = detail[:157] + "..."
    results.append((name, bool(condition), detail))
    mark = "PASS" if condition else "FAIL"
    print(f"  [{mark}] {name}" + (f"  ({detail})" if detail and not condition else ""))
    return bool(condition)


def signup(name, email):
    return request("POST", "/signup", form={"name": name, "email": email, "password": PASSWORD})


def login(email, password=PASSWORD):
    """Returns (session_id or None, set-cookie header, body)."""
    _, h, body = request("POST", "/login", form={"email": email, "password": password})
    sc = h.get("set-cookie", "")
    m = re.search(r"session_id=([0-9a-f]+)", sc)
    return (m.group(1) if m else None), sc, body


def new_user(tag):
    email = f"{tag}.{RUN}@test.local"
    signup(f"Tester {tag}", email)
    sid, _, _ = login(email)
    return email, sid


def cart_quantity(cookie, product_id):
    _, _, body = get("/cart", cookie)
    m = re.search(rf"<tr><td>{product_id}</td><td>[^<]*</td><td>(\d+)</td>", body)
    return int(m.group(1)) if m else 0


class DB:
    """Thin wrapper over the mysql command-line client (optional)."""

    def __init__(self, exe, password, host="127.0.0.1", port=3306):
        self.exe, self.host, self.port = exe, host, str(port)
        self.env = dict(os.environ, MYSQL_PWD=password)

    def q(self, sql):
        out = subprocess.run(
            [self.exe, "-uroot", "-h", self.host, "-P", self.port, "-N", "-B", "ecommerce_db", "-e", sql],
            capture_output=True, text=True, env=self.env, check=True)
        return out.stdout.strip()


# ------------------------------------------------------------------ tests
def test_basic_pages():
    print("\n1. Basic pages and routing")
    s, _, b = get("/")
    check("GET / returns 200 with home page", s == 200 and "Welcome" in b)
    s, _, _ = get("/no-such-page")
    check("Unknown path returns 404", s == 404, f"got {s}")
    s, _, b = get("/products")
    ok = s == 200 and "Wireless Mouse" in b
    check("GET /products lists products", ok,
          "products query failed: " + (re.search(r"<p>(.*?)</p>", b).group(1) if not ok and re.search(r"<p>(.*?)</p>", b) else b[:120]))


def test_signup_login():
    print("\n2. Signup and login")
    email = f"alice.{RUN}@test.local"
    _, _, b = signup("Alice", email)
    check("Signup with new email succeeds", "Signup successful" in b)
    _, _, b = signup("Alice again", email)
    check("Duplicate email is rejected", "already registered" in b)
    _, _, b = request("POST", "/signup", form={"name": "", "email": "x@y.z", "password": ""})
    check("Signup with empty fields is rejected", "All fields are required" in b)
    sid, _, b = login(email, "wrong-password")
    check("Wrong password is rejected, no session", sid is None and "Incorrect password" in b)
    sid, _, b = login(f"nobody.{RUN}@test.local")
    check("Unknown email is rejected", sid is None and "No account found" in b)
    sid, _, b = login("' OR '1'='1")
    check("SQL injection in email does not log in", sid is None)
    sid, sc, b = login(email)
    check("Correct login creates a session", sid is not None and "You are now logged in" in b)
    check("Session id is 32 hex characters (128 bits)", sid is not None and len(sid) == 32)
    check("Cookie is HttpOnly and SameSite=Lax", "HttpOnly" in sc and "SameSite=Lax" in sc, sc)
    _, _, b = get("/", sid)
    check("Logged-in page shows user's name in nav", "Hi, Alice" in b)


def test_xss():
    print("\n3. Output escaping (XSS)")
    email = f"xss.{RUN}@test.local"
    signup("<script>alert(1)</script>", email)
    sid, _, _ = login(email)
    _, _, b = get("/", sid)
    check("User-supplied name is HTML-escaped", "&lt;script&gt;" in b and "<script>alert(1)" not in b)


def test_sessions(db):
    print("\n4. Sessions")
    for path in ("/cart", "/orders", "/session", "/checkout", "/add-cart?product_id=1"):
        _, _, b = get(path)
        check(f"{path} without login asks to log in", "Please log in first" in b)
    _, _, b = get("/cart", "0" * 32)
    check("Made-up session id is rejected", "Please log in first" in b)

    email = f"bob.{RUN}@test.local"
    signup("Bob", email)
    chrome, _, _ = login(email)
    edge, _, _ = login(email)
    check("Two logins give two different sessions", chrome and edge and chrome != edge)
    _, _, b = get("/session", chrome)
    check("/session marks the current browser", "(this browser)" in b and edge in b and chrome in b)

    s, h, b = get("/logout", chrome)
    check("Logout succeeds", "logged out successfully" in b)
    check("Logout expires the cookie", "1970" in h.get("set-cookie", ""), h.get("set-cookie", ""))
    _, _, b = get("/cart", chrome)
    check("Logged-out session is rejected", "Please log in first" in b)
    _, _, b = get("/cart", edge)
    check("Other session (other browser) still works", "Please log in first" not in b)
    _, _, b = get("/logout", chrome)
    check("Logging out twice reports inactive session", "already inactive" in b)

    if db:
        row = db.q(f"SELECT is_active, logout_time IS NOT NULL FROM user_sessions WHERE session_id='{chrome}'")
        check("DB: logged-out row kept with is_active=0 and logout_time set", row.split() == ["0", "1"], row)
        before = db.q(f"SELECT last_activity FROM user_sessions WHERE session_id='{edge}'")
        time.sleep(1.2)
        get("/", edge)
        after = db.q(f"SELECT last_activity FROM user_sessions WHERE session_id='{edge}'")
        check("DB: last_activity updates on each request", after > before, f"{before} -> {after}")


def test_cart_and_checkout(db):
    print("\n5. Cart and checkout")
    _, sid = new_user("carol")
    _, _, b = get("/add-cart?product_id=abc", sid)
    check("Non-numeric product id rejected", "Invalid product ID" in b)
    _, _, b = get("/add-cart?product_id=99999", sid)
    check("Non-existent product rejected", "Product not found" in b)
    _, _, b = get("/add-cart?product_id=1&quantity=0", sid)
    check("Quantity 0 rejected", "greater than 0" in b)
    _, _, b = get("/add-cart?product_id=1&quantity=100000", sid)
    check("Quantity above stock rejected", "exceeds available stock" in b)

    stock_before = int(db.q("SELECT stock FROM products WHERE product_id=1")) if db else None

    _, _, b = get("/add-cart?product_id=1&quantity=2", sid)
    check("Add to cart succeeds", "added to cart successfully" in b)
    get("/add-cart?product_id=1&quantity=3", sid)
    check("Adding same product again sums quantity (2+3=5)", cart_quantity(sid, 1) == 5, f"got {cart_quantity(sid, 1)}")
    get("/add-cart?product_id=2&quantity=1", sid)
    present = cart_quantity(sid, 2) == 1
    get("/remove-cart?product_id=2", sid)
    check("Remove from cart works", present and cart_quantity(sid, 2) == 0)

    _, _, b = get("/checkout", sid)
    m = re.search(r"Order ID: <b>(\d+)</b>", b)
    check("Checkout places the order", "Order placed successfully" in b and m)
    order_id = m.group(1) if m else "?"
    _, _, b = get("/orders", sid)
    check("Order appears in /orders with COD / PENDING",
          re.search(rf"<td>{order_id}</td>.*?COD</td><td>PENDING", b, re.S) is not None)
    _, _, b = get("/checkout", sid)
    check("Second checkout finds no active cart", "No active cart" in b)

    if db and m:
        stock_after = int(db.q("SELECT stock FROM products WHERE product_id=1"))
        check("DB: stock reduced by exactly 5", stock_before - stock_after == 5, f"{stock_before} -> {stock_after}")
        row = db.q(f"SELECT o.status, c.status, p.amount = o.total, "
                   f"(SELECT COUNT(*) FROM cart_items ci WHERE ci.cart_id=o.cart_id), "
                   f"(SELECT price FROM order_items WHERE order_id=o.order_id AND product_id=1) "
                   f"FROM orders o JOIN cart c ON c.cart_id=o.cart_id JOIN payments p ON p.order_id=o.order_id "
                   f"WHERE o.order_id={order_id}")
        parts = row.split()
        check("DB: order PLACED, cart CHECKED_OUT, payment = total, cart emptied",
              parts[:4] == ["PLACED", "CHECKED_OUT", "1", "0"], row)
        check("DB: order_items stores the price snapshot", parts[4:5] == ["599.00"], row)

    get("/add-cart?product_id=1&quantity=1", sid)
    _, _, b = get("/cart", sid)
    check("After checkout, a new ACTIVE cart is created", cart_quantity(sid, 1) == 1)
    get("/remove-cart?product_id=1", sid)
    _, _, b = get("/checkout", sid)
    check("Checkout of an empty cart is refused", "Your cart is empty" in b)


def test_concurrency():
    print("\n6. Concurrency (thread pool)")
    N = 60
    statuses, errors = [], []
    lock = threading.Lock()

    def hit():
        try:
            s, _, _ = get("/")
            with lock:
                statuses.append(s)
        except Exception as e:  # noqa: BLE001
            with lock:
                errors.append(repr(e))

    t0 = time.time()
    threads = [threading.Thread(target=hit) for _ in range(N)]
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    dt = time.time() - t0
    check(f"{N} simultaneous requests all answered 200", statuses.count(200) == N and not errors,
          f"{statuses.count(200)} ok, errors={errors[:2]}")
    print(f"         ({N} requests in {dt:.2f}s)")


def test_oversell(db):
    print("\n7. Race condition: 10 buyers, 3 items in stock")
    if not db:
        print("  [SKIP] needs --mysql to set and verify stock")
        return
    PID, STOCK, BUYERS = 7, 3, 10
    original = db.q(f"SELECT stock FROM products WHERE product_id={PID}")
    db.q(f"UPDATE products SET stock={STOCK} WHERE product_id={PID}")
    try:
        sids = []
        for i in range(BUYERS):
            _, sid = new_user(f"buyer{i}")
            get(f"/add-cart?product_id={PID}&quantity=1", sid)
            sids.append(sid)
        check("All 10 buyers have the item in their cart", all(cart_quantity(s, PID) == 1 for s in sids))

        barrier = threading.Barrier(BUYERS)
        outcomes = []
        lock = threading.Lock()

        def buy(sid):
            barrier.wait()                     # release all checkouts at the same instant
            _, _, b = get("/checkout", sid)
            with lock:
                outcomes.append("ok" if "Order placed" in b else
                                "nostock" if "Insufficient stock" in b or "Stock update failed" in b else b[:80])

        threads = [threading.Thread(target=buy, args=(s,)) for s in sids]
        for t in threads:
            t.start()
        for t in threads:
            t.join()
        check("Exactly 3 checkouts succeed", outcomes.count("ok") == STOCK, str(outcomes))
        check("The other 7 get 'Insufficient stock'", outcomes.count("nostock") == BUYERS - STOCK, str(outcomes))
        stock = int(db.q(f"SELECT stock FROM products WHERE product_id={PID}"))
        check("DB: stock is exactly 0, never negative", stock == 0, f"stock={stock}")
        sold = int(db.q(f"SELECT COALESCE(SUM(oi.quantity),0) FROM order_items oi JOIN orders o ON o.order_id=oi.order_id "
                        f"JOIN users u ON u.user_id=o.user_id WHERE oi.product_id={PID} AND u.email LIKE 'buyer%.{RUN}@test.local'"))
        check("DB: exactly 3 units sold, no half-finished orders", sold == STOCK, f"sold={sold}")
    finally:
        db.q(f"UPDATE products SET stock={original} WHERE product_id={PID}")


# ------------------------------------------------------------------ main
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mysql", help="path to mysql.exe (enables DB checks)")
    ap.add_argument("--db-password", default=os.environ.get("MYSQL_PWD", ""))
    args = ap.parse_args()

    try:
        get("/")
    except OSError:
        sys.exit(f"Server not reachable on http://{HOST}:{PORT}. Start server.exe first.")

    db = DB(args.mysql, args.db_password) if args.mysql else None
    print(f"Testing http://{HOST}:{PORT}  (run id {RUN}, DB checks {'on' if db else 'off'})")

    for section in (test_basic_pages, test_signup_login, test_xss,
                    lambda: test_sessions(db), lambda: test_cart_and_checkout(db),
                    test_concurrency, lambda: test_oversell(db)):
        try:
            section()
        except Exception as e:  # noqa: BLE001  keep going so one bug doesn't hide the rest
            check("section finished without an exception", False, repr(e))

    passed = sum(1 for _, ok, _ in results if ok)
    failed = [(n, d) for n, ok, d in results if not ok]
    print(f"\n{passed}/{len(results)} passed")
    for n, d in failed:
        print(f"  FAILED: {n}" + (f" -> {d}" if d else ""))
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
