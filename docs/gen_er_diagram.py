"""Generate ER diagrams (Chen + Crow's Foot) as SVG for ecommerce_db.

Usage: python docs/gen_er_diagram.py docs
Keep TABLES / REL / ATTR in sync with ecommerce_db.sql.
"""
import math, sys, os
from xml.sax.saxutils import escape

OUT = sys.argv[1]
FONT = "font-family='Segoe UI, Helvetica, Arial, sans-serif'"
INK = "#1f2937"
HEAD = "#1e3a5f"

# ---------------------------------------------------------------- crow's foot
TABLES = {
    "users": [("PK", "user_id", "INT", "AI"), ("UQ", "email", "VARCHAR(100)", "NN"),
              ("", "name", "VARCHAR(100)", "NN"), ("", "password", "VARCHAR(255)", "NN"),
              ("", "created_at", "TIMESTAMP", "now")],
    "user_sessions": [("PK", "session_id", "VARCHAR(64)", "NN"), ("FK", "user_id", "INT", "NN"),
                      ("", "login_time", "TIMESTAMP", "now"), ("", "logout_time", "TIMESTAMP", "NULL"),
                      ("", "last_activity", "TIMESTAMP", "now"), ("", "is_active", "BOOLEAN", "TRUE")],
    "cart": [("PK", "cart_id", "INT", "AI"), ("FK", "user_id", "INT", "NN"),
             ("", "status", "VARCHAR(20)", "'ACTIVE'"), ("", "created_at", "TIMESTAMP", "now")],
    "cart_items": [("PK", "cart_item_id", "INT", "AI"), ("FK", "cart_id", "INT", "NN"),
                   ("FK", "product_id", "INT", "NN"), ("", "quantity", "INT", "1")],
    "payments": [("PK", "payment_id", "INT", "AI"), ("FK UQ", "order_id", "INT", "NN"),
                 ("", "amount", "DECIMAL(10,2)", "NN"), ("", "method", "VARCHAR(30)", "'COD'"),
                 ("", "status", "VARCHAR(20)", "'PENDING'"), ("", "paid_at", "TIMESTAMP", "now")],
    "orders": [("PK", "order_id", "INT", "AI"), ("FK", "user_id", "INT", "NN"),
               ("FK UQ", "cart_id", "INT", "NN"), ("", "total", "DECIMAL(10,2)", "NN"),
               ("", "status", "VARCHAR(20)", "'PENDING'"), ("", "created_at", "TIMESTAMP", "now")],
    "order_items": [("PK", "order_item_id", "INT", "AI"), ("FK", "order_id", "INT", "NN"),
                    ("FK", "product_id", "INT", "NN"), ("", "quantity", "INT", "NN"),
                    ("", "price", "DECIMAL(10,2)", "NN")],
    "products": [("PK", "product_id", "INT", "AI"), ("", "name", "VARCHAR(100)", "NN"),
                 ("", "description", "VARCHAR(255)", "NULL"), ("", "price", "DECIMAL(10,2)", "NN"),
                 ("", "stock", "INT", "0"), ("FK", "category_id", "INT", "NN")],
    "categories": [("PK", "category_id", "INT", "AI"), ("UQ", "category_name", "VARCHAR(50)", "NN")],
}
FOOTERS = {
    "user_sessions": "IDX (user_id) · (is_active) · (login_time)",
    "cart_items": "UNIQUE (cart_id, product_id)",
    "order_items": "UNIQUE (order_id, product_id)",
    "cart": "rule: ≤ 1 'ACTIVE' cart per user (app-enforced)",
}
BW, HH, RH = 330, 38, 24
COLS = [40, 470, 900, 1330]
GRID = {"user_sessions": (0, 0), "users": (1, 0), "cart": (2, 0), "cart_items": (3, 0),
        "payments": (0, 1), "orders": (1, 1), "order_items": (2, 1), "products": (3, 1),
        "categories": (3, 2)}


def box_h(t):
    return HH + RH * (len(TABLES[t]) + (1 if t in FOOTERS else 0)) + 8


ROW_Y = [110]
for r in (0, 1):
    ROW_Y.append(ROW_Y[r] + max(box_h(t) for t, (c, rr) in GRID.items() if rr == r) + 100)


def pos(t):
    c, r = GRID[t]
    return COLS[c], ROW_Y[r]


def row_y(t, col):
    x, y = pos(t)
    i = [a[1] for a in TABLES[t]].index(col)
    return y + HH + RH * i + RH / 2 + 4


def marker(x, y, dx, dy, kind):
    """Crow's-foot symbol at endpoint (x,y); (dx,dy) points from box into the line."""
    px, py = -dy, dx
    s = []
    def bar(d):
        cx, cy = x + dx * d, y + dy * d
        s.append(f"<line x1='{cx+px*8}' y1='{cy+py*8}' x2='{cx-px*8}' y2='{cy-py*8}' stroke='{INK}' stroke-width='1.6'/>")
    def circ(d):
        s.append(f"<circle cx='{x+dx*d}' cy='{y+dy*d}' r='5' fill='white' stroke='{INK}' stroke-width='1.6'/>")
    def foot():
        tx, ty = x + dx * 14, y + dy * 14
        for k in (-9, 0, 9):
            s.append(f"<line x1='{tx}' y1='{ty}' x2='{x+px*k}' y2='{y+py*k}' stroke='{INK}' stroke-width='1.6'/>")
    if kind == "one":
        bar(8); bar(14)
    elif kind == "zero_or_one":
        bar(8); circ(22)
    elif kind == "zero_or_many":
        foot(); circ(22)
    elif kind == "one_or_many":
        foot(); bar(20)
    return "".join(s)


def connector(points, start_kind, end_kind, label, lx, ly, anchor="middle"):
    s = ["<polyline points='" + " ".join(f"{x},{y}" for x, y in points) +
         f"' fill='none' stroke='{INK}' stroke-width='1.6'/>"]
    for (p, q, kind) in ((points[0], points[1], start_kind), (points[-1], points[-2], end_kind)):
        d = math.hypot(q[0] - p[0], q[1] - p[1])
        s.append(marker(p[0], p[1], (q[0] - p[0]) / d, (q[1] - p[1]) / d, kind))
    s.append(f"<text x='{lx}' y='{ly}' {FONT} font-size='12' font-style='italic' fill='#7c2d12' text-anchor='{anchor}'>{escape(label)}</text>")
    return "".join(s)


def table_svg(t):
    x, y = pos(t)
    h = box_h(t)
    s = [f"<rect x='{x+3}' y='{y+3}' width='{BW}' height='{h}' rx='6' fill='#00000018'/>",
         f"<rect x='{x}' y='{y}' width='{BW}' height='{h}' rx='6' fill='white' stroke='{HEAD}' stroke-width='1.5'/>",
         f"<path d='M{x} {y+HH} V{y+6} Q{x} {y} {x+6} {y} H{x+BW-6} Q{x+BW} {y} {x+BW} {y+6} V{y+HH} Z' fill='{HEAD}'/>",
         f"<text x='{x+BW/2}' y='{y+25}' {FONT} font-size='15' font-weight='700' fill='white' text-anchor='middle'>{t}</text>"]
    for i, (k, name, typ, flag) in enumerate(TABLES[t]):
        ry = y + HH + RH * i
        if i % 2:
            s.append(f"<rect x='{x+1}' y='{ry}' width='{BW-2}' height='{RH}' fill='#f1f5f9'/>")
        ty = ry + 16
        if k:
            colr = {"PK": "#b45309", "FK": "#1d4ed8", "UQ": "#047857"}[k.split()[0]]
            s.append(f"<text x='{x+10}' y='{ty}' {FONT} font-size='10.5' font-weight='700' fill='{colr}'>{k}</text>")
        weight = "700" if k.startswith("PK") else "400"
        deco = " text-decoration='underline'" if k.startswith("PK") else ""
        s.append(f"<text x='{x+58}' y='{ty}' {FONT} font-size='13' font-weight='{weight}' fill='{INK}'{deco}>{name}</text>")
        s.append(f"<text x='{x+178}' y='{ty}' font-family='Consolas, monospace' font-size='11.5' fill='#475569'>{typ}</text>")
        s.append(f"<text x='{x+BW-10}' y='{ty}' {FONT} font-size='10.5' fill='#64748b' text-anchor='end'>{escape(flag)}</text>")
    if t in FOOTERS:
        fy = y + HH + RH * len(TABLES[t])
        s.append(f"<line x1='{x}' y1='{fy}' x2='{x+BW}' y2='{fy}' stroke='#cbd5e1'/>")
        s.append(f"<text x='{x+10}' y='{fy+16}' {FONT} font-size='11' font-style='italic' fill='#475569'>{escape(FOOTERS[t])}</text>")
    return "".join(s)


def crowsfoot():
    W = COLS[-1] + BW + 40
    H = ROW_Y[2] + 230
    s = [f"<svg xmlns='http://www.w3.org/2000/svg' width='{W}' height='{H}' viewBox='0 0 {W} {H}'>",
         f"<rect width='{W}' height='{H}' fill='white'/>",
         f"<text x='40' y='45' {FONT} font-size='24' font-weight='700' fill='{HEAD}'>ecommerce_db — Relational ER Diagram (Crow's Foot)</text>",
         f"<text x='40' y='72' {FONT} font-size='13' fill='#475569'>9 tables · 10 foreign-key relationships · generated from ecommerce_db.sql</text>"]
    R = lambda t: pos(t)[0] + BW
    L = lambda t: pos(t)[0]
    T = lambda t: pos(t)[1]
    B = lambda t: pos(t)[1] + box_h(t)
    gap = lambda a, b: (R(a) + L(b)) / 2
    cx = lambda t, f=0.5: pos(t)[0] + BW * f

    def horiz(parent, child, fk, label, kind="zero_or_many"):
        py, cy = row_y(parent, TABLES[parent][0][1]), row_y(child, fk)
        if L(parent) > L(child):  # parent on the right
            xp, xc = L(parent), R(child)
        else:
            xp, xc = R(parent), L(child)
        mx = (xp + xc) / 2
        pts = [(xp, py), (mx, py), (mx, cy), (xc, cy)]
        return connector(pts, "one", kind, label, mx, min(py, cy) - 8)

    def vert(parent, child, x, label, kind="zero_or_many"):
        if T(parent) > T(child):
            pts = [(x, T(parent)), (x, B(child))]
        else:
            pts = [(x, B(parent)), (x, T(child))]
        return connector(pts, "one", kind, label, x + 8, (pts[0][1] + pts[1][1]) / 2 + 4, "start")

    rels = [
        horiz("users", "user_sessions", "user_id", "has"),
        horiz("users", "cart", "user_id", "owns"),
        horiz("cart", "cart_items", "cart_id", "contains"),
        horiz("orders", "payments", "order_id", "paid by", "zero_or_one"),
        horiz("orders", "order_items", "order_id", "includes"),
        horiz("products", "order_items", "product_id", "sold as"),
        vert("users", "orders", cx("users", 0.3), "places"),
        vert("products", "cart_items", cx("products"), "added to"),
        vert("categories", "products", cx("categories"), "classifies"),
    ]
    # cart (1) -> orders (0..1): elbow through the gap between rows
    gy = (B("user_sessions") + T("orders")) / 2 + 10
    x1, x2 = cx("cart", 0.25), cx("orders", 0.75)
    rels.append(connector([(x1, B("cart")), (x1, gy), (x2, gy), (x2, T("orders"))],
                          "one", "zero_or_one", "checked out as", (x1 + x2) / 2, gy - 8))

    s += [table_svg(t) for t in TABLES]
    s += rels

    # legend
    lx, ly = 40, ROW_Y[2]
    s.append(f"<rect x='{lx}' y='{ly}' width='{BW*2+100}' height='200' rx='6' fill='#f8fafc' stroke='#cbd5e1'/>")
    s.append(f"<text x='{lx+16}' y='{ly+26}' {FONT} font-size='14' font-weight='700' fill='{HEAD}'>Legend</text>")
    items = [("one", "exactly one (mandatory)"), ("zero_or_one", "zero or one (optional, FK is UNIQUE)"),
             ("zero_or_many", "zero or many"), ("one_or_many", "one or many")]
    for i, (k, txt) in enumerate(items):
        yy = ly + 52 + i * 26
        s.append(f"<line x1='{lx+20}' y1='{yy}' x2='{lx+80}' y2='{yy}' stroke='{INK}' stroke-width='1.6'/>")
        s.append(marker(lx + 80, yy, -1, 0, k))
        s.append(f"<text x='{lx+96}' y='{yy+4}' {FONT} font-size='12' fill='{INK}'>{txt}</text>")
    keys = [("PK", "#b45309", "primary key"), ("FK", "#1d4ed8", "foreign key"), ("UQ", "#047857", "unique"),
            ("AI", "#64748b", "AUTO_INCREMENT"), ("NN", "#64748b", "NOT NULL"), ("now", "#64748b", "DEFAULT CURRENT_TIMESTAMP"),
            ("'x'", "#64748b", "DEFAULT value")]
    for i, (k, colr, txt) in enumerate(keys):
        yy = ly + 56 + i * 20
        s.append(f"<text x='{lx+400}' y='{yy}' {FONT} font-size='11' font-weight='700' fill='{colr}'>{escape(k)}</text>")
        s.append(f"<text x='{lx+440}' y='{yy}' {FONT} font-size='12' fill='{INK}'>{txt}</text>")
    s.append("</svg>")
    return "".join(s), W, H


# ---------------------------------------------------------------- Chen
def chen():
    W, H = 1720, 1060
    s = [f"<svg xmlns='http://www.w3.org/2000/svg' width='{W}' height='{H}' viewBox='0 0 {W} {H}'>",
         f"<rect width='{W}' height='{H}' fill='white'/>",
         f"<text x='40' y='45' {FONT} font-size='24' font-weight='700' fill='{HEAD}'>ecommerce_db — Conceptual ER Diagram (Chen Notation)</text>",
         f"<text x='40' y='72' {FONT} font-size='13' fill='#475569'>Entities, attributes, relationships, cardinality ratios and participation constraints</text>"]
    E = {"USER": (520, 290), "SESSION": (170, 290), "CART": (960, 290), "PRODUCT": (1380, 520),
         "CATEGORY": (1380, 880), "ORDER": (520, 720), "PAYMENT": (170, 720)}
    EW, EH = 140, 50
    # relationships: name, center, (entity, cardinality, total?) x2, attrs
    REL = [
        ("HAS", (345, 290), ("USER", "1", False), ("SESSION", "N", True), []),
        ("OWNS", (740, 290), ("USER", "1", False), ("CART", "N", True), []),
        ("CONTAINS", (1180, 290), ("CART", "M", False), ("PRODUCT", "N", False), [("quantity", 1180, 190)]),
        ("PLACES", (520, 505), ("USER", "1", False), ("ORDER", "N", True), []),
        ("CHECKED_OUT\nAS", (760, 505), ("CART", "1", False), ("ORDER", "1", True), []),
        ("PAID_BY", (345, 720), ("ORDER", "1", False), ("PAYMENT", "1", True), []),
        ("INCLUDES", (980, 720), ("ORDER", "M", True), ("PRODUCT", "N", False),
         [("quantity", 900, 830), ("unit price", 1060, 830)]),
        ("BELONGS_TO", (1380, 700), ("CATEGORY", "1", False), ("PRODUCT", "N", True), []),
    ]
    # attributes: (label, x, y, kind) kind: key | plain | derived
    ATTR = {
        "USER": [("user_id", 400, 150, "key"), ("email", 500, 130, "plain"), ("name", 610, 150, "plain"),
                 ("password", 660, 205, "plain"), ("created_at", 380, 205, "plain")],
        "SESSION": [("session_id", 90, 160, "key"), ("login_time", 250, 160, "plain"),
                    ("logout_time", 70, 390, "plain"), ("last_activity", 190, 420, "plain"),
                    ("is_active", 320, 390, "plain")],
        "CART": [("cart_id", 870, 175, "key"), ("status", 990, 160, "plain"), ("created_at", 1060, 380, "plain")],
        "PRODUCT": [("product_id", 1560, 410, "key"), ("name", 1600, 470, "plain"),
                    ("description", 1600, 535, "plain"), ("price", 1580, 600, "plain"),
                    ("stock", 1520, 655, "plain")],
        "CATEGORY": [("category_id", 1250, 990, "key"), ("category_name", 1510, 990, "plain")],
        "ORDER": [("order_id", 390, 860, "key"), ("total", 520, 890, "derived"),
                  ("status", 650, 860, "plain"), ("created_at", 680, 800, "plain")],
        "PAYMENT": [("payment_id", 80, 850, "key"), ("amount", 200, 880, "plain"),
                    ("method", 70, 600, "plain"), ("status", 190, 580, "plain"), ("paid_at", 300, 845, "plain")],
    }

    def oval(label, x, y, kind, link):
        w = max(70, len(label) * 7.6 + 24)
        out = [f"<line x1='{x}' y1='{y}' x2='{link[0]}' y2='{link[1]}' stroke='#64748b' stroke-width='1.2'/>",
               f"<ellipse cx='{x}' cy='{y}' rx='{w/2}' ry='17' fill='#fefce8' stroke='#a16207' stroke-width='1.4'"
               + (" stroke-dasharray='5 3'" if kind == "derived" else "") + "/>"]
        deco = " text-decoration='underline' font-weight='700'" if kind == "key" else ""
        out.append(f"<text x='{x}' y='{y+4.5}' {FONT} font-size='12.5' fill='{INK}' text-anchor='middle'{deco}>{escape(label)}</text>")
        return out

    def edge_point(c, toward, w, h):
        dx, dy = toward[0] - c[0], toward[1] - c[1]
        if dx == 0 and dy == 0:
            return c
        t = min(w / 2 / abs(dx) if dx else 1e9, h / 2 / abs(dy) if dy else 1e9)
        return c[0] + dx * t, c[1] + dy * t

    def diamond_point(c, toward, w, h):
        dx, dy = toward[0] - c[0], toward[1] - c[1]
        t = 1 / (abs(dx) / (w / 2) + abs(dy) / (h / 2))
        return c[0] + dx * t, c[1] + dy * t

    lines, shapes, attrs = [], [], []
    for name, rc, a, b, ratts in REL:
        dw = max(110, max(len(p) for p in name.split("\n")) * 9 + 40)
        dh = 64 if "\n" in name else 56
        for ent, card, total in (a, b):
            ec = E[ent]
            p1 = edge_point(ec, rc, EW, EH)
            p2 = diamond_point(rc, ec, dw, dh)
            if total:
                dx, dy = p2[0] - p1[0], p2[1] - p1[1]
                L = math.hypot(dx, dy); ox, oy = -dy / L * 3, dx / L * 3
                for sgn in (1, -1):
                    lines.append(f"<line x1='{p1[0]+ox*sgn}' y1='{p1[1]+oy*sgn}' x2='{p2[0]+ox*sgn}' y2='{p2[1]+oy*sgn}' stroke='{INK}' stroke-width='1.4'/>")
            else:
                lines.append(f"<line x1='{p1[0]}' y1='{p1[1]}' x2='{p2[0]}' y2='{p2[1]}' stroke='{INK}' stroke-width='1.4'/>")
            # cardinality label near entity end, offset perpendicular
            dx, dy = p2[0] - p1[0], p2[1] - p1[1]
            L = math.hypot(dx, dy)
            mx, my = p1[0] + dx / L * 30, p1[1] + dy / L * 30
            nx, ny = -dy / L * 13, dx / L * 13
            shapes.append(f"<text x='{mx+nx}' y='{my+ny+5}' {FONT} font-size='15' font-weight='700' fill='#b91c1c' text-anchor='middle'>{card}</text>")
        pts = f"{rc[0]},{rc[1]-dh/2} {rc[0]+dw/2},{rc[1]} {rc[0]},{rc[1]+dh/2} {rc[0]-dw/2},{rc[1]}"
        shapes.append(f"<polygon points='{pts}' fill='#ecfdf5' stroke='#047857' stroke-width='1.6'/>")
        parts = name.split("\n")
        for i, part in enumerate(parts):
            yy = rc[1] + 4.5 + (i - (len(parts) - 1) / 2) * 15
            shapes.append(f"<text x='{rc[0]}' y='{yy}' {FONT} font-size='12' font-weight='700' fill='#065f46' text-anchor='middle'>{part}</text>")
        for lab, x, y in ratts:
            attrs += oval(lab, x, y, "plain", rc)
    for ent, lst in ATTR.items():
        for lab, x, y, kind in lst:
            attrs += oval(lab, x, y, kind, E[ent])
    ents = []
    for ent, (x, y) in E.items():
        ents.append(f"<rect x='{x-EW/2}' y='{y-EH/2}' width='{EW}' height='{EH}' fill='#eff6ff' stroke='{HEAD}' stroke-width='2'/>")
        ents.append(f"<text x='{x}' y='{y+5.5}' {FONT} font-size='15' font-weight='700' fill='{HEAD}' text-anchor='middle'>{ent}</text>")
    s += attrs + lines + shapes + ents

    # legend
    lx, ly = 760, 900
    s.append(f"<rect x='{lx}' y='{ly}' width='420' height='140' rx='6' fill='#f8fafc' stroke='#cbd5e1'/>")
    s.append(f"<text x='{lx+14}' y='{ly+22}' {FONT} font-size='13' font-weight='700' fill='{HEAD}'>Legend</text>")
    s.append(f"<rect x='{lx+14}' y='{ly+34}' width='46' height='20' fill='#eff6ff' stroke='{HEAD}' stroke-width='2'/>")
    s.append(f"<text x='{lx+70}' y='{ly+48}' {FONT} font-size='12' fill='{INK}'>Entity</text>")
    s.append(f"<polygon points='{lx+37},{ly+62} {lx+60},{ly+74} {lx+37},{ly+86} {lx+14},{ly+74}' fill='#ecfdf5' stroke='#047857' stroke-width='1.5'/>")
    s.append(f"<text x='{lx+70}' y='{ly+78}' {FONT} font-size='12' fill='{INK}'>Relationship</text>")
    s.append(f"<ellipse cx='{lx+37}' cy='{ly+106}' rx='23' ry='11' fill='#fefce8' stroke='#a16207'/>")
    s.append(f"<text x='{lx+70}' y='{ly+110}' {FONT} font-size='12' fill='{INK}'>Attribute (<tspan text-decoration='underline' font-weight='700'>underlined</tspan> = key)</text>")
    s.append(f"<ellipse cx='{lx+37}' cy='{ly+128}' rx='23' ry='9' fill='#fefce8' stroke='#a16207' stroke-dasharray='5 3'/>")
    s.append(f"<text x='{lx+70}' y='{ly+132}' {FONT} font-size='12' fill='{INK}'>Derived attribute</text>")
    c2 = lx + 250
    s.append(f"<line x1='{c2}' y1='{ly+44}' x2='{c2+40}' y2='{ly+44}' stroke='{INK}' stroke-width='1.4'/>")
    s.append(f"<text x='{c2+48}' y='{ly+48}' {FONT} font-size='12' fill='{INK}'>Partial participation</text>")
    for o in (-3, 3):
        s.append(f"<line x1='{c2}' y1='{ly+74+o}' x2='{c2+40}' y2='{ly+74+o}' stroke='{INK}' stroke-width='1.4'/>")
    s.append(f"<text x='{c2+48}' y='{ly+78}' {FONT} font-size='12' fill='{INK}'>Total participation</text>")
    s.append(f"<text x='{c2}' y='{ly+110}' {FONT} font-size='12' fill='{INK}'><tspan fill='#b91c1c' font-weight='700'>1, N, M</tspan> cardinality ratio</text>")
    s.append("</svg>")
    return "".join(s)


svg, _, _ = crowsfoot()
open(os.path.join(OUT, "er_diagram_crowsfoot.svg"), "w", encoding="utf-8").write(svg)
open(os.path.join(OUT, "er_diagram_chen.svg"), "w", encoding="utf-8").write(chen())
print("ok")
