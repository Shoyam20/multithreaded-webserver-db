#include <iostream>
#include <winsock2.h>
#include <cstring>
#include <cctype>
#include <cstdlib>
#include <string>
#include <sstream>
#include <map>
#include <queue>
#include <mutex>
#include <thread>
#include <vector>
#include <windows.h>

#include <mysql.h>

using namespace std;

// ============================================================
//  CONFIG
// ============================================================
const char* DB_HOST = "localhost";
const char* DB_USER = "root";
const char* DB_PASS = "shoyam123";
const char* DB_NAME = "ecommerce_db";
const int   DB_PORT = 3306;

// ============================================================
//  REQUEST / QUEUE
// ============================================================
struct Request { SOCKET clientSocket; };

queue<Request> requestQueue;
mutex queueMutex;
HANDLE requestSemaphore;

// ============================================================
//  DB
// ============================================================
MYSQL* dbConn = nullptr;
mutex dbMutex;

bool initDatabase()
{
    dbConn = mysql_init(nullptr);
    if (!dbConn) { cout << "[DB] mysql_init failed!" << endl; return false; }

    if (!mysql_real_connect(dbConn, DB_HOST, DB_USER, DB_PASS, DB_NAME,
                            DB_PORT, nullptr, 0))
    {
        cout << "[DB] Connection failed: " << mysql_error(dbConn) << endl;
        return false;
    }
    cout << "[DB] Connected to MySQL successfully." << endl;
    return true;
}

// Escape a string for safe use in SQL (prevents SQL injection)
string esc(const string& s)
{
    vector<char> buf(s.size() * 2 + 1);
    unsigned long n = mysql_real_escape_string(dbConn, buf.data(), s.c_str(),
                                               (unsigned long)s.size());
    return string(buf.data(), n);
}

// ============================================================
//  HTML HELPERS
// ============================================================
string pageWrapper(const string& title, const string& body)
{
    ostringstream h;
    h << "<html><head><title>" << title << "</title><style>"
         "body{font-family:Arial;background:#f5f5f5;padding:30px;}"
         "table{border-collapse:collapse;background:white;"
         "box-shadow:0 2px 6px rgba(0,0,0,0.1);}"
         "th,td{border:1px solid #ddd;padding:10px 15px;text-align:left;}"
         "th{background:#2c3e50;color:white;}"
         "tr:nth-child(even){background:#f9f9f9;}"
         "h1{color:#2c3e50;}"
         "input{display:block;margin:8px 0;padding:8px;width:250px;}"
         "button{padding:8px 20px;background:#2c3e50;color:white;border:none;cursor:pointer;}"
         ".msg{padding:10px;background:#e8f5e9;border-left:4px solid #4caf50;margin:10px 0;}"
         ".err{padding:10px;background:#ffebee;border-left:4px solid #f44336;margin:10px 0;}"
         "nav a{margin-right:15px;color:#2c3e50;}"
         "</style></head><body>";
    h << "<nav><a href='/'>Home</a><a href='/products'>Products</a>"
         "<a href='/signup'>Signup</a><a href='/login'>Login</a></nav><hr>";
    h << "<h1>" << title << "</h1>";
    h << body;
    h << "</body></html>";
    return h.str();
}

// ============================================================
//  PRODUCTS PAGE
// ============================================================
string buildProductsPage()
{
    lock_guard<mutex> lock(dbMutex);

    const char* q =
        "SELECT product_id, name, description, price, stock, category "
        "FROM products ORDER BY product_id";

    if (mysql_query(dbConn, q))
        return pageWrapper("Error", string("<p>") + mysql_error(dbConn) + "</p>");

    MYSQL_RES* result = mysql_store_result(dbConn);
    if (!result) return pageWrapper("Error", "<p>No result</p>");

    ostringstream t;
    t << "<table><tr><th>ID</th><th>Name</th><th>Description</th>"
         "<th>Price</th><th>Stock</th><th>Category</th></tr>";

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result)))
    {
        t << "<tr>";
        for (int i = 0; i < 6; i++)
            t << "<td>" << (row[i] ? row[i] : "") << "</td>";
        t << "</tr>";
    }
    t << "</table>";
    mysql_free_result(result);
    return pageWrapper("Our Products", t.str());
}

// ============================================================
//  SIGNUP / LOGIN PAGES (GET — show form)
// ============================================================
string signupForm(const string& msg = "", bool isError = false)
{
    ostringstream b;
    if (!msg.empty())
        b << "<div class='" << (isError ? "err" : "msg") << "'>" << msg << "</div>";

    b << "<form method='POST' action='/signup'>"
         "<input name='name' placeholder='Full Name' required>"
         "<input name='email' type='email' placeholder='Email' required>"
         "<input name='password' type='password' placeholder='Password' required>"
         "<button type='submit'>Sign Up</button>"
         "</form>";
    return pageWrapper("Sign Up", b.str());
}

string loginForm(const string& msg = "", bool isError = false)
{
    ostringstream b;
    if (!msg.empty())
        b << "<div class='" << (isError ? "err" : "msg") << "'>" << msg << "</div>";

    b << "<form method='POST' action='/login'>"
         "<input name='email' type='email' placeholder='Email' required>"
         "<input name='password' type='password' placeholder='Password' required>"
         "<button type='submit'>Log In</button>"
         "</form>";
    return pageWrapper("Login", b.str());
}

// ============================================================
//  HANDLE SIGNUP POST
// ============================================================
string handleSignup(const map<string,string>& form)
{
    string name  = form.count("name")     ? form.at("name")     : "";
    string email = form.count("email")    ? form.at("email")    : "";
    string pass  = form.count("password") ? form.at("password") : "";

    if (name.empty() || email.empty() || pass.empty())
        return signupForm("All fields are required.", true);

    lock_guard<mutex> lock(dbMutex);

    // Check if email already exists
    string check = "SELECT user_id FROM users WHERE email='" + esc(email) + "'";
    if (mysql_query(dbConn, check.c_str()))
        return signupForm(string("DB error: ") + mysql_error(dbConn), true);

    MYSQL_RES* r = mysql_store_result(dbConn);
    bool exists = (mysql_num_rows(r) > 0);
    mysql_free_result(r);

    if (exists)
        return signupForm("Email already registered. Try logging in.", true);

    // Insert
    string ins = "INSERT INTO users (name, email, password) VALUES ('"
                 + esc(name) + "','" + esc(email) + "','" + esc(pass) + "')";

    if (mysql_query(dbConn, ins.c_str()))
        return signupForm(string("Insert failed: ") + mysql_error(dbConn), true);

    return loginForm("Signup successful! Please log in.");
}

// ============================================================
//  HANDLE LOGIN POST
// ============================================================
string handleLogin(const map<string,string>& form)
{
    string email = form.count("email")    ? form.at("email")    : "";
    string pass  = form.count("password") ? form.at("password") : "";

    if (email.empty() || pass.empty())
        return loginForm("Both fields are required.", true);

    lock_guard<mutex> lock(dbMutex);

    string q = "SELECT name, password FROM users WHERE email='" + esc(email) + "'";

    if (mysql_query(dbConn, q.c_str()))
        return loginForm(string("DB error: ") + mysql_error(dbConn), true);

    MYSQL_RES* r = mysql_store_result(dbConn);
    if (!r) return loginForm("Query error", true);

    MYSQL_ROW row = mysql_fetch_row(r);
    if (!row)
    {
        mysql_free_result(r);
        return loginForm("No account found with that email.", true);
    }

    string dbName = row[0] ? row[0] : "";
    string dbPass = row[1] ? row[1] : "";
    mysql_free_result(r);

    if (dbPass != pass)
        return loginForm("Incorrect password.", true);

    // Success
    ostringstream b;
    b << "<div class='msg'>Welcome back, <b>" << dbName << "</b>! "
         "You are now logged in (session tracking coming later).</div>"
         "<p><a href='/products'>Browse products →</a></p>";
    return pageWrapper("Logged In", b.str());
}

// ============================================================
//  HTTP PARSING
// ============================================================
string parsePath(const string& raw)
{
    size_t lineEnd = raw.find("\r\n");
    if (lineEnd == string::npos) return "/";
    string firstLine = raw.substr(0, lineEnd);

    istringstream iss(firstLine);
    string method, path, version;
    iss >> method >> path >> version;

    size_t q = path.find('?');
    if (q != string::npos) path = path.substr(0, q);
    return path;
}

// Read the Content-Length value out of the header block (0 if absent)
size_t contentLength(const string& headers)
{
    string lower;
    for (size_t i = 0; i < headers.size(); i++)
        lower += (char)tolower((unsigned char)headers[i]);

    size_t p = lower.find("content-length:");
    if (p == string::npos) return 0;

    return (size_t)strtoul(headers.c_str() + p + 15, nullptr, 10);
}

string parseMethod(const string& raw)
{
    istringstream iss(raw);
    string method; iss >> method;
    return method;
}

// Parse "name=John&email=a@b.com&password=123"
map<string,string> parseFormBody(const string& raw)
{
    map<string,string> out;

    size_t headerEnd = raw.find("\r\n\r\n");
    if (headerEnd == string::npos) return out;
    string body = raw.substr(headerEnd + 4);

    istringstream iss(body);
    string pair;
    while (getline(iss, pair, '&'))
    {
        size_t eq = pair.find('=');
        if (eq == string::npos) continue;
        string k = pair.substr(0, eq);
        string v = pair.substr(eq + 1);

        // URL-decode: '+' -> ' ',  %XX -> char
        string decoded;
        for (size_t i = 0; i < v.size(); i++)
        {
            if (v[i] == '+') decoded += ' ';
            else if (v[i] == '%' && i + 2 < v.size())
            {
                int c;
                sscanf(v.substr(i + 1, 2).c_str(), "%x", &c);
                decoded += (char)c;
                i += 2;
            }
            else decoded += v[i];
        }
        out[k] = decoded;
    }
    return out;
}

// ============================================================
//  HTTP RESPONSE
// ============================================================
string makeHttpResponse(const string& body, int status = 200)
{
    string text = (status == 200) ? "OK" : "Not Found";
    ostringstream r;
    r << "HTTP/1.1 " << status << " " << text << "\r\n";
    r << "Content-Type: text/html; charset=utf-8\r\n";
    r << "Content-Length: " << body.size() << "\r\n";
    r << "Connection: close\r\n\r\n";
    r << body;
    return r.str();
}

// ============================================================
//  ROUTER
// ============================================================
string route(const string& method, const string& path, const string& rawRequest)
{
    if (path == "/products")
        return buildProductsPage();

    if (path == "/signup")
    {
        if (method == "POST")
            return handleSignup(parseFormBody(rawRequest));
        return signupForm();
    }

    if (path == "/login")
    {
        if (method == "POST")
            return handleLogin(parseFormBody(rawRequest));
        return loginForm();
    }

    if (path == "/")
    {
        string b = "<p>Welcome! Try <a href='/signup'>Signup</a> or "
                   "<a href='/login'>Login</a>, or browse "
                   "<a href='/products'>Products</a>.</p>";
        return pageWrapper("Home", b);
    }

    return pageWrapper("404", "<p>Page not found. <a href='/'>Home</a></p>");
}

// ============================================================
//  WORKER
// ============================================================
void worker(int workerId)
{
    while (true)
    {
        Request req;
        WaitForSingleObject(requestSemaphore, INFINITE);
        {
            lock_guard<mutex> lock(queueMutex);
            req = requestQueue.front();
            requestQueue.pop();
        }

        char buffer[8192];
        string raw;

        // Read until the end of the header block has arrived
        size_t headerEnd = string::npos;
        while ((headerEnd = raw.find("\r\n\r\n")) == string::npos)
        {
            if (raw.size() > 65536) break;
            int n = recv(req.clientSocket, buffer, sizeof(buffer), 0);
            if (n <= 0) break;
            raw.append(buffer, n);
        }
        if (headerEnd == string::npos) { closesocket(req.clientSocket); continue; }

        // Keep reading until the whole body has arrived
        size_t need = contentLength(raw.substr(0, headerEnd));
        while (raw.size() - (headerEnd + 4) < need)
        {
            int n = recv(req.clientSocket, buffer, sizeof(buffer), 0);
            if (n <= 0) break;
            raw.append(buffer, n);
        }

        string method = parseMethod(raw);
        string path   = parsePath(raw);

        cout << "[W" << workerId << "] " << method << " " << path << endl;

        string body   = route(method, path, raw);
        int status    = (body.find("404") != string::npos &&
                         body.find("Page not found") != string::npos) ? 404 : 200;

        string resp = makeHttpResponse(body, status);
        send(req.clientSocket, resp.c_str(), (int)resp.size(), 0);
        closesocket(req.clientSocket);
    }
}

// ============================================================
//  MAIN
// ============================================================
int main()
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    { cout << "Winsock init failed!" << endl; return 1; }

    if (!initDatabase()) { WSACleanup(); return 1; }

    requestSemaphore = CreateSemaphore(NULL, 0, 1000, NULL);
    if (!requestSemaphore) { cout << "Semaphore failed!" << endl; WSACleanup(); return 1; }

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET)
    { cout << "Socket failed!" << endl; WSACleanup(); return 1; }

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(8080);

    if (bind(serverSocket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
    { cout << "Bind failed!" << endl; closesocket(serverSocket); WSACleanup(); return 1; }

    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR)
    { cout << "Listen failed!" << endl; closesocket(serverSocket); WSACleanup(); return 1; }

    cout << "Server on http://localhost:8080" << endl;

    const int NUM_WORKERS = 4;
    vector<thread> workers;
    for (int i = 0; i < NUM_WORKERS; i++)
        workers.emplace_back(worker, i + 1);

    while (true)
    {
        SOCKET client = accept(serverSocket, nullptr, nullptr);
        if (client == INVALID_SOCKET) continue;

        {
            lock_guard<mutex> lock(queueMutex);
            requestQueue.push({client});
        }
        ReleaseSemaphore(requestSemaphore, 1, NULL);
    }

    closesocket(serverSocket);
    mysql_close(dbConn);
    WSACleanup();
    return 0;
}
