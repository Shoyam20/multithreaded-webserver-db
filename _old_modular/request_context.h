#pragma once
#include <string>

// Per-worker-thread context of the request being handled.
extern thread_local std::string tlSessionId;
extern thread_local std::string tlUserName;
extern thread_local std::string tlExtraHeaders;
extern thread_local int         tlUserId;      // 0 when logged out
extern thread_local int         tlCartCount;   // items in the active cart (header badge)
extern thread_local int         tlStatus;      // HTTP status for the response
extern thread_local std::string tlRedirect;    // non-empty => send 302 to this URL
extern thread_local std::string tlNotice;      // flash-message code (?notice=...)
extern thread_local std::string tlSearch;      // current search text (header box)

bool isLoggedIn();
void resetRequestContext();
