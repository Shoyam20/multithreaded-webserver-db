#pragma once
#include "core/http.h"

// Called by a worker thread for every request. It finds out who the user is and runs
// the handler that belongs to the URL. (This is the "site map" of the whole website.)
Response handleRequest(Request& req);
