#define _CRT_RAND_S          // lets us use rand_s(), the operating system's random number source
#include "services.h"
#include <cstdlib>

using namespace std;

// Random text made of hex digits (0-9, a-f), e.g. for salts and session ids.
static string randomHex(int length)
{
    const char* digits = "0123456789abcdef";
    string out;

    for (int i = 0; i < length; i++)
    {
        unsigned int r = 0;
        rand_s(&r);
        out += digits[r % 16];
    }

    return out;
}

// ---- passwords -------------------------------------------------------------------------
// A password is never stored as typed. We store   sha256$<salt>$<hash>
// where hash = SHA-256(salt + password), calculated by MySQL's SHA2().
// The random salt makes two users with the same password get different hashes.

// SHA-256 of (salt + password) as 64 hex characters. (Caller holds dbMutex.)
static string saltedHash(const string& salt, const string& password)
{
    Rows r = dbRows("SELECT SHA2(CONCAT(" + dbQuote(salt) + "," + dbQuote(password) + "),256) AS h");
    return r.empty() ? "" : col(r[0], "h");
}

// The text to store in users.password for a new account.
static string makePasswordRecord(const string& password)
{
    string salt = randomHex(16);
    return "sha256$" + salt + "$" + saltedHash(salt, password);
}

// Does the typed password match the stored text? Splits "sha256$salt$hash" and hashes again.
static bool passwordMatches(const string& stored, const string& password)
{
    size_t secondDollar = stored.find('$', 7);

    if (stored.compare(0, 7, "sha256$") != 0 || secondDollar == string::npos)
        return false;

    string salt = stored.substr(7, secondDollar - 7);
    string hash = stored.substr(secondDollar + 1);

    return hash == saltedHash(salt, password);
}

// ---- accounts -------------------------------------------------------------------------

string signUp(const string& name, const string& email, const string& password)
{
    if (name.empty() || email.empty() || password.empty())
        return "All fields are required.";

    if (email.find('@') == string::npos || email.size() > 100 || name.size() > 100)
        return "Please enter a valid name and email.";

    lock_guard<mutex> lock(dbMutex);

    Rows existing = dbRows("SELECT user_id FROM users WHERE email=" + dbQuote(email));
    if (!existing.empty())
        return "That email is already registered. Try signing in.";

    Result r = dbRun("INSERT INTO users (name, email, password) VALUES (" +
                     dbQuote(name) + "," + dbQuote(email) + "," + dbQuote(makePasswordRecord(password)) + ")");

    return r.ok ? "" : "Could not create the account: " + r.error;
}

string logIn(const string& email, const string& password, User& user)
{
    if (email.empty() || password.empty())
        return "Both fields are required.";

    lock_guard<mutex> lock(dbMutex);

    Rows found = dbRows("SELECT user_id, name, email, password, role FROM users WHERE email=" + dbQuote(email));
    if (found.empty())
        return "No account found with that email.";

    if (!passwordMatches(col(found[0], "password"), password))
        return "Incorrect password.";

    user.id    = atoi(col(found[0], "user_id").c_str());
    user.name  = col(found[0], "name");
    user.email = col(found[0], "email");
    user.role  = col(found[0], "role");
    return "";
}

// ---- sessions -------------------------------------------------------------------------

// Creates a login session: a random 32-character id stored in the database (and in the browser's cookie).
string startSession(int userId)
{
    lock_guard<mutex> lock(dbMutex);

    string sessionId = randomHex(32);

    Result r = dbRun("INSERT INTO user_sessions (session_id, user_id) VALUES (" +
                     dbQuote(sessionId) + "," + to_string(userId) + ")");

    return r.ok ? sessionId : "";
}

// Finds out who a session id belongs to (and notes that the session was just used).
bool userForSession(const string& sessionId, User& user)
{
    if (sessionId.empty()) return false;

    lock_guard<mutex> lock(dbMutex);

    Rows found = dbRows("SELECT u.user_id, u.name, u.email, u.role "
                        "FROM user_sessions s JOIN users u ON s.user_id=u.user_id "
                        "WHERE s.session_id=" + dbQuote(sessionId) + " AND s.is_active=TRUE");
    if (found.empty()) return false;

    user.id    = atoi(col(found[0], "user_id").c_str());
    user.name  = col(found[0], "name");
    user.email = col(found[0], "email");
    user.role  = col(found[0], "role");

    dbRun("UPDATE user_sessions SET last_activity=CURRENT_TIMESTAMP WHERE session_id=" + dbQuote(sessionId));
    return true;
}

// Logout: the session stays in the table but is marked inactive, so the cookie stops working.
void endSession(const string& sessionId)
{
    if (sessionId.empty()) return;

    lock_guard<mutex> lock(dbMutex);

    dbRun("UPDATE user_sessions SET is_active=FALSE, logout_time=CURRENT_TIMESTAMP "
          "WHERE session_id=" + dbQuote(sessionId) + " AND is_active=TRUE");
}

Rows sessionsForUser(int userId)
{
    lock_guard<mutex> lock(dbMutex);

    return dbRows("SELECT session_id, login_time, last_activity, logout_time, is_active "
                  "FROM user_sessions WHERE user_id=" + to_string(userId) +
                  " ORDER BY login_time DESC LIMIT 20");
}

// ---- admin ----------------------------------------------------------------------------

Rows listUsers()
{
    lock_guard<mutex> lock(dbMutex);

    return dbRows("SELECT u.user_id, u.name, u.email, u.role, u.created_at, "
                  "(SELECT COUNT(*) FROM orders o WHERE o.user_id=u.user_id) AS orders "
                  "FROM users u ORDER BY u.user_id");
}

bool makeAdminByEmail(const string& email)
{
    lock_guard<mutex> lock(dbMutex);

    Result r = dbRun("UPDATE users SET role='ADMIN' WHERE email=" + dbQuote(email));
    return r.ok && r.affectedRows > 0;
}

int countAdmins()
{
    lock_guard<mutex> lock(dbMutex);

    Rows r = dbRows("SELECT COUNT(*) AS n FROM users WHERE role='ADMIN'");
    return r.empty() ? 0 : atoi(col(r[0], "n").c_str());
}
