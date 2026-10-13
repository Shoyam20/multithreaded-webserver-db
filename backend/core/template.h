#pragma once
#include <map>
#include <string>
#include <vector>

// ------------------------------------------------------------
//  A tiny template engine, so the C++ code never contains HTML.
//
//  Handlers fill a Data object; the HTML lives in frontend/templates/.
//
//    {{name}}                 value, HTML-escaped (safe for user text)
//    {{{name}}}               value inserted as-is (already-safe HTML)
//    {{#if name}} A {{else}} B {{/if}}   A when name is non-empty (or a non-empty list)
//    {{#each items}} ... {{/each}}       repeat for every entry of the list "items"
//    {{> partials/footer.html}}          include another template file
//
//  Inside {{#each}} the entry's own values are used first, then the page's.
// ------------------------------------------------------------
struct Data
{
    std::map<std::string, std::string>       values;
    std::map<std::string, std::vector<Data>> lists;

    Data& set(const std::string& key, const std::string& value);
    Data& set(const std::string& key, const char* value);
    Data& set(const std::string& key, int value);
    Data& set(const std::string& key, long long value);
    Data& flag(const std::string& key, bool on);          // on -> "1", off -> ""
    Data& add(const std::string& listName, const Data& item);

    void merge(const Data& other);                        // other's entries win
};

std::string htmlEscape(const std::string& text);

// Render frontend/templates/<file> with the given data.
std::string renderTemplate(const std::string& file, const Data& data);
