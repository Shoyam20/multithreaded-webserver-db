#include "template.h"
#include "../config.h"
#include <algorithm>
#include <fstream>
#include <sstream>

// Loads HTML templates and fills in {{values}}.
using namespace std;

// ------------------------------------------------------------
//  Data
// ------------------------------------------------------------
Data& Data::set(const string& k, const string& v) { values[k] = v; return *this; }
Data& Data::set(const string& k, const char* v)    { values[k] = v; return *this; }
Data& Data::set(const string& k, int v)            { values[k] = to_string(v); return *this; }
Data& Data::set(const string& k, long long v)      { values[k] = to_string(v); return *this; }
Data& Data::flag(const string& k, bool on)         { values[k] = on ? "1" : ""; return *this; }
Data& Data::add(const string& name, const Data& item) { lists[name].push_back(item); return *this; }

void Data::merge(const Data& other)
{
    for (const auto& kv : other.values) values[kv.first] = kv.second;
    for (const auto& kv : other.lists)  lists[kv.first] = kv.second;
}

string htmlEscape(const string& text)
{
    string out;
    out.reserve(text.size());

    for (char c : text)
    {
        switch (c)
        {
            case '&':  out += "&amp;";  break;
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&#39;";  break;
            default:   out += c;
        }
    }

    return out;
}

// ------------------------------------------------------------
//  Rendering
// ------------------------------------------------------------
namespace
{
    // A lookup context: the current data plus the enclosing one(s).
    struct Scope
    {
        const Data*  data;
        const Scope* parent;
    };

    string trim(const string& s)
    {
        size_t a = s.find_first_not_of(" \t\r\n");
        if (a == string::npos) return "";
        size_t b = s.find_last_not_of(" \t\r\n");
        return s.substr(a, b - a + 1);
    }

    string lookupValue(const Scope* scope, const string& key)
    {
        for (; scope; scope = scope->parent)
        {
            auto it = scope->data->values.find(key);
            if (it != scope->data->values.end()) return it->second;
        }
        return "";
    }

    const vector<Data>* lookupList(const Scope* scope, const string& key)
    {
        for (; scope; scope = scope->parent)
        {
            auto it = scope->data->lists.find(key);
            if (it != scope->data->lists.end()) return &it->second;
        }
        return nullptr;
    }

    bool isTruthy(const Scope* scope, const string& key)
    {
        string v = lookupValue(scope, key);
        if (!v.empty() && v != "0") return true;

        const vector<Data>* list = lookupList(scope, key);
        return list && !list->empty();
    }

    string readFile(const string& name)
    {
        ifstream in(config::frontendDir + "/templates/" + name, ios::binary);

        if (!in)
            return "<!-- missing template: " + htmlEscape(name) + " -->";

        stringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    // Position of the {{/xxx}} that closes a block opened just before 'from',
    // skipping nested blocks of the same kind. npos if the template is broken.
    size_t findBlockEnd(const string& t, size_t from, const string& open, const string& close)
    {
        int depth = 1;
        size_t pos = from;

        while (true)
        {
            size_t nextOpen  = t.find(open, pos);
            size_t nextClose = t.find(close, pos);

            if (nextClose == string::npos) return string::npos;

            if (nextOpen != string::npos && nextOpen < nextClose)
            {
                depth++;
                pos = nextOpen + open.size();
            }
            else
            {
                if (--depth == 0) return nextClose;
                pos = nextClose + close.size();
            }
        }
    }

    // The {{else}} that belongs to this {{#if}} (not to a nested one).
    size_t findElse(const string& t, size_t from, size_t to)
    {
        int depth = 0;
        size_t pos = from;

        while (pos < to)
        {
            size_t nextIf    = t.find("{{#if ", pos);
            size_t nextEndIf = t.find("{{/if}}", pos);
            size_t nextElse  = t.find("{{else}}", pos);

            size_t nearest = min({ nextIf, nextEndIf, nextElse });
            if (nearest == string::npos || nearest >= to) return string::npos;

            if (nearest == nextIf)         { depth++; pos = nextIf + 6; }
            else if (nearest == nextEndIf) { depth--; pos = nextEndIf + 7; }
            else
            {
                if (depth == 0) return nextElse;
                pos = nextElse + 8;
            }
        }

        return string::npos;
    }

    string renderText(const string& t, const Scope& scope);

    string renderTag(const string& t, size_t& pos, const string& tag, const Scope& scope)
    {
        // {{> file.html}}
        if (tag[0] == '>')
            return renderText(readFile(trim(tag.substr(1))), scope);

        // {{#if name}} ... {{else}} ... {{/if}}
        if (tag.compare(0, 4, "#if ") == 0)
        {
            string name = trim(tag.substr(4));
            size_t end = findBlockEnd(t, pos, "{{#if ", "{{/if}}");
            if (end == string::npos) return "<!-- unclosed #if -->";

            size_t elsePos = findElse(t, pos, end);
            string yes = t.substr(pos, (elsePos == string::npos ? end : elsePos) - pos);
            string no  = (elsePos == string::npos) ? "" : t.substr(elsePos + 8, end - elsePos - 8);

            pos = end + 7;
            return renderText(isTruthy(&scope, name) ? yes : no, scope);
        }

        // {{#each name}} ... {{/each}}
        if (tag.compare(0, 6, "#each ") == 0)
        {
            string name = trim(tag.substr(6));
            size_t end = findBlockEnd(t, pos, "{{#each ", "{{/each}}");
            if (end == string::npos) return "<!-- unclosed #each -->";

            string inner = t.substr(pos, end - pos);
            pos = end + 9;

            string out;
            const vector<Data>* list = lookupList(&scope, name);

            if (list)
                for (const Data& item : *list)
                {
                    Scope itemScope{ &item, &scope };
                    out += renderText(inner, itemScope);
                }

            return out;
        }

        // {{name}}  (escaped)
        return htmlEscape(lookupValue(&scope, tag));
    }

    string renderText(const string& t, const Scope& scope)
    {
        string out;
        size_t pos = 0;

        while (pos < t.size())
        {
            size_t open = t.find("{{", pos);

            if (open == string::npos)
            {
                out.append(t, pos, string::npos);
                break;
            }

            out.append(t, pos, open - pos);

            // {{{name}}}  (raw)
            if (t.compare(open, 3, "{{{") == 0)
            {
                size_t close = t.find("}}}", open + 3);
                if (close == string::npos) { out.append(t, open, string::npos); break; }

                out += lookupValue(&scope, trim(t.substr(open + 3, close - open - 3)));
                pos = close + 3;
                continue;
            }

            size_t close = t.find("}}", open + 2);
            if (close == string::npos) { out.append(t, open, string::npos); break; }

            string tag = trim(t.substr(open + 2, close - open - 2));
            pos = close + 2;

            if (tag.empty()) continue;

            out += renderTag(t, pos, tag, scope);
        }

        return out;
    }
}

string renderTemplate(const string& file, const Data& data)
{
    Scope root{ &data, nullptr };
    return renderText(readFile(file), root);
}
