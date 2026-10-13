#include "log.h"
#include "../config.h"
#include <atomic>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <deque>
#include <iostream>
#include <mutex>

using namespace std;

// ------------------------------------------------------------
//  Per-thread "current request"
// ------------------------------------------------------------
static thread_local RequestLog* currentLog = nullptr;

static const size_t MAX_QUERIES_PER_REQUEST = 150;
static const size_t MAX_SQL_CHARS = 700;

void beginRequestLog(RequestLog* log) { currentLog = log; }
void endRequestLog()                  { currentLog = nullptr; }

void addQueryToCurrentRequest(const string& sql, double ms, bool ok, const string& error,
                              bool isSelect, long long count)
{
    if (!currentLog) return;                       // e.g. start-up queries

    if (currentLog->queries.size() >= MAX_QUERIES_PER_REQUEST)
    {
        currentLog->hiddenQueries++;
        return;
    }

    QueryLog q;
    q.sql = cleanSqlForLog(sql);
    q.ms = ms;
    q.ok = ok;
    q.error = error;
    q.isSelect = isSelect;
    q.count = count;
    currentLog->queries.push_back(q);
}

// ------------------------------------------------------------
//  Cleaning SQL before it is stored or printed
// ------------------------------------------------------------
string cleanSqlForLog(const string& original)
{
    // 1. one line: collapse runs of whitespace
    string sql;
    bool space = false;

    for (char c : original)
    {
        if (isspace((unsigned char)c)) { space = true; continue; }
        if (space && !sql.empty()) sql += ' ';
        space = false;
        sql += c;
    }

    // 2. the password-hashing query contains the real password
    size_t from = 0, p;
    const string replacement = "SHA2(CONCAT('<salt>','<hidden>'),256)";

    while ((p = sql.find("SHA2(CONCAT(", from)) != string::npos)
    {
        size_t end = sql.find(",256)", p);
        if (end == string::npos) break;

        sql.replace(p, end + 5 - p, replacement);
        from = p + replacement.size();
    }

    // 3. inside string literals: hide password hashes, shorten session ids
    string out;

    for (size_t i = 0; i < sql.size(); i++)
    {
        if (sql[i] != '\'') { out += sql[i]; continue; }

        size_t j = i + 1;
        string literal;

        while (j < sql.size())
        {
            if (sql[j] == '\\' && j + 1 < sql.size()) { literal += sql[j]; literal += sql[j + 1]; j += 2; continue; }
            if (sql[j] == '\'') break;
            literal += sql[j++];
        }

        bool hex32 = literal.size() == 32;
        for (char c : literal)
            if (!isxdigit((unsigned char)c)) hex32 = false;

        if (literal.compare(0, 7, "sha256$") == 0)  out += "'<password-hash>'";
        else if (hex32)                            out += "'" + literal.substr(0, 6) + "...'";
        else                                       out += "'" + literal + "'";

        i = j;                                      // skip past the closing quote
    }

    if (out.size() > MAX_SQL_CHARS)
        out = out.substr(0, MAX_SQL_CHARS) + " ...";

    return out;
}

// ------------------------------------------------------------
//  Stored history (ring buffer)
// ------------------------------------------------------------
static deque<RequestLog> history;
static mutex             historyMutex;
static atomic<long long> nextId{ 1 };
static mutex             consoleMutex;

bool shouldLogPath(const string& path)
{
    if (path.compare(0, 8, "/static/") == 0) return false;
    if (path.compare(0, 11, "/admin/logs") == 0) return false;
    return true;
}

string httpStatusText(int status)
{
    switch (status)
    {
        case 200: return "OK";
        case 302: return "Found";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 500: return "Internal Server Error";
        default:  return "";
    }
}


string describeQueryResult(const QueryLog& q)
{
    if (!q.ok)       return "ERROR: " + q.error;
    if (q.isSelect)  return to_string(q.count) + (q.count == 1 ? " row" : " rows");
    return "OK, " + to_string(q.count) + (q.count == 1 ? " row" : " rows") + " affected";
}
static string fixed2(double v)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%.2f", v);
    return buf;
}

string formatForTerminal(const RequestLog& log)
{
    string out = "[" + log.time + "] [W" + to_string(log.worker) + "] " +
                 log.method + " " + log.target + "\n";

    for (const QueryLog& q : log.queries)
    {
        out += "    mysql> " + q.sql + ";\n";

        out += "           -> " + describeQueryResult(q) + " (" + fixed2(q.ms) + " ms)\n";
    }

    if (log.hiddenQueries > 0)
        out += "    ... " + to_string(log.hiddenQueries) + " more queries not shown\n";

    size_t n = log.queries.size() + log.hiddenQueries;

    out += "[W" + to_string(log.worker) + "] " + to_string(log.status) + " " +
           httpStatusText(log.status) + " | " + to_string(n) + (n == 1 ? " query" : " queries") +
           " | " + fixed2(log.ms) + " ms | user: " + log.user + "\n";

    return out;
}

void recordRequest(RequestLog& log)
{
    log.id = nextId++;

    // Terminal: just which worker took which request. The full log (with the
    // SQL of every request) is in the admin panel -> Server logs.
    // Set ECOM_LOG_SQL=1 to also print the SQL here.
    string text = config::logQueriesToConsole
                  ? formatForTerminal(log)
                  : "[W" + to_string(log.worker) + "] " + log.method + " " + log.target + "\n";

    {
        // one write per request, so lines from different workers never interleave
        lock_guard<mutex> lock(consoleMutex);
        cout << text << flush;
    }

    lock_guard<mutex> lock(historyMutex);
    history.push_back(log);

    while ((int)history.size() > config::logBufferSize)
        history.pop_front();
}

vector<RequestLog> recentRequests(int worker, const string& method, bool errorsOnly, int limit)
{
    vector<RequestLog> out;

    lock_guard<mutex> lock(historyMutex);

    // walk from newest to oldest, then reverse so the page reads like a terminal
    for (auto it = history.rbegin(); it != history.rend() && (int)out.size() < limit; ++it)
    {
        if (worker > 0 && it->worker != worker) continue;
        if (!method.empty() && it->method != method) continue;

        if (errorsOnly)
        {
            bool failed = it->status >= 400;
            for (const QueryLog& q : it->queries)
                if (!q.ok) failed = true;
            if (!failed) continue;
        }

        out.push_back(*it);
    }

    return vector<RequestLog>(out.rbegin(), out.rend());
}

void clearRequestLog()
{
    lock_guard<mutex> lock(historyMutex);
    history.clear();
}
