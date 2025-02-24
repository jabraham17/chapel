#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <string>
#include <vector>
#include <sstream>
#include <unistd.h>
#include <sys/wait.h>
#include <unordered_map>
#include <pthread.h>
#include <time.h>
#include <functional>
#include "cdb_back.h"
#include "builtin.h"

/* callback functions */
b_cb_msg  _b_cb_con  = NULL;
b_cb_msg  _b_cb_log  = NULL;
b_cb_msg  _b_cb_err  = NULL;
b_cb_stat _b_cb_stat = NULL;

/* Function Prototypes */
void con_f(b_locale *l, const char* fmt, ...);
void log_f(b_locale *l, const char* fmt, ...);
void err_f(b_locale *l, const char* fmt, ...);
void *reader_thread(void *arg);

/* helper functions */
void b_fatal_error(const char* msg) {
	perror(msg);
	b_cleanup();
	exit(1);
}

//////////////////////////////
// init / connect / cleanup //
//////////////////////////////

/* extern globals */
int num_locales    = 0;
b_locale **locales = NULL;
pthread_mutex_t resource_mutex;

/* Map from user commands to corresponding functions */
typedef void (*FunctionPtr)(std::vector<std::string>);
typedef std::unordered_map<std::string, FunctionPtr> functionMap;
functionMap builtin_command_map = { 
	{"q", &quit_handler},
	{"quit", &quit_handler},
	{"pc", &print_chapel_handler},
	{"print-chapel", &print_chapel_handler},
	{"r", &run_handler}
};

/* helpers / saved references */
char  *locale_string;
char **locale_names;

int port_from_env() {
	char *s;

	s = getenv("CDB_PORT");
	if (!s || !*s)
		b_fatal_error("CDB_PORT not set");
	else
		return atoi(s);

	// else compiler complains
	return 0;
}

int servers_from_env() {
	char *s;
	int i, n, st;

	s = getenv("GASNET_SSH_SERVERS");
	if (!s || !*s)
		b_fatal_error("GASNET_SSH_SERVERS not set"); 
	locale_string = strdup(s);

	// count + null terminate
	n  = 0;
	st = 0;
	for (s = locale_string; *s; s++) {
		if (st == 0 && !isspace(*s)) {
			st = 1;
			n++;
		} else if (st == 1 && isspace(*s)) {
			st = 0;
			*s = '\0';
		}
	}

	locale_names = (char **) malloc(sizeof(char *) * (n + 1));

	// fill
	s = locale_string;
	for (i = 0; i < n; i++) {
		for ( ; isspace(*s); s++) ; // skip whitespace
		locale_names[i] = s;
		while (*s++) ;
	}
	locale_names[n] = NULL;

	return n;
}

void console_cb_bridge(const char *s, void *arg) {
	con_f((b_locale *) arg, s);
}

/* functions from cdb.h */
void b_init(int nl) {
	int i, port, servers;
	b_locale *l;

	// read environment vars
	port    = port_from_env();
	if (port < 1024 || port > 65536)
		b_fatal_error("Port out of range");

	servers = servers_from_env();
	if (servers < nl)
		b_fatal_error("Too few servers in GASNET_SSH_SERVERS");

	num_locales = nl;
	locales = (b_locale **) malloc(sizeof(b_locale *) * (nl + 1));
	for (i = 0; i < nl; i++) {
		l = locales[i] = (b_locale *) malloc(sizeof(b_locale));
		l->id     = i;
		l->port   = port + i; // vary the port for oversubscribing
		l->name   = locale_names[i];
		l->vartype = NULL;
		l->status = LOCALE_RDY;	
	}
	locales[nl] = NULL;
}

void b_connect() {
	b_locale *l, **la;
	char buf[64];

	// create gdbs
	for (la = locales; (l = *la); la++) {
		l->mi = mi_connect_local();
		if (!l->mi)
			b_fatal_error("Couldn't create local gdb");
		// Set connect timeout to 2 seconds, to prevent forever blocking
		// mi_send(l->mi, "-gdb-set tcp connect-timeout 10\n");
	}

	clock_t start = clock();
	clock_t timeout_limit = 15 * CLOCKS_PER_SEC;
	// connect to servers
	for (la = locales; (l = *la); la++) {
		snprintf(buf, 64, "%s:%d", l->name, l->port);
		while (!gmi_target_select(l->mi, "remote", buf)) {
			if (clock() - start > timeout_limit) {
				b_fatal_error("Ran out of time attempting to connect to locales\n");
			}
		}
	}

	// connect console callbacks
	for (la = locales; (l = *la); la++){
		mi_set_console_cb(l->mi, console_cb_bridge, l);
	}

	// start up read threads
	for (la = locales; (l = *la); la++) {
		pthread_create(&l->t, NULL, &reader_thread, l);
		pthread_mutex_init(&l->sl, NULL);
	}

	// status callback
	if (_b_cb_stat)
		for (la = locales; (l = *la); la++)
			_b_cb_stat(l, LOCALE_RDY);
}

void b_disconnect() {
	b_locale *l, **la;
	for (la = locales; (l = *la); la++) {	
		// kill read thread & destory mutex
		pthread_cancel(l->t); // this might be bad...	
		// kill local gdb
		gmi_gdb_exit(l->mi);
	}
	system("pkill amudprun");
	system("pkill gdb");
}

void b_cleanup() {
	b_locale *l, **la;
	for (la = locales; (l = *la); la++)
		free(l);
	free(locale_string);
	free(locale_names);
}

/* reader threads */
int handle_response(b_locale *l) {
	mi_output *o;
	int t;

	log_f(l, "got response...\n");
	o = mi_retire_response(l->mi);
	t = o->tclass;
	switch(t) {
		case 1:
			log_f(l, "stopped\n");
			break;
		case 2:
			log_f(l, "done\n");
			break;
		case 3:
			log_f(l, "running\n");
			break;
		case 5:
			log_f(l, "error\n");
			break;
		default:
			log_f(l, "???\n");
			break;
	}
	mi_free_output(o);
	return t == 1 || t == 2 || t == 5;
}

void *reader_thread(void *arg) {
	b_locale *l;

	l = (b_locale *) arg;
	for (;;) {
		while (!mi_get_response(l->mi))
			usleep(1000);
		if (handle_response(l)) {
			pthread_mutex_lock(&l->sl);
			l->status = LOCALE_RDY;
			if (_b_cb_stat)
				_b_cb_stat(l, LOCALE_RDY);
			pthread_mutex_unlock(&l->sl);
		}
	}
}

/* helper for b_run_few */
int parse_sel_string(char* s) {
	//TODO need to fix this for current implementation of b_run_few
	char* ns;
	int s_n, s_cd, f_cd, cur, last, i;

	f_cd = 0;
	s_n  = 0;
	s_cd = 0;
	last = -1;

	for ( ; f_cd != 2; s++) {
		f_cd = 0;
		switch(*s) {
			case ' ':
			case '\0':
				f_cd++;
			case '-':
				f_cd++;
			case ',':
				if (!s_n)
					return 0;
				s_n = 0;
				*s  = '\0';
				cur = atoi(ns);
				if (cur > last && cur <= num_locales) {  // I think this was off by one... -Paul F.
					for (i = last+1; i < cur; i++)
						locales[i]->run_sel = s_cd;
					locales[cur-1]->run_sel = 1;
					last = cur;
				} else {
				        return 0;
				}
				if (f_cd < 2)
					s_cd = f_cd;
				if (f_cd == 2)
					for (i = cur+1; i < num_locales; i++)
						locales[i]->run_sel = 0;
				break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				if (!s_n) {
					s_n = 1;
					ns  = s;
				}
				break;
			default:
				return 0;
		}
	}
	return 1;
}

// Note: This function returns a pointer to a substring of the original string.
// If the given string was allocated dynamically, the caller must not overwrite
// that pointer with the returned value, since the original pointer must be
// deallocated using the same allocator with which it was allocated.  The return
// value must NOT be deallocated using free() etc.
char *trimwhitespace(char *str)
{
  char *end;

  // Trim leading space
  while(isspace((unsigned char)*str)) str++;

  if(*str == 0)  // All spaces?
    return str;

  // Trim trailing space
  end = str + strlen(str) - 1;
  while(end > str && isspace((unsigned char)*end)) end--;

  // Write new null terminator character
  end[1] = '\0';

  return str;
}

void backend_callback(b_locale* l, char* msg);

// If command is a builtin command, runs the command's function and
// returns 1, returns 0 if not a builtin command.
int builtin(char* cmd) {
	//lock mutex
	//parse command
	std::string command_name;
	std::vector<std::string> args;
	std::stringstream ss(cmd);
	ss >> command_name;

	std::string arg;
	while (ss >> arg) {
		args.push_back(arg);
	}
	if (builtin_command_map.find(command_name) != builtin_command_map.end()) {
		FunctionPtr func = builtin_command_map[command_name];
		func(args);
		return 1;
	}
	return 0;
}

void pc_callback(b_locale *l, char *msg) {
	b_set_callback_console(NULL);
	char *fnname = msg + 7;
	trimwhitespace(fnname);
	(*locales)->vartype = fnname;
	b_set_callback_console(backend_callback);
}


void print_chapel_handler(std::vector<std::string> args) {
	char * varname = &args[0][0];
	b_set_callback_console(pc_callback);
	mi_send((*locales)->mi, "whatis %s\n", varname); //send to first locale
	while ((*locales)->vartype == NULL); //wait for gdb to send type
	mi_send((*locales)->mi, "p chpl_debug_print_%s(&%s, 0, 0)\n", (*locales)->vartype, varname);
	(*locales)->vartype = NULL;
}

/* from cdb.h */
void b_run_one(b_locale *l, char* cmd) {
	pthread_mutex_lock(&l->sl);
	if (true) { // used to be l->status == LOCALE_RDY, didnt let commands be run more than once?
	            // change back if it ever causes a problem
		l->status = LOCALE_RUN;
		if (_b_cb_stat)
			_b_cb_stat(l, LOCALE_RUN);
		mi_send(l->mi, "%s", cmd); // send the command to the gdb
	} else {
		err_f(l, "locale skipped, already running\n");
	}
	pthread_mutex_unlock(&l->sl);
}



void b_run_all(char *cmd) {
	if (builtin(cmd)) {
		
	}
	else {
		b_locale *l, **la;
		for (la = locales; (l = *la); la++){
			b_run_one(l, cmd);
		}
	}
	
}

void b_run_few(char *cmd, char *sel) {
	b_locale *l, **la;
	char *s;
	int r;
	
	s = strdup(sel);
	r = parse_sel_string(s);
	free(s);

	
	if (r) {
		for (la = locales; (l = *la); la++)
			if (l->run_sel)
				b_run_one(l, cmd);
	} else
		err_f(NULL, "bad select string \"%s\"\n", sel);
}

// these are defined above
// b_cb_msg  _b_cb_con  = NULL;
// b_cb_msg  _b_cb_log  = NULL;
// b_cb_msg  _b_cb_err  = NULL;
// b_cb_stat _b_cb_stat = NULL;

void b_set_callback_console(b_cb_msg f) {
	_b_cb_con = f;
}

void b_set_callback_log(b_cb_msg f) {
	_b_cb_log = f;
}

void b_set_callback_error(b_cb_msg f) {
	_b_cb_err = f;
}

void b_set_callback_status(b_cb_stat f) {
	_b_cb_stat = f;
}

/* callback implementation */
#define MSG_BUF_SIZE 1024
char msg_buf[MSG_BUF_SIZE];

void con_f(b_locale* l, const char* fmt, ...) {
	va_list args;
	if (!_b_cb_con)
		return;
	va_start(args, fmt);
	vsnprintf(msg_buf, MSG_BUF_SIZE, fmt, args);
	va_end(args);
	_b_cb_con(l, msg_buf);
}

void log_f(b_locale* l, const char* fmt, ...) {
	va_list args;

	if (!_b_cb_log)
		return;
	va_start(args, fmt);
	vsnprintf(msg_buf, MSG_BUF_SIZE, fmt, args);
	va_end(args);
	_b_cb_log(l, msg_buf);
}

void err_f(b_locale* l, const char* fmt, ...) {
	va_list args;

	if (!_b_cb_err)
		return;
	va_start(args, fmt);
	vsnprintf(msg_buf, MSG_BUF_SIZE, fmt, args);
	va_end(args);
	_b_cb_err(l, msg_buf);
}
