#pragma once
#include "../core/http.h"      // the User struct
#include "../db/database.h"
#include <string>

// ---- accounts ----
// Both return "" on success, otherwise a message to show on the form.
std::string signUp(const std::string& name, const std::string& email, const std::string& password);
std::string logIn(const std::string& email, const std::string& password, User& user);

// ---- sessions (one row in user_sessions for every browser login) ----
std::string startSession(int userId);                       // returns the new session id
bool        userForSession(const std::string& sessionId, User& user);
void        endSession(const std::string& sessionId);
Rows        sessionsForUser(int userId);                    // the 20 newest

// ---- admin ----
Rows        listUsers();                                    // with the number of orders of each
bool        makeAdminByEmail(const std::string& email);     // used by: server.exe --make-admin <email>
int         countAdmins();
