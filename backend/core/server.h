#pragma once

// Opens the listening socket, starts the worker threads and accepts connections forever.
// Every request is passed to handleRequest() (app.cpp). Returns non-zero only if it cannot start.
int runServer(int port, int workerCount);
