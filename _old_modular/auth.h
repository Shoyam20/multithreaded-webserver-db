#pragma once
#include <map>
#include <string>

std::string signupForm(const std::string& msg = "", bool isError = false);
std::string loginForm(const std::string& msg = "", bool isError = false);
std::string handleSignup(const std::map<std::string, std::string>& form);
std::string handleLogin(const std::map<std::string, std::string>& form);
