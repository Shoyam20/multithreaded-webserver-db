#pragma once
#include <string>
#include <vector>

// ------------------------------------------------------------
//  Request log: which worker handled which request, and every SQL
//  statement it ran. Shown in the admin panel (Server logs) and printed
//  to the console in the same format.
//
//  How it fits together:
//    server.cpp   creates a RequestLog and calls beginRequestLog(&log)
//    database.cpp every db.run() calls addQueryToCurrentRequest(...)
//    server.cpp   finishes the request and calls recordRequest(log)
//  The "current request" is thread-local, so each worker only ever
//  sees its own request.
// ------------------------------------------------------------
struct QueryLog
{
    std::string sql;               // already cleaned and redacted
    double      ms = 0;
    bool        ok = true;
    std::string error;
    bool        isSelect = false;
    long long   count = 0;         // rows returned (SELECT) or rows affected
};

struct RequestLog
{
    long long   id = 0;
    std::string time;              // "2026-10-13 14:02:11"
    int         worker = 0;        // which worker thread: 1..N
    std::string method;
    std::string target;            // path + query string (never the form body)
    std::string user = "guest";
    int         status = 0;
    double      ms = 0;
    std::vector<QueryLog> queries;
    int         hiddenQueries = 0; // queries beyond the per-request limit
};

// Thread-local "current request" (set by the worker around the handler call).
void beginRequestLog(RequestLog* log);
void endRequestLog();
void addQueryToCurrentRequest(const std::string& sql, double ms, bool ok,
                              const std::string& error, bool isSelect, long long count);

// Static files and the log pages themselves are not logged (they would
// flood the buffer, and viewing the log would change the log).
bool shouldLogPath(const std::string& path);

// Store a finished request and, if enabled, print it to the console.
void recordRequest(RequestLog& log);

// Newest requests, oldest first. worker 0 = any; method "" = any.
std::vector<RequestLog> recentRequests(int worker, const std::string& method,
                                       bool errorsOnly, int limit);
void clearRequestLog();

std::string httpStatusText(int status);

// "4 rows", "OK, 1 row affected" or "ERROR: ..." (the line shown under each query)
std::string describeQueryResult(const QueryLog& q);

// The console format, e.g.
//   [2026-10-13 14:02:11] [W2] GET /products?category=3
//       mysql> SELECT ... ;
//              -> 4 rows (0.82 ms)
//   [W2] 200 OK | 1 query | 4.12 ms | user: guest
std::string formatForTerminal(const RequestLog& log);

// Collapses whitespace, hides passwords / hashes / session ids, shortens long text.
std::string cleanSqlForLog(const std::string& sql);
