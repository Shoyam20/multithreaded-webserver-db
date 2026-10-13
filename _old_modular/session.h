#pragma once
#include <string>

std::string generateSessionId();
bool createSession(int userId, std::string& sessionId);
int  getSessionUserId(const std::string& sessionId);
bool logoutSession(const std::string& sessionId);
std::string getCookie(const std::string& raw, const std::string& name);
std::string getUserName(int userId);
std::string buildSessionPage(const std::string& sessionId, int userId);
