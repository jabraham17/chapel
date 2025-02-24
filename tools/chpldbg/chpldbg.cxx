/* 
 * Albert Furlong, Ciaran Sanders, Jagannath Natarajan, Ben Johnson
 * Date Started: 7 April 2020
 * Last updated: 27 May 2020
 */

#include <wait.h>
#include <thread>
#include <csignal>
#include "chpldbg.h"

#define MSG_SIZE 512

/* Globals */
std::vector<std::string> Chpldbg::back_buffer;
std::string data_pass_delimiter;

/* Prototypes */
void backend_callback(b_locale* l, char* msg);
void backend_callback_log(b_locale* l, char* msg);
void backend_callback_stat(b_locale* l, int status);
void quit_handler();

void usage(){
	printf("Usage:chple_exe [CHAPEL_ARGS] -nl numLocales programs_args \n");
	printf("See ./chpl_exe --help for all configuration options. \n");
	exit(EXIT_FAILURE);
}

Chpldbg::Chpldbg(int num_locales, int _out_fd, int _in_fd, char** chpl_argv)
{
	main_argv = chpl_argv;
	
	out_fd = _out_fd; // fd for writing to frontend 
	in_fd = _in_fd; // fd for reading from frontend

	b_set_callback_console(backend_callback);
	b_set_callback_error(backend_callback);
	b_set_callback_log(backend_callback_log);
	//std::thread front_thread(&Chpldbg::write_to_front, this);
	b_init(num_locales);
	set_output_target_locale(0);
	connect_backend();
}


/**
 * Function parses arguments. TODO: rewrite
 * @param argv
 * @param argc
 * @return : num locales.
 */
int arg_parse(char** argv, int argc)
{
	data_pass_delimiter = "///";
	for (int i = 1; i < argc; i++)
	{
		std::string curr(argv[i]);
		if (i + 1 < argc && (curr == (std::string)"-nl"))
		{
			return atoi(argv[i + 1]);
		}
		else if (curr[0] == '-')
		{
			std::string numl = "--numLocales=";
			std::string snuml = "-snumLocales=";
			int len = strlen(argv[i]);
			for (int j = 1; j < len; j++)
			{
				if (curr[j] != numl[j] && curr[j] != snuml[j])
				{
					break;
				}
				else if (curr[j] == '=')
				{
					int numLocales = 0;
					j++;
					while(j<len){
						numLocales = (numLocales * 10) + (curr[j] - '0');
						j++;
					}
					return numLocales;
				}
			}
		}
	}
	return 0;
}

/**
 * Define the function call that the backend will make
 * when it wants to pass information forward.
 * @param l : The locale making the request.
 * @param msg : The message to be passed.
 */
void backend_callback(b_locale* l, char* msg) {
	// Format the string and append directly to the message buffer
	std::string message(msg);

	// Don't crash if l is NULL - Paul F.
	if (l != NULL){
	  message = std::to_string(l->id) + data_pass_delimiter+ message;
	  if (l->id == output_target_locale->id && msg[0] != '$') { //second part is sloppy fix to sending the chpldbg print statement to gdb and having it return $1 = null
			printf(msg);
			fflush(stdout);
	  }
	  
	} else {
	  message = data_pass_delimiter + message;
	}
	
}

// If msg comes from a locale equal to locale_output_target (defined in chpldbg.h),
// output to console. Otherwise do nothing for now. 
// void backend_callback(b_locale* l, char* msg) {
// 	if (l->id == locale_output_target) {
// 		write(STDOUT_FILENO, msg, MSG_SIZE);
// 	}
	
// }

void backend_callback_log(b_locale* l, char* msg){
	//TODO: could write to a file for logging purposes
}

void backend_callback_stat(b_locale* l, int status) {
	switch (status)
	{
	case LOCALE_RDY:
		break;
	case LOCALE_RUN:
		break;
	default:
		break;
	}
}

/**
 * Connect to the backend and set callbacks.
 */
void Chpldbg::connect_backend()
{
	// start the child reader thread
	// and connect to the gdbservers
	char** chpl_argv = &main_argv[1];
	pid_t pid;

	char *ev_use_chpldbg = getenv("CHPL_USE_CHPLDBG");
	if (!ev_use_chpldbg){
	  fprintf(stderr, "CHPL_USE_CHPLDBG not set\n");
	  exit(EXIT_FAILURE);
	}
	if(strcmp(ev_use_chpldbg, "gdb") != 0 && strcmp(ev_use_chpldbg, "lldb") != 0){
		// set default value for enviornment variable
		setenv("CHPL_USE_CHPLDBG", "gdb", 0);
	}
	if (pipe(filter_pipe) == -1) {
		perror("pipe");
		exit(EXIT_FAILURE);
	}
	if ((pid = fork()) == -1) {
		perror("fork");
		exit(EXIT_FAILURE);
	}
	else if (pid > 0){
		// parent
		// close(filter_pipe[1]);
		child_pid = pid;
		b_connect();
	}
	else if (pid == 0) {
		// child thread
		// close(filter_pipe[0]);
		// dup2(filter_pipe[1], 1);
		// dup2(filter_pipe[1], 2);
		execv(chpl_argv[0], chpl_argv);
		perror("exec");
		exit(EXIT_FAILURE);
	}
}

//TODO: Change the thread disconnect system to work with c++.
void Chpldbg::disconnect_backend()
{
	pthread_cancel(child_thread);
	pthread_join(child_thread, nullptr);

	// only disconnect the gdbs, dont cleanup
	b_disconnect();

	waitpid(child_pid, nullptr, 0);
}

/**
 * Start threads to read and write from the consumer/producer fd's.
 * @return : 0, at the end of the program.
 */
int Chpldbg::run()
{
	std::thread front_thread(&Chpldbg::read_from_front, this);
	front_thread.join();
	return 0;
}

/**
 * 	Reads messages from the back buffer and writes them to
 * 	the pipe that the gui is reading from. 
 * 	Assumes that the backend is writing to the back buffer.
 * 	A potential major improvement would be to include a metric for the
 * 	rate at which messages are coming in, and spawning threads on demand
 * 	to handle that. This is only necessary should load testing reveal
 * 	that the system is lagging.
 * 	-furlong may 11
 * 	*/
void Chpldbg::write_to_front()
{
	// Worth changing to a proper async structure
	for (;;)
	{
		if (true)
		{
			//read from filter pipe
			//write(out_fd, message.c_str(), MSG_SIZE);
			//back_buffer.erase(back_buffer.begin());
		}
		
	}
}

/**
 * Reads from the gui pipe and writes to a buffer.
 * It expects an unmodified string from comminput.cxx.
 */
void Chpldbg::read_from_front()
{
	std::string command;
	std::string which_locales;
	char buf[MSG_SIZE] = { 0 };
	printf("(cdb) ");
	fflush(stdout);
	for (;;)
	{
		read(in_fd, buf, MSG_SIZE);
		if (buf[0] != 0)
		{
			std::string pipe_msg(buf);
			which_locales = pipe_msg.substr(0, pipe_msg.find(data_pass_delimiter));
			if (which_locales.compare("0")==0){
				size_t ix = pipe_msg.find(data_pass_delimiter) + 
					data_pass_delimiter.length();
				command = pipe_msg.substr(ix, std::string::npos);
				// fprintf(stderr, "call run_all...\n");
				b_run_all(strdup(command.c_str()));
			} else if (which_locales.compare(pipe_msg) == 0) {
				command = pipe_msg;
				// fprintf(stderr, "call run_all.....\n");
				b_run_all(strdup(command.c_str()));	

			} else{
				size_t ix = pipe_msg.find(data_pass_delimiter) +
					data_pass_delimiter.length();
				command = pipe_msg.substr(ix, std::string::npos);
				// fprintf(stderr, "call run_few...\n");
				b_run_few(strdup(command.c_str()),
					       	strdup(which_locales.c_str()));
			}
			b_set_callback_console(backend_callback);
			memset(buf, 0, MSG_SIZE);
		}
		//print "(cdb) " prompt again
		b_run_one(output_target_locale, strdup("printf \"(cdb) \"\n"));
	}
}

int Chpldbg::set_output_target_locale(int locale_num) {
	b_locale *l, **la;

	for (la = locales; (l = *la); la++){
		if (l->id == locale_num) {
			output_target_locale = l;
			return 0;
		}
	}
	return -1;
}

void signal_handler(int x)
{
	b_disconnect();
	printf("\n");
	exit(EXIT_FAILURE);
}

void test_function(char** args) {
	printf("the first arg is %s\n", args[0]);
}

int main(int argc, char** argv)
{
	if (pthread_mutex_init(&resource_mutex, NULL) != 0) {
    	printf("Error initializing mutex!\n");
    	return 1;
  	}
	int num_locales;
	signal(SIGINT, signal_handler); //Henry note: what does this do?
	if (argc >= 3)
	{
		num_locales = arg_parse(argv, argc);
		if (num_locales == 0){ usage(); }
		Chpldbg backend = Chpldbg(num_locales, STDOUT_FILENO, STDIN_FILENO, argv);
		backend.run();
	}
	else{ usage(); }
}
