#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "LocaleTab.h"
#include "chpldbg_fl.h"

LocaleTab::LocaleTab (int x, int y, int w, int h, Fl_Text_Buffer *buf, const char *l)
  : Fl_Group(x, y, w, h, l){
    this->buf = buf;
    display = new Fl_Text_Display(x + 15,y + 25,w - 50,h - 50);
    display->buffer(buf);
    Fl_Group::current()->resizable(display);
}

LocaleTab::~LocaleTab(){
    delete display;
}

void LocaleTab::appendText(char *c){
    this->buf->append(c);
}
