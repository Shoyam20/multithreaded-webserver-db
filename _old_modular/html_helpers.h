#pragma once
#include <string>

std::string htmlEscape(const std::string& s);

// Full page: header, search, category nav, flash notice, body, footer.
// An empty title suppresses the <h1> so a page can render its own heading.
std::string pageWrapper(const std::string& title, const std::string& body);

// "₹1,23,456" (Indian digit grouping, decimals dropped when .00)
std::string rupees(double amount);
std::string rupees(const std::string& decimalText);

// Login/signup card used by the auth pages.
std::string authCard(const std::string& heading, const std::string& sub,
                     const std::string& inner);
