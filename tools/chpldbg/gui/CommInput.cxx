#include "CommInput.h"
unsigned currInput = 0;

void enterCallback(Fl_Widget* w, void* data, int w_fd){
    Fl_Input* i = (Fl_Input *) w;
    CommInput* comm = (CommInput*) data;
    string val = (string) i->value();

    comm->commLog->add(val);
    i->value("");

    // catch the restart command and make sure that
    // it doesn't go to the backend
    if (!val.compare("r")) {
        return;
    }
    
    if (val.compare("")){
        val += "\n";
	
	string which_locales = val.substr(0, val.find("///"));
	if(which_locales.compare(val) == 0){
		int idx = tabs->find(tabs->value());
		val = std::to_string(idx) + "///" + val;
	}
        write(w_fd, val.c_str(), val.length());
    }
}

CommInput::CommInput(int x, int y, int w, int h, CommandLog* commLog, int w_fd, const char *l)
  : Fl_Group(x, y, w, h, l){
  this->commLog = commLog;
  this->in = new Fl_Input(x, y, w, h);
  this->w_fd = w_fd;
  Fl_Group::current()->resizable(in);
}

int CommInput::handle(int e){
    int c;
    switch(e) {
        case FL_KEYUP:
            c = Fl::event_key();

            //check if key is the up arrow
            if (c == FL_Up) {
              char* val = this->commLog->upCommand();
              if(strcmp(val, "")){
                this->in->value(val);
              }
            }

            //check if key is the down arrow
            if (c == FL_Down) {
              char* val = this->commLog->downCommand();
              this->in->value(val);
            }

            //pass the command if enter is pressed
            if (c == FL_Enter) {
                enterCallback(this->in, this, this->w_fd);
            }
            return 1;

        default:
            return Fl_Group::handle(e);
    }
    return 1;
}
