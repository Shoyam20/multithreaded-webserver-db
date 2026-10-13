#pragma once
#include "../core/http.h"      // User
#include "../db/database.h"
#include <string>

// ---- accounts ----
// Returns "" on success, or an error message for the form.
std::string signUp(const std::string& name, const std::string& email, const std::string& password);
std::string logIn(const std::string& email, const std::string& password, User& user);

// ---- sessions (one row in user_sessions per browser login) ----
std::string startSession(int userId);                       // returns the new session id
bool        userForSession(const std::string& sessionId, User& user);
void        endSession(const std::string& sessionId);
Rows        sessionsForUser(int userId);                    // newest 20

// ---- admin ----
Rows        listUsers();                                    // with order count
bool        makeAdminByEmail(const std::string& email);     // used by: server.exe --make-admin <email>
int         countAdmins();
