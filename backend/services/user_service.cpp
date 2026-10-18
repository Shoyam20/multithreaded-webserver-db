#define _CRT_RAND_S         
#include "user_service.h"
#include <cstdlib>

using namespace std;

// Generate a random hexadecimal string for password salts and session IDs.
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

// ------------------------------------------------------------
// Password hashing and verification
// Password format: sha256$<salt>$<hash>
// ------------------------------------------------------------

// Generate a SHA-256 hash using the salt and password through MySQL.
static string saltedHash(const string& salt, const string& password)
{
    Rows r = db.rows("SELECT SHA2(CONCAT(" + db.quote(salt) + "," + db.quote(password) +
                     "),256) AS h");
    return r.empty() ? "" : col(r[0], "h");
}

// Generate a unique salt and store the password in the hashed format.
static string makePasswordRecord(const string& password)
{
    string salt = randomHex(16);
    return "sha256$" + salt + "$" + saltedHash(salt, password);
}

// Check the supplied password against the stored password record.
static bool passwordMatches(const string& stored, const string& password)
{
    // Support accounts that still contain an old plain-text password.
    if (stored.compare(0, 7, "sha256$") != 0)
        return stored == password;

    size_t second = stored.find('$', 7);
    if (second == string::npos) return false;

    string salt = stored.substr(7, second - 7);
    return stored.substr(second + 1) == saltedHash(salt, password);
}

// ------------------------------------------------------------
// User accounts
// ------------------------------------------------------------

// Register a new user after validating the details and checking the email.
string signUp(const string& name, const string& email, const string& password)
{
    if (name.empty() || email.empty() || password.empty())
        return "All fields are required.";

    if (email.find('@') == string::npos || email.size() > 100 || name.size() > 100)
        return "Please enter a valid name and email.";

    DbLock lock;

    // Prevent registration with an email that is already in use.
    Rows existing = db.rows("SELECT user_id FROM users WHERE email=" + db.quote(email));
    if (!existing.empty())
        return "That email is already registered. Try signing in.";

    Result r = db.run("INSERT INTO users (name, email, password) VALUES (" +
                      db.quote(name) + "," + db.quote(email) + "," +
                      db.quote(makePasswordRecord(password)) + ")");

    return r.ok ? "" : "Could not create the account: " + r.error;
}

// Authenticate a user and fill the User object with their account details.
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

    // Upgrade an old plain-text password to the hashed format after login.
    if (col(row, "password").compare(0, 7, "sha256$") != 0)
        db.run("UPDATE users SET password=" + db.quote(makePasswordRecord(password)) +
               " WHERE user_id=" + to_string(user.id));

    return "";
}

// ------------------------------------------------------------
// User sessions
// ------------------------------------------------------------

// Create a new session for a user and return its session ID.
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

// Validate a session and retrieve the associated user's details.
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

// End an active session and record its logout time.
void endSession(const string& sessionId)
{
    if (sessionId.empty()) return;

    db.run("UPDATE user_sessions SET is_active=FALSE, logout_time=CURRENT_TIMESTAMP "
           "WHERE session_id=" + db.quote(sessionId) + " AND is_active=TRUE");
}

// Fetch the recent sessions belonging to a particular user.
Rows sessionsForUser(int userId)
{
    return db.rows("SELECT session_id, login_time, last_activity, logout_time, is_active "
                   "FROM user_sessions WHERE user_id=" + to_string(userId) +
                   " ORDER BY login_time DESC LIMIT 20");
}

// Fetch session details for the admin dashboard.
Rows allSessions(int limit)
{
    return db.rows("SELECT s.session_id, u.name, u.email, s.login_time, s.last_activity, "
                   "s.logout_time, s.is_active "
                   "FROM user_sessions s JOIN users u ON s.user_id=u.user_id "
                   "ORDER BY s.is_active DESC, s.last_activity DESC LIMIT " + to_string(limit));
}

// ------------------------------------------------------------
// Admin: manage users and roles
// ------------------------------------------------------------

// List users with their order counts and active session counts.
Rows listUsers()
{
    return db.rows(
        "SELECT u.user_id, u.name, u.email, COALESCE(u.role,'CUSTOMER') AS role, u.created_at, "
        "(SELECT COUNT(*) FROM orders o WHERE o.user_id=u.user_id) AS orders, "
        "(SELECT COUNT(*) FROM user_sessions s WHERE s.user_id=u.user_id AND s.is_active=TRUE) AS active_sessions "
        "FROM users u ORDER BY u.user_id");
}

// Change a user's role to CUSTOMER or ADMIN.
string setUserRole(int userId, const string& role)
{
    if (role != "CUSTOMER" && role != "ADMIN")
        return "Unknown role.";

    Result r = db.run("UPDATE users SET role=" + db.quote(role) +
                      " WHERE user_id=" + to_string(userId));

    return r.ok ? "" : "Database error: " + r.error;
}

// Promote a user to admin using their email address.
bool makeAdminByEmail(const string& email)
{
    Result r = db.run("UPDATE users SET role='ADMIN' WHERE email=" + db.quote(email));
    return r.ok && r.affectedRows > 0;
}

// Return the total number of admin accounts.
int countAdmins()
{
    Rows r = db.rows("SELECT COUNT(*) AS n FROM users WHERE role='ADMIN'");
    return r.empty() ? 0 : atoi(col(r[0], "n").c_str());
}
