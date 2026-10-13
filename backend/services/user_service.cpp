#define _CRT_RAND_S          // lets us use rand_s(), the OS random number source
#include "user_service.h"
#include <cstdlib>

using namespace std;

// Random hex text from the operating system's random generator.
static string randomHex(int length)
{
    static const char digits[] = "0123456789abcdef";
    string out;

    for (int i = 0; i < length; i++)
    {
        unsigned int r = 0;
        rand_s(&r);
        out += digits[r % 16];
    }

    return out;
}

// Passwords are never stored as typed. We store  sha256$<salt>$<hash>  where
// hash = SHA-256(salt + password), computed by MySQL's SHA2().
// The random salt makes two users with the same password get different hashes.
static string saltedHash(const string& salt, const string& password)
{
    Rows r = db.rows("SELECT SHA2(CONCAT(" + db.quote(salt) + "," + db.quote(password) + "),256) AS h");
    return r.empty() ? "" : col(r[0], "h");
}

// Text to store in users.password for a new account.
static string makePasswordRecord(const string& password)
{
    string salt = randomHex(16);
    return "sha256$" + salt + "$" + saltedHash(salt, password);
}

// Does the typed password match the stored "sha256$salt$hash" text?
static bool passwordMatches(const string& stored, const string& password)
{
    size_t secondDollar = stored.find('$', 7);
    if (stored.compare(0, 7, "sha256$") != 0 || secondDollar == string::npos)
        return false;

    string salt = stored.substr(7, secondDollar - 7);
    string hash = stored.substr(secondDollar + 1);
    return hash == saltedHash(salt, password);
}

// ------------------------------------------------------------
//  Accounts
// ------------------------------------------------------------
string signUp(const string& name, const string& email, const string& password)
{
    if (name.empty() || email.empty() || password.empty())
        return "All fields are required.";

    if (email.find('@') == string::npos || email.size() > 100 || name.size() > 100)
        return "Please enter a valid name and email.";

    DbLock lock;

    Rows existing = db.rows("SELECT user_id FROM users WHERE email=" + db.quote(email));
    if (!existing.empty())
        return "That email is already registered. Try signing in.";

    Result r = db.run("INSERT INTO users (name, email, password) VALUES (" +
                      db.quote(name) + "," + db.quote(email) + "," +
                      db.quote(makePasswordRecord(password)) + ")");

    return r.ok ? "" : "Could not create the account: " + r.error;
}

string logIn(const string& email, const string& password, User& user)
{
    if (email.empty() || password.empty())
        return "Both fields are required.";

    DbLock lock;

    Rows found = db.rows("SELECT user_id, name, email, password, COALESCE(role,'CUSTOMER') AS role "
                         "FROM users WHERE email=" + db.quote(email));

    if (found.empty())
        return "No account found with that email.";

    const Row& row = found[0];

    if (!passwordMatches(col(row, "password"), password))
        return "Incorrect password.";

    user.id    = atoi(col(row, "user_id").c_str());
    user.name  = col(row, "name");
    user.email = col(row, "email");
    user.role  = col(row, "role");

    return "";
}

// ------------------------------------------------------------
//  Sessions
// ------------------------------------------------------------
string startSession(int userId)
{
    for (int attempt = 0; attempt < 5; attempt++)
    {
        string id = randomHex(32);

        Result r = db.run("INSERT INTO user_sessions (session_id, user_id) VALUES (" +
                          db.quote(id) + "," + to_string(userId) + ")");
        if (r.ok) return id;
    }

    return "";
}

bool userForSession(const string& sessionId, User& user)
{
    if (sessionId.empty()) return false;

    Rows found = db.rows(
        "SELECT u.user_id, u.name, u.email, COALESCE(u.role,'CUSTOMER') AS role "
        "FROM user_sessions s JOIN users u ON s.user_id=u.user_id "
        "WHERE s.session_id=" + db.quote(sessionId) + " AND s.is_active=TRUE");

    if (found.empty()) return false;

    user.id    = atoi(col(found[0], "user_id").c_str());
    user.name  = col(found[0], "name");
    user.email = col(found[0], "email");
    user.role  = col(found[0], "role");

    db.run("UPDATE user_sessions SET last_activity=CURRENT_TIMESTAMP WHERE session_id=" +
           db.quote(sessionId));

    return true;
}

void endSession(const string& sessionId)
{
    if (sessionId.empty()) return;

    db.run("UPDATE user_sessions SET is_active=FALSE, logout_time=CURRENT_TIMESTAMP "
           "WHERE session_id=" + db.quote(sessionId) + " AND is_active=TRUE");
}

Rows sessionsForUser(int userId)
{
    return db.rows("SELECT session_id, login_time, last_activity, logout_time, is_active "
                   "FROM user_sessions WHERE user_id=" + to_string(userId) +
                   " ORDER BY login_time DESC LIMIT 20");
}

// ------------------------------------------------------------
//  Admin
// ------------------------------------------------------------
Rows listUsers()
{
    return db.rows(
        "SELECT u.user_id, u.name, u.email, COALESCE(u.role,'CUSTOMER') AS role, u.created_at, "
        "(SELECT COUNT(*) FROM orders o WHERE o.user_id=u.user_id) AS orders "
        "FROM users u ORDER BY u.user_id");
}

bool makeAdminByEmail(const string& email)
{
    Result r = db.run("UPDATE users SET role='ADMIN' WHERE email=" + db.quote(email));
    return r.ok && r.affectedRows > 0;
}

int countAdmins()
{
    Rows r = db.rows("SELECT COUNT(*) AS n FROM users WHERE role='ADMIN'");
    return r.empty() ? 0 : atoi(col(r[0], "n").c_str());
}
