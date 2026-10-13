#include <winsock2.h>
#include <windows.h>
#include "server.h"
#include "catalog.h"
#include "database.h"
#include "http_utils.h"
#include "request_context.h"
#include "router.h"
#include <iostream>
#include <queue>
#include <mutex>
#include <thread>
#include <vector>
using namespace std;

// ============================================================
//  REQUEST / QUEUE
// ============================================================
struct Request { SOCKET clientSocket; };

queue<Request> requestQueue;
mutex queueMutex;
HANDLE requestSemaphore;

// ============================================================
//  WORKER
// ============================================================
void worker(int workerId)
{
    while (true)
    {
        Request req;

        WaitForSingleObject(
            requestSemaphore,
            INFINITE
        );

        {
            lock_guard<mutex> lock(queueMutex);

            req =
                requestQueue.front();

            requestQueue.pop();
        }

        char buffer[8192];

        string raw;

        // Read until the end of the header block has arrived
        size_t headerEnd =
            string::npos;

        while (
            (headerEnd =
                raw.find("\r\n\r\n"))
            == string::npos
        )
        {
            if (raw.size() > 65536)
                break;

            int n =
                recv(
                    req.clientSocket,
                    buffer,
                    sizeof(buffer),
                    0
                );

            if (n <= 0)
                break;

            raw.append(
                buffer,
                n
            );
        }

        if (headerEnd ==
            string::npos)
        {
            closesocket(
                req.clientSocket
            );

            continue;
        }

        // Keep reading until the whole body has arrived
        size_t need =
            contentLength(
                raw.substr(
                    0,
                    headerEnd
                )
            );

        while (
            raw.size() -
            (headerEnd + 4) <
            need
        )
        {
            int n =
                recv(
                    req.clientSocket,
                    buffer,
                    sizeof(buffer),
                    0
                );

            if (n <= 0)
                break;

            raw.append(
                buffer,
                n
            );
        }

        string method =
            parseMethod(raw);

        string path =
            parsePath(raw);

        cout << "[W"
             << workerId
             << "] "
             << method
             << " "
             << path
             << endl;

        resetRequestContext();

        string body =
            route(
                method,
                path,
                raw
            );

        int status = tlStatus;
        string headers = tlExtraHeaders;

        if (!tlRedirect.empty())
        {
            status = 302;
            headers += "Location: " + tlRedirect + "\r\n";
        }

        string resp =
            makeHttpResponse(
                body,
                status,
                headers
            );

        send(
            req.clientSocket,
            resp.c_str(),
            (int)resp.size(),
            0
        );

        closesocket(
            req.clientSocket
        );
    }
}

// ============================================================
//  MAIN
// ============================================================
int runServer()
{
    WSADATA wsaData;

    if (WSAStartup(
            MAKEWORD(2, 2),
            &wsaData) != 0)
    {
        cout << "Winsock init failed!"
             << endl;

        return 1;
    }

    if (!initDatabase())
    {
        WSACleanup();
        return 1;
    }

    if (!loadCategories())
    {
        cout << "[DB] Could not load categories." << endl;
        WSACleanup();
        return 1;
    }

    requestSemaphore =
        CreateSemaphore(
            NULL,
            0,
            1000,
            NULL
        );

    if (!requestSemaphore)
    {
        cout << "Semaphore failed!"
             << endl;

        mysql_close(dbConn);
        WSACleanup();

        return 1;
    }

    SOCKET serverSocket =
        socket(
            AF_INET,
            SOCK_STREAM,
            IPPROTO_TCP
        );

    if (serverSocket ==
        INVALID_SOCKET)
    {
        cout << "Socket failed!"
             << endl;

        CloseHandle(
            requestSemaphore
        );

        mysql_close(dbConn);
        WSACleanup();

        return 1;
    }

    sockaddr_in addr;

    addr.sin_family =
        AF_INET;

    addr.sin_addr.s_addr =
        INADDR_ANY;

    addr.sin_port =
        htons(8080);

    if (bind(
            serverSocket,
            (sockaddr*)&addr,
            sizeof(addr)
        ) == SOCKET_ERROR)
    {
        cout << "Bind failed!"
             << endl;

        closesocket(
            serverSocket
        );

        CloseHandle(
            requestSemaphore
        );

        mysql_close(dbConn);
        WSACleanup();

        return 1;
    }

    if (listen(
            serverSocket,
            SOMAXCONN
        ) == SOCKET_ERROR)
    {
        cout << "Listen failed!"
             << endl;

        closesocket(
            serverSocket
        );

        CloseHandle(
            requestSemaphore
        );

        mysql_close(dbConn);
        WSACleanup();

        return 1;
    }

    cout << "Server on "
         << "http://localhost:8080"
         << endl;

    const int NUM_WORKERS = 4;

    vector<thread> workers;

    for (int i = 0;
         i < NUM_WORKERS;
         i++)
    {
        workers.emplace_back(
            worker,
            i + 1
        );
    }

    while (true)
    {
        SOCKET client =
            accept(
                serverSocket,
                nullptr,
                nullptr
            );

        if (client ==
            INVALID_SOCKET)
        {
            continue;
        }

        {
            lock_guard<mutex>
                lock(queueMutex);

            requestQueue.push(
                {client}
            );
        }

        ReleaseSemaphore(
            requestSemaphore,
            1,
            NULL
        );
    }

    closesocket(
        serverSocket
    );

    CloseHandle(
        requestSemaphore
    );

    mysql_close(
        dbConn
    );

    WSACleanup();

    return 0;
}
