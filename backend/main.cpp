#include "app.h"
#include "config.h"
#include "core/server.h"
#include "db/database.h"
#include "services/category_service.h"
#include "services/user_service.h"
#include <iostream>
#include <string>

using namespace std;

// Start-up sequence:  settings -> database -> web server.
//
//   server.exe                         run the website
//   server.exe --make-admin <email>    give an existing account admin rights
int main(int argc, char** argv)
{
    config::load();

    if (!db.connect())
        return 1;

    loadCategories();

    // One-off command: promote a user to admin, then exit.
    if (argc == 3 && string(argv[1]) == "--make-admin")
    {
        if (makeAdminByEmail(argv[2]))
            cout << argv[2] << " is now an admin." << endl;
        else
            cout << "No account found with email " << argv[2]
                 << ". Sign up on the website first." << endl;

        db.disconnect();
        return 0;
    }

    if (countAdmins() == 0)
        cout << "[Admin] No admin account yet. Sign up on the website, then run:\n"
                "        server.exe --make-admin your@email.com" << endl;

    int exitCode = runServer(config::serverPort, config::workerThreads, handleRequest);

    db.disconnect();
    return exitCode;
}
