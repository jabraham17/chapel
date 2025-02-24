#ifndef BUILTIN_H_
#define BUILTIN_H_

#include <iostream>
#include <unordered_map>
#include <functional>
#include <string>
#include <vector>

/* builtin functions -- these will be mapped to command strings in chpldbg.cxx and
 * called when a user inputs a builtin command. Must take a std::vector<std::string> 
 * argument and have a void return type */

void intialize_builtin(std::vector<std::string> args);
void quit_handler(std::vector<std::string> args);
void print_chapel_handler(std::vector<std::string> args);
void run_handler(std::vector<std::string> args);

#endif