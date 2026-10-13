#include "app.h"
#include "config.h"
#include "core/server.h"
#include "db/database.h"
#include "services/category_service.h"
#include "services/order_service.h"
#include "services/user_service.h"
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

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

    if (!migrateDatabase())
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

    // Demo: a background thread delivers orders whose time has come
    // (see deliverDueOrders in services/order_service.cpp).
    thread([]
    {
        while (true)
        {
            this_thread::sleep_for(chrono::seconds(config::deliveryCheckSeconds));
            deliverDueOrders();
        }
    }).detach();

    cout << "[Demo] Orders are delivered automatically " << config::deliveryMinSeconds << "-"
         << config::deliveryMaxSeconds << " seconds after checkout (customers can cancel until then)." << endl;

    int exitCode = runServer(config::serverPort, config::workerThreads, handleRequest);

    db.disconnect();
    return exitCode;
}
