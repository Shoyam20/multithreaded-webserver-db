#pragma once
#include "core/http.h"

// Called for every request: serves static files, works out who the user is,
// then runs the handler for the URL.
Response handleRequest(Request& req);
