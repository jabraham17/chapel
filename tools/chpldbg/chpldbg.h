//
// Created by joe on 4/7/20.
//

#ifndef CHPLDBG_H
#define CHPLDBG_H

#include <cstdio>
#include <unistd.h>
#include <vector>
#include <string>
#include <cstring>
#include <iostream>
#include "cdb_back.h"
#include "pthread.h"

extern std::string data_pass_delimiter;
b_locale* output_target_locale; //which locale's gdb responses are sent to stdout
/* Prototypes */
int arg_parse(char** argv, int argc);
void backend_callback(b_locale* l, char* msg);

class Chpldbg
{

 private:
 	static std::vector<std::string> back_buffer;
	int filter_pipe[2];
	int out_fd;
	int in_fd;
	std::thread t1;
	pid_t child_pid;
	pthread_t child_thread;

	

	int start_counter;
	int num_locales;
	char** main_argv;

	int get_gasnet_ssh_servers(unsigned int size);

	void disconnect_backend();
	void connect_backend();
	int set_output_target_locale(int locale_num);
	void write_to_front();
	void read_from_front();
 public:
 	void test_function(char** args);
	Chpldbg(int num_locales, int out_pipe_fd, int in_pipe_fd, char **chpl_argv);
	int run();

};
#endif //CHPLDBG_H
