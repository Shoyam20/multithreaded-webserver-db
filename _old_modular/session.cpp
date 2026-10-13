#include "session.h"
#include "database.h"
#include "html_helpers.h"
#include "request_context.h"
#include <random>
#include <sstream>
#include <cctype>
#include <cstdlib>
using namespace std;

// ============================================================
//  SESSION MANAGEMENT
// ============================================================
string generateSessionId()
{
    static const char chars[] = "0123456789abcdef";

    random_device rd;
    mt19937_64 gen(rd());
    uniform_int_distribution<int> dist(0, 15);

    string id;

    for (int i = 0; i < 32; i++)
        id += chars[dist(gen)];

    return id;
}

bool createSession(int userId, string& sessionId)
{
    lock_guard<mutex> lock(dbMutex);

    for (int attempt = 0; attempt < 5; attempt++)
    {
        sessionId = generateSessionId();

        string q =
            "INSERT INTO user_sessions (session_id, user_id) VALUES ('" +
            esc(sessionId) + "'," +
            to_string(userId) + ")";

        if (!mysql_query(dbConn, q.c_str()))
            return true;
    }

    return false;
}

int getSessionUserId(const string& sessionId)
{
    if (sessionId.empty())
        return 0;

    lock_guard<mutex> lock(dbMutex);

    string q =
        "SELECT user_id FROM user_sessions "
        "WHERE session_id='" + esc(sessionId) +
        "' AND is_active=TRUE";

    if (mysql_query(dbConn, q.c_str()))
        return 0;

    MYSQL_RES* r = mysql_store_result(dbConn);

    if (!r)
        return 0;

    MYSQL_ROW row = mysql_fetch_row(r);

    if (!row)
    {
        mysql_free_result(r);
        return 0;
    }

    int userId = atoi(row[0]);

    mysql_free_result(r);

    string update =
        "UPDATE user_sessions "
        "SET last_activity=CURRENT_TIMESTAMP "
        "WHERE session_id='" + esc(sessionId) +
        "' AND is_active=TRUE";

    mysql_query(dbConn, update.c_str());

    return userId;
}

bool logoutSession(const string& sessionId)
{
    if (sessionId.empty())
        return false;

    lock_guard<mutex> lock(dbMutex);

    string q =
        "UPDATE user_sessions "
        "SET is_active=FALSE, logout_time=CURRENT_TIMESTAMP "
        "WHERE session_id='" + esc(sessionId) +
        "' AND is_active=TRUE";

    return mysql_query(dbConn, q.c_str()) == 0 &&
           mysql_affected_rows(dbConn) > 0;
}

// Pull one cookie value out of the raw request headers.
string getCookie(const string& raw, const string& name)
{
    size_t headerEnd = raw.find("\r\n\r\n");

    string headers =
        (headerEnd == string::npos)
        ? raw
        : raw.substr(0, headerEnd);

    string lower;

    for (size_t i = 0; i < headers.size(); i++)
        lower += (char)tolower((unsigned char)headers[i]);

    size_t p = lower.find("\r\ncookie:");

    if (p == string::npos)
        return "";

    size_t lineStart = p + 9;
    size_t lineEnd = headers.find("\r\n", lineStart);

    if (lineEnd == string::npos)
        lineEnd = headers.size();

    string line =
        headers.substr(lineStart, lineEnd - lineStart);

    // Walk the "a=1; b=2" list looking for our name.
    size_t pos = 0;

    while (pos < line.size())
    {
        size_t semi = line.find(';', pos);

        if (semi == string::npos)
            semi = line.size();

        string pair = line.substr(pos, semi - pos);

        size_t a = pair.find_first_not_of(" \t");
        size_t eq = pair.find('=');

        if (a != string::npos && eq != string::npos && eq > a)
        {
            if (pair.substr(a, eq - a) == name)
                return pair.substr(eq + 1);
        }

        pos = semi + 1;
    }

    return "";
}

// Display name for the logged-in user, for the navigation bar.
string getUserName(int userId)
{
    if (userId <= 0)
        return "";

    lock_guard<mutex> lock(dbMutex);

    string q =
        "SELECT name FROM users WHERE user_id=" +
        to_string(userId);

    if (mysql_query(dbConn, q.c_str()))
        return "";

    MYSQL_RES* r = mysql_store_result(dbConn);

    if (!r)
        return "";

    MYSQL_ROW row = mysql_fetch_row(r);

    string name =
        (row && row[0])
        ? row[0]
        : "";

    mysql_free_result(r);

    return name;
}

// ============================================================
//  SESSION PAGE
// ============================================================
// Shows the rows this user has in user_sessions: which account the
// session belongs to, when it started, when it was last used, and
// when it was logged out. The session in use right now is marked.
string buildSessionPage(
    const string& sessionId,
    int userId
)
{
    lock_guard<mutex> lock(dbMutex);

    string q =
        "SELECT s.session_id, u.name, u.email, s.login_time, "
        "s.last_activity, s.logout_time, s.is_active "
        "FROM user_sessions s "
        "JOIN users u ON s.user_id=u.user_id "
        "WHERE s.user_id=" + to_string(userId) + " "
        "ORDER BY s.login_time DESC "
        "LIMIT 20";

    if (mysql_query(dbConn, q.c_str()))
        return pageWrapper(
            "Session",
            string("<div class='err'>") +
            htmlEscape(mysql_error(dbConn)) +
            "</div>"
        );

    MYSQL_RES* result = mysql_store_result(dbConn);

    if (!result)
        return pageWrapper(
            "Session",
            "<div class='err'>No result</div>"
        );

    ostringstream t;

    t << "<p>Signed in as <b>"
      << htmlEscape(tlUserName)
      << "</b>. The highlighted row is the session this browser "
         "is using right now.</p>";

    t << "<div class='tablewrap'><table><tr>"
         "<th>Session ID</th><th>User</th><th>Email</th>"
         "<th>Login Time</th><th>Last Activity</th>"
         "<th>Logout Time</th><th>Status</th>"
         "</tr>";

    MYSQL_ROW row;
    int shown = 0;

    while ((row = mysql_fetch_row(result)))
    {
        string rowSession = row[0] ? row[0] : "";
        bool isCurrent = (rowSession == sessionId);

        t << "<tr"
          << (isCurrent ? " class='current'" : "")
          << ">";

        t << "<td><code>"
          << htmlEscape(rowSession)
          << "</code>"
          << (isCurrent ? " <b>(this browser)</b>" : "")
          << "</td>";

        for (int i = 1; i <= 4; i++)
            t << "<td>" << htmlEscape(row[i] ? row[i] : "") << "</td>";

        // logout_time stays NULL until the session is closed.
        t << "<td>"
          << (row[5] ? htmlEscape(row[5]) : "<span class='out'>&mdash;</span>")
          << "</td>";

        bool active =
            row[6] && (row[6][0] == '1' || row[6][0] == 't');

        t << "<td>"
          << (active
              ? "<span class='active'>Active</span>"
              : "<span class='out'>Logged out</span>")
          << "</td>";

        t << "</tr>";
        shown++;
    }

    t << "</table></div>";

    mysql_free_result(result);

    if (shown == 0)
        return pageWrapper(
            "Session",
            "<div class='err'>No sessions recorded.</div>"
        );

    t << "<p><a class='navbtn' href='/logout'>Log out this session</a></p>";

    return pageWrapper("My Session", t.str());
}

