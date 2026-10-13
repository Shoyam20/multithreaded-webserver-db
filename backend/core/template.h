#pragma once
#include <map>
#include <string>

// A tiny template engine. Pages are HTML files in frontend/templates/ with three kinds of placeholder:
//
//   {{name}}           the value of "name", HTML-escaped (safe for text typed by users)
//   {{{name}}}         the value of "name" inserted as it is (only for HTML the server built itself)
//   {{> partial.html}} the contents of another template file (rendered with the same values)
//
// The values are a plain map from name to text. A name that is not in the map becomes "".
typedef std::map<std::string, std::string> Vars;

// Replaces < > & " ' with their HTML codes, so user text can never act as HTML (XSS protection).
std::string htmlEscape(const std::string& text);

// Reads frontend/templates/<file> and fills in the placeholders.
std::string renderTemplate(const std::string& file, const Vars& vars);
