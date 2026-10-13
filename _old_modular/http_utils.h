#pragma once
#include <cstddef>
#include <map>
#include <string>

std::string urlDecode(const std::string& v);
std::string parsePath(const std::string& raw);
std::string parseMethod(const std::string& raw);
std::size_t contentLength(const std::string& headers);
std::map<std::string, std::string> parseParameters(const std::string& input);
std::map<std::string, std::string> parseFormBody(const std::string& raw);
std::map<std::string, std::string> parseQuery(const std::string& raw);
int getIntParameter(const std::map<std::string, std::string>& params,
                    const std::string& key, int defaultValue = 0);
std::string makeHttpResponse(const std::string& body, int status = 200,
                             const std::string& extraHeaders = "");
