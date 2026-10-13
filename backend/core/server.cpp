#include <winsock2.h>
#include <cstring>
#include "server.h"
#include "http.h"
#include "../app.h"
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

using namespace std;

// ---- The thread pool --------------------------------------------------------------
// The main thread only accepts connections and puts them on this queue.
// The worker threads take connections off the queue, one each, and handle them.
static queue<SOCKET>      pending;          // connections waiting for a worker
static mutex              pendingMutex;     // protects 'pending'
static condition_variable pendingReady;     // wakes a sleeping worker when 'pending' has work
static mutex              printMutex;       // so lines printed by different workers do not mix

static const size_t MAX_HEADER_BYTES = 64 * 1024;
static const size_t MAX_BODY_BYTES   = 1024 * 1024;

// Reads the whole HTTP request (headers, then the body if there is one) from the socket.
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

    size_t bodyLength = contentLengthOf(raw.substr(0, headerEnd));
    if (bodyLength > MAX_BODY_BYTES) return false;

    while (raw.size() - (headerEnd + 4) < bodyLength)
    {
        int n = recv(sock, buffer, sizeof(buffer), 0);
        if (n <= 0) return false;

        raw.append(buffer, n);
    }

    return true;
}

// Sends all the bytes (send() may send only part of them at once).
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

// The code every worker thread runs, forever: wait for a connection, answer it, repeat.
static void worker(int id)
{
    while (true)
    {
        SOCKET client;

        {
            unique_lock<mutex> lock(pendingMutex);

            while (pending.empty())             // sleep until the main thread adds a connection
                pendingReady.wait(lock);

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

        // The one line that shows which worker took this request.
        {
            lock_guard<mutex> lock(printMutex);
            cout << "[Worker " << id << "] " << req.method << " " << req.path << endl;
        }

        Response res;

        try
        {
            res = handleRequest(req);           // all the shop logic is in app.cpp and below
        }
        catch (const exception& e)
        {
            cout << "[Worker " << id << "] ERROR: " << e.what() << endl;
            res = Response::html("<h1>500 - Something went wrong</h1>", 500);
        }

        sendAll(client, buildHttpResponse(res));
        closesocket(client);                    // one request per connection
    }
}

// Opens the socket, starts the workers, then accepts connections forever.
int runServer(int port, int workerCount)
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

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((u_short)port);
    memset(addr.sin_zero, 0, sizeof(addr.sin_zero));

    if (bind(listener, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        cout << "Bind failed! Is another copy of the server already running on port " << port << "?" << endl;
        return 1;
    }

    if (listen(listener, SOMAXCONN) == SOCKET_ERROR)
    {
        cout << "Listen failed!" << endl;
        return 1;
    }

    cout << "Server running on http://localhost:" << port << "  (" << workerCount << " worker threads)" << endl;

    vector<thread> workers;

    for (int i = 1; i <= workerCount; i++)
        workers.push_back(thread(worker, i));

    // The main thread's only job from now on: accept a connection, queue it, wake a worker.
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
