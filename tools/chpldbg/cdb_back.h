#ifndef _CDB_H_
#define _CDB_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "mi_gdb.h"

#define LOCALE_RDY  0
#define LOCALE_RUN  1

/* struct for locale used by the backend, but accessible to the frontend */
/* generally, the frontend can read but should not modify these fields */
/* TODO: this struct needs reworking and needs comments.... what do half
 * of these variables do */
struct locale_st {
	// set after b_init()
	int    id;
	int    port;
	char  *name;
	int    status;
	char *vartype; //used for 'pc' callback
	int   run_sel;
	mi_h  *mi;
	pthread_t t;
	pthread_mutex_t sl;
};
typedef struct locale_st b_locale;

/* defined in chapel_back.c, these have useful values only after calling b_init()
 * num_locales: the number of running locales
 * locales: a null-terminated array of b_locale pointers */
extern pthread_mutex_t resource_mutex;
extern int        num_locales;
extern b_locale **locales;
/* call b_init() after setting up the gdbservers, before doing anything else
 * call b_cleanup() before exiting */
void b_init(int nl);
void b_connect();
void b_disconnect();
void b_cleanup();

/* sets run_sel for every locale, which is used by b_run_few()
 * returns 1 for success, 0 for failure (bad select string)    */
/* examples:
 * sel = "1,2,4"   -- locales 1, 2, 4
 * sel = "1,2-4,6" -- locales 1, 2, 3, 4, 6
 * sel = "2,1"     -- error, bad select string
 * sel = "1,24"    -- locales 1, 24; error if 24 is out of range
 */
int  b_parse_run_sel(char* select);

/* b_run_one(): runs cmd on one locale
 * b_run_all(): runs cmd on all locales
 * b_run_few(): runs cmd on all locales where run_sel is true */
/* note: for all "run" functions, if a locale has status other than LOCALE_RDY,
 * that locale is skipped and an error msg is produced */
/* these functions update status from LOCALE_RDY to LOCALE_RUN */
void b_run_one(b_locale* l, char* cmd);
void b_run_all(char* cmd);
void b_run_few(char* cmd, char* sel);

/* callback functions -- it's a good idea to set these up after b_init(), but
 * before calling b_wait* for the first time */
/* these functions are called as a consequence of calling the b_wait functions */
typedef void (*b_cb_msg)(b_locale* l, char* msg);
typedef void (*b_cb_stat)(b_locale* l, int status);
void b_set_callback_console(b_cb_msg f);
void b_set_callback_log(b_cb_msg f);
void b_set_callback_error(b_cb_msg f);
void b_set_callback_status(b_cb_stat f);
void backend_callback(b_locale* l, char* msg);

#ifdef __cplusplus
}
#endif

#endif
