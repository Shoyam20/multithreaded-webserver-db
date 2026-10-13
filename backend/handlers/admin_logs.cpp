#include "handlers.h"
#include "view.h"
#include "../config.h"
#include "../core/log.h"
#include <cstdio>

using namespace std;

static string ms2(double v)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%.2f", v);
    return buf;
}

// Everything the log page / live feed needs, filtered by ?worker= &method= &errors= &limit=
static Data buildLogData(const Request& req, string& filterQuery)
{
    int worker = req.intParam("worker", 0);
    string method = req.param("method");
    bool errorsOnly = !req.param("errors").empty();
    int limit = req.intParam("limit", 60);
    if (limit < 10) limit = 10;
    if (limit > 300) limit = 300;
    if (method != "GET" && method != "POST") method = "";

    filterQuery = "limit=" + to_string(limit);
    if (worker > 0)        filterQuery += "&worker=" + to_string(worker);
    if (!method.empty())   filterQuery += "&method=" + method;
    if (errorsOnly)        filterQuery += "&errors=1";

    Data page;
    page.set("filterQuery", filterQuery);

    // ---- who did the work: requests and queries per worker (whole buffer) ----
    vector<RequestLog> all = recentRequests(0, "", false, config::logBufferSize);
    vector<long long> requests(config::workerThreads + 1, 0), queries(config::workerThreads + 1, 0);
    long long totalQueries = 0;
    double totalMs = 0;

    for (const RequestLog& r : all)
    {
        if (r.worker >= 1 && r.worker <= config::workerThreads)
        {
            requests[r.worker]++;
            queries[r.worker] += (long long)r.queries.size() + r.hiddenQueries;
        }
        totalQueries += (long long)r.queries.size() + r.hiddenQueries;
        totalMs += r.ms;
    }

    for (int w = 1; w <= config::workerThreads; w++)
    {
        Data s;
        s.set("worker", w).set("requests", requests[w]).set("queries", queries[w]);
        s.set("percent", all.empty() ? 0 : (int)(requests[w] * 100 / (long long)all.size()));
        page.add("workerStats", s);
    }

    page.set("totalRequests", (long long)all.size());
    page.set("totalQueries", totalQueries);
    page.set("avgMs", all.empty() ? "0.00" : ms2(totalMs / (double)all.size()));

    // ---- the log lines ----
    for (const RequestLog& r : recentRequests(worker, method, errorsOnly, limit))
    {
        size_t n = r.queries.size() + r.hiddenQueries;

        Data e;
        e.set("id", r.id).set("time", r.time).set("worker", r.worker);
        e.set("method", r.method).set("target", r.target);
        e.set("status", r.status).set("statusText", httpStatusText(r.status));
        e.set("statusClass", r.status >= 400 ? "err" : (r.status >= 300 ? "redir" : "ok"));
        e.set("ms", ms2(r.ms)).set("user", r.user);
        e.set("queryCount", (long long)n).set("queryWord", n == 1 ? "query" : "queries");
        if (r.hiddenQueries > 0) e.set("hidden", r.hiddenQueries);

        for (const QueryLog& q : r.queries)
        {
            Data d;
            d.set("sql", q.sql).set("result", describeQueryResult(q)).set("ms", ms2(q.ms));
            d.flag("bad", !q.ok);
            e.add("queries", d);
        }

        page.add("entries", e);
    }

    // ---- filter tabs ----
    page.flag("anyWorker", worker == 0);
    for (int w = 1; w <= config::workerThreads; w++)
    {
        Data t;
        t.set("label", "W" + to_string(w));
        t.set("url", "/admin/logs?worker=" + to_string(w) + "&limit=" + to_string(limit));
        t.flag("active", worker == w);
        page.add("workerTabs", t);
    }

    page.set("filterWorker", worker > 0 ? to_string(worker) : "");
    page.set("filterMethod", method);
    page.flag("filterErrors", errorsOnly);
    page.set("filterLimit", limit);
    page.flag("methodGet", method == "GET").flag("methodPost", method == "POST");
    page.flag("limit30", limit == 30).flag("limit60", limit == 60);
    page.flag("limit150", limit == 150).flag("limit300", limit == 300);

    return page;
}

// The full page: summary, filters, and the terminal panel.
Response adminLogs(Request& req)
{
    string filterQuery;
    Data page = buildLogData(req, filterQuery);

    page.set("feedFragment", renderTemplate("admin/logs_feed.html", page));
    return renderAdminPage(req, "Server logs", "admin/logs.html", "logs", page);
}

// Only the panel (the page asks for this every 2 seconds while "Auto-refresh" is on).
Response adminLogsFeed(Request& req)
{
    string filterQuery;
    Data page = buildLogData(req, filterQuery);

    return Response::html(renderTemplate("admin/logs_feed.html", page));
}

Response adminLogsClear(Request& req)
{
    (void)req;
    clearRequestLog();
    return Response::redirect("/admin/logs?notice=deleted");
}
