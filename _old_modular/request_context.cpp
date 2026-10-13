#include "request_context.h"

using namespace std;

thread_local string tlSessionId;
thread_local string tlUserName;
thread_local string tlExtraHeaders;
thread_local int    tlUserId = 0;
thread_local int    tlCartCount = 0;
thread_local int    tlStatus = 200;
thread_local string tlRedirect;
thread_local string tlNotice;
thread_local string tlSearch;

bool isLoggedIn()
{
    return !tlSessionId.empty() && !tlUserName.empty();
}

void resetRequestContext()
{
    tlSessionId.clear();
    tlUserName.clear();
    tlExtraHeaders.clear();
    tlUserId = 0;
    tlCartCount = 0;
    tlStatus = 200;
    tlRedirect.clear();
    tlNotice.clear();
    tlSearch.clear();
}
