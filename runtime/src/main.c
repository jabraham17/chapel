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

#include "chplrt.h"
#include "chpl_rt_utils_static.h"
#include "chpl-init.h"
#include "chplexit.h"
#include "config.h"

static void fixArgs(int * argc, char* argv[], char * newargv[]){

  int nonenviro = 0;
  int enviro = 0;

  char arg_buffer[2048];
  char * arg_buffer_ptr = &arg_buffer[0];

  //advance to first environment variable
  int i = 0;
  char * currentarg = argv[0];
  while (!(currentarg[0] == '-' && currentarg[1] == 'E') ){
    newargv[i] = currentarg;
    i++;
    currentarg = argv[i];
  }

  nonenviro = i; // number of non environmental args

  // now i is at first -E flag

  while (argv[i] != NULL){

    enviro += 2; // 2 args for each (flag + arg)

    arg_buffer_ptr = &arg_buffer[0];

    //find each part of the original argument
    i++;
    currentarg = argv[i]; // this should be KEY=value part. The rest may be orphaned values.
    sprintf(arg_buffer_ptr, "%s", argv[i]);
    arg_buffer_ptr += strlen(arg_buffer_ptr);
    i++;
    currentarg = argv[i];
    while (currentarg != NULL && !(currentarg[0] == '-' && currentarg[1] == 'E') ){

      sprintf(arg_buffer_ptr, " %s", argv[i]);
      arg_buffer_ptr += strlen(arg_buffer_ptr);
      i++;
      currentarg = argv[i];
    }

    //here arg_buffer should contain the whole arg
    char * new_arg = (char*) malloc(sizeof(char) * (strlen(arg_buffer) + 1));
    memcpy(new_arg, arg_buffer, (strlen(arg_buffer) + 1));

    newargv[(nonenviro - 1) + enviro - 1] = (char*)"-E";
    newargv[(nonenviro - 1) + enviro] = new_arg;

  }

  *argc = nonenviro + enviro;
  newargv[(nonenviro - 1) + enviro + 1] = NULL;

}

int main(int argc, char* argv[]) {

  // char * newargv[argc];

  // int newargc = argc;
  // fixArgs(&newargc, argv, newargv); 
  // Initialize the runtime
  // chpl_rt_init(newargc, newargv);
  chpl_rt_init(argc, argv);

  // Run the main function for this node.
  chpl_task_callMain(chpl_executable_init);

  // have everyone exit, returning the value returned by the user written main
  // or 0 if it didn't return anything
  chpl_exit_all(chpl_gen_main_arg.return_value);

  return 0; // should never get here
}
