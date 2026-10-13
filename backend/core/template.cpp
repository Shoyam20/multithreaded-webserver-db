#include "template.h"
#include <fstream>
#include <sstream>

using namespace std;

// Replaces the five characters that have a special meaning in HTML.
string htmlEscape(const string& text)
{
    string out;

    for (size_t i = 0; i < text.size(); i++)
    {
        char c = text[i];

        if (c == '&')       out += "&amp;";
        else if (c == '<')  out += "&lt;";
        else if (c == '>')  out += "&gt;";
        else if (c == '"')  out += "&quot;";
        else if (c == '\'') out += "&#39;";
        else                out += c;
    }

    return out;
}

// Returns the whole text of a template file.
static string readFile(const string& name)
{
    ifstream in(("frontend/templates/" + name).c_str(), ios::binary);

    if (!in)
        return "<!-- missing template: " + htmlEscape(name) + " -->";

    stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

// Value of a placeholder, or "" if the page did not provide it.
static string lookup(const Vars& vars, const string& name)
{
    Vars::const_iterator it = vars.find(name);
    return (it == vars.end()) ? "" : it->second;
}

// Removes spaces around a placeholder name: "{{ name }}" -> "name".
static string trim(const string& text)
{
    size_t first = text.find_first_not_of(" \t\r\n");
    if (first == string::npos) return "";

    size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

// Walks through the template text; copies plain text, replaces placeholders.
string renderTemplate(const string& file, const Vars& vars)
{
    string text = readFile(file);
    string out;
    size_t pos = 0;

    while (pos < text.size())
    {
        size_t open = text.find("{{", pos);

        if (open == string::npos)                   // no more placeholders
        {
            out += text.substr(pos);
            break;
        }

        out += text.substr(pos, open - pos);        // plain text before the placeholder

        if (text.compare(open, 3, "{{{") == 0)      // {{{name}}} - raw value
        {
            size_t close = text.find("}}}", open + 3);
            if (close == string::npos) { out += text.substr(open); break; }

            out += lookup(vars, trim(text.substr(open + 3, close - open - 3)));
            pos = close + 3;
        }
        else                                        // {{name}} or {{> file}}
        {
            size_t close = text.find("}}", open + 2);
            if (close == string::npos) { out += text.substr(open); break; }

            string tag = trim(text.substr(open + 2, close - open - 2));

            if (!tag.empty() && tag[0] == '>')
                out += renderTemplate(trim(tag.substr(1)), vars);
            else
                out += htmlEscape(lookup(vars, tag));

            pos = close + 2;
        }
    }

    return out;
}
