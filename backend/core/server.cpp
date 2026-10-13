#include <winsock2.h>
#include "server.h"
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

using namespace std;

// ------------------------------------------------------------
//  The accept loop puts new connections on a queue; worker threads take
//  them off one at a time and handle them.
// ------------------------------------------------------------
static queue<SOCKET>      pending;
static mutex              pendingMutex;
static condition_variable pendingReady;
static mutex              printMutex;       // so lines printed by different workers do not mix

static const size_t MAX_HEADER_BYTES = 64 * 1024;
static const size_t MAX_BODY_BYTES   = 1024 * 1024;


// Read the whole HTTP request (headers + body) from the socket.
static bool readRequest(SOCKET sock, string& raw)
{
    char buffer[8192];
    size_t headerEnd = string::npos;

    while ((headerEnd = raw.find("\r\n\r\n")) == string::npos)
    {
        if (raw.size() > MAX_HEADER_BYTES) return false;

        int n = recv(sock, buffer, sizeof(buffer), 0);
        if (n <= 0) return false;

        raw.append(buffer, n);
    }

    size_t need = contentLengthOf(raw.substr(0, headerEnd));
    if (need > MAX_BODY_BYTES) return false;

    while (raw.size() - (headerEnd + 4) < need)
    {
        int n = recv(sock, buffer, sizeof(buffer), 0);
        if (n <= 0) return false;

        raw.append(buffer, n);
    }

    return true;
}

static void sendAll(SOCKET sock, const string& data)
{
    size_t sent = 0;

    while (sent < data.size())
    {
        int n = send(sock, data.c_str() + sent, (int)(data.size() - sent), 0);
        if (n <= 0) return;
        sent += n;
    }
}

static void worker(int id, RequestHandler handler)
{
    while (true)
    {
        SOCKET client;

        {
            unique_lock<mutex> lock(pendingMutex);
            pendingReady.wait(lock, [] { return !pending.empty(); });
            client = pending.front();
            pending.pop();
        }

        string raw;
        Request req;

        if (!readRequest(client, raw) || !parseRequest(raw, req))
        {
            closesocket(client);
            continue;
        }

        // The one line that shows which worker thread took this request.
        {
            lock_guard<mutex> lock(printMutex);
            cout << "[Worker " << id << "] " << req.method << " " << req.path << endl;
        }

        Response res;

        try
        {
            res = handler(req);
        }
        catch (const exception& e)
        {
            cout << "[Worker " << id << "] ERROR: " << e.what() << endl;
            res = Response::html("<h1>500 - Something went wrong</h1>", 500);
        }

        sendAll(client, buildHttpResponse(res));
        closesocket(client);
    }
}

int runServer(int port, int workerCount, RequestHandler handler)
{
    WSADATA wsa;

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        cout << "Winsock init failed!" << endl;
        return 1;
    }

    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (listener == INVALID_SOCKET)
    {
        cout << "Socket failed!" << endl;
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((u_short)port);

    if (bind(listener, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        cout << "Bind failed! Is another copy of the server already running on port "
             << port << "?" << endl;
        return 1;
    }

    if (listen(listener, SOMAXCONN) == SOCKET_ERROR)
    {
        cout << "Listen failed!" << endl;
        return 1;
    }

    cout << "Server running on http://localhost:" << port << endl;

    vector<thread> workers;

    for (int i = 0; i < workerCount; i++)
        workers.emplace_back(worker, i + 1, handler);

    while (true)
    {
        SOCKET client = accept(listener, nullptr, nullptr);
        if (client == INVALID_SOCKET) continue;

        {
            lock_guard<mutex> lock(pendingMutex);
            pending.push(client);
        }

        pendingReady.notify_one();
    }
}
