#pragma once
#include "http.h"
#include <functional>

using RequestHandler = std::function<Response(Request&)>;

// Opens the listening socket and serves requests with a pool of worker
// threads. Each request is parsed, passed to 'handler', and the response sent
// back. Blocks forever; returns a non-zero exit code if it cannot start.
int runServer(int port, int workerCount, RequestHandler handler);
