#ifndef CommInput_h_
#define CommInput_h_

#include <FL/Fl_Group.H>
#include <FL/Fl_Input.H>
#include <FL/Fl.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Text_Editor.H>
#include <FL/Fl_Widget.H>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <iostream>
#include <unistd.h>
#include "CommandLog.h" 
#include "chpldbg_fl.h"
using namespace std;

class CommInput : public Fl_Group {

 public:
  Fl_Text_Buffer *buf;
  vector<string> *commands;
  CommandLog *commLog;
 
  /* Constructor */ 
  CommInput(int x, int y,int w, int h, CommandLog *commLog, int w_fd, const char *l = 0);
  int handle(int event);

 private:  
  Fl_Input *in;
  int w_fd; 
};
#endif
