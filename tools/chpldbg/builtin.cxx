#include "builtin.h"
#include "cdb_back.h"

void intiialize_builtin() {

}

void run_handler(std::vector<std::string> args) {
    //for now just make it restart the program
    std::string input;
    for (;;) {
        printf("The program being debugged has been started already.\n");
        printf("Start it from the beginning? (y or n) ");

        std::cin >> input;

        if (input == "y") {
            b_disconnect();
            b_cleanup();
            b_init(3);
            b_connect();
        } else if (input == "n") {
            return;
        } else {
            printf("Please answer y or n.\n");
        }
        //if args exit with that arg
    }
}

void quit_handler(std::vector<std::string> args) {
    std::string input;
    int exit_code = 0;
    if (!args.empty()) {
        try {
            exit_code = stoi(args[0]);
        } catch (const std::invalid_argument& e){
            exit_code = 0;
        }
        
    }
    for (;;) {
        printf("A debugging session is active.\n");
        printf("Quit anyway? (y or n) ");

        std::cin >> input;

        if (input == "y") {
            b_disconnect();
            exit(exit_code);
        } else if (input == "n") {
            return;
        } else {
            printf("Please answer y or n.\n");
        }
        //if args exit with that arg
    }
}

