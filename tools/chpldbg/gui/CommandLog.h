#ifndef COMMAND_LOG_
#define COMMAND_LOG_
#include <FL/Fl_Group.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Text_Buffer.H>
#include <vector>
#include <string>
using namespace std;

class CommandLog {
 public:
  CommandLog(vector<string> *commands, Fl_Text_Display *disp, Fl_Text_Buffer *buf);
  char* upCommand();
  char* downCommand();
  void moveToEnd();
  void add(string val);
  Fl_Text_Buffer * getBuf();
  
 private:

  Fl_Text_Display *disp;
  Fl_Text_Buffer *buf;
  vector<string> *commands;
  unsigned currCommand;
};

#endif
