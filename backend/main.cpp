#include "config.h"
#include "core/server.h"
#include "db/database.h"
#include "services/user_service.h"
#include <iostream>
#include <string>

using namespace std;

// Start-up:  connect to MySQL, then run the web server.
//
//   server.exe                         run the website
//   server.exe --make-admin <email>    give an existing account admin rights, then exit
int main(int argc, char** argv)
{
    if (!dbConnect())
        return 1;

    // One-off command: promote a user to admin, then exit.
    if (argc == 3 && string(argv[1]) == "--make-admin")
    {
        if (makeAdminByEmail(argv[2]))
            cout << argv[2] << " is now an admin." << endl;
        else
            cout << "No account found with email " << argv[2] << ". Sign up on the website first." << endl;

        dbClose();
        return 0;
    }

    if (countAdmins() == 0)
        cout << "[Admin] No admin account yet. Sign up on the website, then run:\n"
                "        server.exe --make-admin your@email.com" << endl;

    int exitCode = runServer(SERVER_PORT, WORKER_COUNT);

    dbClose();
    return exitCode;
}
