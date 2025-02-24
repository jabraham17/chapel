/*
 * Copyright 2020-2025 Hewlett Packard Enterprise Development LP
 * Copyright 2004-2019 Cray Inc.
 * Other additional copyright holders may be indicated within.
 *
 * The entirety of this work is licensed under the Apache License,
 * Version 2.0 (the "License"); you may not use this file except
 * in compliance with the License.
 *
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <string.h>
#include "chpllaunch.h"
#include "chpl-mem.h"
#include "error.h"

// To get CHPL_THIRD_PARTY from chpl invocation
#include "chplcgfns.h"

#define LAUNCH_PATH_HELP WRAP_TO_STR(LAUNCH_PATH)
#define WRAP_TO_STR(x) TO_STR(x)
#define TO_STR(x) #x


extern char** environ;
static void add_env_options(int* argc, char** argv[]) {
  int envc;
  int new_argc;
  char** new_argv;
  int i;

  if (environ == NULL)
    return;

  //
  // Count the number of environment entries.
  //
  for (i = 0; environ[i] != NULL; i++) ;
  envc = i;

  //
  // Create a new argv with space for -E options for the env vars.
  //
  new_argc = *argc + 2 * envc;
  new_argv = (char **)chpl_mem_allocMany(new_argc, sizeof((*argv)[0]),
                                         CHPL_RT_MD_COMMAND_BUFFER, -1, 0);

  //
  // Duplicate the old argv into the start of the new one.
  //
  memcpy(new_argv, (*argv), *argc * sizeof((*argv)[0]));

  //
  // Add a -E option for each environment variable.
  //
  for (i = 0; i < envc; i++) {
    new_argv[*argc + 2 * i + 0] = (char*) "-E";
    new_argv[*argc + 2 * i + 1] = environ[i];
  }

  //
  // Return the new argv.
  //
  *argc = new_argc;
  *argv = new_argv;
}

static char** chpl_launch_create_argv(const char *launch_cmd,
                                      int argc, char* argv[],
                                      int32_t numLocales) {
  static char nlbuf[16];
  int largc;
  const int largv_size = 7;
  char *largv[largv_size];

  largc = 0;
  largv[largc++] = (char *) launch_cmd;
  largv[largc++] = (char *) "-np";
  snprintf(nlbuf, sizeof(nlbuf), "%d", numLocales);
  largv[largc++] = nlbuf;

  {
    const char* ev_use_chpldbg = getenv("CHPL_USE_CHPLDBG");
    const char* ev_cdb_port = getenv("CDB_PORT");
    const char* ev_test = getenv("CHPL_USE_CDB");

    // if we are using chpldbg we should start the program with gdbserver//lldbserver
    if (ev_use_chpldbg != NULL) {
      char *server_path = chpl_mem_alloc(PATH_MAX,
                                        CHPL_RT_MD_COMMAND_BUFFER, -1, 0);
      char buf[512];
      gethostname(buf, 512);

      int addr_len = strlen("localhost:") + sizeof(ev_cdb_port);
      char *server_addr = chpl_mem_alloc(addr_len, CHPL_RT_MD_COMMAND_BUFFER, -1, 0);
      snprintf(server_addr, addr_len, "localhost:%s", ev_cdb_port);


      if (strcmp(ev_use_chpldbg, "lldb") == 0) { // use lldb
        if (chpl_run_cmdstr("which lldb-server", server_path, PATH_MAX) > 0) {
          // lldb-server g localhost:3333 -- ./test_real ...
          largv[largc++] = server_path;
          largv[largc++] = (char *) "g";
          largv[largc++] = server_addr;
          largv[largc++] = (char *) "--";
        }
      }
      else if(strcmp(ev_use_chpldbg, "gdb") == 0) { // Use gdb
        if (chpl_run_cmdstr("which gdbserver", server_path, PATH_MAX) > 0) {
          largv[largc++] = server_path;
          largv[largc++] = server_addr;
        } else {
          chpl_warning("chpl_comm_use_cdb ignored because no gdbserver", 0, 0);
        }
      }
    }
  }
  {
    const char* s = getenv("GASNET_SPAWNFN");
    if (s == NULL || strcmp(s, "S") == 0)
      add_env_options(&argc, &argv);
  }

  return chpl_bundle_exec_args(argc, argv, largc, largv);
}

int chpl_launch(int argc, char* argv[], int32_t numLocales,
                int32_t numLocalesPerNode) {

  if (numLocalesPerNode > 1) {
    chpl_launcher_no_colocales_error(NULL);
  }

  int len = strlen(CHPL_THIRD_PARTY) + strlen(WRAP_TO_STR(LAUNCH_PATH)) + strlen("amudprun") + 2;
  char *cmd = chpl_mem_allocMany(len, sizeof(char), CHPL_RT_MD_COMMAND_BUFFER, -1, 0);
  snprintf(cmd, len, "%s/%samudprun", CHPL_THIRD_PARTY, WRAP_TO_STR(LAUNCH_PATH));

  return chpl_launch_using_exec(cmd,
                                chpl_launch_create_argv(cmd, argc, argv,
                                                        numLocales),
                                argv[0]);
}


int chpl_launch_handle_arg(int argc, char* argv[], int argNum,
                           int32_t lineno, int32_t filename) {
  return 0;
}


const argDescTuple_t* chpl_launch_get_help(void) {
  return NULL;
}
