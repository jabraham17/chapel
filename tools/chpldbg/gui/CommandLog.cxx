#include "CommandLog.h"

// Creates a new CommandLog.
// commands - the list of commands
// buf -  the buffer which displays the commands
CommandLog::CommandLog(vector<string> *commands, Fl_Text_Display *disp, Fl_Text_Buffer *buf) {
  this->disp = disp;
  this->buf = buf;
  this->commands = commands;
}

// This function is called from CommInput when the user presses the 'up' key.
// Returns the command entered most recently before the current command.
char* CommandLog::upCommand(){
  char* val = (char*) "";
  if(this->currCommand > 0){
    this->currCommand--;
  }
  if(this->commands->size() > 0){
    val = (char*) this->commands->at(this->currCommand).c_str();
    this->disp->scroll(this->currCommand, 0);
  }
  return val;
}

// Invoked from CommInput when the user presses the 'down' key.
// Returns the command entered most recently after the current command.
char* CommandLog::downCommand(){
  char* val = (char*) "";
  unsigned size = this->commands->size();
 
  if(this->currCommand < size){
    val = (char*) this->commands->at(this->currCommand).c_str();
    this->disp->scroll(this->currCommand, 0);
    this->currCommand++;
  }

  return val;
}

void CommandLog::moveToEnd(){
  unsigned size = this->commands->size();
  this->currCommand = size;
}

// Adds a command to the end of the list.
void CommandLog::add(string val){
  this->commands->push_back(val);
  val += "\n";
  this->buf->append(val.c_str());
  this->disp->scroll(this->currCommand, 0);
  this->currCommand = this->commands->size();
}

// Returns the buffer for this log
Fl_Text_Buffer* CommandLog::getBuf(){
  return this->buf;
}

