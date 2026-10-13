#pragma once
#include <string>

std::string route(const std::string& method, const std::string& path,
                  const std::string& rawRequest);
